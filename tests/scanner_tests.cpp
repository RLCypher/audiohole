#include "audiohole/scanner.hpp"
#include "audiohole/scan_utility.hpp"
#include "audiohole/test_suite.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <set>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace audiohole::scanner;
namespace {
using Test = std::pair<const char*, void(*)()>;
std::vector<Test>& tests() { static std::vector<Test> all; return all; }
struct Register { Register(const char* name, void(*body)()) { tests().emplace_back(name, body); } };
#define TEST(name) void name(); Register register_##name(#name, name); void name()
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string("Check failed: ") + #condition + " at line " + std::to_string(__LINE__)); } while (false)

std::string bytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot open fixture file: " + path_utf8(path));
  const std::string result(std::istreambuf_iterator<char>(in), {});
  if (in.bad()) throw std::runtime_error("Cannot read fixture file: " + path_utf8(path));
  return result;
}
Json read(const fs::path& path) { return Json::parse(bytes(path)); }
void write(const fs::path& path, const Json& data) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << data.dump(2) << '\n'; out.flush();
  if (!out) throw std::runtime_error("Cannot write fixture file: " + path_utf8(path));
}
// The suite nests six cases, renamed Unicode records, reports and atomic-write
// suffixes under this root. Use extended Windows paths so runner-specific TEMP
// lengths cannot prevent the fixture from reaching the behavior under test.
fs::path fixture_root() {
  auto path = fs::absolute(fs::temp_directory_path() / new_logical_id("audiohole-tests"));
  path.make_preferred();
#ifdef _WIN32
  const auto native = path.native();
  if (native.starts_with(L"\\\\?\\")) return path;
  if (native.starts_with(L"\\\\")) return fs::path(L"\\\\?\\UNC\\" + native.substr(2));
  return fs::path(L"\\\\?\\" + native);
#else
  return path;
#endif
}
struct Fixture {
  fs::path root = fixture_root();
  fs::path portable = root / "portable", local = root / "local";
  DeviceStore store{portable, local};
  ~Fixture() {
    std::error_code ignored;
    if (fs::exists(root)) for (auto& entry : fs::recursive_directory_iterator(root))
      fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::add, ignored);
    fs::remove_all(root, ignored);
  }
  std::vector<fs::path> files() const {
    std::vector<fs::path> result;
    for (const auto* dir : {"recording", "playback"}) {
      const auto path = portable / "devices" / dir;
      if (!fs::exists(path)) continue;
      for (auto& entry : fs::directory_iterator(path))
        if (entry.path().extension() == ".json") result.push_back(entry.path());
    }
    std::sort(result.begin(), result.end());
    return result;
  }
};
Endpoint endpoint(std::string id = "endpoint-one", std::string name = "Speakers") {
  Endpoint e;
  e.endpoint_id = std::move(id); e.device_name = std::move(name);
  e.direction = "render"; e.state = "active";
  e.reported_capabilities = {{"default_mix_format", status("unavailable")}, {"exclusive_mode", "unknown"}};
  e.driver = {{"version", "1.0"}, {"association_status", "available"}};
  return e;
}
Discovery discovery(std::vector<Endpoint> endpoints) { return {std::move(endpoints), {}, true, false}; }
constexpr auto first = "2026-11-03T01:00:00Z";
constexpr auto second = "2026-11-03T02:00:00Z";

TEST(initial_discovery_and_repeat) {
  Fixture f;
  auto report = f.store.reconcile(discovery({endpoint()}), first);
  CHECK(report.result == "completed"); CHECK(report.saved == 1);
  CHECK(f.files().size() == 1);
  const auto original = read(f.files()[0]);
  CHECK(original["notes"] == "");
  CHECK(original["metadata"]["schema_version"] == 2);
  CHECK(original["identity"]["direction"] == "render");
  CHECK(original.dump().find("endpoint-one") == std::string::npos);
  auto changed = original;
  changed["notes"] = "User notes\nwith exact spacing  and Unicode: \xc3\xa9";
  changed["user_extension"] = {{"keep", true}};
  write(f.files()[0], changed);
  report = f.store.reconcile(discovery({endpoint()}), second);
  CHECK(report.result == "completed"); CHECK(f.files().size() == 1);
  const auto current = read(f.files()[0]);
  CHECK(current["notes"] == changed["notes"]);
  CHECK(current["user_extension"] == changed["user_extension"]);
  CHECK(current["logical_id"] == original["logical_id"]);
  CHECK(current["scan_history"]["first_scanned"] == first);
  CHECK(current["scan_history"]["last_scanned"] == second);
  CHECK(read(fs::path(f.files()[0].string() + ".bak")) == changed);
  CHECK(bytes(f.files()[0]).find("\"display_name\"") < bytes(f.files()[0]).find("\"device_name\""));
}
TEST(external_rename_and_stale_display_name) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  const auto old = f.files()[0];
  const auto renamed = old.parent_path() / "Desk Speakers.json";
  const auto id = read(old)["logical_id"];
  fs::rename(old, renamed);
  auto record = read(renamed);
  record["display_name"] = "Ignored stale field";
  write(renamed, record);
  CHECK(f.store.reconcile(discovery({endpoint()}), second).result == "completed");
  CHECK(f.files().size() == 1);
  CHECK(read(renamed)["display_name"] == "Desk Speakers");
  CHECK(read(renamed)["device_name"] == "Speakers");
  CHECK(read(renamed)["logical_id"] == id);
}
TEST(windows_name_changes_preserve_original_name) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  CHECK(f.store.reconcile(discovery({endpoint("endpoint-one", "Renamed in Windows")}), second).result == "completed");
  CHECK(f.files().size() == 1);
  const auto record = read(f.files()[0]);
  CHECK(record["display_name"] == "Speakers");
  CHECK(record["device_name"] == "Speakers");
  CHECK(record["identity"]["current_device_name"] == "Renamed in Windows");
  CHECK(record["identity"]["name_history"].size() == 1);
}
TEST(identical_names_keep_separate_ids_and_files) {
  Fixture f;
  auto scan = discovery({endpoint("one"), endpoint("two")});
  CHECK(f.store.reconcile(scan, first).result == "completed");
  CHECK(f.files().size() == 2);
  CHECK(read(f.files()[0])["logical_id"] != read(f.files()[1])["logical_id"]);
  CHECK(f.store.reconcile(scan, second).result == "completed");
  CHECK(f.files().size() == 2);
}
TEST(same_hardware_capture_and_render_are_separate) {
  Fixture f;
  auto capture = endpoint("capture"); capture.direction = "capture";
  CHECK(f.store.reconcile(discovery({endpoint(), capture}), first).result == "completed");
  CHECK(f.files().size() == 2);
}
TEST(missing_devices_remain_and_reconnect) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  const auto original = bytes(f.files()[0]);
  CHECK(f.store.reconcile(discovery({}), second).result == "completed");
  CHECK(bytes(f.files()[0]) == original);
  auto summary = read(f.portable / "devices" / "playback_devices.json");
  CHECK(summary["devices"][0]["availability"] == "not_present");
  CHECK(f.store.reconcile(discovery({endpoint()}), second).result == "completed");
  CHECK(f.files().size() == 1);
}
TEST(ambiguous_id_change_does_not_merge_or_duplicate) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  const auto original = bytes(f.files()[0]);
  const auto report = f.store.reconcile(discovery({endpoint("changed-id")}), second);
  CHECK(report.result == "partially_completed"); CHECK(report.saved == 0);
  CHECK(report.diagnostics[0].code == "identity_ambiguity");
  CHECK(f.files().size() == 1); CHECK(bytes(f.files()[0]) == original);
}
TEST(durable_fingerprint_recovers_after_cache_deletion) {
  Fixture f;
  auto e = endpoint();
  e.fingerprint = {{"usb", {{"serial_number", "serial-1"}, {"vendor_id", "1234"}, {"product_id", "5678"},
                   {"interface_number", "02"}}}, {"endpoint", {{"role", "role-a"}}}};
  f.store.reconcile(discovery({e}), first);
  const auto id = read(f.files()[0])["logical_id"];
  fs::remove(f.local / "device_bindings.json");
  e.endpoint_id = "new-id";
  CHECK(f.store.reconcile(discovery({e}), second).result == "completed");
  CHECK(f.files().size() == 1); CHECK(read(f.files()[0])["logical_id"] == id);
}
TEST(multiple_endpoints_claiming_one_record_are_rejected) {
  Fixture f;
  auto e = endpoint();
  e.fingerprint = {{"usb", {{"serial_number", "serial-1"}, {"vendor_id", "1234"}, {"product_id", "5678"},
                   {"interface_number", "02"}}}, {"endpoint", {{"role", "role-a"}}}};
  f.store.reconcile(discovery({e}), first);
  auto other = e; other.endpoint_id = "second-id";
  const auto report = f.store.reconcile(discovery({e, other}), second);
  CHECK(report.saved == 0); CHECK(report.result == "partially_completed"); CHECK(f.files().size() == 1);
}
TEST(malformed_record_blocks_updates) {
  Fixture f;
  const auto bad = f.portable / "devices" / "playback" / "Broken.json";
  fs::create_directories(bad.parent_path()); std::ofstream(bad) << "{ broken";
  const auto report = f.store.reconcile(discovery({endpoint()}), first);
  CHECK(report.result == "failed"); CHECK(report.saved == 0);
  CHECK(bytes(bad) == "{ broken"); CHECK(f.files().size() == 1);
  CHECK(!fs::exists(f.local / "device_bindings.json"));
}
TEST(duplicate_logical_ids_block_updates) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  write(f.files()[0].parent_path() / "Duplicate.json", read(f.files()[0]));
  const auto report = f.store.reconcile(discovery({endpoint()}), second);
  CHECK(report.result == "failed"); CHECK(report.saved == 0);
}
TEST(partial_record_preserves_unknown_fields) {
  Fixture f;
  const auto path = f.portable / "devices" / "playback" / "Imported.json";
  write(path, {{"device_name", "Imported"}, {"logical_id", "imported-id"},
               {"identity", {{"direction", "render"}}}, {"metadata", {{"schema_version", 2}}},
               {"custom", Json::array({1, "unchanged"})}});
  CHECK(f.store.reconcile(discovery({}), first).result == "completed");
  const auto record = read(path);
  CHECK(record["notes"] == ""); CHECK(record["custom"] == Json::array({1, "unchanged"}));
  CHECK(record["display_name"] == "Imported");
}
TEST(legacy_schema_migration_retains_attempt_semantics) {
  Fixture f;
  const auto path = f.portable / "devices" / "playback" / "Legacy.json";
  Json legacy{{"windows_name", "Old Windows name"}, {"notes", "Leave alone"},
    {"identity", {{"logical_id", "permanent-id"}, {"direction", "render"}}},
    {"scan_history", {{"first_scanned", first}, {"last_scanned", second}, {"last_scan_result", "interrupted"}}},
    {"metadata", {{"schema_version", 1}}}, {"extension", 12}};
  write(path, legacy);
  CHECK(f.store.reconcile(discovery({}), second).result == "completed");
  const auto record = read(path);
  CHECK(record["logical_id"] == "permanent-id"); CHECK(!record["identity"].contains("logical_id"));
  CHECK(record["device_name"] == "Old Windows name"); CHECK(!record.contains("windows_name"));
  CHECK(record["scan_history"]["last_attempt"] == second);
  CHECK(record["scan_history"]["last_scanned"] == status("unknown"));
  CHECK(record["notes"] == "Leave alone"); CHECK(record["extension"] == 12);
  CHECK(read(fs::path(path.string() + ".bak")) == legacy);
}
TEST(conflicting_legacy_fields_are_not_guessed) {
  Fixture f;
  const auto path = f.portable / "devices" / "playback" / "Legacy.json";
  write(path, {{"windows_name", "Old"}, {"device_name", "Conflicting"},
    {"identity", {{"logical_id", "id"}, {"direction", "render"}}}, {"metadata", {{"schema_version", 1}}}});
  const auto original = bytes(path);
  CHECK(f.store.reconcile(discovery({}), first).result == "failed");
  CHECK(bytes(path) == original);
}
TEST(future_schema_is_never_rewritten) {
  Fixture f;
  const auto path = f.portable / "devices" / "playback" / "Future.json";
  write(path, {{"metadata", {{"schema_version", 99}}}, {"new_data", true}});
  const auto original = bytes(path);
  CHECK(f.store.reconcile(discovery({endpoint()}), first).result == "failed");
  CHECK(bytes(path) == original);
}
TEST(incomplete_discovery_does_not_mark_missing) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  const auto cache = bytes(f.local / "device_bindings.json");
  Discovery failed; failed.diagnostics.push_back({"enumeration_failure", "failure", {}, {}});
  CHECK(f.store.reconcile(failed, second).result == "failed");
  CHECK(bytes(f.local / "device_bindings.json") == cache);
}
TEST(driver_change_retains_evidence_but_marks_stale) {
  Fixture f;
  auto e = endpoint();
  f.store.reconcile(discovery({e}), first);
  auto record = read(f.files()[0]);
  record["probed_capabilities"]["verified_configurations"] = Json::array({{{"result", "verified"}}});
  write(f.files()[0], record);
  e.driver["version"] = "2.0";
  CHECK(f.store.reconcile(discovery({e}), second).result == "completed");
  const auto current = read(f.files()[0]);
  CHECK(current["probed_capabilities"]["verified_configurations"] == record["probed_capabilities"]["verified_configurations"]);
  CHECK(current["probed_capabilities"]["evidence_currency"] == "requires_revalidation");
  CHECK(current["scan_history"]["driver_history"].size() == 1);
}
TEST(unsupported_format_query_does_not_claim_runtime_verification) {
  Fixture f;
  auto e = endpoint();
  e.reported_capabilities["format_queries"] = Json::array({{{"mode", "exclusive"},
    {"support", "unsupported"}, {"hresult", "0x88890008"}, {"stream_verification", "not_tested"}}});
  f.store.reconcile(discovery({e}), first);
  const auto record = read(f.files()[0]);
  CHECK(record["reported_capabilities"]["format_queries"] == e.reported_capabilities["format_queries"]);
  CHECK(record["probed_capabilities"]["scan_status"] == "not_tested");
}
TEST(read_only_record_is_not_replaced) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  const auto path = f.files()[0]; const auto original = bytes(path);
  fs::permissions(path, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);
  CHECK(f.store.reconcile(discovery({endpoint()}), second).result == "failed");
  CHECK(bytes(path) == original);
}
class FaultWriter final : public AtomicWriter {
 public:
  std::shared_ptr<AtomicWriter> native = make_atomic_writer();
  std::string fail_name;
  void write(const fs::path& path, const std::string& value, const std::optional<std::string>& expected) override {
    if (path_utf8(path.filename()) == fail_name) throw std::runtime_error("Injected I/O failure");
    native->write(path, value, expected);
  }
};
TEST(binding_write_failure_prevents_new_records) {
  Fixture f;
  auto writer = std::make_shared<FaultWriter>(); writer->fail_name = "device_bindings.json";
  DeviceStore store(f.portable, f.local, writer);
  CHECK(store.reconcile(discovery({endpoint()}), first).result == "failed");
  CHECK(f.files().empty());
}
TEST(record_write_failure_does_not_destroy_previous_record) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  auto writer = std::make_shared<FaultWriter>(); writer->fail_name = path_utf8(f.files()[0].filename());
  DeviceStore store(f.portable, f.local, writer);
  const auto original = bytes(f.files()[0]);
  CHECK(store.reconcile(discovery({endpoint()}), second).result == "failed");
  CHECK(bytes(f.files()[0]) == original);
}
TEST(atomic_writer_detects_external_changes) {
  Fixture f; const auto path = f.portable / "test.json";
  auto writer = make_atomic_writer();
  writer->write(path, "{\"value\":1}\n", std::nullopt);
  const auto original = bytes(path);
  write(path, {{"externally_changed", true}});
  const auto external = bytes(path);
  bool failed = false;
  try { writer->write(path, "{\"value\":2}\n", original); } catch (...) { failed = true; }
  CHECK(failed); CHECK(bytes(path) == external);
}
TEST(atomic_writer_does_not_replace_with_invalid_json) {
  Fixture f; const auto path = f.portable / "test.json";
  auto writer = make_atomic_writer(); writer->write(path, "{}\n", std::nullopt);
  bool failed = false;
  try { writer->write(path, "broken", "{}\n"); } catch (...) { failed = true; }
  CHECK(failed); CHECK(bytes(path) == "{}\n");
}
class FakeProvider final : public EndpointProvider {
 public:
  bool called = false;
  std::vector<std::string> capability_queries;
  Discovery answer = discovery({endpoint()});
  Discovery discover(std::stop_token, const ProgressSink& progress, const EndpointFilter& filter = {}) override {
    called = true; if (progress) progress({"enumerating", {}, 1, 1});
    auto result = answer; result.endpoints.clear();
    result.full_inventory = !filter;
    for (const auto& e : answer.endpoints) {
      auto identity = e; identity.reported_capabilities = Json::object();
      if (filter && !filter(identity)) continue;
      capability_queries.push_back(e.endpoint_id);
      result.endpoints.push_back(e);
    }
    return result;
  }
};
TEST(cancellation_does_not_persist_partial_inventory) {
  Fixture f; FakeProvider provider; BasicScanner scanner(provider, f.store);
  std::stop_source stop; stop.request_stop();
  CHECK(scanner.scan(stop.get_token()).result == "interrupted"); CHECK(!provider.called);
  stop = std::stop_source();
  const auto report = scanner.scan(stop.get_token(), [&](const Progress&) { stop.request_stop(); });
  CHECK(report.result == "interrupted"); CHECK(f.files().empty());
}
TEST(filename_sanitization_preserves_readable_names) {
  CHECK(safe_filename("Speakers (Realtek USB Audio)") == "Speakers (Realtek USB Audio)");
  CHECK(safe_filename("CON") == "_CON"); CHECK(safe_filename("com1.txt") == "_com1.txt");
  CHECK(safe_filename("A/B:C*? . ") == "A_B_C__"); CHECK(safe_filename("...") == "Audio Device");
  Fixture f;
  CHECK(f.store.reconcile(discovery({endpoint("one", "A/B"), endpoint("two", "A:B")}), first).result == "completed");
  CHECK(f.files().size() == 2);
}
TEST(unicode_names_and_notes_round_trip) {
  Fixture f;
  auto e = endpoint("one", "R\xc3\x98\x44\x45 Speakers");
  CHECK(f.store.reconcile(discovery({e}), first).result == "completed");
  CHECK(path_utf8(f.files()[0].stem()) == e.device_name);
  CHECK(read(f.files()[0])["device_name"] == e.device_name);
}
TEST(duplicate_json_keys_block_updates) {
  Fixture f;
  const auto path = f.portable / "devices" / "playback" / "Bad.json";
  fs::create_directories(path.parent_path());
  std::ofstream(path) << "{\"logical_id\":\"one\",\"logical_id\":\"two\"}";
  const auto original = bytes(path);
  CHECK(f.store.reconcile(discovery({endpoint()}), first).result == "failed");
  CHECK(bytes(path) == original);
}
TEST(invalid_imported_timestamp_is_not_silently_replaced) {
  Fixture f;
  f.store.reconcile(discovery({endpoint()}), first);
  auto data = read(f.files()[0]); data["scan_history"]["last_scanned"] = "2026-02-30T25:00:00Z";
  write(f.files()[0], data); const auto original = bytes(f.files()[0]);
  CHECK(f.store.reconcile(discovery({endpoint()}), second).result == "failed");
  CHECK(bytes(f.files()[0]) == original);
}
TEST(nested_extension_survives_machine_updates) {
  Fixture f; auto e = endpoint();
  e.reported_capabilities["default_mix_format"] = {{"sample_rate_hz", 48000}};
  f.store.reconcile(discovery({e}), first);
  auto data = read(f.files()[0]);
  data["reported_capabilities"]["default_mix_format"]["user_tag"] = "keep";
  data["identity"]["fingerprint"]["user_tag"] = "keep";
  write(f.files()[0], data);
  CHECK(f.store.reconcile(discovery({e}), second).result == "completed");
  const auto current = read(f.files()[0]);
  CHECK(current["reported_capabilities"]["default_mix_format"]["user_tag"] == "keep");
  CHECK(!current["scan_history"].contains("fingerprint_history"));
  CHECK(!current["scan_history"].contains("driver_history"));
}
TEST(invalid_local_cache_does_not_create_duplicate_records) {
  Fixture f; f.store.reconcile(discovery({endpoint()}), first);
  const auto original = bytes(f.files()[0]);
  write(f.local / "device_bindings.json", {{"metadata", {{"schema_version", 99}}}});
  CHECK(f.store.reconcile(discovery({endpoint()}), second).result == "failed");
  CHECK(f.files().size() == 1); CHECK(bytes(f.files()[0]) == original);
}
TEST(partial_discovery_retains_previous_completed_timestamp) {
  Fixture f; f.store.reconcile(discovery({endpoint()}), first);
  auto partial = discovery({endpoint()}); partial.complete = false;
  CHECK(f.store.reconcile(partial, second).result == "partially_completed");
  const auto current = read(f.files()[0]);
  CHECK(current["scan_history"]["last_scanned"] == first);
  CHECK(current["scan_history"]["last_attempt"] == second);
}
TEST(corrupt_summary_regenerates_without_losing_records) {
  Fixture f; f.store.reconcile(discovery({endpoint()}), first);
  const auto summary = f.portable / "devices" / "playback_devices.json";
  std::ofstream(summary) << "corrupted generated view";
  CHECK(f.store.reconcile(discovery({endpoint()}), second).result == "completed");
  CHECK(read(summary)["devices"].size() == 1);
  CHECK(bytes(fs::path(summary.string() + ".bak")) == "corrupted generated view");
  CHECK(f.files().size() == 1);
}
TEST(utility_defaults_anchor_to_executable_directory) {
  const auto options = utility_options({}, fs::path("example") / "scanner");
  CHECK(options.results_root == fs::path("example") / "scanner" / "AudioHole-Scan");
  CHECK(options.portable_root == options.results_root / "portable");
  CHECK(options.local_root == options.results_root / "machine-local");
  const auto explicit_options = utility_options({"--results", "custom", "--output", "records"}, "ignored");
  CHECK(explicit_options.portable_root == "records");
  CHECK(explicit_options.local_root == fs::path("custom") / "machine-local");
}
TEST(historical_windows_instances_do_not_create_duplicate_records) {
  Fixture f;
  auto current = endpoint("current", "Same Device");
  auto old = endpoint("historical", "Same Device"); old.state = "not_present";
  const auto report = f.store.reconcile(discovery({old, current}), first);
  CHECK(report.result == "completed"); CHECK(report.discovered == 2);
  CHECK(report.saved == 1); CHECK(report.historical_skipped == 1); CHECK(f.files().size() == 1);
  const auto id = read(f.files()[0])["logical_id"];
  const auto repeat = f.store.reconcile(discovery({current, old}), second);
  CHECK(repeat.result == "completed"); CHECK(repeat.historical_skipped == 1);
  CHECK(f.files().size() == 1); CHECK(read(f.files()[0])["logical_id"] == id);
}
TEST(known_missing_device_is_preserved_and_disabled_unplugged_are_saved) {
  Fixture f;
  auto old = endpoint();
  f.store.reconcile(discovery({old}), first);
  const auto id = read(f.files()[0])["logical_id"];
  old.state = "not_present";
  auto disabled = endpoint("disabled", "Disabled Device"); disabled.state = "disabled";
  auto unplugged = endpoint("unplugged", "Unplugged Device"); unplugged.state = "unplugged";
  const auto report = f.store.reconcile(discovery({old, disabled, unplugged}), second);
  CHECK(report.result == "completed"); CHECK(report.historical_skipped == 0); CHECK(f.files().size() == 3);
  bool retained = false;
  for (const auto& path : f.files()) retained = retained || read(path)["logical_id"] == id;
  CHECK(retained);
}
TEST(utility_rejects_invalid_arguments_before_scanning) {
  for (const std::vector<fs::path> args : {std::vector<fs::path>{"--unknown"},
       {"--output"}, {"--results", "--debug-log", "file.txt"}, {"--output", "a", "--output", "b"}}) {
    bool rejected = false;
    try { (void)utility_options(args, "unused"); } catch (...) { rejected = true; }
    CHECK(rejected);
  }
  CHECK(utility_options({"--help"}, "unused").help);
}
TEST(utility_outputs_json_and_detailed_debug_text) {
  Fixture f;
  auto options = utility_options({"--results", f.root / "results"}, f.root);
  auto factory = [] {
    auto provider = std::make_unique<FakeProvider>();
    provider->answer.endpoints[0].reported_capabilities["format_queries"] = Json::array({
      {{"hresult", "0x88890008"}, {"mode", "exclusive"}, {"stream_verification", "not_tested"}}});
    return provider;
  };
  const auto first_run = run_scan_utility(options, factory);
  CHECK(first_run.report.result == "completed");
  CHECK(read(first_run.report_json)["result"] == "completed");
  CHECK(fs::exists(options.portable_root / "devices" / "playback" / "Speakers.json"));
  const auto log = bytes(first_run.debug_log);
  CHECK(log.find("endpoint-one") != std::string::npos);
  CHECK(log.find("0x88890008") != std::string::npos);
  CHECK(log.find("reported data; no streaming verification") != std::string::npos);
  CHECK(log.find("Final scan result") != std::string::npos);
  const auto second_run = run_scan_utility(options, factory);
  CHECK(second_run.report.result == "completed");
  CHECK(second_run.debug_log != first_run.debug_log);
  CHECK(second_run.report_json != first_run.report_json);
  CHECK(bytes(first_run.debug_log) == log);
}
TEST(utility_logs_provider_creation_failure) {
  Fixture f;
  const auto options = utility_options({"--results", f.root / "results"}, f.root);
  const auto result = run_scan_utility(options, []() -> std::unique_ptr<EndpointProvider> {
    throw std::runtime_error("Simulated unavailable Windows API");
  });
  CHECK(result.report.result == "failed");
  CHECK(bytes(result.debug_log).find("Simulated unavailable Windows API") != std::string::npos);
  CHECK(read(result.report_json)["result"] == "failed");
  CHECK(!fs::exists(options.portable_root / "devices"));
}
TEST(utility_cancellation_still_writes_debug_and_run_report) {
  Fixture f;
  const auto options = utility_options({"--results", f.root / "results"}, f.root);
  std::stop_source stop;
  const auto result = run_scan_utility(options, [] { return std::make_unique<FakeProvider>(); },
    stop.get_token(), [&](const Progress&) { stop.request_stop(); });
  CHECK(result.report.result == "interrupted");
  CHECK(read(result.report_json)["result"] == "interrupted");
  CHECK(bytes(result.debug_log).find("interrupted") != std::string::npos);
  CHECK(!fs::exists(options.portable_root / "devices"));
}
TEST(utility_explicit_debug_text_is_append_only) {
  Fixture f;
  fs::create_directories(f.root);
  for (const auto* ending : {"\n", "\r\n"}) {
    const auto path = f.root / (std::string(ending) == "\n" ? "debug-lf.txt" : "debug-crlf.txt");
    const std::string original = std::string("Previous user debug text") + ending;
    // Binary fixture I/O makes the expected bytes explicit on every platform.
    // The logger must append without normalizing either newline convention.
    std::ofstream(path, std::ios::binary) << original;
    const auto options = utility_options({"--results", f.root / "results", "--debug-log", path}, f.root);
    const auto result = run_scan_utility(options, [] { return std::make_unique<FakeProvider>(); });
    CHECK(result.report.result == "completed");
    const auto appended = bytes(path);
    CHECK(appended.starts_with(original)); CHECK(appended.size() > original.size());
  }
}
TEST(utility_debug_failure_does_not_scan_or_modify_json) {
  Fixture f;
  const auto path = f.root / "user.json";
  write(path, {{"notes", "do not modify"}});
  const auto original = bytes(path);
  const auto options = utility_options({"--results", f.root / "results", "--debug-log", path}, f.root);
  bool called = false;
  const auto result = run_scan_utility(options, [&] { called = true; return std::make_unique<FakeProvider>(); });
  CHECK(result.report.result == "failed"); CHECK(!called);
  CHECK(bytes(path) == original); CHECK(read(result.report_json)["result"] == "failed");
}
TEST(utility_report_export_failure_is_visible_in_debug_text) {
  Fixture f;
  const auto options = utility_options({"--results", f.root / "results"}, f.root);
  fs::create_directories(options.results_root);
  std::ofstream(options.results_root / "reports") << "Block the reports directory";
  const auto result = run_scan_utility(options, [] { return std::make_unique<FakeProvider>(); });
  CHECK(result.report.result == "partially_completed");
  CHECK(result.report.diagnostics.back().code == "report_write_failed");
  CHECK(bytes(result.debug_log).find("Structured report export failed") != std::string::npos);
}
Json probe_format() {
  return {{"sample_rate_hz", 48000}, {"channel_count", 2}, {"container_bits", 32}, {"valid_bits", 32},
    {"channel_mask", 3}, {"sample_format", "float32"}, {"format_tag", 65534},
    {"subformat_guid", "{00000003-0000-0010-8000-00AA00389B71}"}, {"block_align_bytes", 8}, {"average_bytes_per_second", 384000}};
}
Endpoint probe_endpoint() {
  auto e = endpoint(); e.reported_capabilities["default_mix_format"] = probe_format();
  e.reported_capabilities["configured_device_format"] = probe_format(); return e;
}
class FakeProbeRunner final : public ProbeRunner {
 public:
  unsigned calls = 0;
  std::vector<std::string> targets;
  bool throw_error = false;
  Json answer{{"classification", "verified"}, {"cleanup", "completed"}, {"verification_scope", "buffer_processing"},
    {"runtime_duration_ms", 2000}, {"serviced_frames", 96000}, {"audio_events", 200}, {"signal_verification", "not_tested"}};
  std::function<void()> on_stage;
  Json execute(const ProbeJob& job, std::stop_token, const ProbeStageSink& stage) override {
    ++calls;
    targets.push_back(job.target.logical_id);
    stage({{"api", "Initialize"}, {"phase", "end"}, {"hresult", "0x00000000"}});
    if (on_stage) on_stage();
    if (throw_error) throw std::runtime_error("Simulated worker crash");
    return answer;
  }
};
TEST(deep_planner_is_seeded_bounded_and_deduplicated) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  const auto target = f.store.probe_targets().at(0);
  const auto plan = plan_deep_candidates(target, {});
  CHECK(plan.size() == 8); CHECK(plan.front().provenance == "windows_configured_device_format");
  std::set<std::string> keys;
  std::set<unsigned> rates;
  for (const auto& c : plan) { validate_probe_format(c.format); keys.insert(c.mode + c.format.dump()); rates.insert(c.format["sample_rate_hz"].get<unsigned>()); }
  CHECK(keys.size() == plan.size()); CHECK(rates.size() > 2);
}
TEST(deep_limits_and_descriptors_reject_invalid_work) {
  bool failed = false;
  try { validate_deep_limits({65, 2000, 10000, 120000, "both"}); } catch (...) { failed = true; } CHECK(failed);
  auto format = probe_format(); format["block_align_bytes"] = 1;
  failed = false; try { validate_probe_format(format); } catch (...) { failed = true; } CHECK(failed);
  format = probe_format(); format["channel_mask"] = 1;
  failed = false; try { validate_probe_format(format); } catch (...) { failed = true; } CHECK(failed);
  format = probe_format(); format["sample_format"] = "pcm_s16";
  failed = false; try { validate_probe_format(format); } catch (...) { failed = true; } CHECK(failed);
}
TEST(deep_private_protocol_requires_consent_and_exact_identity) {
  ProbeJob job{"probe-valid", {"logical", "endpoint", "Device", "render", Json::object()}, {probe_format(), "shared", "test"}, 2000, 10000};
  const auto original = probe_job_json(job);
  CHECK(parse_probe_job(original).target.endpoint_id == "endpoint");
  for (const auto& bad : std::vector<Json>{Json{{"consent", false}}, Json{{"duration_ms", -4294965296LL}},
       Json{{"direction", "both"}}, Json{{"attempt_id", "../outside"}}}) {
    auto input = original;
    for (auto it = bad.begin(); it != bad.end(); ++it) input[it.key()] = it.value();
    bool failed = false; try { (void)parse_probe_job(input); } catch (...) { failed = true; } CHECK(failed);
  }
}
TEST(deep_checkpoints_preserve_notes_extensions_and_unverified_signal) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  auto record = read(f.files()[0]); record["notes"] = "Keep these notes"; record["custom"] = 42; write(f.files()[0], record);
  const auto target = f.store.probe_targets().at(0);
  FakeProbeRunner runner; DeepLimits limits; limits.max_attempts = 1;
  const auto report = run_deep_scan(f.store, runner, target, limits, true);
  CHECK(report.result == "completed"); CHECK(report.verified == 1); CHECK(runner.calls == 1);
  const auto updated = read(f.files()[0]);
  CHECK(updated["notes"] == "Keep these notes"); CHECK(updated["custom"] == 42);
  CHECK(updated["logical_id"] == target.logical_id);
  const auto& attempt = updated["probed_capabilities"]["test_attempts"][0];
  CHECK(attempt["signal_verification"] == "not_tested"); CHECK(!attempt.contains("endpoint_id"));
  CHECK(attempt["stages"].size() == 1);
  const auto path = f.portable / "probe_history" / (attempt["attempt_id"].get<std::string>() + ".json");
  CHECK(read(path)["classification"] == "verified");
  CHECK(read(f.portable / "devices" / "playback_devices.json")["devices"][0]["probed_capabilities"] == updated["probed_capabilities"]);
}
TEST(deep_denied_consent_never_runs_worker) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  FakeProbeRunner runner;
  const auto report = run_deep_scan(f.store, runner, f.store.probe_targets().at(0), {}, false);
  CHECK(report.result == "failed"); CHECK(runner.calls == 0); CHECK(!fs::exists(f.portable / "probe_history"));
}
TEST(deep_unknown_cleanup_quarantines_endpoint) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  FakeProbeRunner runner; runner.answer = {{"classification", "inconclusive"}, {"cleanup", "unknown"}};
  const auto report = run_deep_scan(f.store, runner, f.store.probe_targets().at(0), {}, true);
  CHECK(report.result == "partially_completed"); CHECK(runner.calls == 1); CHECK(report.inconclusive == 1);
  CHECK(report.diagnostics.back().code == "cleanup_uncertain");
}
TEST(deep_crash_persists_inconclusive_attempt_and_stops) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  FakeProbeRunner runner; runner.throw_error = true;
  const auto report = run_deep_scan(f.store, runner, f.store.probe_targets().at(0), {}, true);
  CHECK(runner.calls == 1); CHECK(report.inconclusive == 1);
  const auto attempt = read(f.files()[0])["probed_capabilities"]["test_attempts"][0];
  CHECK(attempt["status"] == "completed"); CHECK(attempt["classification"] == "inconclusive");
  CHECK(attempt["stages"].size() == 1); CHECK(attempt["reason"] == "Simulated worker crash");
}
TEST(deep_rejects_verification_without_runtime_evidence) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  FakeProbeRunner runner; runner.answer.erase("serviced_frames");
  const auto report = run_deep_scan(f.store, runner, f.store.probe_targets().at(0), {}, true);
  CHECK(report.verified == 0); CHECK(report.inconclusive == 1); CHECK(runner.calls == 1);
}
TEST(deep_cancellation_preserves_completed_stage) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  std::stop_source stop;
  FakeProbeRunner runner; runner.answer = {{"classification", "inconclusive"}, {"cleanup", "completed"}};
  runner.on_stage = [&] { stop.request_stop(); };
  const auto report = run_deep_scan(f.store, runner, f.store.probe_targets().at(0), {}, true, stop.get_token());
  CHECK(report.result == "interrupted"); CHECK(runner.calls == 1);
  const auto attempt = read(f.files()[0])["probed_capabilities"]["test_attempts"][0];
  CHECK(attempt["status"] == "interrupted"); CHECK(attempt["stages"].size() == 1);
}
TEST(deep_cli_requires_target_for_noninteractive_consent) {
  bool failed = false; try { (void)utility_options({"--deep", "--confirm-disruption"}, "unused"); } catch (...) { failed = true; } CHECK(failed);
  failed = false; try { (void)utility_options({"--device", "id"}, "unused"); } catch (...) { failed = true; } CHECK(failed);
  const auto options = utility_options({"--deep", "--device", "id", "--confirm-disruption", "--mode", "exclusive"}, "unused");
  CHECK(options.deep && options.confirm_disruption && options.deep_limits.mode == "exclusive");
}
TEST(deep_utility_declined_consent_does_not_create_runner) {
  Fixture f; auto options = utility_options({"--deep", "--results", f.root / "results"}, f.root);
  DeepInteraction interaction;
  interaction.select_target = [](const auto& targets) { return targets.at(0).logical_id; };
  interaction.confirm = [](const auto&, const auto&) { return false; };
  bool called = false;
  const auto result = run_scan_utility(options, [] { auto p = std::make_unique<FakeProvider>(); p->answer = discovery({probe_endpoint()}); return p; }, {}, {},
    [&] { called = true; return std::make_unique<FakeProbeRunner>(); }, interaction);
  CHECK(result.report.result == "interrupted"); CHECK(!called);
  CHECK(read(result.report_json)["deep_scan"]["result"] == "interrupted");
}
TEST(deep_expected_format_rejections_are_completed_scan_evidence) {
  Fixture f; f.store.reconcile(discovery({probe_endpoint()}), first);
  FakeProbeRunner runner; runner.answer = {{"classification", "failed"}, {"cleanup", "completed"}, {"support", "unsupported"}};
  DeepLimits limits; limits.max_attempts = 2;
  const auto report = run_deep_scan(f.store, runner, f.store.probe_targets().at(0), limits, true);
  CHECK(report.result == "completed"); CHECK(report.planned == 2); CHECK(report.failed == 2); CHECK(runner.calls == 2);
  CHECK(read(f.files()[0])["probed_capabilities"]["failed_configurations"].size() == 2);
}
TEST(deep_utility_persists_result_in_debug_and_json) {
  Fixture f; auto options = utility_options({"--deep", "--results", f.root / "results", "--max-attempts", "1"}, f.root);
  DeepInteraction interaction;
  interaction.select_target = [](const auto& targets) { return targets.at(0).logical_id; };
  interaction.confirm = [](const auto&, const auto&) { return true; };
  const auto result = run_scan_utility(options, [] { auto p = std::make_unique<FakeProvider>(); p->answer = discovery({probe_endpoint()}); return p; }, {}, {},
    [] { return std::make_unique<FakeProbeRunner>(); }, interaction);
  CHECK(result.report.result == "completed"); CHECK(result.deep_report["verified"] == 1);
  CHECK(read(result.report_json)["deep_scan"]["verified"] == 1);
  CHECK(bytes(result.debug_log).find("Deep Scan target and consent") != std::string::npos);
  CHECK(bytes(result.debug_log).find("Deep Scan finished") != std::string::npos);
  const auto& attempt = result.deep_report["attempts"][0];
  CHECK(nlohmann::json(attempt["format"]) == nlohmann::json(probe_format())); CHECK(attempt["cleanup"] == "completed");
  CHECK(attempt["runtime_duration_ms"] == 2000); CHECK(attempt["stage_count"] == 1);
  CHECK(!attempt.contains("endpoint_id"));
  CHECK(read(result.report_json)["scan_kind"] == "deep");
  CHECK(read(result.report_json)["basic_scan"]["result"] == "completed");
}
Discovery batch_discovery() {
  auto capture = probe_endpoint(); capture.endpoint_id = "batch-capture"; capture.direction = "capture";
  capture.device_name = "Batch microphone";
  auto render = probe_endpoint(); render.endpoint_id = "batch-render"; render.device_name = "Batch speakers";
  auto disabled = endpoint("batch-disabled", "Disabled"); disabled.state = "disabled";
  auto historical = endpoint("batch-history", "Old speakers"); historical.state = "not_present";
  return discovery({capture, render, disabled, historical});
}
TEST(deep_batch_options_require_explicit_consistent_scope) {
  const auto options = utility_options({"--deep-all", "--confirm-disruption", "--system-timeout-ms", "7200000"}, "unused");
  CHECK(options.deep && options.deep_all && options.confirm_disruption); CHECK(options.system_timeout_ms == 7200000);
  CHECK(utility_options({"--deep-all", "--system-timeout-ms", "0"}, "unused").system_timeout_ms == 0);
  for (const auto& args : std::vector<std::vector<fs::path>>{
      {"--deep-all", "--device", "id"}, {"--deep", "--system-timeout-ms", "2000"},
      {"--deep-all", "--system-timeout-ms", "999"}, {"--deep-all", "--system-timeout-ms", "7200001"}}) {
    bool failed = false; try { (void)utility_options(args, "unused"); } catch (...) { failed = true; } CHECK(failed);
  }
}
TEST(deep_batch_tests_all_active_directions_and_aggregates_format_rejections) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  const auto targets = f.store.probe_targets(); CHECK(targets.size() == 2);
  FakeProbeRunner runner; runner.answer = {{"classification", "failed"}, {"cleanup", "completed"},
    {"failed_api", "IsFormatSupported"}, {"hresult", "0x88890008"}};
  DeepLimits limits; limits.max_attempts = 2;
  std::vector<Json> snapshots;
  const auto result = run_deep_scan_batch(f.store, runner, targets, limits, 10000, true, {}, {},
    [&](const auto& report) { snapshots.push_back(Json(report)); });
  const Json report = result;
  CHECK(report["result"] == "completed"); CHECK(report["devices_selected"] == 2);
  CHECK(report["devices_completed"] == 2); CHECK(report["attempted"] == 4); CHECK(report["failed"] == 4);
  CHECK(runner.targets == std::vector<std::string>({targets[0].logical_id, targets[0].logical_id, targets[1].logical_id, targets[1].logical_id}));
  CHECK(report["devices"][0]["direction"] == "capture"); CHECK(report["devices"][1]["direction"] == "render");
  CHECK(report["devices"][0]["attempts"][0]["hresult"] == "0x88890008");
  CHECK(snapshots.front()["devices_not_tested"] == 2); CHECK(snapshots.back()["devices_completed"] == 2);
}
TEST(deep_batch_uncertain_cleanup_stops_other_devices) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  FakeProbeRunner runner; runner.answer = {{"classification", "inconclusive"}, {"cleanup", "unknown"}};
  const Json report = run_deep_scan_batch(f.store, runner, f.store.probe_targets(), {}, 10000, true);
  CHECK(runner.calls == 1); CHECK(report["result"] == "partially_completed");
  CHECK(report["devices_not_tested"] == 1); CHECK(report["devices"][1]["result"] == "not_tested");
  CHECK(report["diagnostics"][0]["code"] == "batch_stopped_after_device_error");
}
TEST(deep_batch_endpoint_without_pcm_candidates_does_not_block_other_devices) {
  Fixture f; auto inventory = batch_discovery();
  inventory.endpoints[0].reported_capabilities = Json::object();
  f.store.reconcile(inventory, first);
  FakeProbeRunner runner; DeepLimits limits; limits.max_attempts = 1;
  const Json report = run_deep_scan_batch(f.store, runner, f.store.probe_targets(), limits, 10000, true);
  CHECK(runner.calls == 1); CHECK(report["result"] == "partially_completed"); CHECK(report["devices_processed"] == 2);
  CHECK(report["devices"][0]["diagnostics"][0]["code"] == "no_probe_candidates");
  CHECK(report["devices"][1]["result"] == "completed"); CHECK(report["verified"] == 1);
}
TEST(deep_batch_cancellation_keeps_remaining_devices_untested) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  std::stop_source stop;
  FakeProbeRunner runner; runner.answer = {{"classification", "inconclusive"}, {"cleanup", "completed"}};
  runner.on_stage = [&] { stop.request_stop(); };
  const Json report = run_deep_scan_batch(f.store, runner, f.store.probe_targets(), {}, 10000, true, stop.get_token());
  CHECK(runner.calls == 1); CHECK(report["result"] == "interrupted"); CHECK(report["devices_not_tested"] == 1);
  CHECK(report["devices"][0]["attempts"][0]["status"] == "interrupted");
}
TEST(deep_batch_system_deadline_prevents_launch_without_processing_budget) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  FakeProbeRunner runner;
  const Json report = run_deep_scan_batch(f.store, runner, f.store.probe_targets(), {}, 1000, true);
  CHECK(runner.calls == 0); CHECK(report["result"] == "partially_completed"); CHECK(report["devices_not_tested"] == 2);
  CHECK(report["diagnostics"][0]["code"] == "system_budget_exhausted");
}
TEST(deep_batch_denied_consent_and_duplicate_targets_launch_nothing) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  FakeProbeRunner runner;
  auto targets = f.store.probe_targets();
  CHECK(run_deep_scan_batch(f.store, runner, targets, {}, 10000, false).result == "failed");
  targets.push_back(targets[0]);
  CHECK(run_deep_scan_batch(f.store, runner, targets, {}, 10000, true).result == "failed"); CHECK(runner.calls == 0);
}
TEST(deep_batch_checkpoint_error_preserves_first_device_and_stops_scheduling) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  FakeProbeRunner runner; DeepLimits limits; limits.max_attempts = 1;
  const Json report = run_deep_scan_batch(f.store, runner, f.store.probe_targets(), limits, 10000, true, {}, {},
    [](const auto& state) { if (Json(state)["devices_processed"] == 1) throw std::runtime_error("Checkpoint write failed"); });
  CHECK(runner.calls == 1); CHECK(report["result"] == "failed"); CHECK(report["verified"] == 1);
  CHECK(report["devices_not_tested"] == 1); CHECK(report["devices"][0]["result"] == "completed");
}
TEST(deep_batch_utility_declines_once_without_selecting_or_launching) {
  Fixture f; const auto options = utility_options({"--deep-all", "--results", f.root / "results"}, f.root);
  DeepInteraction interaction;
  interaction.select_target = [](const auto&) -> std::string { throw std::runtime_error("Must not select one endpoint"); };
  unsigned confirmations = 0; bool launched = false;
  interaction.confirm_all = [&](const auto& targets, const auto&, unsigned) { ++confirmations; CHECK(targets.size() == 2); return false; };
  const auto result = run_scan_utility(options, [] { auto p = std::make_unique<FakeProvider>(); p->answer = batch_discovery(); return p; }, {}, {},
    [&] { launched = true; return std::make_unique<FakeProbeRunner>(); }, interaction);
  CHECK(confirmations == 1); CHECK(!launched); CHECK(result.report.result == "interrupted");
}
TEST(deep_batch_utility_checkpoints_report_between_devices_and_preserves_basic_partial_result) {
  Fixture f; const auto options = utility_options({"--deep-all", "--confirm-disruption", "--max-attempts", "1", "--results", f.root / "results"}, f.root);
  unsigned stages = 0;
  const auto result = run_scan_utility(options, [] {
    auto p = std::make_unique<FakeProvider>(); p->answer = batch_discovery(); p->answer.complete = false;
    p->answer.diagnostics.push_back({"endpoint_query_failed", "Fixture unreadable name", {}, {}}); return p;
  }, {}, {}, [&] {
    auto p = std::make_unique<FakeProbeRunner>();
    p->on_stage = [&] {
      if (++stages != 2) return;
      fs::path path;
      for (const auto& entry : fs::directory_iterator(options.results_root / "reports"))
        if (entry.path().extension() == ".json") path = entry.path();
      CHECK(!path.empty());
      const auto snapshot = read(path);
      CHECK(snapshot["result"] == "running"); CHECK(snapshot["basic_scan"]["result"] == "partially_completed");
      CHECK(snapshot["deep_scan"]["devices"][0]["result"] == "completed");
      CHECK(snapshot["deep_scan"]["devices"][1]["result"] == "running");
    };
    return p;
  });
  CHECK(stages == 2); CHECK(result.report.result == "partially_completed"); CHECK(result.deep_report["result"] == "completed");
  CHECK(result.deep_report["devices_completed"] == 2); CHECK(result.deep_report["verified"] == 2);
  const auto report = read(result.report_json); CHECK(report["scan_kind"] == "deep-all");
  CHECK(report["deep_scan"]["devices"][1]["attempts"][0]["signal_verification"] == "not_tested");
  CHECK(result.report_json.filename().string().find("-deep-all-") != std::string::npos);
}
TEST(scoped_update_queries_only_new_devices_and_repeated_update_queries_none) {
  Fixture f; f.store.reconcile(discovery({endpoint("known", "Known speakers")}), first);
  auto data = read(f.files()[0]); data["notes"] = "Preserve\n\xc3\xa9 spaces  here"; write(f.files()[0], data);
  const auto original = bytes(f.files()[0]);
  FakeProvider provider; provider.answer = discovery({endpoint("known", "Known speakers"), endpoint("new", "New mic")});
  auto report = run_scoped_basic(provider, f.store, "update");
  CHECK(report.scan.result == "completed"); CHECK(report.known_skipped == 1); CHECK(report.selected_endpoint_ids == std::vector<std::string>{"new"});
  CHECK(provider.capability_queries == std::vector<std::string>{"new"}); CHECK(bytes(f.files()[0]) == original || bytes(f.files()[1]) == original);
  provider.capability_queries.clear();
  report = run_scoped_basic(provider, f.store, "update");
  CHECK(report.scan.result == "completed"); CHECK(report.selected_endpoint_ids.empty()); CHECK(provider.capability_queries.empty()); CHECK(f.files().size() == 2);
}
TEST(scoped_selected_device_does_not_query_or_mark_other_endpoints_missing) {
  Fixture f; f.store.reconcile(batch_discovery(), first);
  const auto targets = f.store.probe_targets();
  FakeProvider provider; provider.answer = batch_discovery();
  const auto report = run_scoped_basic(provider, f.store, "device", targets[0].logical_id);
  CHECK(report.scan.result == "completed"); CHECK(provider.capability_queries == std::vector<std::string>{targets[0].endpoint_id});
  CHECK(f.store.probe_targets().size() == 2);
  provider.capability_queries.clear();
  CHECK(run_scoped_basic(provider, f.store, "device", "missing-id").scan.result == "failed");
  CHECK(provider.capability_queries.empty());
}
TEST(scoped_update_does_not_treat_ambiguous_or_returning_devices_as_new) {
  Fixture f; f.store.reconcile(discovery({endpoint("old", "Speakers")}), first);
  FakeProvider provider; provider.answer = discovery({endpoint("changed", "Speakers")});
  const auto report = run_scoped_basic(provider, f.store, "update");
  CHECK(report.ambiguous_skipped == 1); CHECK(report.scan.result == "partially_completed");
  CHECK(provider.capability_queries.empty()); CHECK(f.files().size() == 1);
  provider.answer = discovery({endpoint("old", "Speakers")});
  CHECK(run_scoped_basic(provider, f.store, "update").known_skipped == 1); CHECK(provider.capability_queries.empty());
}
TEST(scoped_no_new_devices_with_incomplete_identity_enumeration_is_partial) {
  Fixture f; f.store.reconcile(discovery({endpoint()}), first);
  FakeProvider provider; provider.answer = discovery({endpoint()}); provider.answer.complete = false;
  provider.answer.diagnostics.push_back({"endpoint_query_failed", "Unreadable other endpoint name", {}, {}});
  const auto report = run_scoped_basic(provider, f.store, "update");
  CHECK(report.scan.result == "partially_completed"); CHECK(provider.capability_queries.empty()); CHECK(f.files().size() == 1);
}
TEST(scoped_update_ignores_unseen_historical_alias_without_false_ambiguity) {
  Fixture f; f.store.reconcile(discovery({endpoint("current", "Speakers")}), first);
  auto ghost = endpoint("historical", "Speakers"); ghost.state = "not_present";
  FakeProvider provider; provider.answer = discovery({endpoint("current", "Speakers"), ghost});
  const auto report = run_scoped_basic(provider, f.store, "update");
  CHECK(report.scan.result == "completed"); CHECK(report.ambiguous_skipped == 0);
  CHECK(provider.capability_queries.empty()); CHECK(f.files().size() == 1);
}
TEST(scoped_filename_rename_and_notes_preserved_without_retesting_known_endpoint) {
  Fixture f; f.store.reconcile(discovery({endpoint()}), first);
  const auto old = f.files()[0]; auto data = read(old); data["notes"] = "\xc3\xa9\nKeep  spacing\n"; write(old, data);
  const auto renamed = old.parent_path() / "User filename.json"; fs::rename(old, renamed);
  FakeProvider provider;
  const auto report = run_scoped_basic(provider, f.store, "update");
  CHECK(report.scan.result == "completed"); CHECK(provider.capability_queries.empty()); CHECK(f.files().size() == 1);
  const auto current = read(renamed);
  CHECK(current["notes"] == data["notes"]); CHECK(current["logical_id"] == data["logical_id"]);
  CHECK(current["device_name"] == data["device_name"]); CHECK(current["display_name"] == "User filename");
}
TestSuiteOptions quick_suite_options(const fs::path& parent) {
  TestSuiteOptions options; options.output_parent = parent; options.data_source = "simulated_local_contract_fixture";
  options.deep_limits = {1, 250, 2000, 10000, "both"}; options.system_timeout_ms = 60000; return options;
}
TestSuiteResult fixture_suite(const fs::path& parent, bool fault = false, std::stop_token stop = {}) {
  return run_test_suite(quick_suite_options(parent), [] { auto p = std::make_unique<FakeProvider>(); p->answer = batch_discovery(); return p; },
    [fault] { auto r = std::make_unique<FakeProbeRunner>();
      if (fault) r->answer = {{"classification", "inconclusive"}, {"cleanup", "unknown"}}; return r;
    }, true, stop);
}
void check_suite_result(const TestSuiteResult& result, const std::string& expected) {
  if (result.result != expected)
    throw std::runtime_error("Expected suite result " + expected + "; manifest: " + result.manifest.dump(2));
}
TEST(automated_suite_covers_six_scopes_and_checks_unicode_notes_and_filenames) {
  Fixture f; const auto result = fixture_suite(f.root / "suite");
  check_suite_result(result, "completed"); CHECK(fs::exists(result.archive)); CHECK(fs::file_size(result.archive) > 1000);
  CHECK(result.manifest["cases"].size() == 6); CHECK(result.manifest["hardware_source"] == "simulated_local_contract_fixture");
  for (const auto& c : result.manifest["cases"]) {
    CHECK(c["result"] == "completed"); CHECK(c["user_edits_verification"]["result"] == "passed");
    CHECK(c["phases"].size() == c["planned_phases"].get<std::size_t>());
    CHECK(c["user_edits_verification"]["checked"] >= 2);
    if (c["scope"] == "update") {
      CHECK(c["phases"].size() == 2); CHECK(c["phases"][0]["selected_endpoints"] == 1);
      CHECK(c["phases"][1]["selected_endpoints"] == 0); CHECK(c["phases"][1]["repeat_update_selected_none"] == true);
    }
    if (c["scope"] == "device") for (const auto& phase : c["phases"]) CHECK(phase["selected_endpoints"] == 1);
  }
  CHECK(bytes(result.directory / "Third-Party-Notices.txt").find("miniz") != std::string::npos);
  CHECK(read(result.directory / "manifest.json")["result"] == "completed");
}
TEST(automated_suite_denied_consent_starts_no_provider_and_writes_nothing) {
  Fixture f; bool called = false;
  const auto result = run_test_suite(quick_suite_options(f.root / "suite"), [&]() -> std::unique_ptr<EndpointProvider> { called = true; return {}; }, {}, false);
  CHECK(!called); CHECK(result.result == "failed"); CHECK(result.directory.empty()); CHECK(!fs::exists(f.root / "suite"));
}
TEST(automated_suite_worker_uncertainty_stops_later_cases_but_packages_results) {
  Fixture f; const auto result = fixture_suite(f.root / "suite", true);
  check_suite_result(result, "partially_completed"); CHECK(fs::exists(result.archive));
  CHECK(result.manifest["cases"][4]["result"] == "not_tested"); CHECK(result.manifest["cases"][5]["result"] == "not_tested");
  CHECK(result.manifest.contains("halt_reason"));
}
TEST(automated_suite_precanceled_run_packages_manifest_without_testing) {
  Fixture f; std::stop_source stop; stop.request_stop();
  const auto result = fixture_suite(f.root / "suite", false, stop.get_token());
  CHECK(result.result == "interrupted"); CHECK(fs::exists(result.archive));
  for (const auto& c : result.manifest["cases"]) CHECK(c["result"] == "not_tested");
}
TEST(automated_suite_provider_failure_still_creates_diagnostic_zip) {
  Fixture f;
  const auto result = run_test_suite(quick_suite_options(f.root / "suite"), []() -> std::unique_ptr<EndpointProvider> {
    throw std::runtime_error("Injected provider creation failure");
  }, [] { return std::make_unique<FakeProbeRunner>(); }, true);
  CHECK(result.result == "failed"); CHECK(fs::exists(result.archive));
  CHECK(result.manifest["diagnostics"][0]["message"] == "Injected provider creation failure");
}
TEST(automated_suite_detects_note_corruption_in_its_test_copy) {
  Fixture f; const auto options = quick_suite_options(f.root / "suite"); bool modified = false;
  const auto result = run_test_suite(options, [] { auto p = std::make_unique<FakeProvider>(); p->answer = batch_discovery(); return p; },
    [] { return std::make_unique<FakeProbeRunner>(); }, true, {}, [&](const Progress& p) {
      if (modified || p.phase != "suite_phase_started" || p.details.value("label", "") != "rescan-after-filename-and-notes-edits") return;
      for (const auto& run : fs::directory_iterator(options.output_parent)) {
        const auto dir = run.path() / "full-basic" / "portable" / "devices" / "recording";
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::directory_iterator(dir)) if (entry.path().extension() == ".json") {
          auto data = read(entry.path()); data["notes"] = "Corruption injected by the development test"; write(entry.path(), data); modified = true; break;
        }
      }
    });
  if (!modified) throw std::runtime_error("Suite did not reach the note-corruption phase; manifest: " + result.manifest.dump(2));
  check_suite_result(result, "failed"); CHECK(fs::exists(result.archive));
  CHECK(result.manifest["cases"][0]["user_edits_verification"]["result"] == "failed");
}
TEST(result_zip_refuses_overwrite_and_symlinks) {
  Fixture f; const auto dir = f.root / "zip-source"; fs::create_directories(dir);
  std::ofstream(dir / "hello.txt") << "hello";
  const auto zip = f.root / "result.zip"; zip_results(dir, zip);
  const auto original = bytes(zip); bool failed = false;
  try { zip_results(dir, zip); } catch (...) { failed = true; } CHECK(failed); CHECK(bytes(zip) == original);
#ifndef _WIN32
  fs::create_symlink(dir / "hello.txt", dir / "link"); failed = false;
  try { zip_results(dir, f.root / "unsafe.zip"); } catch (...) { failed = true; } CHECK(failed);
  CHECK(!fs::exists(f.root / "unsafe.zip"));
#endif
}
#ifdef _WIN32
fs::path test_executable() {
  std::wstring path(32768, L'\0');
  const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  CHECK(size && size < path.size()); path.resize(size); return fs::path(path);
}
int fixture_worker() {
  std::string line; std::getline(std::cin, line);
  const auto request = Json::parse(line);
  const auto id = request["attempt_id"].get<std::string>();
  const auto behavior = request["logical_id"].get<std::string>();
  if (behavior == "hang") { Sleep(INFINITE); return 2; }
  if (behavior == "crash") return 17;
  if (behavior == "malformed") { std::cout << "not JSON\n" << std::flush; return 0; }
  std::cout << Json{{"type", "stage"}, {"attempt_id", id}, {"stage", Json{{"api", "Fixture"},
    {"phase", behavior == "malformed_stage" ? "invalid" : "end"}}}}.dump() << '\n' << std::flush;
  Json result{{"classification", "verified"}, {"cleanup", "completed"}, {"verification_scope", "buffer_processing"},
    {"runtime_duration_ms", request["duration_ms"]}, {"serviced_frames", 1000}, {"audio_events", 3}};
  if (behavior == "missing_evidence") result.erase("serviced_frames");
  std::cout << Json{{"type", "result"}, {"attempt_id", behavior == "wrong_id" ? "wrong" : id}, {"result", result}}.dump() << '\n' << std::flush;
  return 0;
}
TEST(windows_atomic_writes_support_deep_unicode_fixture_paths) {
  Fixture f;
  auto directory = f.root;
  while (directory.native().size() < 280) directory /= "Nested fixture records";
  fs::create_directories(directory);
  const auto path = directory / utf8_path("Notes - \xc3\xa9 \xce\xa9.json");
  auto writer = make_atomic_writer();
  const std::string original = Json{{"notes", "Keep Unicode: \xc3\xa9 \xce\xa9"}, {"extension", true}}.dump(2) + '\n';
  writer->write(path, original, std::nullopt);
  CHECK(bytes(path) == original);
  auto updated = Json::parse(original); updated["notes"] = "Edited notes";
  const auto replacement = updated.dump(2) + '\n';
  writer->write(path, replacement, original);
  CHECK(bytes(path) == replacement);
  auto backup = path; backup += ".bak";
  CHECK(bytes(backup) == original); CHECK(read(path)["extension"] == true);
}
TEST(windows_worker_protocol_rejects_faults_and_bounds_hangs) {
  auto runner = make_process_probe_runner(test_executable());
  ProbeJob job{"probe-fixture", {"valid", "not-a-real-device", "Fixture", "render", Json::object()},
    {probe_format(), "shared", "fixture"}, 250, 1500};
  unsigned stages = 0;
  CHECK(runner->execute(job, {}, [&](const Json&) { ++stages; })["classification"] == "verified");
  CHECK(stages == 1);
  for (const auto* behavior : {"crash", "malformed", "malformed_stage", "wrong_id", "missing_evidence", "hang"}) {
    job.target.logical_id = behavior;
    const auto start = std::chrono::steady_clock::now();
    const auto result = runner->execute(job, {}, [](const Json&) {});
    CHECK(result["classification"] == "inconclusive"); CHECK(result["cleanup"] == "unknown");
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(6));
  }
}
TEST(windows_worker_cancellation_requests_termination) {
  auto runner = make_process_probe_runner(test_executable());
  ProbeJob job{"probe-cancel", {"hang", "not-a-real-device", "Fixture", "render", Json::object()},
    {probe_format(), "shared", "fixture"}, 250, 10000};
  std::stop_source stop;
  std::jthread cancel([&] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); stop.request_stop(); });
  const auto start = std::chrono::steady_clock::now();
  const auto result = runner->execute(job, stop.get_token(), [](const Json&) {});
  CHECK(result["classification"] == "inconclusive");
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
}
#endif
}

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--suite-fixture") {
    const auto result = fixture_suite(utf8_path(argv[2]));
    std::cout << path_utf8(result.archive) << '\n'; return result.result == "completed" ? 0 : 1;
  }
#ifdef _WIN32
  if (argc == 2 && std::string(argv[1]) == "--probe-worker") {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    return fixture_worker();
  }
#else
  (void)argc; (void)argv;
#endif
  int failed = 0;
  for (const auto& [name, body] : tests()) {
    try { body(); std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
  }
  std::cout << tests().size() - failed << '/' << tests().size() << " passed\n";
  return failed ? 1 : 0;
}
