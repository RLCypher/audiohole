#include "native_runtime.hpp"
#include "taskbar_icon_cache.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace audiohole::plug_board::windows_detail {
namespace {
std::string stamp(const std::string& value) {
    WIN32_FILE_ATTRIBUTE_DATA information{};
    if (value.empty() ||
        !GetFileAttributesExW(wide(value).c_str(), GetFileExInfoStandard, &information))
        return "unavailable";
    return std::to_string(file_time(information.ftLastWriteTime)) + ':' +
           std::to_string(information.nFileSizeHigh) + ':' +
           std::to_string(information.nFileSizeLow);
}
std::string cache_key(const ApplicationObservation& application, unsigned pixels,
                      const WindowIconSnapshot& window, const ShortcutIconSnapshot& shortcut) {
    // File metadata is queried on the Shell worker, never during GUI drawing.
    // Process IDs do not key icon identity. A changed executable path, file
    // version timestamp, metadata resource or DPI size creates a new cache key.
    Document sources = Document::array();
    std::set<std::string> images;
    if (application.identity.kind == IdentityKind::Win32Image)
        images.insert(application.identity.value);
    for (const auto& session : application.sessions)
        if (session.process)
            images.insert(session.process->executable_path);
    for (const auto& image : images)
        sources.push_back({image, stamp(image)});
    std::set<std::string> resources;
    for (const auto& session : application.sessions)
        if (!session.icon_path.empty())
            resources.insert(session.icon_path);
    return Document{{"kind", static_cast<int>(application.identity.kind)},
                    {"identity", application.identity.value},
                    {"pixels", pixels},
                    {"taskbar_window", window.signature},
                    {"shortcut", {shortcut.match, shortcut.path, shortcut.resource}},
                    {"images", std::move(sources)},
                    {"resources", resources}}
        .dump();
}
} // namespace

// Retry only during a finite startup window. Audio discovery can precede the
// game window and WM_SETICON; a successful engine-resource lookup is not final.
// There is no recurring window enumeration after the final deadline, and hiding
// the view cancels every deadline. All foreign calls stay on this owned STA.
struct IconWorker::Implementation {
    using Clock = std::chrono::steady_clock;
    static constexpr std::array<std::chrono::milliseconds, 5> startup_delays{
        std::chrono::milliseconds(250), std::chrono::milliseconds(1000),
        std::chrono::milliseconds(3000), std::chrono::milliseconds(10000),
        std::chrono::milliseconds(30000)};
    struct Request {
        ApplicationObservation application;
        unsigned pixels;
        std::uint64_t generation;
        std::uint64_t sequence;
        std::uint64_t epoch;
        Clock::time_point started;
        Clock::time_point due;
        std::size_t retry = 0;
    };
    struct Cached {
        ApplicationImage image;
        Clock::time_point expires;
        std::uint64_t used;
    };
    struct StartupWindow {
        std::string processes;
        Clock::time_point started;
        std::uint64_t used;
    };
    BackendCallbacks callbacks;
    std::shared_ptr<NativeCounters> counters;
    std::filesystem::path cache_file;
    std::mutex mutex;
    std::condition_variable_any ready;
    std::map<std::string, Request> pending;
    std::map<std::string, std::uint64_t> latest;
    std::map<std::string, StartupWindow> startups;
    std::map<std::string, Cached> cache;
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<std::uint64_t> source_epoch{1};
    bool suspended = false;
    std::jthread worker;
    Implementation(BackendCallbacks value, std::shared_ptr<NativeCounters> metrics,
                   std::filesystem::path root)
        : callbacks(std::move(value)), counters(std::move(metrics)),
          cache_file(root.empty() ? std::filesystem::path{} : root / "icons.json") {
        worker = std::jthread([this](std::stop_token stop) { run(stop); });
    }
    static std::string request_key(const Request& request) {
        return request.application.observation_key + ':' + std::to_string(request.pixels);
    }
    bool current(const Request& request) const {
        const auto found = latest.find(request_key(request));
        return !suspended && request.epoch == source_epoch.load() && found != latest.end() &&
               found->second == request.sequence;
    }
    void report(const OperationResult& result) {
        if (!result.ok && callbacks.diagnostic)
            callbacks.diagnostic({utc_timestamp(), Severity::Warning, result.code, result.detail});
    }
    void run(std::stop_token stop) {
        ++counters->workers;
        Apartment apartment(true);
        TaskbarIconCache retained(cache_file);
        report(retained.load_result());
        try {
            require(apartment.result(), "Initialize cached Shell icon STA");
            std::uint64_t indexed_epoch = 0;
            std::vector<ShortcutIconEntry> shortcut_index;
            std::optional<Clock::time_point> flush_due;
            while (!stop.stop_requested()) {
                std::optional<Request> request;
                {
                    std::unique_lock lock(mutex);
                    while (!stop.stop_requested()) {
                        const auto next =
                            std::min_element(pending.begin(), pending.end(),
                                             [](const auto& first, const auto& second) {
                                                 return first.second.due < second.second.due;
                                             });
                        const auto now = Clock::now();
                        if (flush_due && now >= *flush_due)
                            break;
                        if (next != pending.end() && now >= next->second.due) {
                            request = std::move(next->second);
                            pending.erase(next);
                            break;
                        }
                        auto deadline = flush_due.value_or(Clock::time_point::max());
                        if (next != pending.end())
                            deadline = std::min(deadline, next->second.due);
                        const auto observed = sequence.load();
                        ready.wait_until(lock, stop, deadline,
                                         [&] { return sequence.load() != observed; });
                    }
                }
                if (stop.stop_requested())
                    break;
                if (!request) {
                    report(retained.flush());
                    flush_due.reset();
                    continue;
                }
                ApplicationImage image;
                std::string retained_key;
                try {
                    const auto epoch = source_epoch.load();
                    if (epoch != indexed_epoch) {
                        cache.clear();
                        shortcut_index = read_shortcut_icons();
                        indexed_epoch = epoch;
                    }
                    WindowIconSnapshot window;
                    try {
                        window = observe_window_icon(request->application, request->pixels);
                    } catch (...) {
                        window.signature = "taskbar_metadata_unavailable";
                    }
                    const auto shortcut =
                        observe_shortcut_icon(request->application, window, shortcut_index);
                    const auto key =
                        cache_key(request->application, request->pixels, window, shortcut);
                    const auto cached = cache.find(key);
                    // A window can update an HICON in place. Startup retries
                    // decode it again even when the borrowed handle is unchanged.
                    if (request->retry == 0 && cached != cache.end() &&
                        Clock::now() < cached->second.expires) {
                        ++counters->icon_cache_hits;
                        cached->second.used = ++sequence;
                        image = cached->second.image;
                    } else {
                        ++counters->icon_extractions;
                        image = resolve_icon_pixels(request->application, request->pixels, &window,
                                                    &shortcut);
                        const auto expires = image.result.ok && image.application_specific
                                                 ? Clock::time_point::max()
                                                 : Clock::now() + std::chrono::seconds(60);
                        cache[key] = {image, expires, ++sequence};
                        auto bytes = [&] {
                            std::size_t total = 0;
                            for (const auto& [name, entry] : cache)
                                total += entry.image.rgba.size();
                            return total;
                        };
                        while (cache.size() > 512 || bytes() > 64 * 1024 * 1024) {
                            const auto oldest =
                                std::min_element(cache.begin(), cache.end(),
                                                 [](const auto& first, const auto& second) {
                                                     return first.second.used < second.second.used;
                                                 });
                            cache.erase(oldest);
                        }
                    }
                    // Full canonical image + file version is stable across
                    // process lifetimes. Runtime windows/session resources and
                    // requested DPI must not make a closed-app cache miss.
                    const auto identity = canonical_file_identity(request->application.identity);
                    retained_key = Document{{"kind", static_cast<int>(identity.kind)},
                                            {"identity", identity.value},
                                            {"file", identity.kind == IdentityKind::Win32Image
                                                         ? stamp(identity.value)
                                                         : std::string{}}}
                                       .dump();
                    if (const auto* previous = retained.find(retained_key)) {
                        const bool weaker_class_icon =
                            image.source == "taskbar_window_class_icon" &&
                            previous->source != "taskbar_window_class_icon";
                        if (!image.result.ok || !image.source.starts_with("taskbar_") ||
                            weaker_class_icon) {
                            const auto fallback = image.source;
                            image = scale_icon_pixels(*previous, request->pixels);
                            image.evidence["retained_taskbar"] = true;
                            image.evidence["current_fallback_source"] = fallback;
                        }
                    }
                } catch (const std::exception& error) {
                    image.result = {false, "icon_resolution_failed", error.what()};
                    image.source = "generic_fallback";
                }
                bool deliver;
                {
                    std::lock_guard lock(mutex);
                    deliver = !stop.stop_requested() && current(*request);
                    if (deliver) {
                        // Retained pixels are a fallback only, not new evidence
                        // to persist. A fresh taskbar observation always wins.
                        if (!image.evidence.value("retained_taskbar", false))
                            retained.remember(retained_key, image);
                        const auto key = request_key(*request);
                        bool scheduled = false;
                        if (!request->application.verified_processes.empty())
                            while (request->retry < startup_delays.size()) {
                                const auto due =
                                    request->started + startup_delays[request->retry++];
                                if (due <= Clock::now())
                                    continue;
                                request->due = due;
                                pending.insert_or_assign(key, *request);
                                scheduled = true;
                                break;
                            }
                        if (!scheduled)
                            latest.erase(key);
                    }
                }
                if (retained.dirty() && !flush_due)
                    flush_due = Clock::now() + std::chrono::milliseconds(500);
                if (deliver && callbacks.icon_ready)
                    callbacks.icon_ready(request->generation, request->application.observation_key,
                                         std::move(image));
            }
        } catch (const std::exception& error) {
            if (callbacks.diagnostic)
                callbacks.diagnostic(
                    {utc_timestamp(), Severity::Error, "icon_worker_failed", error.what()});
        }
        report(retained.flush());
        cache.clear();
        --counters->workers;
    }
};

IconWorker::IconWorker(BackendCallbacks callbacks, std::shared_ptr<NativeCounters> counters,
                       std::filesystem::path cache_root)
    : implementation_(std::make_unique<Implementation>(std::move(callbacks), std::move(counters),
                                                       std::move(cache_root))) {}

IconWorker::~IconWorker() {
    shutdown();
}
void IconWorker::request(const std::vector<ApplicationObservation>& applications, unsigned pixels,
                         std::uint64_t generation) {
    if (pixels < 16 || pixels > 256)
        return;
    std::lock_guard lock(implementation_->mutex);
    implementation_->suspended = false;
    for (const auto& application : applications) {
        const auto key = application.observation_key + ':' + std::to_string(pixels);
        if (implementation_->pending.size() >= 256 && !implementation_->pending.contains(key)) {
            const auto oldest =
                std::min_element(implementation_->pending.begin(), implementation_->pending.end(),
                                 [](const auto& first, const auto& second) {
                                     return first.second.sequence < second.second.sequence;
                                 });
            implementation_->latest.erase(oldest->first);
            implementation_->pending.erase(oldest);
            ++implementation_->counters->dropped;
        }
        const auto sequence = ++implementation_->sequence;
        implementation_->latest[key] = sequence;
        const auto now = Implementation::Clock::now();
        auto started = now;
        if (!application.verified_processes.empty()) {
            std::set<std::string> processes;
            for (const auto& token : application.verified_processes)
                processes.insert(Document{{"pid", token.pid},
                                          {"created", token.creation_time},
                                          {"image", token.executable_path}}
                                     .dump());
            const auto signature = Document(processes).dump();
            auto found = implementation_->startups.find(key);
            if (found != implementation_->startups.end() && found->second.processes == signature) {
                // Session churn must not restart a completed startup window for
                // the same process lifetime. Explicit hide/Refresh resets it.
                started = found->second.started;
                found->second.used = sequence;
            } else
                implementation_->startups[key] = {signature, now, sequence};
            while (implementation_->startups.size() > 256) {
                const auto oldest = std::min_element(
                    implementation_->startups.begin(), implementation_->startups.end(),
                    [](const auto& left, const auto& right) {
                        return left.second.used < right.second.used;
                    });
                implementation_->startups.erase(oldest);
            }
        }
        implementation_->pending.insert_or_assign(
            key, Implementation::Request{application, pixels, generation, sequence,
                                         implementation_->source_epoch.load(), started, now, 0});
    }
    implementation_->ready.notify_one();
}
void IconWorker::cancel_pending() {
    std::lock_guard lock(implementation_->mutex);
    implementation_->suspended = true;
    implementation_->pending.clear();
    implementation_->latest.clear();
    implementation_->startups.clear();
    ++implementation_->source_epoch;
    ++implementation_->sequence;
    implementation_->ready.notify_all();
}
void IconWorker::invalidate_sources() {
    cancel_pending();
}
void IconWorker::shutdown() {
    if (implementation_->worker.joinable()) {
        implementation_->worker.request_stop();
        implementation_->ready.notify_all();
        implementation_->worker.join();
    }
}

} // namespace audiohole::plug_board::windows_detail
