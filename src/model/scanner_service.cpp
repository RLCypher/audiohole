#include "model/scanner_service.h"
#include "audiohole/scoped_scan.hpp"
#include "audiohole/test_suite.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace audiohole {
namespace {
namespace scan = audiohole::scanner;
using Json = scan::Json;
const Json& field(const Json& object, const char* key) {
    static const Json unknown{{"status", "unknown"}};
    return object.is_object() && object.contains(key) ? object.at(key) : unknown;
}
std::string value(const Json& object) {
    if (object.is_string())
        return object.get<std::string>();
    if (object.is_object() && object.contains("status") && object.at("status").is_string())
        return object.at("status").get<std::string>();
    return object.is_null() ? "unknown" : object.dump();
}
std::string diagnostic(const scan::Diagnostic& item) {
    return item.code + ": " + item.message +
           (item.context.empty() ? "" : " [" + item.context + "]") +
           (item.native_code.empty() ? "" : " (" + item.native_code + ")");
}
std::string descriptor(const Json& format) {
    if (!format.is_object() || format.contains("status"))
        return value(format);
    const auto sample = value(field(format, "sample_format"));
    const auto representation =
        sample == "float32"          ? std::string("32-bit float")
        : sample == "pcm_s16"        ? std::string("16-bit integer PCM")
        : sample == "pcm_s24_packed" ? std::string("24-bit integer PCM (packed)")
        : sample == "pcm_s32" ? value(field(format, "valid_bits")) + "-bit integer PCM in a " +
                                    value(field(format, "container_bits")) + "-bit container"
                              : sample;
    return value(field(format, "sample_rate_hz")) + " Hz / " +
           value(field(format, "channel_count")) + " channels / " + representation;
}
std::string support_text(const Json& query) {
    if (field(query, "hresult") == "0x00000001" && query.contains("closest_match"))
        return "closest format offered: " + descriptor(query.at("closest_match"));
    const auto support = value(field(query, "support"));
    return support == "supported"     ? "Windows-reported support"
           : support == "unsupported" ? "requested format unsupported"
                                      : support;
}
std::string attempt_text(const Json& attempt) {
    if (!attempt.is_object())
        return "unknown";
    const auto support = value(field(attempt, "support"));
    if (field(attempt, "hresult") == "0x00000001")
        return "closest format offered; exact request was not streamed";
    if (support == "unsupported")
        return "requested format unsupported";
    const auto classification = value(field(attempt, "classification"));
    return classification == "verified" ? "short stream processed (limited evidence)"
                                        : classification;
}
const Json& latest_attempt(const Json& probed) {
    const auto& attempts = field(probed, "test_attempts");
    const auto& id = field(probed, "latest_attempt_id");
    if (attempts.is_array()) {
        for (const auto& attempt : attempts)
            if (attempt.is_object() && field(attempt, "attempt_id") == id)
                return attempt;
        if (!attempts.empty())
            return attempts.back();
    }
    return field(probed, "latest_attempt_id_missing");
}
std::string reported_formats(const Json& reported, bool compact) {
    const auto& queries = field(reported, "format_queries");
    if (!queries.is_array())
        return "Unknown / not tested.\n";
    std::string result;
    std::set<std::string> shown;
    unsigned count = 0;
    for (const auto& query : queries) {
        if (!query.is_object())
            continue;
        const auto& configuration = field(query, "configuration");
        const auto mode = value(field(query, "mode"));
        const auto line = mode + ": " + descriptor(configuration);
        if (compact) {
            if (field(query, "support") != "supported" || !shown.insert(line).second)
                continue;
            if (count++ >= 6)
                continue;
            result += "  " + line + "\n";
        } else {
            result += line + " — " + support_text(query) +
                      "\n  Container: " + value(field(configuration, "container_bits")) +
                      " bits; valid bits: " + value(field(configuration, "valid_bits")) +
                      "; channel mask: " + value(field(configuration, "channel_mask")) +
                      "; format tag: " + value(field(configuration, "format_tag")) +
                      "; subformat: " + value(field(configuration, "subformat_guid")) +
                      "\n  HRESULT: " + value(field(query, "hresult")) + "\n\n";
        }
    }
    if (result.empty())
        result = "No supported candidate recorded; coverage may be incomplete.\n";
    if (compact)
        result += count > 6 ? "  More formats and exact tuples: select Reported formats.\n"
                            : "  Exact tuples and rejected candidates: select Reported formats.\n";
    return result;
}
std::vector<Endpoint> project(const scan::InventorySnapshot& inventory, const std::string& source) {
    std::vector<Endpoint> result;
    std::map<std::string, unsigned> claims;
    for (const auto& saved : inventory.devices) {
        const auto& id = field(saved.local_binding, "endpoint_id");
        if (id.is_string())
            ++claims[id.get<std::string>()];
    }
    for (const auto& saved : inventory.devices) {
        const auto& record = saved.record;
        const auto& identity = field(record, "identity");
        const auto& reported = field(record, "reported_capabilities");
        const auto& history = field(record, "scan_history");
        Endpoint endpoint;
        endpoint.id = value(field(record, "logical_id"));
        endpoint.display_name = value(field(record, "display_name"));
        endpoint.windows_name = value(field(record, "device_name"));
        endpoint.notes = value(field(record, "notes"));
        endpoint.capture = field(identity, "direction") == "capture";
        endpoint.hidden = record.value("hidden", false);
        auto detail = std::make_shared<DeviceInspection>();
        detail->source = source;
        detail->path = scan::path_utf8(saved.path);
        detail->edit_bytes = saved.original_bytes;
        detail->observed_state = value(field(saved.local_binding, "state"));
        detail->last_observed = value(field(saved.local_binding, "last_seen"));
        const auto& local_id = field(saved.local_binding, "endpoint_id");
        const bool ambiguous = local_id.is_string() && claims[local_id.get<std::string>()] > 1;
        endpoint.availability = ambiguous                            ? Availability::Ambiguous
                                : detail->observed_state == "active" ? Availability::Available
                                : detail->observed_state == "disabled" ||
                                        detail->observed_state == "unplugged" ||
                                        detail->observed_state == "not_present"
                                    ? Availability::Unavailable
                                    : Availability::Unknown;
        const auto& probed = field(record, "probed_capabilities");
        const auto& latest = latest_attempt(probed);
        const auto& diagnostics = field(record, "diagnostics");
        detail->warning = ambiguous || (diagnostics.is_array() && !diagnostics.empty()) ||
                          (latest.is_object() && (field(latest, "cleanup") == "unknown" ||
                                                  (field(latest, "classification") == "failed" &&
                                                   field(latest, "support") != "unsupported")));
        const auto next_action =
            ambiguous ? "Resolve the ambiguous binding before configuring a route."
            : endpoint.availability == Availability::Unavailable
                ? "Reconnect or enable the device, then Refresh availability."
            : endpoint.availability == Availability::Unknown
                ? "Refresh availability or run a Basic update from the device's right-click menu."
            : detail->warning ? "Review Diagnostics and the latest Deep attempt; older history is "
                                "retained separately."
                              : "Inspect reported formats; use the device's right-click menu for "
                                "an update or Deep test.";
        detail->views[0] =
            "Display name: " + endpoint.display_name +
            "\nDevice type: " + (endpoint.capture ? "Capture / Recording" : "Playback") +
            "\nLast observed availability: " +
            (ambiguous ? "ambiguous binding" : detail->observed_state) +
            "\nObservation time: " + detail->last_observed + "\nNext action: " + next_action +
            "\n\nWindows-configured format: " +
            descriptor(field(reported, "configured_device_format")) +
            "\nWindows mix format: " + descriptor(field(reported, "default_mix_format")) +
            "\nWindows-reported support (summary):\n" + reported_formats(reported, true) +
            "\nLast completed Basic scan: " + value(field(history, "last_scanned")) +
            "\nLast Basic attempt: " + value(field(history, "last_attempt")) +
            "\nLast Deep attempt: " + value(field(history, "last_advanced_scan")) +
            "\nLatest Deep result: " + attempt_text(latest) +
            "\nOlder attempts remain in Deep evidence; a rejected candidate does not establish "
            "device unusability.\n" +
            "\nDriver: " + value(field(field(record, "driver"), "name")) +
            "\nDriver version: " + value(field(field(record, "driver"), "version")) +
            "\n\nTechnical details\nOriginal Windows name: " + endpoint.windows_name +
            "\nCurrent Windows name: " + value(field(identity, "current_device_name")) +
            "\nLogical ID: " + endpoint.id + "\nSource: " +
            (source == "real_windows_mmdevice_wasapi" ? "Windows — MMDevice / WASAPI" : source) +
            "\nHidden: " + (endpoint.hidden ? "yes" : "no") + "\nRecord: " + detail->path +
            "\n\nScan evidence has limits. Windows availability and reported support, and short "
            "Deep tests, "
            "do not establish sustained streaming reliability, channel mapping or end-to-end "
            "latency. See Deep evidence for v0.4 limitations.";
        detail->views[1] = identity.dump(2);
        detail->views[2] =
            "Windows-reported format queries (bounded coverage, not exhaustive)\n\n" +
            reported_formats(reported, false) +
            "Configured format: " + descriptor(field(reported, "configured_device_format")) +
            "\nMix format: " + descriptor(field(reported, "default_mix_format")) +
            "\nOEM format: " + descriptor(field(reported, "oem_device_format")) +
            "\n\nReported periods / constraints (100 ns units or frames, as named):\n" +
            field(reported, "device_periods").dump(2) + "\nShared engine periods:\n" +
            field(reported, "shared_engine_periods").dump(2);
        detail->views[3] =
            "Scanner v0.4 Deep evidence is a limited stream test under recorded "
            "conditions. Its verified label has a known frame-progress weakness; do not treat it "
            "as a Healthy or Recommended setting. Busy/policy denial is not unsupported.\n\n" +
            field(record, "probed_capabilities").dump(2);
        detail->views[4] = field(record, "driver").dump(2);
        detail->views[5] = history.dump(2);
        detail->views[6] = field(record, "diagnostics").dump(2);
        detail->views[7] = record.dump(2);
        detail->views[8] =
            "Machine-local binding; never use Windows IDs as portable profile identity.\n\n" +
            saved.local_binding.dump(2);
        endpoint.inspection = std::move(detail);
        result.push_back(std::move(endpoint));
    }
    return result;
}
std::string excerpt(const std::string& text) {
    if (text.size() <= 4096)
        return text;
    std::size_t end = 4096;
    while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
        --end;
    return text.substr(0, end) + "\n[Console excerpt; full details in the analysis bundle.]";
}
void write_json(const std::filesystem::path& path, const Json& document) {
    scan::make_atomic_writer()->write(path, document.dump(2) + "\n", std::nullopt);
}
} // namespace

struct ScannerService::Impl {
    struct Job {
        std::uint64_t generation = 0;
        bool scan = false, deep = false;
        ScanScope scope = ScanScope::Update;
        std::string logical_id;
        ScanOptions options;
        ScannerActionRequest action;
        std::stop_token stop;
    };
    ScannerServiceOptions options;
    ScannerFactories factories;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::optional<Job> pending;
    std::deque<ScannerUpdate> updates;
    std::stop_source cancellation;
    std::atomic<bool> busy = false;
    bool stopping = false;
    std::uint64_t next_generation = 0;
    scan::InventorySnapshot inventory;
    std::filesystem::path last_session;
    std::jthread worker;

    void publish(ScannerUpdate update) {
        std::lock_guard lock(mutex);
        if (updates.size() >= 128)
            updates.pop_front(); // Only sampled UI progress; disk evidence is independent.
        updates.push_back(std::move(update));
        if (updates.back().terminal)
            busy = false;
    }
    std::uint64_t submit(Job job) {
        std::lock_guard lock(mutex);
        if (stopping || busy)
            return 0;
        cancellation = std::stop_source{};
        job.stop = cancellation.get_token();
        job.generation = ++next_generation;
        const auto generation = job.generation;
        pending = std::move(job);
        busy = true;
        ready.notify_one();
        return generation;
    }
    void work(std::stop_token thread_stop) {
        for (;;) {
            Job job;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&] { return stopping || pending.has_value(); });
                if (stopping || thread_stop.stop_requested())
                    return;
                job = std::move(*pending);
                pending.reset();
            }
            run(job);
        }
    }
    void run(const Job& job) {
        ScannerUpdate final;
        final.generation = job.generation;
        final.terminal = true;
        final.phase = "finished";
        final.result = "completed";
        try {
            scan::DeviceStore store(options.portable_root, options.local_root);
            if (job.stop.stop_requested())
                final.result = "interrupted";
            else if (job.scan)
                run_scan(job, store, final);
            else if (job.action.kind == ScannerActionKind::SetHidden) {
                store.set_hidden(job.action.logical_id, job.action.hidden);
                final.detail = job.action.hidden
                                   ? "Device hidden; routing and saved references are unchanged."
                                   : "Device unhidden.";
            } else if (job.action.kind == ScannerActionKind::EditMetadata) {
                store.edit_metadata(job.action.logical_id, job.action.display_name,
                                    job.action.notes, job.action.expected_path,
                                    job.action.expected_bytes);
                final.detail = "Device name and notes saved. Logical identity and profile "
                               "references are unchanged.";
            } else if (job.action.kind == ScannerActionKind::RefreshAvailability) {
                auto provider = make_provider();
                scan::Discovery identities;
                auto discovery = provider->discover(job.stop, {}, [&](const auto& endpoint) {
                    identities.endpoints.push_back(endpoint);
                    return false;
                });
                identities.complete = discovery.complete;
                identities.cancelled = discovery.cancelled;
                identities.diagnostics = discovery.diagnostics;
                identities.full_inventory = true;
                const auto refreshed = store.refresh_metadata(identities, scan::utc_now());
                final.result = refreshed.result;
                final.detail = "Known-device availability refreshed without capability tests.";
                for (const auto& item : refreshed.diagnostics)
                    final.detail += "\n" + diagnostic(item);
            } else if (job.action.kind == ScannerActionKind::Export) {
                if (last_session.empty())
                    throw std::runtime_error("Run a scan before exporting its analysis bundle.");
                scan::zip_results(last_session, job.action.output);
                final.detail = "Analysis bundle exported: " + scan::path_utf8(job.action.output);
            } else
                final.detail = "Saved scanner records reloaded.";
            auto loaded = store.inventory();
            final.inventory_valid = loaded.complete;
            if (loaded.complete) {
                inventory = std::move(loaded);
                final.devices = project(inventory, options.source);
            } else {
                final.result = "failed";
                for (const auto& item : loaded.diagnostics)
                    final.detail += "\n" + diagnostic(item);
            }
        } catch (const std::exception& error) {
            final.result = job.stop.stop_requested() ? "interrupted" : "failed";
            final.detail = error.what();
        }
        if (job.scan && !final.session_path.empty()) {
            try {
                write_json(scan::utf8_path(final.session_path) / "completion.json",
                           {{"result", final.result},
                            {"detail", final.detail},
                            {"finished_utc", scan::utc_now()}});
            } catch (const std::exception& error) {
                final.detail += "\nCould not save terminal evidence: " + std::string(error.what());
                final.result = "failed";
            }
        }
        if (final.session_path.empty())
            final.session_path = scan::path_utf8(last_session);
        publish(std::move(final));
    }
    std::unique_ptr<scan::EndpointProvider> make_provider() {
        auto provider = factories.provider ? factories.provider() : scan::make_windows_provider();
        if (!provider)
            throw std::runtime_error("Scanner provider is unavailable.");
        return provider;
    }
    void run_scan(const Job& job, scan::DeviceStore& store, ScannerUpdate& final) {
        const auto directory = options.local_root / "sessions" /
                               scan::utf8_path(scan::safe_filename(scan::utc_now()) + "-" +
                                               scan::new_logical_id("scan"));
        std::filesystem::create_directories(directory.parent_path());
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Could not create an isolated scan session directory.");
        last_session = directory;
        final.session_path = scan::path_utf8(directory);
        const std::string scope = job.scope == ScanScope::Full       ? "full"
                                  : job.scope == ScanScope::Selected ? "device"
                                                                     : "update";
        write_json(directory / "request.json",
                   {{"source", options.source},
                    {"scanner_version", "0.4.0"},
                    {"scope", scope},
                    {"depth", job.deep ? "deep" : "basic"},
                    {"logical_id", job.logical_id},
                    {"mode", job.options.mode},
                    {"max_attempts", job.options.attempts},
                    {"duration_ms", job.options.duration_ms},
                    {"attempt_timeout_ms", job.options.attempt_timeout_ms},
                    {"device_timeout_ms", job.options.device_timeout_ms},
                    {"system_timeout_ms", job.options.system_timeout_ms},
                    {"consent", job.options.consent},
                    {"started_utc", scan::utc_now()},
                    {"operating_context", job.options.operating_context}});
        std::ofstream log(directory / "progress.jsonl", std::ios::binary);
        if (!log)
            throw std::runtime_error("Could not create the scan progress log.");
        std::uint64_t log_bytes = 0;
        bool truncated = false;
        const auto progress = [&](const scan::Progress& progress) {
            const Json data{{"phase", progress.phase},
                            {"endpoint_id", progress.endpoint_id},
                            {"completed", progress.completed},
                            {"total", progress.total},
                            {"details", progress.details}};
            const auto bytes = data.dump() + "\n";
            if (log_bytes + bytes.size() <= 64ULL * 1024 * 1024) {
                log << bytes;
                log_bytes += bytes.size();
            } else
                truncated = true;
            ScannerUpdate update;
            update.generation = job.generation;
            update.phase = progress.phase;
            update.detail = excerpt(bytes);
            update.percent = progress.total ? static_cast<int>(std::min<std::size_t>(
                                                  99, progress.completed * 100 / progress.total))
                                            : -1;
            publish(std::move(update));
        };
        auto provider = make_provider();
        const auto basic =
            scan::run_scoped_basic(*provider, store, scope, job.logical_id, job.stop, progress);
        write_json(directory / "basic.json", basic);
        final.result = basic.scan.result;
        final.detail =
            "Basic: " + basic.scan.result +
            "; selected capability queries: " + std::to_string(basic.selected_endpoint_ids.size()) +
            "; known identities refreshed: " + std::to_string(basic.metadata_updated) + ".";
        for (const auto& item : basic.scan.diagnostics)
            final.detail += "\n" + diagnostic(item);
        if (job.deep && (final.result == "completed" || final.result == "partially_completed") &&
            !job.stop.stop_requested()) {
            auto targets = store.probe_targets();
            std::erase_if(targets, [&](const auto& target) {
                return std::find(basic.selected_endpoint_ids.begin(),
                                 basic.selected_endpoint_ids.end(),
                                 target.endpoint_id) == basic.selected_endpoint_ids.end();
            });
            if (targets.empty()) {
                const bool no_new_devices =
                    scope == "update" && basic.selected_endpoint_ids.empty();
                if (!no_new_devices)
                    final.result = "unavailable";
                final.detail += no_new_devices ? "\nNo new devices; zero Deep tests scheduled."
                                               : "\nNo selected active, unambiguous target is "
                                                 "available. No substitute was tested.";
            } else {
                auto runner = factories.runner
                                  ? factories.runner()
                                  : scan::make_process_probe_runner(options.probe_executable);
                if (!runner)
                    throw std::runtime_error("Deep probe runner is unavailable.");
                const scan::DeepLimits limits{job.options.attempts, job.options.duration_ms,
                                              job.options.attempt_timeout_ms,
                                              job.options.device_timeout_ms, job.options.mode};
                const auto deep = scan::run_deep_scan_batch(
                    store, *runner, targets, limits, job.options.system_timeout_ms,
                    job.options.consent, job.stop, progress);
                Json document = deep;
                document["scope"] = scope;
                document["requested_logical_id"] = job.logical_id;
                write_json(directory / "deep.json", document);
                final.result = deep.result == "completed" ? final.result : deep.result;
                final.detail +=
                    "\nDeep: " + deep.result + "; targets: " + std::to_string(targets.size()) +
                    ". v0.4 stream evidence is limited; it is not a health/recommendation badge.";
                for (const auto& item : deep.diagnostics)
                    final.detail += "\n" + diagnostic(item);
            }
        }
        if (job.stop.stop_requested())
            final.result = "interrupted";
        log.flush();
        if (!log)
            throw std::runtime_error("Could not flush scan evidence.");
        const auto snapshot = store.inventory();
        Json index{{"valid", snapshot.complete},
                   {"source", options.source},
                   {"devices", Json::array()},
                   {"diagnostics", snapshot.diagnostics}};
        for (const auto& device : snapshot.devices) {
            const auto relative = std::filesystem::path("devices") /
                                  device.path.parent_path().filename() / device.path.filename();
            const auto destination = directory / relative;
            std::filesystem::create_directories(destination.parent_path());
            std::ofstream file(destination, std::ios::binary);
            file << device.original_bytes;
            file.flush();
            if (!file)
                throw std::runtime_error("Could not snapshot a device record for analysis.");
            auto archive_path = scan::path_utf8(relative);
            std::replace(archive_path.begin(), archive_path.end(), '\\', '/');
            index["devices"].push_back({{"logical_id", device.record.at("logical_id")},
                                        {"record", archive_path},
                                        {"local_binding", device.local_binding}});
        }
        index["history_files"] = Json::array();
        std::set<std::string> histories;
        std::uint64_t history_bytes = 0;
        for (const auto& device : snapshot.devices) {
            std::vector<const Json*> references;
            const auto& capabilities = field(device.record, "probed_capabilities");
            for (const auto* key :
                 {"test_attempts", "verified_configurations", "accepted_configurations",
                  "failed_configurations", "inconclusive_configurations"}) {
                const auto& rows = field(capabilities, key);
                if (rows.is_array())
                    for (const auto& row : rows)
                        references.push_back(&row);
            }
            for (const auto* row : references) {
                const auto& attempt = *row;
                const auto& reference = field(attempt, "history_file");
                if (!reference.is_string())
                    continue;
                auto name = reference.get<std::string>();
                std::replace(name.begin(), name.end(), '\\', '/');
                if (!histories.insert(name).second)
                    continue;
                Json entry{{"path", name}, {"status", "unavailable"}};
                const auto relative = scan::utf8_path(name);
                const auto filename = scan::path_utf8(relative.filename());
                if (relative.is_absolute() ||
                    relative.parent_path() != std::filesystem::path("probe_history") ||
                    filename.empty() || filename == "." || filename == ".." ||
                    relative.extension() != ".json") {
                    entry["reason"] =
                        "Invalid history reference; no file outside probe_history was read.";
                } else
                    try {
                        const auto source = options.portable_root / relative;
                        if (std::filesystem::is_symlink(source.parent_path()) ||
                            std::filesystem::is_symlink(source) ||
                            !std::filesystem::is_regular_file(source))
                            throw std::runtime_error(
                                "Referenced history is missing or is not a regular file");
                        const auto size = std::filesystem::file_size(source);
                        if (size > 16 * 1024 * 1024 || history_bytes + size > 64 * 1024 * 1024 ||
                            histories.size() > 4096)
                            throw std::runtime_error("History snapshot budget reached");
                        std::ifstream input(source, std::ios::binary);
                        std::string bytes(static_cast<std::size_t>(size), '\0');
                        input.read(bytes.data(), static_cast<std::streamsize>(size));
                        if (!input || input.peek() != std::char_traits<char>::eof())
                            throw std::runtime_error("History changed or could not be read");
                        const auto destination = directory / relative;
                        std::filesystem::create_directories(destination.parent_path());
                        std::ofstream output(destination, std::ios::binary);
                        output << bytes;
                        output.flush();
                        if (!output)
                            throw std::runtime_error("Cannot snapshot raw history");
                        history_bytes += size;
                        entry["status"] = "included";
                    } catch (const std::exception& error) {
                        entry["reason"] = error.what();
                    }
                index["history_files"].push_back(std::move(entry));
            }
        }
        write_json(directory / "inventory.json", index);
        write_json(directory / "result.json",
                   {{"result", final.result},
                    {"detail", final.detail},
                    {"finished_utc", scan::utc_now()},
                    {"progress_log_truncated", truncated},
                    {"note", "Real session evidence; no automated test-fixture renaming or record "
                             "omission performed."}});
        if (truncated)
            final.detail +=
                "\nProgress log reached its 64 MiB budget; truncation is recorded in result.json.";
    }
};

ScannerService::ScannerService(ScannerServiceOptions options, ScannerFactories factories)
    : impl_(std::make_unique<Impl>()) {
    if (options.portable_root.empty() || options.local_root.empty() ||
        std::filesystem::absolute(options.portable_root).lexically_normal() ==
            std::filesystem::absolute(options.local_root).lexically_normal())
        throw std::invalid_argument(
            "Isolated portable and machine-local scanner roots are required.");
    if (factories.provider && options.source == "real_windows_mmdevice_wasapi")
        throw std::invalid_argument("Injected scanner tests must declare their evidence source.");
    impl_->options = std::move(options);
    impl_->factories = std::move(factories);
    impl_->worker = std::jthread([this](std::stop_token stop) { impl_->work(stop); });
}
ScannerService::~ScannerService() {
    shutdown();
}
std::uint64_t ScannerService::start_scan(bool deep, ScanScope scope, const std::string& id,
                                         const ScanOptions& options) {
    if (scope != ScanScope::Full && scope != ScanScope::Update && scope != ScanScope::Selected)
        throw std::invalid_argument("Invalid scan scope.");
    if (options.operating_context.size() > 8192)
        throw std::invalid_argument("Operating context exceeds the 8192-byte limit.");
    if (scope == ScanScope::Selected && id.empty())
        throw std::invalid_argument("Select a saved scanner device before a per-device scan.");
    if (deep) {
        if (!options.consent)
            throw std::invalid_argument("Deep probing requires explicit consent.");
        scan::validate_deep_limits({options.attempts, options.duration_ms,
                                    options.attempt_timeout_ms, options.device_timeout_ms,
                                    options.mode});
        if (options.system_timeout_ms < 1000 || options.system_timeout_ms > 3600000)
            throw std::invalid_argument(
                "The Deep scan system budget must be 1 second to 60 minutes.");
    }
    Impl::Job job;
    job.scan = true;
    job.deep = deep;
    job.scope = scope;
    job.logical_id = id;
    job.options = options;
    return impl_->submit(std::move(job));
}
std::uint64_t ScannerService::action(const ScannerActionRequest& request) {
    Impl::Job job;
    job.action = request;
    return impl_->submit(std::move(job));
}
void ScannerService::cancel() {
    std::stop_source source;
    {
        std::lock_guard lock(impl_->mutex);
        source = impl_->cancellation;
    }
    source.request_stop();
}
void ScannerService::shutdown() {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stopping = true;
    }
    cancel();
    impl_->worker.request_stop();
    impl_->ready.notify_all();
    if (impl_->worker.joinable())
        impl_->worker.join();
    impl_->busy = false;
}
bool ScannerService::busy() const {
    return impl_->busy;
}
std::vector<ScannerUpdate> ScannerService::drain() {
    std::lock_guard lock(impl_->mutex);
    std::vector<ScannerUpdate> result;
    while (!impl_->updates.empty()) {
        result.push_back(std::move(impl_->updates.front()));
        impl_->updates.pop_front();
    }
    return result;
}
} // namespace audiohole
