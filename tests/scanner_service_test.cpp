#include "audiohole/scoped_scan.hpp"
#include "model/application_controller.h"
#include "model/configuration_controller.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

namespace audiohole {
TEST(ScannerLookup, OnlyPositiveAbsenceAndMissingMetadataAreExpected) {
    scanner::Endpoint endpoint;
    endpoint.endpoint_id = "transient-id";
    endpoint.direction = "capture";
    endpoint.state = "not_present";
    const std::string api = "IPropertyStore::GetValue(PKEY_Device_FriendlyName)";
    for (const auto* code :
         {"0x80070490", "0x8007048F", "0x88890004", "0xE000020B", "0x800F020B"}) {
        const auto diagnostic =
            scanner::endpoint_lookup_failure(endpoint, api, code, "Unavailable");
        EXPECT_EQ(diagnostic.code, "absent_endpoint_metadata_unavailable");
        const scanner::Json json = diagnostic;
        EXPECT_EQ(json.at("endpoint_state"), "not_present");
        EXPECT_EQ(json.at("api"), api);
        EXPECT_EQ(json.at("native_code"), code);
        EXPECT_EQ(json.at("context"), endpoint.endpoint_id);
    }
    EXPECT_EQ(scanner::endpoint_lookup_failure(endpoint, api, "0x00000000", "Empty", true).code,
              "absent_endpoint_metadata_unavailable");
    EXPECT_EQ(
        scanner::endpoint_lookup_failure(endpoint, api, "0x00000000", "Wrong property type").code,
        "endpoint_query_failed");
    EXPECT_EQ(scanner::endpoint_lookup_failure(endpoint, api, "0x80070005", "Access denied").code,
              "endpoint_query_failed");
    EXPECT_EQ(
        scanner::endpoint_lookup_failure(endpoint, "IMMDevice::GetId", "0x80070490", "No ID").code,
        "endpoint_query_failed");
    for (const auto* state : {"active", "disabled", "unplugged", "unknown", ""}) {
        endpoint.state = state;
        const auto diagnostic =
            scanner::endpoint_lookup_failure(endpoint, api, "0x80070490", "Unavailable");
        EXPECT_EQ(diagnostic.code, "endpoint_query_failed") << state;
        EXPECT_FALSE(diagnostic.endpoint_state.empty());
    }
    endpoint.state = "not_present";
    endpoint.direction.clear();
    EXPECT_EQ(scanner::endpoint_lookup_failure(endpoint, api, "0x80070490", "Unavailable").code,
              "endpoint_query_failed");
    endpoint.direction = "capture";
    endpoint.endpoint_id.clear();
    EXPECT_EQ(scanner::endpoint_lookup_failure(endpoint, api, "0x80070490", "Unavailable").code,
              "endpoint_query_failed");
    // Existing callers and old reports do not acquire empty optional fields.
    const scanner::Json legacy = scanner::Diagnostic{"old", "message", "context", {}};
    EXPECT_FALSE(legacy.contains("endpoint_state"));
    EXPECT_FALSE(legacy.contains("api"));
}
namespace {
namespace scan = scanner;
using Json = scan::Json;
using namespace std::chrono_literals;
const char* first = "2026-11-03T01:00:00Z";
const char* second = "2026-11-03T02:00:00Z";
std::string bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
void write(const std::filesystem::path& path, const Json& json) {
    std::ofstream(path, std::ios::binary) << json.dump(2) << '\n';
}
scan::Endpoint device(const std::string& id, const std::string& direction = "render") {
    scan::Endpoint e;
    e.endpoint_id = id;
    e.direction = direction;
    e.device_name = id;
    e.state = "active";
    e.driver = {{"version", "1.0"}, {"association_status", "available"}};
    const Json format{
        {"sample_rate_hz", 48000}, {"channel_count", 2},
        {"container_bits", 32},    {"valid_bits", 32},
        {"channel_mask", 3},       {"sample_format", "float32"},
        {"format_tag", 65534},     {"subformat_guid", "{00000003-0000-0010-8000-00AA00389B71}"},
        {"block_align_bytes", 8},  {"average_bytes_per_second", 384000}};
    e.reported_capabilities = {{"default_mix_format", format},
                               {"configured_device_format", format}};
    return e;
}
scan::Discovery discovery(std::vector<scan::Endpoint> devices) {
    return {std::move(devices), {}, true, false};
}
struct Evidence {
    std::vector<scan::Endpoint> endpoints{device("Capture", "capture"), device("Playback")};
    std::vector<std::string> queried, probed;
    bool complete = true, block = false;
    std::vector<scan::Diagnostic> diagnostics;
    std::atomic<bool> entered = false;
};
class Provider final : public scan::EndpointProvider {
  public:
    explicit Provider(std::shared_ptr<Evidence> evidence) : evidence_(std::move(evidence)) {}
    scan::Discovery discover(std::stop_token stop, const scan::ProgressSink& progress,
                             const scan::EndpointFilter& filter) override {
        evidence_->entered = true;
        while (evidence_->block && !stop.stop_requested())
            std::this_thread::sleep_for(1ms);
        auto result = discovery({});
        for (const auto& e : evidence_->endpoints) {
            if (stop.stop_requested())
                break;
            if (!filter || filter(e)) {
                evidence_->queried.push_back(e.endpoint_id);
                result.endpoints.push_back(e);
                if (progress)
                    progress({"fixture_query",
                              e.endpoint_id,
                              result.endpoints.size(),
                              evidence_->endpoints.size(),
                              {}});
            }
        }
        result.diagnostics = evidence_->diagnostics;
        result.cancelled = stop.stop_requested();
        result.complete = !result.cancelled && evidence_->complete;
        return result;
    }

  private:
    std::shared_ptr<Evidence> evidence_;
};
class Runner final : public scan::ProbeRunner {
  public:
    explicit Runner(std::shared_ptr<Evidence> evidence) : evidence_(std::move(evidence)) {}
    Json execute(const scan::ProbeJob& job, std::stop_token,
                 const scan::ProbeStageSink& stage) override {
        evidence_->probed.push_back(job.target.endpoint_id);
        stage({{"api", "fixture"}, {"phase", "end"}});
        return {{"classification", "inconclusive"},
                {"cleanup", "completed"},
                {"signal_verification", "not_tested"}};
    }

  private:
    std::shared_ptr<Evidence> evidence_;
};
class ScannerIntegration : public ::testing::Test {
  protected:
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / scan::new_logical_id("gui-scan-test");
    std::filesystem::path portable = root / "test", local = root / "test-local";
    scan::DeviceStore store{portable, local};
    std::shared_ptr<Evidence> evidence = std::make_shared<Evidence>();
    ~ScannerIntegration() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    std::unique_ptr<ScannerService> service() {
        return std::make_unique<ScannerService>(
            ScannerServiceOptions{portable, local, {}, "injected_test_fixture"},
            ScannerFactories{[e = evidence] { return std::make_unique<Provider>(e); },
                             [e = evidence] { return std::make_unique<Runner>(e); }});
    }
    ScannerUpdate finish(ScannerService& service, std::uint64_t generation) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline) {
            for (auto& update : service.drain())
                if (update.generation == generation && update.terminal)
                    return update;
            std::this_thread::sleep_for(1ms);
        }
        ADD_FAILURE() << "Scanner did not finish generation " << generation;
        return {};
    }
    void finish(Controller& controller) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        do {
            controller.advance(10);
            if (!controller.snapshot().scanner.busy)
                return;
            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);
        FAIL() << "Controller scanner remained busy";
    }
};
TEST_F(ScannerIntegration, ReadOnlyInventoryHonorsFilenameAndPreservesHiddenExtensions) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    auto record = store.inventory().devices.front();
    const auto renamed = record.path.parent_path() / scan::utf8_path("Desk é microphone.json");
    std::filesystem::rename(record.path, renamed);
    auto json = Json::parse(bytes(renamed));
    json["notes"] = "External notes\nwith whitespace  ";
    json["extension"] = {{"keep", true}};
    write(renamed, json);
    const auto original = bytes(renamed);
    auto view = store.inventory();
    ASSERT_TRUE(view.complete);
    EXPECT_EQ(bytes(renamed), original);
    const auto id = json.at("logical_id").get<std::string>();
    store.set_hidden(id, true);
    json = Json::parse(bytes(renamed));
    EXPECT_TRUE(json.at("hidden").get<bool>());
    EXPECT_EQ(json.at("display_name"), "Desk é microphone");
    EXPECT_EQ(json.at("notes"), "External notes\nwith whitespace  ");
    EXPECT_TRUE(json.at("extension").at("keep"));
    EXPECT_EQ(json.at("logical_id"), id);
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), second).result, "completed");
    EXPECT_TRUE(Json::parse(bytes(renamed)).at("hidden"));
    json = Json::parse(bytes(renamed));
    json["hidden"] = "yes";
    write(renamed, json);
    const auto invalid = bytes(renamed);
    EXPECT_FALSE(store.inventory().complete);
    EXPECT_THROW(store.set_hidden(id, false), std::exception);
    EXPECT_EQ(bytes(renamed), invalid);
}
TEST_F(ScannerIntegration, UpdateRefreshesKnownAvailabilityWithoutRepeatingCapabilityQueries) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto before = store.inventory();
    ASSERT_TRUE(before.complete);
    evidence->endpoints[0].state = "disabled";
    evidence->endpoints[0].reported_capabilities = {{"should_not_replace", true}};
    evidence->endpoints.push_back(device("New playback"));
    Provider provider(evidence);
    const auto report = scan::run_scoped_basic(provider, store, "update");
    EXPECT_EQ(report.scan.result, "completed");
    EXPECT_EQ(evidence->queried, std::vector<std::string>{"New playback"});
    const auto after = store.inventory();
    ASSERT_TRUE(after.complete);
    ASSERT_EQ(after.devices.size(), 3u);
    const auto id = before.devices[0].record.at("logical_id");
    const auto found = std::find_if(after.devices.begin(), after.devices.end(),
                                    [&](const auto& d) { return d.record.at("logical_id") == id; });
    ASSERT_NE(found, after.devices.end());
    EXPECT_EQ(found->local_binding.at("state"), "disabled");
    EXPECT_EQ(found->record.at("scan_history").at("last_scanned"), first);
    EXPECT_EQ(found->record.at("reported_capabilities"),
              before.devices[0].record.at("reported_capabilities"));
}
TEST_F(ScannerIntegration, InactiveFullScanKeepsSavedIdentityAndReportsMetadataOnly) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto before = store.inventory();
    const auto capture =
        std::find_if(before.devices.begin(), before.devices.end(), [](const auto& d) {
            return d.record.at("identity").at("direction") == "capture";
        });
    ASSERT_NE(capture, before.devices.end());
    auto record = capture->record;
    record["notes"] = "Keep the disconnected microphone";
    record["extension"] = {{"owned_by_user", true}};
    write(capture->path, record);
    evidence->endpoints[0].state = "not_present";
    Provider provider(evidence);
    std::vector<scan::Progress> events;
    const auto report =
        scan::run_scoped_basic(provider, store, "full", {}, {},
                               [&](const scan::Progress& event) { events.push_back(event); });
    ASSERT_EQ(report.scan.result, "completed");
    const auto selected = std::find_if(events.begin(), events.end(), [](const auto& event) {
        return event.phase == "scope_selection" && event.endpoint_id == "Capture";
    });
    ASSERT_NE(selected, events.end());
    EXPECT_EQ(selected->details.at("capability_queries"), "metadata_only");
    const auto after = store.inventory();
    ASSERT_TRUE(after.complete);
    ASSERT_EQ(after.devices.size(), 2u);
    const auto retained =
        std::find_if(after.devices.begin(), after.devices.end(), [&](const auto& d) {
            return d.record.at("logical_id") == record.at("logical_id");
        });
    ASSERT_NE(retained, after.devices.end());
    EXPECT_EQ(retained->record.at("device_name"), record.at("device_name"));
    EXPECT_EQ(retained->record.at("notes"), record.at("notes"));
    EXPECT_EQ(retained->record.at("extension"), record.at("extension"));
    EXPECT_EQ(retained->local_binding.at("state"), "not_present");
}
TEST_F(ScannerIntegration, SelectedRefreshDoesNotTouchUnrelatedRecordsOrInferAbsence) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto before = store.inventory();
    ASSERT_EQ(before.devices.size(), 2u);
    const auto id = before.devices[0].record.at("logical_id").get<std::string>();
    const auto unrelated = bytes(before.devices[1].path);
    Provider provider(evidence);
    auto report = scan::run_scoped_basic(provider, store, "device", id);
    EXPECT_EQ(report.selected_endpoint_ids, std::vector<std::string>{"Capture"});
    EXPECT_EQ(evidence->queried, report.selected_endpoint_ids);
    EXPECT_EQ(bytes(before.devices[1].path), unrelated);
    evidence->endpoints.clear();
    evidence->complete = false;
    scan::run_scoped_basic(provider, store, "full");
    const auto after = store.inventory();
    ASSERT_TRUE(after.complete);
    EXPECT_EQ(after.devices[1].local_binding.at("state"), "active");
}
TEST_F(ScannerIntegration, AllSixScopeDepthCombinationsKeepDeepTargetsExactAndExportContext) {
    auto worker = service();
    ScanOptions options;
    options.consent = true;
    options.attempts = 1;
    options.operating_context = "fixture application context";
    for (bool deep : {false, true})
        for (auto scope : {ScanScope::Full, ScanScope::Update, ScanScope::Selected}) {
            evidence->queried.clear();
            evidence->probed.clear();
            const auto inventory = store.inventory();
            const auto id =
                inventory.devices.empty()
                    ? std::string{}
                    : inventory.devices.front().record.at("logical_id").get<std::string>();
            const auto generation = worker->start_scan(deep, scope, id, options);
            ASSERT_NE(generation, 0u);
            const auto update = finish(*worker, generation);
            EXPECT_EQ(update.result,
                      deep && scope != ScanScope::Update ? "partially_completed" : "completed")
                << update.detail;
            EXPECT_TRUE(update.inventory_valid);
            ASSERT_EQ(update.devices.size(), 2u);
            EXPECT_TRUE(update.devices[0].formats.empty());
            if (scope == ScanScope::Update) {
                EXPECT_TRUE(evidence->queried.empty());
                EXPECT_TRUE(evidence->probed.empty());
            } else if (scope == ScanScope::Selected) {
                EXPECT_EQ(evidence->queried, std::vector<std::string>{"Capture"});
                if (deep)
                    EXPECT_EQ(evidence->probed, evidence->queried);
            } else {
                EXPECT_EQ(evidence->queried.size(), 2u);
                if (deep)
                    EXPECT_EQ(evidence->probed.size(), 2u);
            }
            const auto session = scan::utf8_path(update.session_path);
            const auto request = Json::parse(bytes(session / "request.json"));
            EXPECT_EQ(request.at("operating_context"), options.operating_context);
            EXPECT_EQ(request.at("source"), "injected_test_fixture");
            EXPECT_TRUE(std::filesystem::exists(session / "completion.json"));
            if (deep && scope != ScanScope::Update) {
                const auto report = Json::parse(bytes(session / "deep.json"));
                EXPECT_EQ(report.at("scope"), scope == ScanScope::Selected ? "device" : "full");
                const auto inventory = Json::parse(bytes(session / "inventory.json"));
                ASSERT_FALSE(inventory.at("history_files").empty());
                for (const auto& history : inventory.at("history_files")) {
                    EXPECT_EQ(history.at("status"), "included");
                    EXPECT_TRUE(std::filesystem::exists(
                        session / scan::utf8_path(history.at("path").get<std::string>())));
                }
                for (const auto& saved : inventory.at("devices"))
                    EXPECT_EQ(saved.at("record").get<std::string>().find('\\'), std::string::npos);
            }
        }
    ScannerActionRequest export_request;
    export_request.kind = ScannerActionKind::Export;
    export_request.output = root / "analysis.zip";
    EXPECT_EQ(finish(*worker, worker->action(export_request)).result, "completed");
    const auto archive = bytes(export_request.output);
    ASSERT_FALSE(archive.empty());
    EXPECT_EQ(finish(*worker, worker->action(export_request)).result, "failed");
    EXPECT_EQ(bytes(export_request.output), archive);
}
TEST_F(ScannerIntegration, SelectedCapabilityCompletionSurvivesUnrelatedInventoryWarnings) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto before = store.inventory();
    const auto id = before.devices.front().record.at("logical_id").get<std::string>();
    const auto unrelated = bytes(before.devices.back().path);
    evidence->complete = false;
    evidence->diagnostics = {
        {"endpoint_query_failed", "Windows endpoint name unavailable", "unrelated-ghost", {}}};
    Provider provider(evidence);
    const auto report = scan::run_scoped_basic(provider, store, "device", id);
    EXPECT_EQ(report.scan.result, "partially_completed");
    EXPECT_FALSE(report.inventory_complete);
    EXPECT_TRUE(report.selected_target_complete);
    const auto after = store.inventory();
    ASSERT_TRUE(after.complete);
    EXPECT_TRUE(after.devices.front().record.at("scan_history").at("last_scanned").is_string());
    EXPECT_NE(after.devices.front().record.at("scan_history").at("last_scanned"), first);
    EXPECT_EQ(bytes(before.devices.back().path), unrelated);
}
TEST_F(ScannerIntegration, DeepUpdateTestsOnlyNewTargetsAndAvailabilityRefreshNeverCreatesRecords) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto before = store.inventory();
    evidence->endpoints[0].state = "disabled";
    evidence->endpoints.push_back(device("New output"));
    auto worker = service();
    ScannerActionRequest refresh;
    refresh.kind = ScannerActionKind::RefreshAvailability;
    const auto refreshed = finish(*worker, worker->action(refresh));
    EXPECT_EQ(refreshed.result, "completed");
    EXPECT_EQ(refreshed.devices.size(), 2u);
    EXPECT_TRUE(evidence->queried.empty());
    EXPECT_EQ(store.inventory().devices.front().record.at("scan_history").at("last_scanned"),
              first);
    ScanOptions options;
    options.consent = true;
    options.attempts = 1;
    const auto update = finish(*worker, worker->start_scan(true, ScanScope::Update, {}, options));
    EXPECT_EQ(update.result, "partially_completed");
    EXPECT_EQ(evidence->queried, std::vector<std::string>{"New output"});
    EXPECT_EQ(evidence->probed, evidence->queried);
    EXPECT_EQ(update.devices.size(), 3u);
    const auto missing_id = before.devices.front().record.at("logical_id").get<std::string>();
    evidence->endpoints.erase(evidence->endpoints.begin());
    evidence->queried.clear();
    evidence->probed.clear();
    const auto missing =
        finish(*worker, worker->start_scan(true, ScanScope::Selected, missing_id, options));
    EXPECT_EQ(missing.result, "failed");
    EXPECT_TRUE(evidence->queried.empty());
    EXPECT_TRUE(evidence->probed.empty());
    EXPECT_NE(missing.detail.find("no substitute"), std::string::npos);
}
TEST_F(ScannerIntegration, CancellationIsNonblockingAndValidationNeverStartsUnconsentedDeepWork) {
    auto worker = service();
    ScanOptions options;
    EXPECT_THROW(worker->start_scan(true, ScanScope::Full, {}, options), std::invalid_argument);
    EXPECT_THROW(worker->start_scan(false, ScanScope::Selected, {}, options),
                 std::invalid_argument);
    EXPECT_FALSE(evidence->entered);
    evidence->block = true;
    const auto generation = worker->start_scan(false, ScanScope::Full, {}, options);
    ASSERT_NE(generation, 0u);
    EXPECT_EQ(worker->start_scan(false, ScanScope::Full, {}, options), 0u);
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!evidence->entered && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    ASSERT_TRUE(evidence->entered);
    worker->cancel();
    const auto update = finish(*worker, generation);
    EXPECT_EQ(update.result, "interrupted");
    EXPECT_FALSE(worker->busy());
    evidence->block = false;
    const auto next = worker->start_scan(false, ScanScope::Full, {}, options);
    EXPECT_GT(next, generation);
    EXPECT_EQ(finish(*worker, next).result, "completed");
}
TEST_F(ScannerIntegration, CancellationCompletesOnlyWhenControllerConsumesWorkerCleanup) {
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    evidence->block = true;
    auto consumer = controller.subscribe();
    consumer.poll();
    const auto accepted = controller.submit(DiscoverDevices{false, ScanScope::Full, {}, {}});
    ASSERT_EQ(accepted.outcome, Outcome::Accepted);
    controller.scan(false);
    EXPECT_TRUE(controller.snapshot().scanner.busy);
    const auto early = consumer.poll();
    EXPECT_TRUE(std::none_of(early.results.begin(), early.results.end(), [](const auto& result) {
        return result.outcome == Outcome::Cancelled;
    }));
    finish(controller);
    const auto completed = consumer.poll();
    EXPECT_TRUE(
        std::any_of(completed.results.begin(), completed.results.end(), [&](const auto& result) {
            return result.request_id == accepted.request_id && result.outcome == Outcome::Cancelled;
        }));
    EXPECT_FALSE(controller.snapshot().scanner.busy);
    EXPECT_EQ(controller.snapshot().scanner.result, "interrupted");
}
TEST_F(ScannerIntegration,
       MetadataSaveRenamesFileAndPreservesIdentityReferencesAndExtensionsAcrossRestart) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto original = store.inventory().devices.front();
    auto data = original.record;
    data["hidden"] = true;
    data["extension"] = {{"keep", true}};
    write(original.path, data);
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    auto edited = controller.snapshot().endpoints.front();
    const auto logical_id = edited.id, windows_name = edited.windows_name;
    const auto profile_id = controller.create_profile("Rename references");
    auto profile = *audiohole::profile(controller.snapshot(), profile_id);
    auto hole = controller.make_hole();
    hole.forward.source = logical_id;
    hole.forward.destination = controller.snapshot().endpoints.back().id;
    profile.holes.push_back(hole);
    ASSERT_TRUE(controller.save(profile, false).ok);
    auto consumer = controller.subscribe();
    consumer.poll();
    edited.display_name = "Desk microphone é";
    edited.notes = "New notes\nUnicode Ω; spaces  stay.";
    EXPECT_TRUE(controller.update_endpoint(edited).ok);
    EXPECT_TRUE(controller.snapshot().scanner.busy);
    const auto generation = controller.snapshot().scanner.generation;
    finish(controller);
    EXPECT_EQ(controller.snapshot().scanner.result, "completed");
    const auto* saved = endpoint(controller.snapshot(), logical_id);
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->display_name, edited.display_name);
    EXPECT_EQ(saved->notes, edited.notes);
    EXPECT_EQ(saved->windows_name, windows_name);
    EXPECT_TRUE(saved->hidden);
    EXPECT_EQ(audiohole::profile(controller.snapshot(), profile_id)->holes.front().forward.source,
              logical_id);
    EXPECT_FALSE(std::filesystem::exists(original.path));
    const auto new_path =
        original.path.parent_path() / scan::utf8_path(edited.display_name + ".json");
    const auto record = Json::parse(bytes(new_path));
    EXPECT_TRUE(record["extension"]["keep"]);
    EXPECT_EQ(record["logical_id"], logical_id);
    EXPECT_EQ(controller.snapshot().scanner.completed_generation, generation);
    const auto batch = consumer.poll();
    EXPECT_TRUE(std::any_of(batch.results.begin(), batch.results.end(), [](const auto& result) {
        return result.command == "scanner.action" && result.outcome == Outcome::Completed;
    }));
    ConfigurationController restarted(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service());
    finish(restarted);
    ASSERT_TRUE(endpoint(restarted.snapshot(), logical_id));
    EXPECT_EQ(endpoint(restarted.snapshot(), logical_id)->display_name, edited.display_name);
    EXPECT_EQ(endpoint(restarted.snapshot(), logical_id)->notes, edited.notes);
}
TEST_F(ScannerIntegration,
       MetadataSaveRejectsStaleEditorsCollisionsAndInvalidNamesWithoutOverwriting) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto original = store.inventory().devices.front();
    const auto id = original.record.at("logical_id").get<std::string>();
    for (const auto* invalid : {"", "CON", "Name/escape", "Name.", "Name "})
        EXPECT_THROW(store.edit_metadata(id, invalid, "", scan::path_utf8(original.path),
                                         original.original_bytes),
                     std::exception);
    auto external = original.record;
    external["notes"] = "Externally edited notes";
    write(original.path, external);
    EXPECT_THROW(store.edit_metadata(id, "Renamed", "stale", scan::path_utf8(original.path),
                                     original.original_bytes),
                 std::exception);
    EXPECT_EQ(Json::parse(bytes(original.path))["notes"], "Externally edited notes");
    const auto fresh = store.inventory().devices.front();
    const auto conflict = fresh.path.parent_path() / "Taken.json";
    auto other = fresh.record;
    other["logical_id"] = "capture-other";
    other["notes"] = "Another device, never overwrite";
    write(conflict, other);
    const auto conflict_bytes = bytes(conflict);
    EXPECT_THROW(store.edit_metadata(id, "Taken", "notes", scan::path_utf8(fresh.path),
                                     fresh.original_bytes),
                 std::exception);
    EXPECT_EQ(bytes(conflict), conflict_bytes);
    EXPECT_EQ(bytes(fresh.path), fresh.original_bytes);
}
TEST_F(ScannerIntegration, RefreshDoesNotRebaseAnOlderMetadataEditorSnapshot) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto original = store.inventory().devices.front();
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    auto draft = controller.snapshot().endpoints.front();
    auto external = original.record;
    external["notes"] = "Edited by another application";
    write(original.path, external);
    ASSERT_TRUE(controller.scanner_action(ScannerActionRequest{}).ok);
    finish(controller);
    EXPECT_EQ(endpoint(controller.snapshot(), draft.id)->notes, "Edited by another application");
    draft.display_name = "Stale editor rename";
    draft.notes = "Do not overwrite the external notes";
    ASSERT_TRUE(controller.update_endpoint(draft).ok);
    finish(controller);
    EXPECT_EQ(controller.snapshot().scanner.result, "failed");
    EXPECT_NE(controller.snapshot().scanner.detail.find("changed externally"), std::string::npos);
    EXPECT_EQ(bytes(original.path), external.dump(2) + "\n");
    EXPECT_FALSE(std::filesystem::exists(original.path.parent_path() / "Stale editor rename.json"));
}
class RejectMetadataWriter final : public scan::AtomicWriter {
  public:
    void write(const std::filesystem::path&, const std::string&,
               const std::optional<std::string>&) override {
        throw std::runtime_error("Injected metadata write failure");
    }
};
TEST_F(ScannerIntegration, FailedMetadataWriteRestoresOriginalFilenameAndBytes) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto original = store.inventory().devices.front();
    scan::DeviceStore failing(portable, local, std::make_shared<RejectMetadataWriter>());
    EXPECT_THROW(failing.edit_metadata(original.record.at("logical_id"), "New name", "New notes",
                                       scan::path_utf8(original.path), original.original_bytes),
                 std::exception);
    EXPECT_EQ(bytes(original.path), original.original_bytes);
    EXPECT_FALSE(std::filesystem::exists(original.path.parent_path() / "New name.json"));
    EXPECT_EQ(store.inventory().devices.size(), 2u);
}
TEST_F(ScannerIntegration, CaseOnlyRenameRetainsTheSameRecord) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto original = store.inventory().devices.front();
    store.edit_metadata(original.record.at("logical_id"), "capture", "case rename",
                        scan::path_utf8(original.path), original.original_bytes);
    const auto loaded = store.inventory();
    ASSERT_TRUE(loaded.complete);
    ASSERT_EQ(loaded.devices.size(), 2u);
    EXPECT_EQ(loaded.devices.front().record.at("display_name"), "capture");
    EXPECT_EQ(loaded.devices.front().record.at("logical_id"), original.record.at("logical_id"));
}
TEST_F(ScannerIntegration, OverviewPrioritizesStatusAndDistinguishesFormatsAndHistoricalFailures) {
    auto endpoint = evidence->endpoints.front();
    const auto format = endpoint.reported_capabilities.at("default_mix_format");
    endpoint.reported_capabilities["format_queries"] = Json::array({{{"configuration", format},
                                                                     {"mode", "shared"},
                                                                     {"support", "supported"},
                                                                     {"hresult", "0x00000000"}}});
    ASSERT_EQ(store.reconcile(discovery({endpoint}), first).result, "completed");
    auto original = store.inventory().devices.front();
    auto data = original.record;
    data["probed_capabilities"]["latest_attempt_id"] = "recent";
    data["probed_capabilities"]["test_attempts"] = Json::array(
        {{{"attempt_id", "old"}, {"classification", "inconclusive"}, {"cleanup", "unknown"}},
         {{"attempt_id", "recent"},
          {"classification", "failed"},
          {"support", "unsupported"},
          {"cleanup", "completed"}}});
    write(original.path, data);
    auto worker = service();
    const auto loaded = finish(*worker, worker->action(ScannerActionRequest{}));
    ASSERT_EQ(loaded.devices.size(), 1u);
    const auto& detail = *loaded.devices.front().inspection;
    const auto& overview = detail.views[0];
    EXPECT_LT(overview.find("Display name:"), overview.find("Logical ID:"));
    EXPECT_LT(overview.find("Last observed availability:"),
              overview.find("Windows-configured format:"));
    EXPECT_NE(overview.find("32-bit float"), std::string::npos);
    EXPECT_NE(overview.find("requested format unsupported"), std::string::npos);
    EXPECT_FALSE(detail.warning);
    EXPECT_FALSE(detail.unusable);
    EXPECT_NE(detail.views[2].find("shared:"), std::string::npos);
}
TEST_F(ScannerIntegration, ControllerKeepsScannerRecordsSeparateAndCorrelatesHideCompletion) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    ASSERT_EQ(controller.snapshot().endpoints.size(), 2u);
    auto consumer = controller.subscribe();
    consumer.poll();
    const auto id = controller.snapshot().endpoints.front().id;
    const auto profile_id = controller.create_profile("Profile");
    Profile profile = *audiohole::profile(controller.snapshot(), profile_id);
    auto hole = controller.make_hole();
    hole.forward.source = id;
    hole.forward.destination = controller.snapshot().endpoints.back().id;
    profile.holes.push_back(hole);
    ASSERT_TRUE(controller.save(profile, false).ok);
    ScannerActionRequest hide;
    hide.kind = ScannerActionKind::SetHidden;
    hide.logical_id = id;
    hide.hidden = true;
    const auto accepted = controller.submit(ScannerAction{hide});
    EXPECT_EQ(accepted.outcome, Outcome::Accepted);
    finish(controller);
    ASSERT_TRUE(endpoint(controller.snapshot(), id)->hidden);
    EXPECT_EQ(audiohole::profile(controller.snapshot(), profile_id)->holes.front().forward.source,
              id);
    const auto batch = consumer.poll();
    const auto terminal =
        std::find_if(batch.results.begin(), batch.results.end(), [&](const auto& r) {
            return r.request_id == accepted.request_id && r.outcome == Outcome::Completed;
        });
    EXPECT_NE(terminal, batch.results.end());
    const auto configuration = Json::parse(bytes(portable / "configuration.json"));
    EXPECT_TRUE(configuration.at("devices").empty());
    {
        ConfigurationController restarted(
            std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service());
        finish(restarted);
        ASSERT_TRUE(endpoint(restarted.snapshot(), id));
        EXPECT_TRUE(endpoint(restarted.snapshot(), id)->hidden);
        ASSERT_TRUE(audiohole::profile(restarted.snapshot(), profile_id));
        EXPECT_EQ(
            audiohole::profile(restarted.snapshot(), profile_id)->holes.front().forward.source, id);
    }
    const auto good = controller.snapshot().endpoints;
    const auto path = store.inventory().devices.front().path;
    std::ofstream(path) << "{ broken";
    ScannerActionRequest reload;
    EXPECT_TRUE(controller.scanner_action(reload).ok);
    finish(controller);
    EXPECT_EQ(controller.snapshot().scanner.result, "failed");
    EXPECT_NE(controller.snapshot().scanner.detail.find(scan::path_utf8(path.filename())),
              std::string::npos);
    EXPECT_EQ(controller.snapshot().endpoints.size(), good.size());
    EXPECT_TRUE(endpoint(controller.snapshot(), id)->hidden);
}
} // namespace
TEST_F(ScannerIntegration, UnlimitedBatchRunsItsPlanAndExportsRawStagesAndMonotonicTimings) {
    auto worker = service();
    ScanOptions options;
    options.consent = true;
    options.system_timeout_ms = 0;
    options.attempts = 1;
    const auto result = finish(*worker, worker->start_scan(true, ScanScope::Full, {}, options));
    EXPECT_EQ(result.result, "partially_completed"); // Deliberately inconclusive fixture results.
    EXPECT_EQ(evidence->probed.size(), 2u);
    const auto directory = scan::utf8_path(result.session_path);
    const auto request = Json::parse(bytes(directory / "request.json"));
    EXPECT_EQ(request["system_timeout_ms"], 0);
    EXPECT_TRUE(request.contains("build_id"));
    std::ifstream journal(directory / "progress.jsonl");
    std::string line;
    unsigned stages = 0;
    while (std::getline(journal, line)) {
        const auto event = Json::parse(line);
        EXPECT_TRUE(event["elapsed_ms"].is_number_integer());
        EXPECT_TRUE(event["timestamp_utc"].is_string());
        if (event["phase"] == "deep_stage") {
            ++stages;
            EXPECT_TRUE(event["endpoint_id"].is_string());
        }
    }
    EXPECT_EQ(stages, 2u);
}
TEST_F(ScannerIntegration, ClearRequiresConfirmationAndPreservesProfilesAndOtherFiles) {
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    const auto id = controller.snapshot().endpoints.front().id;
    const auto profile_id = controller.create_profile("Keep my profile");
    auto profile = *audiohole::profile(controller.snapshot(), profile_id);
    auto hole = controller.make_hole();
    hole.forward.source = id;
    hole.forward.destination = controller.snapshot().endpoints.back().id;
    profile.holes.push_back(hole);
    ASSERT_TRUE(controller.save(profile, false).ok);
    write(portable / "unrelated.json", {{"keep", true}});
    std::filesystem::create_directories(local / "sessions");
    write(local / "sessions" / "old.json", {{"evidence", true}});
    std::filesystem::create_directories(portable / "probe_history");
    write(portable / "probe_history" / "old.json", {{"evidence", true}});
    write(local / "device_bindings.json.bak", {{"backup", true}});
    ScannerActionRequest request;
    request.kind = ScannerActionKind::ClearInventory;
    ASSERT_TRUE(controller.scanner_action(request).ok);
    finish(controller);
    EXPECT_EQ(controller.snapshot().scanner.result, "failed");
    EXPECT_EQ(controller.snapshot().endpoints.size(), 2u);
    const auto outside = root / "outside-scanner-data";
    std::filesystem::create_directory(outside);
    write(outside / "user.json", {{"preserve", true}});
    std::error_code link_error;
    const auto link = portable / "probe_history" / "external-link";
    std::filesystem::create_directory_symlink(outside, link, link_error);
    if (!link_error) {
        EXPECT_THROW(store.clear_inventory(), std::exception);
        EXPECT_EQ(store.inventory().devices.size(), 2u);
        EXPECT_TRUE(std::filesystem::exists(outside / "user.json"));
        std::filesystem::remove(link);
    }
    request.confirmed = true;
    auto consumer = controller.subscribe();
    consumer.poll();
    ASSERT_TRUE(controller.scanner_action(request).ok);
    finish(controller);
    EXPECT_TRUE(controller.snapshot().endpoints.empty());
    EXPECT_EQ(controller.snapshot().scanner.result, "completed");
    EXPECT_FALSE(std::filesystem::exists(portable / "devices"));
    EXPECT_FALSE(std::filesystem::exists(local / "sessions"));
    EXPECT_FALSE(std::filesystem::exists(local / "device_bindings.json"));
    EXPECT_FALSE(std::filesystem::exists(local / "device_bindings.json.bak"));
    EXPECT_FALSE(std::filesystem::exists(portable / "probe_history"));
    EXPECT_TRUE(std::filesystem::exists(portable / "unrelated.json"));
    EXPECT_TRUE(std::filesystem::exists(outside / "user.json"));
    EXPECT_EQ(audiohole::profile(controller.snapshot(), profile_id)->holes.front().forward.source,
              id);
    ConfigurationController restarted(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service());
    finish(restarted);
    EXPECT_TRUE(restarted.snapshot().endpoints.empty());
    ASSERT_TRUE(audiohole::profile(restarted.snapshot(), profile_id));
    EXPECT_EQ(audiohole::profile(restarted.snapshot(), profile_id)->holes.front().forward.source,
              id);
    const auto updates = consumer.poll();
    EXPECT_TRUE(std::any_of(updates.results.begin(), updates.results.end(), [](const auto& result) {
        return result.command == "scanner.action" && result.outcome == Outcome::Completed;
    }));
}
TEST_F(ScannerIntegration, InvalidScanAdmissionDoesNotCreateAnOperationalWarning) {
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    ScanOptions options;
    options.consent = true;
    options.attempts = 0;
    controller.scan(true, true, ScanScope::Full, {}, options);
    EXPECT_FALSE(controller.snapshot().scanning);
    EXPECT_FALSE(controller.snapshot().scanner.busy);
    EXPECT_TRUE(std::any_of(controller.snapshot().events.begin(),
                            controller.snapshot().events.end(), [](const auto& event) {
                                return event.type == "scanner-action-failed" &&
                                       event.severity == Severity::Warning;
                            }));
    EXPECT_TRUE(std::none_of(controller.snapshot().events.begin(),
                             controller.snapshot().events.end(), [](const auto& event) {
                                 return event.severity == Severity::Error ||
                                        event.type == "scan-failed";
                             }));
}
TEST_F(ScannerIntegration, UnlimitedBatchDoesNotExpireTheControllerAndRemainsCancellable) {
    evidence->block = true;
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<TestJsonPersistence>(portable / "configuration.json"), service()));
    finish(controller);
    auto consumer = controller.subscribe();
    consumer.poll();
    ScanOptions options;
    options.consent = true;
    options.system_timeout_ms = 0;
    controller.scan(true, true, ScanScope::Full, {}, options);
    ASSERT_TRUE(controller.snapshot().scanner.busy);
    for (int i = 0; i < 20; ++i)
        controller.advance(30000);
    const auto before_cancel = consumer.poll();
    EXPECT_TRUE(
        std::none_of(before_cancel.results.begin(), before_cancel.results.end(),
                     [](const auto& result) { return result.outcome == Outcome::TimedOut; }));
    controller.scan(false);
    finish(controller);
    EXPECT_EQ(controller.snapshot().scanner.result, "interrupted");
    const auto canceled = consumer.poll();
    EXPECT_TRUE(
        std::any_of(canceled.results.begin(), canceled.results.end(),
                    [](const auto& result) { return result.outcome == Outcome::Cancelled; }));
}
class CountingWriter final : public scan::AtomicWriter {
  public:
    unsigned writes = 0;
    unsigned summaries = 0;
    bool delay_next_write = false;
    std::shared_ptr<scan::AtomicWriter> actual = scan::make_atomic_writer();
    void write(const std::filesystem::path& path, const std::string& data,
               const std::optional<std::string>& expected) override {
        ++writes;
        if (path.filename() == "recording_devices.json" ||
            path.filename() == "playback_devices.json")
            ++summaries;
        if (delay_next_write) {
            delay_next_write = false;
            std::this_thread::sleep_for(800ms);
        }
        actual->write(path, data, expected);
    }
};
class ManyStages final : public scan::ProbeRunner {
  public:
    unsigned calls = 0;
    std::function<void()> edit;
    Json execute(const scan::ProbeJob&, std::stop_token,
                 const scan::ProbeStageSink& stage) override {
        ++calls;
        for (int i = 0; i < 80; ++i) {
            stage({{"api", "fixture-stage"}, {"phase", "end"}, {"number", i}});
            if (i == 40 && edit)
                edit();
        }
        return {{"classification", "inconclusive"}, {"cleanup", "completed"}};
    }
};
TEST_F(ScannerIntegration,
       StageJournalAvoidsFullRewritesAndRetainsExternalEditsAndTerminalEvidence) {
    auto writer = std::make_shared<CountingWriter>();
    scan::DeviceStore counted(portable, local, writer);
    ASSERT_EQ(counted.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto record = counted.inventory().devices.front();
    const auto target = counted.probe_targets().front();
    writer->writes = 0;
    ManyStages runner;
    runner.edit = [&] {
        auto edited = Json::parse(bytes(record.path));
        EXPECT_EQ(edited["probed_capabilities"]["test_attempts"][0]["classification"],
                  "inconclusive");
        EXPECT_EQ(edited["probed_capabilities"]["test_attempts"][0]["status"], "running");
        edited["notes"] = "External edit during probing";
        write(record.path, edited);
    };
    unsigned journaled = 0;
    scan::DeepLimits limits;
    limits.max_attempts = 1;
    const auto report =
        scan::run_deep_scan(counted, runner, target, limits, true, {}, [&](const auto& event) {
            if (event.phase == "deep_stage")
                ++journaled;
        });
    EXPECT_EQ(report.attempted, 1u);
    EXPECT_EQ(journaled, 80u);
    EXPECT_LE(writer->writes, 8u); // Work is bounded per attempt, not per API event.
    const auto saved = Json::parse(bytes(record.path));
    EXPECT_EQ(saved["notes"], "External edit during probing");
    EXPECT_EQ(saved["probed_capabilities"]["test_attempts"][0]["stages"].size(), 80u);
    EXPECT_EQ(report.attempts[0]["stage_count"], 80);
    EXPECT_TRUE(report.attempts[0].contains("initial_checkpoint_ms"));
    EXPECT_TRUE(report.attempts[0].contains("worker_wall_ms"));
}
} // namespace audiohole

namespace audiohole {
TEST_F(ScannerIntegration, CheckpointLatencyDoesNotLaunchWorkerAfterDeviceBudgetIsSpent) {
    auto writer = std::make_shared<CountingWriter>();
    scan::DeviceStore counted(portable, local, writer);
    ASSERT_EQ(counted.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto target = counted.probe_targets().front();
    writer->delay_next_write = true;
    ManyStages runner;
    scan::DeepLimits limits{1, 250, 2000, 2000, "shared"};
    const auto report = scan::run_deep_scan(counted, runner, target, limits, true);
    EXPECT_EQ(runner.calls, 0u);
    EXPECT_EQ(report.result, "partially_completed");
    ASSERT_EQ(report.attempts.size(), 1u);
    EXPECT_EQ(report.attempts[0]["classification"], "inconclusive");
    EXPECT_NE(report.attempts[0]["reason"].get<std::string>().find("budget exhausted"),
              std::string::npos);
}
TEST_F(ScannerIntegration, RoutingChoicesUseExactModeSpecificDescriptorsAndPreserveUnknownLayout) {
    for (auto& e : evidence->endpoints) {
        auto format = e.reported_capabilities.at("default_mix_format");
        format["channel_mask"] = Json{{"status", "unknown"}};
        e.reported_capabilities["format_queries"] = Json::array({{{"configuration", format},
                                                                  {"mode", "shared"},
                                                                  {"support", "supported"},
                                                                  {"hresult", "0x00000000"}}});
    }
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    auto worker = service();
    const auto loaded = finish(*worker, worker->action(ScannerActionRequest{}));
    Snapshot snapshot;
    snapshot.operating_mode = OperatingMode::Test;
    snapshot.endpoints = loaded.devices;
    ASSERT_EQ(snapshot.endpoints.size(), 2u);
    Direction d;
    for (const auto& e : snapshot.endpoints)
        (e.capture ? d.source : d.destination) = e.id;
    auto options = recommend(snapshot, d);
    ASSERT_EQ(options.compatible.size(), 1u);
    ASSERT_TRUE(options.compatible[0].wave);
    EXPECT_FALSE(options.compatible[0].wave->channel_mask);
    Profile p{"profile", "Microphone monitoring", "User note", {Hole{}}};
    p.holes.front().id = "hole";
    p.holes.front().forward = d;
    PreparedProfile plan;
    ASSERT_TRUE(prepare_profile(snapshot, p, plan).ok);
    ASSERT_EQ(plan.paths.size(), 1u);
    EXPECT_EQ(plan.paths[0].mode, "Shared");
    EXPECT_EQ(plan.paths[0].format, options.compatible[0]);
    auto shared_preferred = snapshot;
    for (auto& e : shared_preferred.endpoints) {
        e.capabilities[0].mode = "Exclusive";
        auto shared = e.capabilities[0];
        shared.mode = "Shared";
        shared.format.rate = 96000;
        shared.format.wave->average_bytes_per_second *= 2;
        e.capabilities.push_back(shared);
    }
    EXPECT_EQ(recommend(shared_preferred, d).compatible.front().rate, 96000);
    PreparedProfile shared_plan;
    ASSERT_TRUE(prepare_profile(shared_preferred, p, shared_plan).ok);
    EXPECT_EQ(shared_plan.paths[0].mode, "Shared");
    const auto original = plan.paths;
    d.settings.preferences.common.mode = "Exclusive";
    d.settings.refresh_projection();
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty());
    p.holes.front().forward = d;
    p.holes.front().advanced_tuning = true;
    // Missing Exclusive observations do not prove rejection. Pass explicit
    // intent to runtime initialization without upgrading evidence to support.
    EXPECT_TRUE(prepare_profile(snapshot, p, plan).ok);
    EXPECT_TRUE(plan.paths[0].runtime_negotiation);
    EXPECT_EQ(plan.paths[0].mode, "Exclusive");
    const auto before_unavailable = plan.paths;
    snapshot.endpoints.front().availability = Availability::Unavailable;
    EXPECT_FALSE(prepare_profile(snapshot, p, plan).ok);
    EXPECT_EQ(plan.paths[0].mode, before_unavailable[0].mode);
    snapshot.endpoints.front().availability = Availability::Available;
    d.settings.preferences.common.mode.reset();
    d.settings.refresh_projection();
    snapshot.endpoints.back().capabilities[0].mode = "Exclusive";
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty()); // No union across modes.
    snapshot.endpoints.back().capabilities[0].mode = "Shared";
    snapshot.endpoints.back().capabilities[0].format.wave->channel_mask = 3;
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty()); // Unknown is not stereo.
    snapshot.endpoints.back().capabilities[0].format.wave->channel_mask.reset();
    snapshot.endpoints.back().capabilities[0].format.wave->valid_bits = 24;
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty()); // Not just rate/channels/sample type.
    snapshot.endpoints.back().capabilities[0].format.wave->valid_bits = 32;
    auto rejected = snapshot.endpoints.back().capabilities[0];
    rejected.support = FormatSupport::Unsupported;
    snapshot.endpoints.back().capabilities.push_back(rejected);
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty()); // Conflicting reports require rescan.
}
TEST_F(ScannerIntegration, ClosestMatchesAndMixFormatsNeverBecomeSupportedRequests) {
    for (auto& e : evidence->endpoints) {
        const auto format = e.reported_capabilities.at("default_mix_format");
        e.reported_capabilities["format_queries"] = Json::array({{{"configuration", format},
                                                                  {"mode", "shared"},
                                                                  {"support", "supported"},
                                                                  {"hresult", "0x00000001"},
                                                                  {"closest_match", format}},
                                                                 {{"configuration", format},
                                                                  {"mode", "exclusive"},
                                                                  {"support", "unsupported"},
                                                                  {"hresult", "0x88890008"}}});
    }
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    auto worker = service();
    Snapshot snapshot;
    snapshot.endpoints = finish(*worker, worker->action(ScannerActionRequest{})).devices;
    Direction d;
    for (const auto& e : snapshot.endpoints) {
        (e.capture ? d.source : d.destination) = e.id;
        EXPECT_TRUE(e.formats.empty()); // No flattened mode-independent capability claims.
        ASSERT_EQ(e.capabilities.size(), 2u);
        EXPECT_EQ(e.capabilities[0].support, FormatSupport::Unknown);
        EXPECT_EQ(e.capabilities[1].support, FormatSupport::Unsupported);
    }
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty());
}
TEST_F(ScannerIntegration, ScannerBackedProfileSurvivesRestartRenameHideAndUnavailableDevices) {
    for (auto& e : evidence->endpoints) {
        const auto format = e.reported_capabilities.at("default_mix_format");
        e.reported_capabilities["format_queries"] = Json::array({{{"configuration", format},
                                                                  {"mode", "shared"},
                                                                  {"support", "supported"},
                                                                  {"hresult", "0x00000000"}}});
    }
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto file = portable / "configuration.json";
    ConfigurationController controller(std::make_unique<TestJsonPersistence>(file), service());
    finish(controller);
    const auto id = controller.create_profile("Voice → headphones");
    ASSERT_FALSE(id.empty());
    auto p = *profile(controller.snapshot(), id);
    auto h = controller.make_hole();
    h.notes = "Keep my channel layout and identity";
    for (const auto& e : controller.snapshot().endpoints)
        (e.capture ? h.forward.source : h.forward.destination) = e.id;
    p.holes.push_back(h);
    ASSERT_TRUE(controller.save(p, false).ok);
    const auto saved = *profile(controller.snapshot(), id);
    EXPECT_EQ(saved.holes[0].forward.settings.preferences, Preferences{});
    for (const auto& endpoint : controller.snapshot().endpoints) {
        ASSERT_FALSE(endpoint.capabilities.empty());
        ASSERT_TRUE(endpoint.capabilities.front().format.wave);
    }
    EXPECT_EQ(saved.holes[0].forward.source_name, "Capture");
    const auto json = Json::parse(bytes(file));
    EXPECT_TRUE(json.at("devices").empty());
    EXPECT_EQ(json.at("schema_version"), 6);
    EXPECT_EQ(json.at("profiles")[0]["holes"][0]["forward"]["source"], h.forward.source);
    EXPECT_FALSE(json["profiles"][0]["holes"][0]["forward"]["settings"].contains("format"));
    EXPECT_EQ(bytes(file).find("endpoint_id"), std::string::npos);
    auto device = store.inventory().devices.front();
    store.set_hidden(device.record.at("logical_id"), true);
    std::filesystem::rename(device.path,
                            device.path.parent_path() / "Renamed outside AudioHole.json");
    evidence->endpoints.clear();
    ASSERT_EQ(store.reconcile(discovery({}), second).result, "completed");
    ConfigurationController restarted(std::make_unique<TestJsonPersistence>(file), service());
    finish(restarted);
    EXPECT_EQ(*profile(restarted.snapshot(), id), saved);
    EXPECT_EQ(configuration_devices(restarted.snapshot()).size(), 1u);
    auto with_legacy = restarted.snapshot();
    with_legacy.endpoints.push_back(Endpoint{});
    EXPECT_EQ(configuration_devices(with_legacy).size(),
              1u); // Legacy Test fixtures are not offered.
    PreparedProfile prepared;
    EXPECT_FALSE(prepare_profile(restarted.snapshot(), saved, prepared).ok);
    EXPECT_TRUE(restarted.save(saved, false).ok); // Missing devices do not prevent saving drafts.
    EXPECT_EQ(profile(restarted.snapshot(), id)->holes[0].forward.source, h.forward.source);
    EXPECT_EQ(profile(restarted.snapshot(), id)->holes[0].forward.destination,
              h.forward.destination);
}
TEST_F(ScannerIntegration, DeepSummariesRefreshOncePerDeviceWhileEachAttemptRemainsDurable) {
    auto writer = std::make_shared<CountingWriter>();
    scan::DeviceStore counted(portable, local, writer);
    ASSERT_EQ(counted.reconcile(discovery(evidence->endpoints), first).result, "completed");
    const auto target = counted.probe_targets().front();
    writer->writes = writer->summaries = 0;
    ManyStages runner;
    scan::DeepLimits limits;
    limits.max_attempts = 4;
    limits.mode = "both";
    const auto report = scan::run_deep_scan(counted, runner, target, limits, true);
    ASSERT_EQ(report.attempted, 4u);
    EXPECT_EQ(writer->summaries, 2u);
    EXPECT_EQ(writer->writes, 4u * report.attempted + 2u);
    for (const auto& attempt : report.attempts) {
        const auto history = portable / "probe_history" /
                             scan::utf8_path(attempt["attempt_id"].get<std::string>() + ".json");
        EXPECT_EQ(Json::parse(bytes(history))["stages"].size(), 80u);
    }
    const auto inventory = counted.inventory();
    const auto& device =
        *std::find_if(inventory.devices.begin(), inventory.devices.end(),
                      [&](const auto& d) { return d.record["logical_id"] == target.logical_id; });
    const auto summary = Json::parse(bytes(portable / "devices" / "recording_devices.json"));
    const auto row =
        std::find_if(summary["devices"].begin(), summary["devices"].end(),
                     [&](const auto& d) { return d["logical_id"] == target.logical_id; });
    ASSERT_NE(row, summary["devices"].end());
    EXPECT_EQ((*row)["probed_capabilities"], device.record["probed_capabilities"]);
}
TEST_F(ScannerIntegration, PackedAndContainer24BitFormatsRemainDistinctAndReadable) {
    for (auto& e : evidence->endpoints) {
        auto format = e.reported_capabilities["default_mix_format"];
        format["sample_format"] = "pcm_s24_in_32";
        format["valid_bits"] = 24;
        format["subformat_guid"] = "{00000001-0000-0010-8000-00AA00389B71}";
        e.reported_capabilities["format_queries"] = Json::array({{{"configuration", format},
                                                                  {"mode", "shared"},
                                                                  {"support", "supported"},
                                                                  {"hresult", "0x00000000"}}});
    }
    ASSERT_EQ(store.reconcile(discovery(evidence->endpoints), first).result, "completed");
    auto worker = service();
    Snapshot snapshot;
    snapshot.endpoints = finish(*worker, worker->action(ScannerActionRequest{})).devices;
    Direction d;
    for (const auto& e : snapshot.endpoints) {
        (e.capture ? d.source : d.destination) = e.id;
        EXPECT_NE(e.inspection->views[2].find("24-bit integer PCM in a 32-bit container"),
                  std::string::npos);
    }
    const auto choices = recommend(snapshot, d);
    ASSERT_EQ(choices.compatible.size(), 1u);
    EXPECT_EQ(choices.compatible[0].representation, "pcm_s24_in_32");
    EXPECT_EQ(choices.compatible[0].wave->valid_bits, 24);
    EXPECT_EQ(choices.compatible[0].wave->container_bits, 32);
}
} // namespace audiohole
