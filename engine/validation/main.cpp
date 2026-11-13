#include "audiohole/engine/runtime.hpp"
#include "audiohole/scanner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace engine = audiohole::engine;
namespace scan = audiohole::scanner;
namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

namespace {
std::atomic<bool> interrupted{false};

std::string read_file(const fs::path& path) {
    if (!fs::is_regular_file(path) || fs::file_size(path) > 2 * 1024 * 1024)
        throw std::runtime_error("Profile must be a regular UTF-8 JSON file at most 2 MiB.");
    std::ifstream input(path, std::ios::binary);
    std::string bytes(2 * 1024 * 1024 + 1, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input.eof() || input.bad())
        throw std::runtime_error("Profile could not be read completely.");
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    return bytes;
}
void print_issues(const std::vector<engine::Issue>& issues) {
    for (const auto& issue : issues)
        std::cout << "  " << issue.location << ": " << issue.detail << '\n';
}
template <class T> Json optional(const std::optional<T>& value) {
    return value ? Json(*value) : Json(nullptr);
}
Json format(const engine::StreamFormat& f) {
    return {{"sample_rate_hz", f.sample_rate_hz},
            {"channels", f.channel_count},
            {"sample_format", engine::name(f.sample_format)}};
}
Json stream(const engine::StreamConfiguration& s) {
    Json attempts = Json::array();
    for (const auto& attempt : s.access_attempts) {
        Json issues = Json::array();
        for (const auto& issue : attempt.issues)
            issues.push_back(
                {{"code", static_cast<int>(issue.code)},
                 {"location", issue.location},
                 {"detail", issue.detail},
                 {"native_code",
                  issue.native_code.empty() ? Json(nullptr) : Json(issue.native_code)}});
        attempts.push_back({{"mode", engine::name(attempt.mode)},
                            {"succeeded", attempt.succeeded},
                            {"native_open_attempted", attempt.native_open_attempted},
                            {"issues", issues}});
    }
    return {
        {"client_format", format(s.client_format)},
        {"reported_native_format", s.native_format ? format(*s.native_format) : Json(nullptr)},
        {"mode", engine::name(s.mode)},
        {"access_policy", engine::name(s.access_policy)},
        {"requested_mode",
         s.requested_mode ? Json(engine::name(*s.requested_mode)) : Json(nullptr)},
        {"exclusive_mode_preferred", s.exclusive_mode_preferred},
        {"access_fell_back", s.access_fell_back},
        {"access_attempts", attempts},
        {"period_frames", optional(s.period_frames)},
        {"allocated_buffer_frames", optional(s.allocated_buffer_frames)},
        {"timing_sample_rate_hz", optional(s.timing_sample_rate_hz)},
        {"applied_buffer_request_ms", optional(s.applied_buffer_request_ms)},
        {"shared_default_period_frames", optional(s.shared_default_period_frames)},
        {"shared_minimum_period_frames", optional(s.shared_minimum_period_frames)},
        {"shared_fundamental_period_frames", optional(s.shared_fundamental_period_frames)},
        {"shared_maximum_period_frames", optional(s.shared_maximum_period_frames)},
        {"period_selection", s.period_selection.empty() ? Json(nullptr) : Json(s.period_selection)},
        {"low_latency_fallback",
         s.low_latency_fallback.empty() ? Json(nullptr) : Json(s.low_latency_fallback)},
        {"render_ahead_target_frames", optional(s.render_ahead_target_frames)}};
}
Json underrun_event(const engine::UnderrunEvent& e) {
    return {
        {"sequence", e.sequence},
        {"rendered_frame_index", e.rendered_frame_index},
        {"source_published_frames", e.source_published_frames},
        {"source_frames_available_before", e.source_frames_available_before},
        {"requested_output_frames", e.requested_output_frames},
        {"produced_output_frames", e.produced_output_frames},
        {"controller_target_source_frames", e.controller_target_source_frames},
        {"requested_correction_ppm", e.requested_correction_ppm},
        {"render_qpc_100ns", optional(e.render_qpc_100ns)},
        {"last_capture_device_position_frames", optional(e.last_capture_device_position_frames)},
        {"last_capture_qpc_100ns", optional(e.last_capture_qpc_100ns)},
        {"capture_delivery_gap_last_us", optional(e.capture_delivery_gap_last_us)},
        {"capture_packet_age_last_us", optional(e.capture_packet_age_last_us)},
        {"native_padding_before_frames", optional(e.native_padding_before_frames)},
        {"native_render_request_frames", optional(e.native_render_request_frames)}};
}
Json snapshot(const engine::RuntimeSnapshot& s) {
    Json paths = Json::array();
    for (const auto& path : s.paths) {
        Json issues = Json::array();
        auto add = [&](const auto& list) {
            for (const auto& issue : list)
                issues.push_back(
                    {{"code", static_cast<int>(issue.code)},
                     {"location", issue.location},
                     {"detail", issue.detail},
                     {"native_code",
                      issue.native_code.empty() ? Json(nullptr) : Json(issue.native_code)}});
        };
        add(path.issues);
        const auto& o = path.observation;
        Json item{
            {"hole_id", path.hole_id},
            {"direction", path.direction == engine::DirectionId::Forward ? "forward" : "reverse"},
            {"state", engine::name(path.state)},
            {"captured_frames", optional(o.captured_frames)},
            {"rendered_frames", optional(o.rendered_frames)},
            {"transferred_frames", optional(o.transferred_frames)},
            {"queued_source_frames", optional(o.queued_frames)},
            {"controller_target_source_frames", optional(o.controller_target_source_frames)},
            {"capture_packet_min_frames", optional(o.capture_packet_min_frames)},
            {"capture_packet_max_frames", optional(o.capture_packet_max_frames)},
            {"render_request_min_frames", optional(o.render_request_min_frames)},
            {"render_request_max_frames", optional(o.render_request_max_frames)},
            {"underruns", optional(o.underruns)},
            {"overruns", optional(o.overruns)},
            {"capture_discontinuities", optional(o.discontinuities)},
            {"startup_discontinuities", optional(o.startup_discontinuities)},
            {"timestamp_errors", optional(o.timestamp_errors)},
            {"requested_correction_ppm", optional(o.correction_ppm)},
            {"measured_clock_drift_ppm", optional(o.measured_clock_drift_ppm)},
            {"capture_delivery_gap_max_us", optional(o.capture_delivery_gap_max_us)},
            {"capture_drain_max_frames", optional(o.capture_drain_max_frames)},
            {"capture_packet_age_last_us", optional(o.capture_packet_age_last_us)},
            {"capture_packet_age_max_us", optional(o.capture_packet_age_max_us)},
            {"render_padding_before_frames", optional(o.render_padding_before_frames)},
            {"render_padding_after_submit_estimate_frames",
             optional(o.render_padding_after_submit_estimate_frames)},
            {"render_empty_wakes", optional(o.render_empty_wakes)},
            {"render_capture_waits", optional(o.render_capture_waits)},
            {"render_capture_wait_timeouts", optional(o.render_capture_wait_timeouts)},
            {"capture_wait_timer_available", optional(o.capture_wait_timer_available)},
            {"native_capture_wakes", optional(o.native_capture_wakes)},
            {"native_render_wakes", optional(o.native_render_wakes)},
            {"native_wait_timeouts", optional(o.native_wait_timeouts)},
            {"native_initial_silence_frames", optional(o.native_initial_silence_frames)},
            {"native_startup_padding_silence_frames",
             optional(o.native_startup_padding_silence_frames)},
            {"native_submitted_frames", optional(o.native_submitted_frames)},
            {"native_render_request_max_frames", optional(o.native_render_request_max_frames)},
            {"last_underrun", o.last_underrun ? underrun_event(*o.last_underrun) : Json(nullptr)},
            {"dropped_underrun_contexts", optional(o.dropped_underrun_contexts)},
            {"progress_age_ms", optional(path.progress_age_ms)},
            {"issues", issues}};
        if (path.negotiated) {
            const auto& n = *path.negotiated;
            item["backend"] = engine::name(n.backend);
            item["source"] = stream(n.source);
            item["destination"] = stream(n.destination);
            item["processing"] = n.processing;
            item["synchronization"] = n.synchronization;
            item["selection_reason"] = n.selection_reason;
            item["fifo_target_source_frames"] = optional(n.fifo_target_source_frames);
            item["fifo_capacity_source_frames"] = optional(n.fifo_capacity_source_frames);
            item["preferred_clock_device"] = n.preferred_clock_device;
            item["clock_preference_applied"] = n.clock_preference_applied;
        }
        paths.push_back(std::move(item));
    }
    return {{"operation", s.operation},
            {"revision", s.revision},
            {"profile_id", s.profile_id},
            {"state", engine::name(s.state)},
            {"armed", s.armed},
            {"paths", std::move(paths)},
            {"measured_end_to_end_latency_ms", nullptr},
            {"physical_signal_verified", false}};
}

engine::BindingSnapshot bindings(const fs::path& portable, const fs::path& local) {
    // Read-only use of the existing authoritative scanner parser. This host
    // never scans, reconciles, writes notes, creates new IDs or renames records.
    scan::DeviceStore store(portable, local);
    const auto inventory = store.inventory();
    for (const auto& diagnostic : inventory.diagnostics)
        std::cout << "Registry: " << diagnostic.code << ": " << diagnostic.message << '\n';
    engine::BindingSnapshot result;
    result.revision =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    for (const auto& saved : inventory.devices) {
        const auto& record = saved.record;
        const auto& binding = saved.local_binding;
        engine::LocalBinding endpoint;
        endpoint.logical_id = record.at("logical_id").get<std::string>();
        endpoint.direction = record.at("identity").at("direction") == "capture"
                                 ? engine::EndpointDirection::Capture
                                 : engine::EndpointDirection::Render;
        if (binding.contains("endpoint_id") && binding.at("endpoint_id").is_string())
            endpoint.endpoint_id = binding.at("endpoint_id").get<std::string>();
        const auto state = binding.value("state", Json(nullptr));
        endpoint.state =
            state == "active" ? engine::BindingState::Available : engine::BindingState::Unavailable;
        result.endpoints.push_back(std::move(endpoint));
    }
    return result;
}

std::string stamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream result;
    result << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
    return result.str();
}

// Non-real-time host logger. Bounded to 16 MiB per file, never writes samples,
// never uploads data, and lives outside the engine module and all callbacks.
class Log {
  public:
    void open(const fs::path& root, const fs::path& profile, const std::string& bytes,
              std::uint64_t operation, engine::LatencyPreset preset) {
        started_ = std::chrono::steady_clock::now();
        fs::create_directories(root);
        const auto base =
            stamp() + "-" + scan::new_logical_id("run") + "-" + std::to_string(operation);
        json_.open(root / (base + ".jsonl"), std::ios::binary | std::ios::app);
        text_.open(root / (base + ".txt"), std::ios::binary | std::ios::app);
        if (!json_ || !text_)
            throw std::runtime_error("Could not open validation logs.");
        write({{"event", "start_request"},
               {"profile_file", scan::path_utf8(profile.filename())},
               {"profile_record", Json::parse(bytes)},
               {"build", "AudioHole engine validation 0.6"},
               {"latency_preset", preset == engine::LatencyPreset::Low ? "low" : "baseline"},
               {"status", "prototype; hardware behavior not yet validated"}});
        std::cout << "Logs: " << scan::path_utf8(root / base) << ".jsonl / .txt\n";
    }
    void write(const Json& value) {
        if (!json_.is_open() || !text_.is_open() || size_ > 16 * 1024 * 1024)
            return;
        auto record = value;
        record["format"] = "audiohole-engine-validation-event";
        record["schema_version"] = 1;
        record["elapsed_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - started_)
                                   .count();
        record["timestamp_utc"] = scan::utc_now();
        const auto bytes = record.dump();
        const auto readable = record.dump(2);
        if (size_ + readable.size() + bytes.size() > 16 * 1024 * 1024) {
            std::cout << "Log bound reached; routing continues, further logging is disabled.\n";
            json_.close();
            text_.close();
            return;
        }
        json_ << bytes << '\n';
        text_ << readable << "\n\n";
        json_.flush();
        text_.flush();
        size_ += bytes.size() + readable.size() + 3;
        if (!json_ || !text_) {
            std::cout << "Log write failed; routing continues. Use status and Stop as needed.\n";
            json_.close();
            text_.close();
        }
    }
    void close() {
        json_.close();
        text_.close();
        size_ = 0;
    }

  private:
    std::ofstream json_, text_;
    std::size_t size_ = 0;
    std::chrono::steady_clock::time_point started_;
};

#ifdef _WIN32
BOOL WINAPI console_signal(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        interrupted.store(true, std::memory_order_release);
        return TRUE;
    }
    return FALSE;
}
std::string utf8(const std::wstring& value) {
    const int count =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count < 1)
        return {};
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), count, nullptr, nullptr);
    return result;
}
fs::path executable_folder() {
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        throw std::runtime_error("Executable path unavailable.");
    path.resize(length);
    return fs::path(path).parent_path();
}
fs::path default_registry() {
    std::wstring path(32768, L'\0');
    const auto length =
        GetEnvironmentVariableW(L"LOCALAPPDATA", path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        return {};
    path.resize(length);
    return fs::path(path) / "AudioHole" / "PreviewBuild" / "test";
}
#else
fs::path executable_folder() {
    return fs::current_path();
}
fs::path default_registry() {
    return {};
}
#endif

int run(const std::vector<std::string>& arguments) {
    fs::path profiles = executable_folder() / "Profile";
    fs::path results = executable_folder() / "Results";
    fs::path registry = default_registry(), local;
    std::optional<fs::path> check;
    auto preset = engine::LatencyPreset::Baseline;
    for (std::size_t i = 1; i < arguments.size(); ++i) {
        const auto& option = arguments[i];
        if (option == "--low-latency") {
            preset = engine::LatencyPreset::Low;
            continue;
        }
        if ((option == "--profiles" || option == "--results" || option == "--inventory" ||
             option == "--local" || option == "--check") &&
            i + 1 < arguments.size()) {
            const auto path = scan::utf8_path(arguments[++i]);
            if (option == "--profiles")
                profiles = path;
            else if (option == "--results")
                results = path;
            else if (option == "--inventory")
                registry = path;
            else if (option == "--local")
                local = path;
            else
                check = path;
        } else {
            std::cout << "Usage: audiohole-engine-validation [--profiles PATH] [--results PATH] "
                         "[--inventory PATH] "
                         "[--local PATH] [--check FILE] [--low-latency]\n";
            return option == "--help" ? 0 : 2;
        }
    }
    if (check) {
        const auto parsed = engine::parse_profile(read_file(*check));
        if (!parsed.ok()) {
            print_issues(parsed.issues);
            return 2;
        }
        const auto prepared = engine::prepare_profile(*parsed.value);
        if (!prepared.ok()) {
            print_issues(prepared.issues);
            return 2;
        }
        std::cout << "Valid routing intent: " << parsed.value->display_name << "; "
                  << prepared.value->routes.size() << " paths. No devices opened.\n";
        return 0;
    }
    if (local.empty() && !registry.empty())
        local = registry.parent_path() / "test-local";
    if (!fs::exists(profiles))
        fs::create_directories(profiles);
    engine::Runtime runtime(preset);
    std::vector<fs::path> files;
    fs::path selected;
    Log log;
    std::cout
        << "AudioHole engine validation 0.6 — standalone, no GUI integration.\n"
           "Routes real audio after Start. Explicit Exclusive requests can interrupt other audio.\n"
           "Processing/synchronization are prototypes; latency/quality are unmeasured.\n"
        << "Latency preset: " << (preset == engine::LatencyPreset::Low ? "low" : "baseline")
        << " (native WASAPI / Experimental only; miniaudio unchanged).\n"
        << "Profile folder: " << scan::path_utf8(profiles) << '\n'
        << "Read-only device inventory: " << scan::path_utf8(registry) << '\n'
        << "Read-only local bindings: " << scan::path_utf8(local) << '\n';
    for (const auto& backend : runtime.backends())
        std::cout << engine::name(backend.id) << ": "
                  << (backend.available ? "available" : "unavailable") << " — " << backend.detail
                  << '\n';
    auto list = [&] {
        files.clear();
        for (const auto& entry : fs::directory_iterator(profiles))
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        for (std::size_t i = 0; i < files.size(); ++i)
            std::cout << i + 1 << ": " << scan::path_utf8(files[i].filename()) << '\n';
        if (files.empty())
            std::cout << "Place individual audiohole-profile JSON files in the Profile folder.\n";
    };
    list();
    std::cout << "Commands: list, select NUMBER, start, stop, status, quit.\n> " << std::flush;
    std::string line;
    auto next_log = std::chrono::steady_clock::now();
    auto tick = [&] {
        const auto state = runtime.snapshot();
        if (std::chrono::steady_clock::now() >= next_log) {
            log.write(snapshot(state));
            next_log = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        }
    };
    bool quit = false;
    auto command = [&](const std::string& input) {
        if (input == "quit" || input == "exit") {
            quit = true;
            runtime.stop();
            log.write({{"event", "quit_request"}, {"operation", runtime.snapshot().operation}});
        } else if (input == "list")
            list();
        else if (input.starts_with("select ")) {
            std::size_t used = 0;
            const auto value = std::stoul(input.substr(7), &used);
            if (used != input.size() - 7 || value == 0 || value > files.size())
                std::cout << "Choose a listed profile number.\n";
            else {
                selected = files[value - 1];
                std::cout << "Selected " << scan::path_utf8(selected.filename()) << '\n';
            }
        } else if (input == "start") {
            if (selected.empty()) {
                std::cout << "Select a profile first.\n";
                return;
            }
            const auto bytes = read_file(selected);
            const auto profile = engine::parse_profile(bytes);
            if (!profile.ok()) {
                print_issues(profile.issues);
                return;
            }
            if (registry.empty() || !fs::exists(registry) || !fs::exists(local)) {
                std::cout
                    << "Scanned registry/local bindings are unavailable. Run a real GUI scan or "
                       "supply --inventory and --local. Names are never used to guess devices.\n";
                return;
            }
            const auto result = runtime.start(*profile.value, bindings(registry, local));
            if (!result.ok()) {
                print_issues(result.issues);
                return;
            }
            try {
                log.close();
                log.open(results, selected, bytes, *result.value, preset);
            } catch (...) {
                runtime.stop(); // No unreported live route after a failed Start log.
                throw;
            }
            std::cout
                << "Start admitted. Use status for negotiated settings and actual progress.\n";
        } else if (input == "stop") {
            runtime.stop();
            // Request cancellation before potentially slow log flushing.
            log.write({{"event", "stop_request"}, {"operation", runtime.snapshot().operation}});
            std::cout << "Stop requested; status reports cleanup completion.\n";
        } else if (input == "status")
            std::cout << snapshot(runtime.snapshot()).dump(2) << '\n';
        else if (!input.empty())
            std::cout << "Unknown command. Use list, select NUMBER, start, stop, status, quit.\n";
    };
#ifdef _WIN32
    const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(console, &mode)) {
        // Poll console input so telemetry and Ctrl+C are handled on the owning
        // control thread while stdin is otherwise idle. No detached reader.
        while (!quit && !interrupted.load(std::memory_order_acquire)) {
            tick();
            if (WaitForSingleObject(console, 100) != WAIT_OBJECT_0)
                continue;
            INPUT_RECORD record{};
            DWORD count = 0;
            if (!ReadConsoleInputW(console, &record, 1, &count))
                break;
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                continue;
            const wchar_t character = record.Event.KeyEvent.uChar.UnicodeChar;
            if (character == L'\r') {
                std::cout << '\n';
                try {
                    command(line);
                } catch (const std::exception& error) {
                    std::cout << error.what() << '\n';
                }
                line.clear();
                std::cout << "> " << std::flush;
            } else if (character == L'\b' && !line.empty()) {
                line.pop_back();
                std::cout << "\b \b" << std::flush;
            } else if (character >= L' ' && character < 127 && line.size() < 1024) {
                line.push_back(static_cast<char>(character));
                std::cout << static_cast<char>(character) << std::flush;
            }
        }
    } else
#endif
    {
        while (!quit && std::getline(std::cin, line)) {
            tick();
            try {
                command(line);
            } catch (const std::exception& error) {
                std::cout << error.what() << '\n';
            }
            std::cout << "> " << std::flush;
        }
    }
    runtime.stop();
    while (runtime.snapshot().state != engine::State::Stopped) {
        tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    log.write(snapshot(runtime.snapshot()));
    std::cout << "Stopped; all owned streams and workers cleaned up.\n";
    return 0;
}
} // namespace

#ifdef _WIN32
int wmain(int count, wchar_t** values) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(console_signal, TRUE);
    std::vector<std::string> arguments;
    for (int i = 0; i < count; ++i)
        arguments.push_back(utf8(values[i]));
    try {
        return run(arguments);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#else
int main(int count, char** values) {
    try {
        return run({values, values + count});
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
