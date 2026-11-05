#include "audiohole/scoped_scan.hpp"
#include "model/application_controller.h"
#include "model/configuration_controller.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

namespace audiohole {
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
} // namespace audiohole
