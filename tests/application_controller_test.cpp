#include "model/application_controller.h"
#include "model/configuration_controller.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <thread>
using namespace audiohole;
namespace {
ApplicationController preview() {
    return ApplicationController(std::make_unique<SimulatedController>());
}
TEST(Communication, CorrelatesAcceptanceAndCompletionWithoutClaimingHardware) {
    auto controller = preview();
    auto consumer = controller.subscribe();
    ASSERT_NE(consumer.poll().state, nullptr);
    const auto accepted = controller.submit(StartRouting{"profile-simplex"});
    ASSERT_EQ(accepted.outcome, Outcome::Accepted);
    EXPECT_TRUE(accepted.simulated);
    EXPECT_EQ(controller.snapshot().state, State::Starting);
    controller.advance(300);
    const auto batch = consumer.poll();
    ASSERT_EQ(batch.results.size(), 2u);
    EXPECT_EQ(batch.results[0].request_id, accepted.request_id);
    EXPECT_EQ(batch.results[1].request_id, accepted.request_id);
    EXPECT_EQ(batch.results[1].outcome, Outcome::Completed);
    EXPECT_EQ(batch.state->state, State::Running);
    EXPECT_EQ(presentation_baseline(*batch.state).expression, "idle");
    ASSERT_TRUE(batch.telemetry.has_value());
    EXPECT_EQ(batch.telemetry->latency_ms.status, InformationState::Unavailable);
    EXPECT_FALSE(batch.telemetry->latency_ms.value.has_value());
    EXPECT_FALSE(batch.telemetry->applied_buffer_ms.value.has_value());
}
TEST(Communication, RejectsUnsupportedBufferAndMissingProfileWithoutChangingConfiguration) {
    auto controller = preview();
    const auto before = controller.snapshot().revision;
    const auto buffer = controller.submit(UpdateBuffer{"hole-1", false, 4});
    EXPECT_EQ(buffer.outcome, Outcome::Rejected);
    EXPECT_FALSE(buffer.result.ok);
    EXPECT_EQ(controller.snapshot().revision, before);
    const auto missing = controller.submit(LoadProfile{"missing"});
    EXPECT_EQ(missing.outcome, Outcome::Rejected);
    EXPECT_GT(missing.request_id, buffer.request_id);
    const auto loaded = controller.submit(LoadProfile{"profile-simplex"});
    EXPECT_EQ(loaded.outcome, Outcome::Completed);
    ASSERT_TRUE(loaded.loaded_profile.has_value());
    EXPECT_EQ(loaded.loaded_profile->id, "profile-simplex");
    EXPECT_EQ(controller.snapshot().state, State::Stopped);
}
TEST(Communication, MultipleConsumersHaveIndependentBoundedMailboxesAndLifecycle) {
    auto controller = preview();
    auto gui = controller.subscribe();
    auto tray = controller.subscribe({false, false, true});
    gui.poll();
    tray.poll();
    for (int index = 0; index < 150; ++index)
        controller.submit(RequestStatus{});
    const auto batch = gui.poll();
    EXPECT_TRUE(batch.gap);
    ASSERT_NE(batch.state, nullptr);
    EXPECT_EQ(batch.results.size(), 128u);
    EXPECT_TRUE(tray.poll().results.empty());
    gui.reset();
    EXPECT_TRUE(gui.poll().results.empty());
    // Repeated subscribe/unsubscribe does not leak the sixteen available slots.
    for (int index = 0; index < 50; ++index) {
        auto temporary = controller.subscribe();
        EXPECT_NE(temporary.poll().state, nullptr);
    }
    auto late = controller.subscribe();
    EXPECT_EQ(late.poll().state->state, controller.snapshot().state);
}
TEST(Communication, BoundedNonRealtimePostingRequiresControlThreadDrain) {
    auto controller = preview();
    auto consumer = controller.subscribe();
    consumer.poll();
    std::uint64_t first_id = 0;
    std::thread producer([&] {
        EXPECT_THROW(controller.snapshot(), std::logic_error);
        EXPECT_THROW(controller.submit(RequestStatus{}), std::logic_error);
        for (int index = 0; index < 64; ++index) {
            const auto receipt = controller.post(RequestStatus{});
            EXPECT_TRUE(receipt.accepted);
            if (index == 0)
                first_id = receipt.request_id;
        }
        EXPECT_FALSE(controller.post(RequestStatus{}).accepted);
    });
    producer.join();
    EXPECT_TRUE(consumer.poll().results.empty());
    controller.advance(0);
    const auto completed = consumer.poll();
    ASSERT_EQ(completed.results.size(), 64u);
    EXPECT_EQ(completed.results.front().request_id, first_id);
    EXPECT_EQ(completed.results.front().outcome, Outcome::Completed);
}
TEST(Communication, StopCancelsAcceptedActivationAndBeepAndRejectsOverlap) {
    auto controller = preview();
    auto consumer = controller.subscribe();
    consumer.poll();
    const auto beep = controller.submit(StartBeep{"mixer-out"});
    ASSERT_EQ(beep.outcome, Outcome::Accepted);
    EXPECT_EQ(controller.submit(StartBeep{"mixer-out"}).outcome, Outcome::Rejected);
    controller.submit(CancelBeep{});
    const auto events = consumer.poll();
    EXPECT_TRUE(std::any_of(events.results.begin(), events.results.end(), [&](const auto& result) {
        return result.request_id == beep.request_id && result.outcome == Outcome::Cancelled;
    }));
    const auto start = controller.submit(StartRouting{"profile-simplex"});
    ASSERT_EQ(start.outcome, Outcome::Accepted);
    const auto stop = controller.submit(StopRouting{});
    ASSERT_EQ(stop.outcome, Outcome::Accepted);
    controller.advance(300);
    const auto stopped = consumer.poll();
    EXPECT_TRUE(
        std::any_of(stopped.results.begin(), stopped.results.end(), [&](const auto& result) {
            return result.request_id == start.request_id && result.outcome == Outcome::Cancelled;
        }));
    EXPECT_TRUE(
        std::any_of(stopped.results.begin(), stopped.results.end(), [&](const auto& result) {
            return result.request_id == stop.request_id && result.outcome == Outcome::Completed;
        }));
    EXPECT_EQ(stopped.state->state, State::Stopped);
}
TEST(Communication, SnapshotOwnershipAndWarningPrioritySurviveLaterMutations) {
    auto controller = preview();
    auto consumer = controller.subscribe();
    const auto original = consumer.poll().state;
    controller.activate("profile-simplex");
    controller.advance(300);
    EXPECT_EQ(original->state, State::Stopped);
    auto current = consumer.poll().state;
    ASSERT_EQ(current->state, State::Running);
    controller.fail_path(current->profiles.front().holes.front().id, false);
    const auto baseline = presentation_baseline(controller.snapshot());
    EXPECT_GT(baseline.priority, 0);
    EXPECT_EQ(current->state, State::Running);
}
TEST(Communication, SurvivingSubscriptionReceivesShutdownMarker) {
    Subscription survivor;
    {
        auto controller = preview();
        survivor = controller.subscribe();
        survivor.poll();
    }
    EXPECT_TRUE(survivor.poll().closed);
    survivor.reset();
}
TEST(Communication, DeviceLossCancelsBeepWithoutClaimingPlaybackCompletion) {
    auto controller = preview();
    auto consumer = controller.subscribe();
    consumer.poll();
    const auto beep = controller.submit(StartBeep{"mixer-out"});
    ASSERT_EQ(beep.outcome, Outcome::Accepted);
    auto device = *endpoint(controller.snapshot(), "mixer-out");
    device.availability = Availability::Unavailable;
    ASSERT_TRUE(controller.update_endpoint(device).ok);
    controller.advance(1);
    const auto batch = consumer.poll();
    EXPECT_TRUE(std::any_of(batch.results.begin(), batch.results.end(), [&](const auto& result) {
        return result.request_id == beep.request_id && result.outcome == Outcome::Cancelled;
    }));
    EXPECT_FALSE(std::any_of(batch.results.begin(), batch.results.end(), [&](const auto& result) {
        return result.request_id == beep.request_id && result.outcome == Outcome::Completed;
    }));
}
} // namespace
