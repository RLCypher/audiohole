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
