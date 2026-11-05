#include "audiohole/scan_utility.hpp"
#include "storage_internal.hpp"

#include <chrono>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

#ifndef AUDIOHOLE_SCANNER_VERSION
#define AUDIOHOLE_SCANNER_VERSION "development"
#endif

namespace audiohole::scanner {
UtilityOptions utility_options(const std::vector<fs::path>& arguments, const fs::path& executable_directory) {
  UtilityOptions result;
  result.results_root = executable_directory / "AudioHole-Scan";
  std::set<std::string> seen;
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    const auto option = path_utf8(arguments[i]);
    if (option == "--help") { result.help = true; continue; }
    if (!seen.insert(option).second) throw std::invalid_argument("Duplicate option: " + option);
    if (option == "--deep") { result.deep = true; continue; }
    if (option == "--deep-all") { result.deep = true; result.deep_all = true; continue; }
    if (option == "--confirm-disruption") { result.confirm_disruption = true; continue; }
    if (option != "--results" && option != "--output" && option != "--local-output" && option != "--debug-log" &&
        option != "--device" && option != "--mode" && option != "--max-attempts" && option != "--duration-ms" &&
        option != "--attempt-timeout-ms" && option != "--total-timeout-ms" && option != "--system-timeout-ms")
      throw std::invalid_argument("Unknown option: " + option + "; use --help");
    if (i + 1 >= arguments.size()) throw std::invalid_argument("Missing value: " + option);
    const auto path = arguments[++i];
    if (path.empty() || path_utf8(path).starts_with("--"))
      throw std::invalid_argument("Expected a path after " + option);
    if (option == "--results") result.results_root = path;
    else if (option == "--output") result.portable_root = path;
    else if (option == "--local-output") result.local_root = path;
    else if (option == "--debug-log") result.debug_log = path;
    else if (option == "--device") result.logical_device = path_utf8(path);
    else if (option == "--mode") result.deep_limits.mode = path_utf8(path);
    else {
      std::size_t consumed = 0;
      const auto value = std::stoul(path_utf8(path), &consumed);
      if (consumed != path_utf8(path).size() || value > (option == "--system-timeout-ms" ? 7200000UL : 600000UL))
        throw std::invalid_argument("Invalid numeric Deep Scan option");
      if (option == "--max-attempts") result.deep_limits.max_attempts = value;
      else if (option == "--duration-ms") result.deep_limits.duration_ms = static_cast<unsigned>(value);
      else if (option == "--attempt-timeout-ms") result.deep_limits.attempt_timeout_ms = static_cast<unsigned>(value);
      else if (option == "--system-timeout-ms") result.system_timeout_ms = static_cast<unsigned>(value);
      else result.deep_limits.total_timeout_ms = static_cast<unsigned>(value);
    }
  }
  if (result.portable_root.empty()) result.portable_root = result.results_root / "portable";
  if (result.local_root.empty()) result.local_root = result.results_root / "machine-local";
  if (!result.deep) {
    for (const auto* key : {"--confirm-disruption", "--device", "--mode", "--max-attempts", "--duration-ms", "--attempt-timeout-ms", "--total-timeout-ms", "--system-timeout-ms"})
      if (seen.contains(key)) throw std::invalid_argument("Deep Scan options require --deep");
  } else {
    validate_deep_limits(result.deep_limits);
    if (result.deep_all && !result.logical_device.empty()) throw std::invalid_argument("--deep-all and --device are mutually exclusive");
    if (result.confirm_disruption && result.logical_device.empty() && !result.deep_all)
      throw std::invalid_argument("--confirm-disruption requires --device ID or --deep-all");
    if (seen.contains("--system-timeout-ms") && !result.deep_all)
      throw std::invalid_argument("--system-timeout-ms requires --deep-all");
    if (result.system_timeout_ms < 1000 || result.system_timeout_ms > 7200000)
      throw std::invalid_argument("System timeout must be 1000..7200000 ms");
  }
  return result;
}

namespace {
class DebugLog {
 public:
  explicit DebugLog(const fs::path& path) {
    if (path.extension() != ".txt") throw std::runtime_error("Debug log must have a .txt extension");
    if (fs::exists(path) && (fs::is_symlink(fs::symlink_status(path)) || !fs::is_regular_file(path)))
      throw std::runtime_error("Debug log must be a regular file");
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
    stream_.open(path, std::ios::binary | std::ios::app);
    if (!stream_) throw std::runtime_error("Cannot open debug text file: " + path_utf8(path));
    stream_.exceptions(std::ios::badbit | std::ios::failbit);
  }
  void event(const std::string& label, const Json& details = Json::object()) {
    stream_ << '[' << utc_now() << "] " << label;
    if (!details.empty()) stream_ << '\n' << details.dump(2);
    stream_ << '\n';
    stream_.flush(); // Retain the last API stage even if a driver stalls afterward.
  }
 private:
  std::ofstream stream_;
};
class LoggedProvider final : public EndpointProvider {
 public:
  LoggedProvider(EndpointProvider& provider, DebugLog& log) : provider_(provider), log_(log) {}
  Discovery discover(std::stop_token stop, const ProgressSink& progress, const EndpointFilter& filter = {}) override {
    log_.event("Discovery started");
    auto discovery = provider_.discover(stop, progress, filter);
    for (const auto& endpoint : discovery.endpoints)
      log_.event("Endpoint observation (reported data; no streaming verification)", Json{
        {"device_name", endpoint.device_name}, {"endpoint_id", endpoint.endpoint_id},
        {"direction", endpoint.direction}, {"state", endpoint.state},
        {"fingerprint", endpoint.fingerprint}, {"local_identity", endpoint.local_identity},
        {"driver", endpoint.driver}, {"reported_capabilities", endpoint.reported_capabilities},
        {"diagnostics", endpoint.diagnostics}});
    log_.event("Discovery finished", Json{{"complete", discovery.complete}, {"cancelled", discovery.cancelled},
      {"observed_endpoints", discovery.endpoints.size()}, {"diagnostics", discovery.diagnostics}});
    return discovery;
  }
 private:
  EndpointProvider& provider_;
  DebugLog& log_;
};
std::string compiler() {
#ifdef _MSC_VER
  return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__MINGW32__)
  return std::string("MinGW GCC ") + __VERSION__;
#elif defined(__GNUC__)
  return std::string("GCC ") + __VERSION__;
#else
  return "unknown";
#endif
}
}

UtilityResult run_scan_utility(const UtilityOptions& options, const ProviderFactory& factory,
                              std::stop_token stop, const ProgressSink& progress,
                              const ProbeRunnerFactory& probe_factory, const DeepInteraction& interaction) {
  UtilityResult result;
  const auto start = std::chrono::steady_clock::now();
  const auto started_utc = utc_now();
  auto run_id = started_utc;
  for (auto& character : run_id) if (character == ':') character = '-';
  const auto scan_kind = options.deep_all ? "deep-all" : options.deep ? "deep" : "basic";
  run_id += '-' + new_logical_id(scan_kind);
  result.debug_log = options.debug_log.empty() ? options.results_root / "logs" / (run_id + ".txt") : options.debug_log;
  result.report_json = options.results_root / "reports" / (run_id + ".json");
  std::unique_ptr<DebugLog> log;
  Json basic_report = status("not_tested");
  std::optional<std::string> report_bytes;
  const auto export_report = [&](const Json& snapshot) {
    const auto bytes = snapshot.dump(2) + '\n';
    make_atomic_writer()->write(result.report_json, bytes, report_bytes);
    report_bytes = bytes;
  };
  const Json limits{{"mode", options.deep_limits.mode}, {"max_attempts_per_device", options.deep_limits.max_attempts},
    {"duration_ms", options.deep_limits.duration_ms}, {"attempt_timeout_ms", options.deep_limits.attempt_timeout_ms},
    {"device_timeout_ms", options.deep_limits.total_timeout_ms}, {"system_timeout_ms", options.system_timeout_ms}};
  const auto annotate_deep_report = [&] {
    result.deep_report["limits"] = limits;
    result.deep_report["coverage"] = "bounded_candidate_matrix; unattempted configurations remain untested";
  };
  try {
    log = std::make_unique<DebugLog>(result.debug_log);
    log->event("AudioHole standalone Basic Scan started", Json{
      {"version", AUDIOHOLE_SCANNER_VERSION}, {"compiler", compiler()}, {"architecture_bits", sizeof(void*) * 8},
#ifdef _WIN32
      {"platform", "Windows"},
#else
      {"platform", "non-Windows (hardware discovery unavailable)"},
#endif
      {"started_utc", started_utc}, {"run_id", run_id},
      {"portable_root", path_utf8(fs::absolute(options.portable_root))},
      {"local_root", path_utf8(fs::absolute(options.local_root))},
      {"debug_log", path_utf8(fs::absolute(result.debug_log))},
      {"report_json", path_utf8(fs::absolute(result.report_json))},
      {"scan_kind", options.deep ? "basic_then_consented_deep" : "basic_query_only"}, {"stream_tests", "not_tested"}});
    auto provider = factory();
    if (!provider) throw std::runtime_error("Endpoint provider factory returned no provider");
    LoggedProvider observed(*provider, *log);
    DeviceStore store(options.portable_root, options.local_root);
    BasicScanner scanner(observed, store);
    const auto report_progress = [&](const Progress& item) {
      Json details{{"phase", item.phase}, {"endpoint_id", item.endpoint_id},
                   {"completed", item.completed}, {"total", item.total}};
      if (!item.details.empty()) details["details"] = item.details;
      log->event("Progress", details);
      if (progress) progress(item);
    };
    result.report = scanner.scan(stop, report_progress);
    basic_report = result.report;
    if (options.deep && result.report.result != "failed" && result.report.result != "interrupted") {
      detail::StoreLock deep_lock(options.local_root / "deep-scan-session");
      const auto targets = store.probe_targets();
      if (options.deep_all) {
        if (targets.empty()) throw std::runtime_error("No active, uniquely bound endpoints are available for Deep Scan");
        const bool consent = options.confirm_disruption ||
          (interaction.confirm_all && interaction.confirm_all(targets, options.deep_limits, options.system_timeout_ms));
        log->event("Full-system Deep Scan scope and consent", {{"devices_selected", targets.size()}, {"limits", limits}, {"consent", consent}});
        if (!consent) {
          result.deep_report = {{"result", "interrupted"}, {"scope", "all_active_endpoints"}, {"reason", "Active probing not authorized"}};
          result.report.result = "interrupted";
        } else {
          if (!probe_factory) throw std::runtime_error("No isolated probe runner was supplied");
          auto runner = probe_factory();
          if (!runner) throw std::runtime_error("Probe runner factory returned no runner");
          const auto batch = run_deep_scan_batch(store, *runner, targets, options.deep_limits, options.system_timeout_ms,
            true, stop, report_progress, [&](const DeepBatchReport& checkpoint) {
              result.deep_report = checkpoint; annotate_deep_report();
              Json snapshot = result.report;
              snapshot["result"] = "running"; snapshot["basic_scan"] = basic_report;
              snapshot["deep_scan"] = result.deep_report; snapshot["scan_kind"] = scan_kind;
              snapshot["started_utc"] = started_utc; snapshot["updated_utc"] = utc_now();
              snapshot["debug_log"] = path_utf8(fs::absolute(result.debug_log));
              export_report(snapshot);
            });
          result.deep_report = batch; annotate_deep_report();
          result.report.diagnostics.insert(result.report.diagnostics.end(), batch.diagnostics.begin(), batch.diagnostics.end());
          if (batch.result != "completed") result.report.result = batch.result;
          log->event("Full-system Deep Scan finished", result.deep_report);
        }
      } else {
        auto selected = options.logical_device;
        if (selected.empty() && interaction.select_target) selected = interaction.select_target(targets);
        const auto target = std::find_if(targets.begin(), targets.end(), [&](const auto& t) { return t.logical_id == selected; });
        if (selected.empty()) {
          result.deep_report = Json{{"result", "interrupted"}, {"reason", "No Deep Scan target selected"}};
          result.report.result = "interrupted";
        } else {
          if (target == targets.end()) throw std::runtime_error("Selected logical device is not uniquely bound to an active endpoint");
          const bool consent = options.confirm_disruption || (interaction.confirm && interaction.confirm(*target, options.deep_limits));
          log->event("Deep Scan target and consent", {{"logical_id", target->logical_id}, {"display_name", target->display_name},
            {"direction", target->direction}, {"mode", options.deep_limits.mode}, {"consent", consent}});
          if (!consent) {
            result.deep_report = Json{{"result", "interrupted"}, {"reason", "Active probing not authorized"}};
            result.report.result = "interrupted";
          } else {
            if (!probe_factory) throw std::runtime_error("No isolated probe runner was supplied");
            auto runner = probe_factory();
            if (!runner) throw std::runtime_error("Probe runner factory returned no runner");
            const auto deep = run_deep_scan(store, *runner, *target, options.deep_limits, true, stop, report_progress);
            result.deep_report = deep;
            result.deep_report["display_name"] = target->display_name;
            result.deep_report["direction"] = target->direction;
            result.deep_report["scope"] = "single_endpoint";
            annotate_deep_report();
            result.report.diagnostics.insert(result.report.diagnostics.end(), deep.diagnostics.begin(), deep.diagnostics.end());
            if (deep.result != "completed") result.report.result = deep.result;
            log->event("Deep Scan finished", result.deep_report);
          }
        }
      }
    }
  } catch (const std::exception& error) {
    result.report.result = "failed";
    result.report.diagnostics.push_back({"utility_failed", error.what(), path_utf8(result.debug_log), {}});
  }
  const auto finished_utc = utc_now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  if (log) {
    try {
      log->event("Final scan result", Json(result.report));
      log->event("Session finished; exporting structured report", Json{{"finished_utc", finished_utc},
        {"elapsed_ms", elapsed}, {"report_json", path_utf8(fs::absolute(result.report_json))}});
    } catch (const std::exception& error) {
      result.report.diagnostics.push_back({"debug_log_write_failed", error.what(), path_utf8(result.debug_log), {}});
      if (result.report.result != "failed") result.report.result = "partially_completed";
    }
  }
  Json report = result.report;
  report["deep_scan"] = result.deep_report;
  report["basic_scan"] = basic_report;
  report["scan_kind"] = scan_kind;
  report["started_utc"] = started_utc;
  report["finished_utc"] = finished_utc;
  report["elapsed_ms"] = elapsed;
  report["debug_log"] = path_utf8(fs::absolute(result.debug_log));
  try {
    export_report(report);
  } catch (const std::exception& error) {
    result.report.diagnostics.push_back({"report_write_failed", error.what(), path_utf8(result.report_json), {}});
    if (result.report.result != "failed") result.report.result = "partially_completed";
    if (log) try { log->event("Structured report export failed", Json(result.report)); } catch (...) {}
  }
  return result;
}
}
