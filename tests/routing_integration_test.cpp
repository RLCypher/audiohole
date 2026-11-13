#include "model/application_controller.h"
#include "support/engine_fixture.h"
#include <gtest/gtest.h>

using namespace audiohole;
namespace {
template <class Predicate> void until(Controller& controller, Predicate done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done()) {
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
        controller.advance(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
struct RoutingIntegration : testing::Test {
    std::shared_ptr<testing_engine::Control> control = std::make_shared<testing_engine::Control>();
    testing_engine::Store* store = nullptr;
    std::unique_ptr<ConfigurationController> controller;
    void SetUp() override {
        auto saved = std::make_unique<testing_engine::Store>();
        store = saved.get();
        controller = std::make_unique<ConfigurationController>(std::move(saved), nullptr, nullptr,
                                                               testing_engine::service(control));
        until(*controller, [&] { return controller->snapshot().capabilities.routing; });
    }
    void TearDown() override {
        control->block_open = false;
        control->block_cleanup = false;
        controller->stop();
        until(*controller, [&] {
            return controller->snapshot().state == State::Stopped &&
                   !controller->snapshot().routing_pending;
        });
    }
};
TEST_F(RoutingIntegration, ActualRuntimeStartAndStopPreserveSavedIntentAndPublishReceipts) {
    const auto saved = store->records.profiles;
    control->block_open = true;
    ASSERT_TRUE(controller->activate("engine-profile").ok);
    EXPECT_EQ(controller->snapshot().state, State::Starting);
    EXPECT_FALSE(controller->snapshot().state == State::Running);
    until(*controller, [&] { return control->opens > 0; });
    EXPECT_FALSE(controller->activate("engine-profile").ok);
    EXPECT_FALSE(controller->remove_profile("engine-profile").ok);
    control->block_open = false;
    until(*controller, [&] { return controller->snapshot().state == State::Running; });
    ASSERT_TRUE(controller->snapshot().routing_status);
    EXPECT_EQ(controller->snapshot().paths.size(), 2u);
    auto text = routing_diagnostics(*controller->snapshot().routing_status, &saved[0]);
    EXPECT_NE(text.find("48000 Hz / 2 ch / float32"), std::string::npos);
    EXPECT_NE(text.find("Allocated buffer: 480 frames"), std::string::npos);
    EXPECT_NE(text.find("Transferred frames"), std::string::npos);
    EXPECT_EQ(store->saves, 0);
    EXPECT_EQ(store->records.profiles, saved);
    control->block_cleanup = true;
    controller->stop();
    until(*controller, [&] { return control->cleaning.load(); });
    EXPECT_EQ(controller->snapshot().state, State::Stopping);
    EXPECT_FALSE(controller->snapshot().armed);
    EXPECT_FALSE(controller->activate("engine-profile").ok);
    control->block_cleanup = false;
    until(*controller, [&] { return controller->snapshot().state == State::Stopped; });
    EXPECT_EQ(control->releases, 2);
}
TEST_F(RoutingIntegration, StopSupersedesQueuedStartAndLateProgress) {
    control->block_open = true;
    ASSERT_TRUE(controller->activate("engine-profile").ok);
    controller->stop();
    control->block_open = false;
    until(*controller, [&] { return controller->snapshot().state == State::Stopped; });
    for (int tick = 0; tick < 20; ++tick)
        controller->advance(10);
    EXPECT_FALSE(controller->snapshot().armed);
    EXPECT_EQ(controller->snapshot().state, State::Stopped);
}
TEST_F(RoutingIntegration, StartupDeadlineIsCancellableAndImmediateStillWaitsForAdmission) {
    ASSERT_TRUE(controller->schedule("engine-profile", 1000).ok);
    controller->advance(500);
    EXPECT_EQ(controller->snapshot().pending_ms, 500);
    EXPECT_EQ(control->opens, 0);
    controller->stop();
    until(*controller, [&] { return controller->snapshot().state == State::Stopped; });
    controller->advance(30000);
    EXPECT_EQ(control->opens, 0);
    EXPECT_TRUE(controller->snapshot().pending_profile.empty());
    ASSERT_TRUE(controller->schedule("engine-profile", 0).ok);
    until(*controller, [&] { return controller->snapshot().state == State::Running; });
    EXPECT_TRUE(controller->snapshot().pending_profile.empty());
}
TEST_F(RoutingIntegration, SavedEditsCannotRewriteTheLiveAssignmentsOrLabels) {
    ASSERT_TRUE(controller->activate("engine-profile").ok);
    until(*controller, [&] { return controller->snapshot().state == State::Running; });
    auto edited = store->records.profiles[0];
    edited.display_name = "A saved edit for next time";
    edited.holes[0].forward.destination = "another-render-endpoint";
    ASSERT_TRUE(controller->save(edited, false).ok);
    ASSERT_TRUE(controller->snapshot().routing_intent);
    EXPECT_EQ(controller->snapshot().routing_intent->display_name, "Engine contract fixture");
    EXPECT_EQ(controller->snapshot().routing_intent->holes[0].forward.destination,
              "render-forward");
    EXPECT_EQ(controller->snapshot().profiles[0].holes[0].forward.destination,
              "another-render-endpoint");
    EXPECT_EQ(controller->snapshot().state, State::Running);
}
TEST_F(RoutingIntegration, AmbiguousBindingsDoNotOpenAndHealthyDirectionSurvivesSiblingFailure) {
    control->fail_return = true;
    ASSERT_TRUE(controller->activate("engine-profile").ok);
    until(*controller, [&] { return controller->snapshot().state == State::Degraded; });
    EXPECT_EQ(controller->snapshot().paths[0].state, State::Running);
    EXPECT_EQ(controller->snapshot().paths[1].state, State::Failed);
    EXPECT_NE(controller->snapshot().paths[1].detail.find("TEST-EXCLUSIVE"), std::string::npos);
    controller->stop();
    until(*controller, [&] { return controller->snapshot().state == State::Stopped; });
    Snapshot s;
    s.endpoints = store->records.devices;
    s.endpoints[0].availability = Availability::Ambiguous;
    const auto bindings = routing_bindings(s);
    EXPECT_EQ(bindings.endpoints[0].state, engine::BindingState::Ambiguous);
    auto p = routing_profile(store->records.profiles[0]);
    ASSERT_TRUE(p.ok());
    auto prepared = engine::prepare_profile(*p.value);
    ASSERT_TRUE(prepared.ok());
    EXPECT_FALSE(engine::bind_route(prepared.value->routes[0], bindings).ok());
}
TEST_F(RoutingIntegration, ForcedModeFailureAndUnavailableBackendNeverSilentlyFallback) {
    auto p = store->records.profiles[0];
    p.holes[0].forward.advanced_tuning = true;
    p.holes[0].forward.settings.preferences.common.mode = "Exclusive";
    p.holes[0].forward.exclusive_mode_preferred = true;
    p.holes[0].duplex = false;
    control->refuse_exclusive = true;
    ASSERT_TRUE(controller->save(p, false).ok);
    ASSERT_TRUE(controller->activate(p.id).ok);
    until(*controller, [&] { return controller->snapshot().state == State::Failed; });
    EXPECT_EQ(control->opens, 1);
    EXPECT_EQ(store->records.profiles[0].holes[0].forward.settings.preferences.common.mode,
              "Exclusive");
    controller->stop();
    until(*controller, [&] { return controller->snapshot().state == State::Stopped; });
    p.holes[0].routing_engine = "miniaudio";
    ASSERT_TRUE(controller->save(p, false).ok);
    ASSERT_TRUE(controller->activate(p.id).ok); // Queue admission only.
    until(*controller, [&] { return controller->snapshot().state == State::Failed; });
    EXPECT_FALSE(controller->snapshot().armed);
    EXPECT_EQ(control->opens, 1);
}
TEST_F(RoutingIntegration, IndependentPreferencesAndExactBindingsMatchTheEngineContract) {
    auto p = store->records.profiles[0];
    auto& d = p.holes[0].forward;
    d.advanced_tuning = true;
    d.exclusive_mode_preferred = true;
    d.settings.preferences.common.sample_rate_hz = 48000;
    d.settings.preferences.source.channel_count = 4;
    d.settings.preferences.destination.mode = "Shared";
    d.settings.preferences.channel_map = {{2, 0}, {3, 1}};
    d.settings.preferences.preferred_clock_device = "capture";
    auto projected = routing_profile(p);
    ASSERT_TRUE(projected.ok());
    const auto& forward = projected.value->holes[0].forward;
    EXPECT_TRUE(forward.advanced_tuning);
    EXPECT_TRUE(forward.exclusive_mode_preferred);
    EXPECT_EQ(forward.preferences.source.channel_count, 4);
    EXPECT_EQ(forward.preferences.destination.mode, engine::AccessMode::Shared);
    EXPECT_EQ(forward.preferences.channel_map[0].source, 2);
    EXPECT_EQ(forward.preferences.preferred_clock_device, "capture");
    EXPECT_FALSE(projected.value->holes[0].reverse.advanced_tuning);
    Snapshot s;
    s.endpoints = store->records.devices;
    s.endpoints[0].display_name = "Renamed microphone";
    EXPECT_EQ(routing_bindings(s).endpoints[0].endpoint_id, "local-capture-forward");
    s.endpoints[0].inspection.reset();
    EXPECT_EQ(routing_bindings(s).endpoints[0].state, engine::BindingState::Unknown);
}
TEST(RoutingService, FactoryFailureStillAllowsConfirmedStopAndShutdown) {
    auto routing = make_routing_service([]() -> std::unique_ptr<engine::Runtime> {
        throw std::runtime_error("Factory failure fixture");
    });
    ApplicationController controller(std::make_unique<ConfigurationController>(
        std::make_unique<testing_engine::Store>(), nullptr, nullptr, std::move(routing)));
    until(controller, [&] { return controller.snapshot().state == State::Failed; });
    controller.stop();
    until(controller, [&] {
        return controller.snapshot().state == State::Stopped &&
               !controller.snapshot().routing_pending;
    });
    EXPECT_FALSE(controller.snapshot().armed);
}
TEST(RoutingService, ApplicationMailboxWaitsForAdmissionAndReportsFailureRatherThanCancellation) {
    auto control = std::make_shared<testing_engine::Control>();
    auto store = std::make_unique<testing_engine::Store>();
    store->records.profiles[0].holes[0].routing_engine =
        "miniaudio"; // Not registered by this fixture.
    ApplicationController app(std::make_unique<ConfigurationController>(
        std::move(store), nullptr, nullptr, testing_engine::service(control)));
    until(app, [&] { return app.snapshot().capabilities.routing; });
    auto mailbox = app.subscribe();
    mailbox.poll();
    const auto request = app.submit(StartRouting{"engine-profile"});
    ASSERT_EQ(request.outcome, Outcome::Accepted);
    EXPECT_TRUE(app.snapshot().routing_pending);
    app.advance(0);
    auto early = mailbox.poll();
    bool failed = false;
    for (const auto& response : early.results)
        if (response.request_id == request.request_id) {
            EXPECT_TRUE(response.outcome == Outcome::Accepted ||
                        response.outcome == Outcome::Failed);
            failed |= response.outcome == Outcome::Failed;
        }
    until(app, [&] { return app.snapshot().state == State::Failed; });
    ASSERT_FALSE(app.snapshot().routing_admission_issues.empty());
    EXPECT_EQ(app.snapshot().routing_admission_issues.front().code,
              engine::ErrorCode::BackendUnavailable);
    const auto terminal = mailbox.poll();
    for (const auto& response : terminal.results)
        if (response.request_id == request.request_id) {
            EXPECT_EQ(response.outcome, Outcome::Failed);
            failed = true;
        }
    EXPECT_TRUE(failed);
    EXPECT_EQ(control->opens, 0);
    app.stop();
    until(app, [&] { return app.snapshot().state == State::Stopped; });
}
} // namespace
