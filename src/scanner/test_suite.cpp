#include "audiohole/test_suite.hpp"
#include "storage_internal.hpp"
#include "embedded_notices.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

#ifndef AUDIOHOLE_SCANNER_VERSION
#define AUDIOHOLE_SCANNER_VERSION "development"
#endif

namespace audiohole::scanner {
namespace {
Json load(const fs::path& path) { return detail::parse_json(detail::read_optional(path).value()); }
void save(const fs::path& path, const Json& data) {
  make_atomic_writer()->write(path, data.dump(2) + '\n', detail::read_optional(path));
}
void text_file(const fs::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary);
  output << text; output.flush();
  if (!output) throw std::runtime_error("Cannot write test file: " + path_utf8(path));
}
std::vector<fs::path> records(const fs::path& root) {
  std::vector<fs::path> paths;
  for (const auto* direction : {"recording", "playback"}) {
    const auto folder = root / "portable" / "devices" / direction;
    if (!fs::exists(folder)) continue;
    for (const auto& entry : fs::directory_iterator(folder)) {
      if (entry.is_symlink()) throw std::runtime_error("Symlink in test device inventory");
      if (entry.path().extension() == ".json") paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end()); return paths;
}
Json prepare_user_edits(const fs::path& root) {
  Json expected = Json::array();
  unsigned index = 0;
  for (const auto& path : records(root)) {
    auto data = load(path);
    const auto name = "User Renamed " + data["identity"]["direction"].get<std::string>() + " " + std::to_string(++index) + " - \xc3\xa9.json";
    const auto note = "Preserve this note exactly.\nSecond line: \xc3\xa9 \xce\xa9 \xe6\xb8\xac\xe8\xa9\xa6; spaces  stay.\n";
    data["notes"] = note;
    // Deliberately leave display_name stale. The next scan must reconcile it
    // from the filename without changing the original Windows name or notes.
    save(path, data);
    const auto renamed = path.parent_path() / utf8_path(name);
    fs::rename(path, renamed);
    expected.push_back(Json{{"logical_id", data["logical_id"]}, {"relative_file", path_utf8(renamed.lexically_relative(root))},
      {"display_name", path_utf8(renamed.stem())}, {"device_name", data["device_name"]}, {"notes", note}});
  }
  return expected;
}
Json check_user_edits(const fs::path& root, const Json& expected, const std::string& omitted_id = {}) {
  Json result{{"result", "passed"}, {"checked", 0}, {"checks", Json::array()}};
  std::map<std::string, std::vector<fs::path>> ids;
  for (const auto& path : records(root)) ids[load(path)["logical_id"].get<std::string>()].push_back(path);
  for (const auto& e : expected) {
    const auto id = e["logical_id"].get<std::string>();
    if (id == omitted_id) continue; // Explicit Update fixture, not a deleted user record.
    const auto path = root / utf8_path(e["relative_file"]);
    Json row{{"logical_id", id}, {"filename_preserved", fs::exists(path)},
      {"single_record", ids[id].size() == 1}, {"notes_preserved", false},
      {"logical_id_preserved", false}, {"device_name_preserved", false}, {"display_name_from_filename", false}};
    if (fs::exists(path)) {
      const auto data = load(path);
      row["notes_preserved"] = data["notes"] == e["notes"];
      row["logical_id_preserved"] = data["logical_id"] == e["logical_id"];
      row["device_name_preserved"] = data["device_name"] == e["device_name"];
      row["display_name_from_filename"] = data["display_name"] == e["display_name"];
    }
    for (auto it = row.begin(); it != row.end(); ++it) if (it.value().is_boolean() && !it.value().get<bool>()) result["result"] = "failed";
    result["checks"].push_back(std::move(row));
  }
  result["checked"] = result["checks"].size();
  if (result["checks"].empty()) {
    result["result"] = "not_tested";
    result["reason"] = "No retained edited records were available for this check";
  }
  return result;
}
void seed(const fs::path& source, const fs::path& destination, const std::string& omitted_id = {}) {
  for (const auto& path : records(source)) {
    if (load(path)["logical_id"] == omitted_id) continue;
    const auto target = destination / path.lexically_relative(source);
    fs::create_directories(target.parent_path());
    fs::copy_file(path, target); // No overwrite, and only inside the new suite directory.
  }
  auto bindings = load(source / "machine-local" / "device_bindings.json");
  bindings["bindings"].erase(omitted_id);
  save(destination / "machine-local" / "device_bindings.json", bindings);
}
bool uncertain(const Json& deep) {
  if (deep.contains("diagnostics")) for (const auto& d : deep["diagnostics"])
    if (d.value("code", "") == "batch_stopped_after_device_error" || d.value("code", "") == "deep_batch_failed") return true;
  return false;
}
}

TestSuiteResult run_test_suite(const TestSuiteOptions& options, const ProviderFactory& provider_factory,
    const ProbeRunnerFactory& runner_factory, bool consent, std::stop_token stop, const ProgressSink& progress) {
  TestSuiteResult result;
  result.manifest = {{"suite_version", AUDIOHOLE_SCANNER_VERSION}, {"result", "running"}, {"started_utc", utc_now()},
    {"hardware_source", options.data_source},
    {"audio_content_saved", false}, {"signal_verification", "not_tested"},
    {"limits", {{"mode", options.deep_limits.mode}, {"max_attempts_per_device", options.deep_limits.max_attempts},
      {"duration_ms", options.deep_limits.duration_ms}, {"attempt_timeout_ms", options.deep_limits.attempt_timeout_ms},
      {"device_timeout_ms", options.deep_limits.total_timeout_ms}, {"system_timeout_ms", options.system_timeout_ms}}},
    {"update_fixture", "A real endpoint is withheld ONLY from a copied saved inventory; no endpoint is simulated, disconnected, or created"},
    {"cases", Json::array()}, {"diagnostics", Json::array()}};
  for (const auto* depth : {"basic", "deep"}) for (const auto* scope : {"full", "update", "device"})
    result.manifest["cases"].push_back(Json{{"id", std::string(scope) + "-" + depth}, {"scope", scope}, {"depth", depth},
      {"result", "not_tested"}, {"phases", Json::array()}});
  // Reject before hardware or filesystem access when the suite is unconsented.
  if (!consent) {
    result.manifest["result"] = "failed";
    result.manifest["diagnostics"].push_back({{"code", "consent_required"}, {"message", "Automated live Deep tests require explicit authorization"}});
    return result;
  }
  std::ofstream log;
  std::map<fs::path, std::string> snapshots;
  const auto checkpoint = [&] {
    const auto path = result.directory / "manifest.json";
    const auto bytes = result.manifest.dump(2) + '\n';
    make_atomic_writer()->write(path, bytes, snapshots.contains(path) ? std::optional(snapshots[path]) : std::nullopt);
    snapshots[path] = bytes;
  };
  const auto begin = std::chrono::steady_clock::now();
  const auto deadline = begin + std::chrono::milliseconds(options.system_timeout_ms);
  bool halted = false;
  std::string halt_reason;
  Json expected = Json::array();
  try {
    validate_deep_limits(options.deep_limits);
    if (options.system_timeout_ms < 1000 || options.system_timeout_ms > 7200000)
      throw std::invalid_argument("Suite system timeout must be 1000..7200000 ms");
    fs::create_directories(options.output_parent);
    auto name = utc_now(); std::replace(name.begin(), name.end(), ':', '-');
    name = "AudioHole-Test-" + name + "-" + new_logical_id("run");
    result.directory = options.output_parent / utf8_path(name);
    result.archive = options.output_parent / utf8_path(name + ".zip");
    if (!fs::create_directory(result.directory)) throw std::runtime_error("Cannot create a unique test directory");
    result.manifest["archive"] = path_utf8(result.archive.filename());
    log.open(result.directory / "debug.txt", std::ios::binary);
    if (!log) throw std::runtime_error("Cannot open suite debug TXT");
    log.exceptions(std::ios::badbit | std::ios::failbit);
    text_file(result.directory / "Third-Party-Notices.txt", embedded_notices());
    text_file(result.directory / "README.txt",
      "AudioHole automated scan test. Data source: " + options.data_source + "\n"
      "manifest.json lists all six scope/depth cases, results, and report paths.\n"
      "debug.txt contains identity selections, API stages, HRESULTs and local IDs.\n"
      "The distributed executable uses only real Windows endpoints. No audio samples are saved.\n"
      "Update uses a copied inventory with one REAL endpoint's saved record omitted to exercise discovery.\n"
      "No fake endpoint or actual unplug/replug is involved. The repeat Update should select no known devices.\n"
      "All file renames/note edits occur in this private test copy; original scans/configs are untouched.\n"
      "Cases retain separate device snapshots so repeated logical devices across CASE folders are intentional.\n"
      "Verified means bounded buffer processing only; signal quality and long-term stability are untested.\n"
      "Send the sibling ZIP for analysis. It includes device/driver identifiers and machine-local debug paths.\n");
    checkpoint();
    const auto remaining = [&]() -> unsigned {
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      return ms > 0 ? static_cast<unsigned>(ms) : 0;
    };
    const auto ready = [&] {
      if (halted) return false;
      if (stop.stop_requested()) { halted = true; halt_reason = "Canceled by user"; return false; }
      if (remaining() < options.deep_limits.duration_ms + 1000) { halted = true; halt_reason = "Suite scheduling deadline exhausted"; return false; }
      return true;
    };
    const auto base = result.directory / "full-basic";
    std::vector<ProbeTarget> baseline_targets;
    std::string withheld_id, withheld_endpoint;
    auto provider = provider_factory();
    if (!provider) throw std::runtime_error("No endpoint provider was supplied");
    auto runner = runner_factory();
    if (!runner) throw std::runtime_error("No live probe runner was supplied");
    for (auto& test : result.manifest["cases"]) {
      if (!ready()) break;
      const auto case_id = test["id"].get<std::string>();
      const auto scope = test["scope"].get<std::string>();
      const bool deep = test["depth"] == "deep";
      const auto root = result.directory / utf8_path(case_id);
      const auto omit = scope == "update" ? withheld_id : std::string{};
      test["result"] = "running";
      test["planned_phases"] = scope == "device" ? baseline_targets.size() : (scope == "update" || case_id == "full-basic") ? 2 : 1;
      checkpoint();
      if (case_id != "full-basic") seed(base, root, omit);
      DeviceStore store(root / "portable", root / "machine-local");
      if (scope == "update") {
        test["fixture_omitted_logical_id"] = omit;
        test["fixture_endpoint_id"] = withheld_endpoint;
      }
      std::size_t phase_index = 0;
      const auto phase = [&](const std::string& label, const std::string& selected_id, bool no_new_expected = false) {
        const auto tag = std::to_string(++phase_index) + "-" + label;
        const auto report_path = root / "reports" / utf8_path(tag + ".json");
        Json report{{"case", case_id}, {"label", label}, {"scope", scope}, {"depth", deep ? "deep" : "basic"},
          {"selected_logical_id", selected_id}, {"started_utc", utc_now()}, {"result", "running"}};
        const auto events = [&](const Progress& p) {
          log << '[' << utc_now() << "] " << case_id << '/' << tag << '\n'
            << Json{{"phase", p.phase}, {"endpoint_id", p.endpoint_id}, {"completed", p.completed}, {"total", p.total}, {"details", p.details}}.dump(2) << '\n';
          log.flush(); if (progress) progress(p);
        };
        events({"suite_phase_started", {}, phase_index, 0, {{"case", case_id}, {"label", label}, {"logical_id", selected_id}}});
        const auto basic = run_scoped_basic(*provider, store, scope, selected_id, stop, events);
        report["scope_selection"] = basic;
        report["result"] = basic.scan.result;
        report["deep_scan"] = status("not_tested");
        save(report_path, report); // Preserve the Basic result before opening a stream.
        if (deep && basic.scan.result != "failed" && basic.scan.result != "interrupted" && ready()) {
          auto targets = store.probe_targets();
          std::erase_if(targets, [&](const auto& t) {
            return std::find(basic.selected_endpoint_ids.begin(), basic.selected_endpoint_ids.end(), t.endpoint_id) == basic.selected_endpoint_ids.end();
          });
          if (targets.empty()) {
            report["deep_scan"] = {{"result", scope == "update" && basic.selected_endpoint_ids.empty() ? "completed" : "not_tested"},
              {"devices_selected", 0}, {"reason", "No eligible selected active endpoints; no stream opened"}};
            if (report["deep_scan"]["result"] == "not_tested") report["result"] = "partially_completed";
          } else {
            const auto time_left = remaining();
            if (time_left < options.deep_limits.duration_ms + 1000) {
              halted = true; halt_reason = "Suite scheduling deadline exhausted";
              report["result"] = "partially_completed";
              report["deep_scan"] = {{"result", "not_tested"}, {"reason", halt_reason}};
            } else {
              const auto batch = run_deep_scan_batch(store, *runner, targets, options.deep_limits,
                time_left, true, stop, events, [&](const auto& state) { report["deep_scan"] = state; save(report_path, report); });
              report["deep_scan"] = batch;
              if (batch.result != "completed") report["result"] = batch.result;
              if (uncertain(report["deep_scan"])) { halted = true; halt_reason = "Worker cleanup or probe failure; later hardware tests were stopped"; }
            }
          }
        }
        if (deep && basic.scan.result != "failed" && basic.scan.result != "interrupted" &&
            report["deep_scan"].value("status", "") == "not_tested") {
          report["result"] = stop.stop_requested() ? "interrupted" : "partially_completed";
          report["deep_scan"] = {{"result", "not_tested"}, {"reason", halt_reason.empty() ? "Suite stopped before Deep tests" : halt_reason}};
        }
        if (no_new_expected) {
          report["repeat_update_selected_none"] = basic.selected_endpoint_ids.empty();
          if (!basic.selected_endpoint_ids.empty()) report["result"] = "partially_completed";
        }
        if (scope == "update" && label == "scan") {
          const bool found = !withheld_endpoint.empty() && std::find(basic.selected_endpoint_ids.begin(), basic.selected_endpoint_ids.end(), withheld_endpoint) != basic.selected_endpoint_ids.end();
          report["withheld_real_endpoint_selected"] = found;
          if (!found && report["result"] == "completed") report["result"] = "partially_completed";
        }
        report["finished_utc"] = utc_now(); save(report_path, report);
        Json summary{{"report_file", path_utf8(report_path.lexically_relative(result.directory))}, {"result", report["result"]},
          {"label", label}, {"selected_endpoints", basic.selected_endpoint_ids.size()}, {"known_skipped", basic.known_skipped}};
        if (no_new_expected) summary["repeat_update_selected_none"] = report["repeat_update_selected_none"];
        test["phases"].push_back(std::move(summary)); checkpoint();
        return report;
      };
      if (scope == "device") {
        if (baseline_targets.empty()) test["reason"] = "No active real endpoint was available for per-device tests";
        for (const auto& target : baseline_targets) {
          if (!ready()) break;
          phase("selected-device", target.logical_id);
        }
      } else {
        if (ready()) phase("scan", {});
        if (case_id == "full-basic" && ready()) {
          if (!fs::exists(root / "machine-local" / "device_bindings.json")) throw std::runtime_error("Baseline inventory could not be saved");
          expected = prepare_user_edits(base);
          save(result.directory / "user_edit_expectations.json", Json{{"records", expected}});
          phase("rescan-after-filename-and-notes-edits", {});
          baseline_targets = store.probe_targets();
          if (!baseline_targets.empty()) {
            withheld_id = baseline_targets.front().logical_id;
            withheld_endpoint = baseline_targets.front().endpoint_id;
          } else if (!expected.empty()) {
            withheld_id = expected.front()["logical_id"].get<std::string>();
            withheld_endpoint = load(base / "machine-local" / "device_bindings.json")["bindings"][withheld_id]["endpoint_id"].get<std::string>();
          }
        }
        if (scope == "update" && ready()) phase("repeat-update-no-new-devices", {}, true);
      }
      test["user_edits_verification"] = check_user_edits(root, expected, omit);
      test["result"] = test["phases"].empty() ? "not_tested" : "completed";
      for (const auto& p : test["phases"]) {
        if (p["result"] == "failed") test["result"] = "failed";
        else if (p["result"] != "completed" && test["result"] != "failed") test["result"] = "partially_completed";
      }
      if (test["user_edits_verification"]["result"] == "failed") test["result"] = "failed";
      if (test["phases"].size() < test["planned_phases"].get<std::size_t>() && test["result"] == "completed")
        test["result"] = "partially_completed";
      if (stop.stop_requested() && test["result"] != "failed") test["result"] = "interrupted";
      checkpoint();
    }
    if (halted) result.manifest["halt_reason"] = halt_reason;
    result.result = stop.stop_requested() ? "interrupted" : halted ? "partially_completed" : "completed";
    for (const auto& test : result.manifest["cases"]) {
      if (test["result"] == "failed" && result.result != "interrupted") result.result = "failed";
      else if (test["result"] != "completed" && result.result == "completed") result.result = "partially_completed";
    }
  } catch (const std::exception& e) {
    result.result = stop.stop_requested() ? "interrupted" : "failed";
    result.manifest["diagnostics"].push_back({{"code", "suite_failed"}, {"message", e.what()}});
  }
  result.manifest["result"] = result.result;
  for (auto& test : result.manifest["cases"]) {
    if (test["result"] == "running") {
      test["result"] = result.result == "interrupted" ? "interrupted" : "failed";
      test["reason"] = "Suite ended before this case could be finalized";
    }
    if (test["result"] == "not_tested" && !test.contains("reason"))
      test["reason"] = halt_reason.empty() ? "Suite ended before this case was reached" : halt_reason;
  }
  result.manifest["finished_utc"] = utc_now();
  result.manifest["elapsed_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
  if (!result.directory.empty()) {
    try {
      checkpoint();
      if (log.is_open()) { log << "Suite finished: " << result.result << '\n'; log.close(); }
      zip_results(result.directory, result.archive);
    } catch (const std::exception& e) {
      result.result = "failed"; result.manifest["result"] = "failed";
      result.manifest["diagnostics"].push_back({{"code", "result_packaging_failed"}, {"message", e.what()}});
      try { checkpoint(); } catch (...) {}
      result.archive.clear(); // Never advertise a ZIP that did not finish.
    }
  }
  return result;
}
}
