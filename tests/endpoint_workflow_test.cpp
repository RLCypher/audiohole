#include "model/application_controller.h"
#include "model/build_preferences.h"
#include "model/configuration_controller.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace audiohole;
namespace {
class MemoryStore final : public ConfigurationPersistence {
  public:
    ConfigurationRecords records;
    int saves = 0;
    Result load(ConfigurationRecords& out) override {
        out = records;
        return {true, "Loaded"};
    }
    Result save(const ConfigurationRecords& in) override {
        records = in;
        ++saves;
        return {true, "Saved"};
    }
};
class Checks final : public DiagnosticService {
  public:
    DiagnosticReading state;
    std::string binding;
    int cancelled = 0, launches = 0;
    bool start(const std::string& logical, const std::string& local, bool capture) override {
        if (state.active)
            return false;
        ++launches;
        binding = local;
        state = {true, capture, {}, logical, "Opening"};
        return true;
    }
    void cancel() override {
        ++cancelled;
    }
    DiagnosticReading reading() const override {
        return state;
    }
    void finish() {
        state.active = false;
        state.peak.reset();
        state.detail = "Released";
    }
};
Endpoint device(const char* id, bool capture) {
    Endpoint e;
    e.id = id;
    e.display_name = id;
    e.capture = capture;
    e.availability = Availability::Available;
    auto info = std::make_shared<DeviceInspection>();
    info->diagnostic_endpoint_id = std::string("local-") + id;
    e.inspection = info;
    return e;
}
struct EndpointChecks : testing::Test {
    MemoryStore* store = nullptr;
    Checks* checks = nullptr;
    std::unique_ptr<ConfigurationController> controller;
    void SetUp() override {
        auto persistence = std::make_unique<MemoryStore>();
        store = persistence.get();
        store->records.devices = {device("capture", true), device("playback", false)};
        auto worker = std::make_unique<Checks>();
        checks = worker.get();
        controller = std::make_unique<ConfigurationController>(std::move(persistence), nullptr,
                                                               std::move(worker));
    }
};
TEST_F(EndpointChecks, ExactBindingReadingsAndCleanupAreSeparateFromConfiguration) {
    EXPECT_FALSE(controller->check_level("playback").ok);
    EXPECT_FALSE(controller->beep("capture").ok);
    EXPECT_FALSE(controller->beep("unknown").ok);
    ASSERT_TRUE(controller->check_level("capture").ok);
    EXPECT_EQ(checks->binding, "local-capture");
    EXPECT_FALSE(controller->snapshot().input_peak);
    EXPECT_FALSE(controller->beep("playback").ok);
    checks->state.peak = .72f;
    controller->advance(100);
    EXPECT_EQ(controller->snapshot().monitor_source, "capture");
    EXPECT_FLOAT_EQ(*controller->snapshot().input_peak, .72f);
    controller->monitor(false);
    EXPECT_GT(checks->cancelled, 0);
    EXPECT_TRUE(controller->snapshot().monitoring); // Cleanup has not completed yet.
    EXPECT_FALSE(controller->beep("playback").ok);
    checks->finish();
    controller->advance(100);
    EXPECT_FALSE(controller->snapshot().monitoring);
    EXPECT_FALSE(controller->snapshot().input_peak);
    ASSERT_TRUE(controller->beep("playback").ok);
    EXPECT_EQ(checks->binding, "local-playback");
    controller->stop();
    EXPECT_GT(checks->cancelled, 1);
    checks->finish();
    controller->advance(100);
    EXPECT_TRUE(controller->snapshot().beep_destination.empty());
    EXPECT_EQ(store->saves, 0);
    EXPECT_FALSE(controller->snapshot().armed);
}
TEST_F(EndpointChecks, SubscribersReceiveMeasuredLevelAndReleasedState) {
    ApplicationController app(std::move(controller));
    auto subscription = app.subscribe();
    subscription.poll();
    ASSERT_TRUE(app.check_level("capture").ok);
    auto update = subscription.poll();
    ASSERT_TRUE(update.state);
    EXPECT_TRUE(update.state->monitoring);
    checks->state.peak = .5f;
    app.advance(100);
    update = subscription.poll();
    ASSERT_TRUE(update.state);
    EXPECT_EQ(update.state->input_peak, .5f);
    checks->finish();
    app.advance(100);
    update = subscription.poll();
    ASSERT_TRUE(update.state);
    EXPECT_FALSE(update.state->monitoring);
    EXPECT_FALSE(update.state->input_peak);
}
TEST_F(EndpointChecks, AsynchronousFailureIsPublishedAndClearedForTheNextCheck) {
    ApplicationController app(std::move(controller));
    auto subscription = app.subscribe();
    subscription.poll();
    ASSERT_TRUE(app.beep("playback").ok);
    checks->finish();
    checks->state.failed = true;
    checks->state.detail = "The playback endpoint was disconnected.";
    app.advance(100);
    const auto update = subscription.poll();
    ASSERT_TRUE(update.state);
    EXPECT_TRUE(update.state->beep_destination.empty());
    EXPECT_TRUE(update.state->diagnostic_failed);
    EXPECT_EQ(update.state->diagnostic_detail, checks->state.detail);
    ASSERT_TRUE(app.check_level("capture").ok);
    EXPECT_FALSE(app.snapshot().diagnostic_failed);
    EXPECT_EQ(store->saves, 0);
}
TEST_F(EndpointChecks, ChangedBindingOrAvailabilityCancelsWithoutRetargeting) {
    ASSERT_TRUE(controller->check_level("capture").ok);
    auto info =
        std::const_pointer_cast<DeviceInspection>(controller->snapshot().endpoints[0].inspection);
    info->diagnostic_endpoint_id = "new-local-capture";
    controller->advance(100);
    EXPECT_GT(checks->cancelled, 0);
    EXPECT_EQ(checks->binding, "local-capture");
    EXPECT_EQ(checks->launches, 1);
    checks->finish();
    controller->advance(100);
    ASSERT_TRUE(controller->check_level("capture").ok);
    EXPECT_EQ(checks->binding, "new-local-capture");
    EXPECT_EQ(store->saves, 0);
}
TEST(EndpointAssignments, OccupiedSlotsClearAndInactiveReturnHaveExactIdentity) {
    Hole h;
    h.duplex = true;
    h.forward.source = "capture-main";
    h.forward.destination = "render-main";
    h.reverse.source = "capture-return";
    h.reverse.destination = "render-return";
    EXPECT_TRUE(validate_endpoint_assignments(h).ok);
    EXPECT_FALSE(endpoint_used_elsewhere(h, false, true, "capture-main"));
    EXPECT_TRUE(endpoint_used_elsewhere(h, true, true, "capture-main"));
    EXPECT_FALSE(endpoint_used_elsewhere(h, true, true, ""));
    h.reverse.source = h.forward.source;
    EXPECT_FALSE(validate_endpoint_assignments(h).ok);
    h.reverse.source.clear();
    EXPECT_TRUE(validate_endpoint_assignments(h).ok);
    h.reverse.source = h.forward.source;
    h.duplex = false;
    EXPECT_TRUE(validate_endpoint_assignments(h).ok);
    EXPECT_FALSE(endpoint_used_elsewhere(h, false, true, "capture-main"));
}
TEST(EndpointAssignments, SaveRejectsDuplicateWithinHoleButAllowsSeparateHoles) {
    SimulatedController c;
    auto p = *profile(c.snapshot(), "profile-duplex");
    const auto original = p;
    p.holes[0].reverse.source = p.holes[0].forward.source;
    EXPECT_FALSE(c.save(p, false).ok);
    EXPECT_EQ(*profile(c.snapshot(), p.id), original);
    p = original;
    p.holes.push_back(p.holes.front());
    p.holes.back().id = "second-hole";
    EXPECT_TRUE(c.save(p, false).ok);
}
TEST(RoutingPolicy, WaitingIsExplicitlyStartedAndStopDisarmsRecovery) {
    SimulatedController c;
    auto p = *profile(c.snapshot(), "profile-simplex");
    auto source = *endpoint(c.snapshot(), p.holes[0].forward.source);
    source.availability = Availability::Unavailable;
    ASSERT_TRUE(c.update_endpoint(source).ok);
    c.wait_for_devices(false);
    EXPECT_FALSE(c.activate(p.id).ok);
    EXPECT_FALSE(c.snapshot().armed);
    c.wait_for_devices(true);
    ASSERT_TRUE(c.activate(p.id).ok);
    c.advance(500);
    EXPECT_EQ(c.snapshot().state, State::Ready);
    c.stop();
    source.availability = Availability::Available;
    c.update_endpoint(source);
    c.advance(500);
    EXPECT_EQ(c.snapshot().state, State::Stopped);
    EXPECT_FALSE(c.snapshot().armed);
}
TEST(RoutingPolicy, FailedAutomaticSwitchKeepsWorkingProfileUntilWaitingReplacementConfirmed) {
    SimulatedController c;
    ASSERT_TRUE(c.activate("profile-simplex").ok);
    c.advance(500);
    auto replacement = *profile(c.snapshot(), "profile-duplex");
    auto source = *endpoint(c.snapshot(), replacement.holes[0].reverse.source);
    source.availability = Availability::Unavailable;
    c.update_endpoint(source);
    EXPECT_FALSE(c.activate(replacement.id).ok);
    EXPECT_EQ(c.snapshot().active_profile, "profile-simplex");
    ASSERT_TRUE(c.activate(replacement.id, true).ok);
    EXPECT_EQ(c.snapshot().active_profile, replacement.id);
}
TEST(RoutingPolicy, PreferencesRoundTripWithConservativeDefaultsAndStrictTypes) {
    const auto root = std::filesystem::temp_directory_path() /
                      ("audiohole-policy-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } cleanup{root};
    BuildPreferences prefs(root);
    ASSERT_TRUE(prefs.load().ok);
    EXPECT_TRUE(prefs.values().automatically_wait_for_devices);
    EXPECT_FALSE(prefs.values().activate_selected_profile);
    auto next = prefs.values();
    next.automatically_wait_for_devices = false;
    next.activate_selected_profile = true;
    ASSERT_TRUE(prefs.update(next).ok);
    BuildPreferences restarted(root);
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_EQ(restarted.values(), next);
    auto file = root / "preferences.json";
    std::ifstream in(file);
    auto json = nlohmann::json::parse(in);
    in.close();
    json["activate_selected_profile"] = "true";
    std::ofstream(file) << json.dump();
    BuildPreferences malformed(root);
    EXPECT_FALSE(malformed.load().ok);
}
TEST(CaptureCheck, SimulatedSourceIsExplicitAndDisconnectCancelsIt) {
    SimulatedController c;
    EXPECT_FALSE(c.check_level("vr-out").ok);
    ASSERT_TRUE(c.check_level("mixer-in").ok);
    EXPECT_EQ(c.snapshot().monitor_source, "mixer-in");
    EXPECT_TRUE(c.snapshot().input_peak);
    auto source = *endpoint(c.snapshot(), "mixer-in");
    source.availability = Availability::Unavailable;
    c.update_endpoint(source);
    EXPECT_FALSE(c.snapshot().monitoring);
    EXPECT_FALSE(c.snapshot().input_peak);
}
} // namespace
