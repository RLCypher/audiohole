#include "model/controller.h"
#include "presentation/messages.h"
#include <algorithm>
#include <gtest/gtest.h>
using namespace audiohole;
namespace presentation = audiohole::presentation;

TEST(Simulator, DuplexFailureKeepsHealthyDirectionAndRecoveryIsCancellable) {
    SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-duplex").ok);
    EXPECT_EQ(controller.snapshot().state, State::Starting);
    controller.advance(300);
    ASSERT_EQ(controller.snapshot().paths.size(), 2u);
    EXPECT_EQ(controller.snapshot().state, State::Running);
    controller.fail_path(controller.snapshot().paths.front().hole_id, false);
    EXPECT_EQ(controller.snapshot().state, State::Degraded);
    EXPECT_EQ(controller.snapshot().paths[1].state, State::Running);
    controller.recover();
    EXPECT_EQ(controller.snapshot().state, State::Recovering);
    controller.stop();
    controller.advance(5000);
    EXPECT_FALSE(controller.snapshot().armed);
    EXPECT_EQ(controller.snapshot().state, State::Stopped);
}
TEST(Simulator, SavedEditsDoNotChangeLivePlanUntilApply) {
    SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-simplex").ok);
    controller.advance(300);
    auto draft = *profile(controller.snapshot(), "profile-simplex");
    draft.holes.front().enabled = false;
    ASSERT_TRUE(controller.save(draft, false).ok);
    EXPECT_EQ(controller.snapshot().paths.size(), 1u);
    EXPECT_EQ(controller.snapshot().state, State::Running);
    ASSERT_TRUE(controller.activate(draft.id).ok);
    controller.advance(300);
    EXPECT_TRUE(controller.snapshot().paths.empty());
    EXPECT_EQ(controller.snapshot().state, State::Ready);
}
TEST(Simulator, DeviceRemovalAndReturnDoNotRearmAfterStop) {
    SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-duplex").ok);
    controller.advance(300);
    auto device = *endpoint(controller.snapshot(), "mixer-in");
    device.availability = Availability::Unavailable;
    ASSERT_TRUE(controller.update_endpoint(device).ok);
    EXPECT_EQ(controller.snapshot().state, State::Degraded);
    controller.stop();
    controller.advance(300);
    device.availability = Availability::Available;
    controller.update_endpoint(device);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().state, State::Stopped);
    EXPECT_FALSE(controller.snapshot().armed);
}
TEST(Simulator, AmbiguousDeviceWaitsWithoutSilentRebinding) {
    SimulatedController controller;
    auto draft = *profile(controller.snapshot(), "profile-simplex");
    draft.holes.front().forward.source = "twin-in";
    ASSERT_TRUE(controller.save(draft, true).ok);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().state, State::Ready);
    EXPECT_EQ(endpoint(controller.snapshot(), "twin-in")->availability, Availability::Ambiguous);
}
TEST(Simulator, FailedSwitchRetainsPreviousProfile) {
    SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-simplex").ok);
    controller.advance(300);
    auto draft = *profile(controller.snapshot(), "profile-duplex");
    draft.holes.front().forward.source = "missing-in";
    controller.save(draft, false);
    EXPECT_FALSE(controller.activate(draft.id).ok);
    EXPECT_EQ(controller.snapshot().active_profile, "profile-simplex");
    EXPECT_EQ(controller.snapshot().state, State::Running);
    EXPECT_TRUE(controller.activate(draft.id, true).ok);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().state, State::Degraded);
}
TEST(Simulator, PendingDeadlineDoesNotMoveOnRefreshAndStopCancelsIt) {
    SimulatedController controller;
    ASSERT_TRUE(controller.schedule("profile-simplex", 1000).ok);
    controller.advance(400);
    EXPECT_EQ(controller.snapshot().pending_ms, 600);
    for (int i = 0; i < 10; ++i)
        (void)controller.snapshot();
    EXPECT_EQ(controller.snapshot().pending_ms, 600);
    controller.stop();
    controller.advance(2000);
    EXPECT_EQ(controller.snapshot().state, State::Stopped);
    EXPECT_TRUE(controller.snapshot().pending_profile.empty());
}
TEST(Simulator, CapacityCountsEnabledWaitingHolesAndDuplexOnce) {
    SimulatedController controller;
    auto draft = *profile(controller.snapshot(), "profile-duplex");
    while (draft.holes.size() < 8) {
        auto hole = draft.holes.front();
        hole.id = controller.make_hole().id;
        hole.forward.source = "missing-in";
        draft.holes.push_back(hole);
    }
    EXPECT_TRUE(validate(controller.snapshot(), draft).ok);
    auto extra = draft.holes.front();
    extra.id = controller.make_hole().id;
    draft.holes.push_back(extra);
    EXPECT_FALSE(validate(controller.snapshot(), draft).ok);
    draft.holes.back().enabled = false;
    EXPECT_TRUE(validate(controller.snapshot(), draft).ok);
}
TEST(Simulator, ManualOverrideIsRetainedAndUnsupportedTupleRejected) {
    SimulatedController controller;
    auto draft = *profile(controller.snapshot(), "profile-simplex");
    auto& direction = draft.holes.front().forward;
    direction.settings.automatic = false;
    direction.settings.format = {96000, 2, "float32"};
    EXPECT_FALSE(validate(controller.snapshot(), draft).ok);
    EXPECT_TRUE(controller.save(draft, false).ok);
    EXPECT_EQ(profile(controller.snapshot(), draft.id)->holes.front().forward.settings.format.rate,
              96000);
    direction.destination = "mixer-out";
    EXPECT_TRUE(validate(controller.snapshot(), draft).ok);
    const auto evidence = recommend(controller.snapshot(), direction).explanation;
    EXPECT_NE(evidence.find("Both endpoints list this format"), std::string::npos);
    EXPECT_NE(evidence.find("48 kHz"), std::string::npos);
    EXPECT_NE(evidence.find("does not establish a shared clock"), std::string::npos);
}
TEST(Simulator, IdentityAndWindowsNameSurviveDisplayNameEditing) {
    SimulatedController controller;
    auto device = *endpoint(controller.snapshot(), "mixer-in");
    const auto original_name = device.windows_name;
    device.display_name = "User chosen R/L name";
    device.notes = "Keep these notes";
    device.windows_name = "must not be accepted";
    ASSERT_TRUE(controller.update_endpoint(device).ok);
    const auto* saved = endpoint(controller.snapshot(), device.id);
    EXPECT_EQ(saved->windows_name, original_name);
    EXPECT_EQ(saved->notes, device.notes);
    EXPECT_EQ(saved->display_name, device.display_name);
}
TEST(Simulator, BeepHasExplicitTargetAndExclusiveConflictIsNotBypassed) {
    SimulatedController controller;
    EXPECT_FALSE(controller.beep("").ok);
    EXPECT_FALSE(controller.beep("mixer-in").ok);
    EXPECT_TRUE(controller.beep("vr-out").ok);
    EXPECT_EQ(controller.snapshot().beep_destination, "vr-out");
    controller.advance(1000);
    EXPECT_TRUE(controller.snapshot().beep_destination.empty());
    auto draft = *profile(controller.snapshot(), "profile-simplex");
    draft.holes.front().forward.settings.mode = "Exclusive";
    ASSERT_TRUE(controller.save(draft, true).ok);
    controller.advance(300);
    EXPECT_FALSE(controller.beep("vr-out").ok);
    EXPECT_TRUE(controller.beep("mixer-out").ok);
    controller.stop();
    EXPECT_TRUE(controller.snapshot().beep_destination.empty());
}
TEST(Simulator, StopCancelsScanAndMonitoringAndDiagnosticsAreBounded) {
    SimulatedController controller;
    controller.scan(true);
    controller.monitor(true);
    controller.advance(400);
    EXPECT_TRUE(controller.snapshot().scanning);
    controller.stop();
    EXPECT_FALSE(controller.snapshot().scanning);
    EXPECT_FALSE(controller.snapshot().monitoring);
    for (int i = 0; i < 600; ++i)
        controller.create_profile("Fixture");
    EXPECT_EQ(controller.snapshot().events.size(), 500u);
}
TEST(Simulator, DuplicateUsesFreshProfileAndHoleIds) {
    SimulatedController controller;
    const auto id = controller.create_profile("Duplicate", "profile-duplex");
    const auto* original = profile(controller.snapshot(), "profile-duplex");
    const auto* duplicate = profile(controller.snapshot(), id);
    ASSERT_NE(duplicate, nullptr);
    EXPECT_NE(duplicate->id, original->id);
    EXPECT_NE(duplicate->holes.front().id, original->holes.front().id);
    EXPECT_EQ(duplicate->holes.front().forward.source, original->holes.front().forward.source);
}
TEST(Presentation, SeededSequenceReplaysWithoutImmediateRepetition) {
    presentation::Messages first(42), second(42);
    first.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR);
    second.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR);
    for (int event = 0; event < static_cast<int>(presentation::Message::Count); ++event) {
        for (auto mode : {presentation::Mode::Normal, presentation::Mode::UwU}) {
            std::string previous;
            for (std::uint64_t occurrence = 1; occurrence < 30; ++occurrence) {
                const auto id = static_cast<presentation::Message>(event);
                const auto current = first.show(id, mode, occurrence);
                EXPECT_EQ(current.commentary, second.show(id, mode, occurrence).commentary);
                if (first.variants(id, presentation::mode_id(mode)).size() > 1)
                    EXPECT_NE(current.commentary, previous);
                EXPECT_EQ(current.technical, presentation::technical(id));
                previous = current.commentary;
            }
        }
    }
}
TEST(Presentation, UnchangedRefreshAndVanillaNeverConsumeSelection) {
    unsigned calls = 0;
    presentation::Messages messages([&calls](unsigned) {
        ++calls;
        return 0u;
    });
    messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR);
    const auto initial = messages.show(presentation::Message::Ready, presentation::Mode::Normal, 1);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(
            messages.show(presentation::Message::Ready, presentation::Mode::Normal, 1).commentary,
            initial.commentary);
    EXPECT_EQ(calls, 1u);
    for (std::uint64_t i = 0; i < 24; ++i) {
        const auto result =
            messages.show(static_cast<presentation::Message>(i), presentation::Mode::Vanilla, i);
        EXPECT_TRUE(result.commentary.empty());
        EXPECT_EQ(result.technical, presentation::technical(static_cast<presentation::Message>(i)));
    }
    EXPECT_EQ(calls, 1u);
}
TEST(Presentation, InjectedWeightedSelectionRespectsBoundsAndExcludesPrevious) {
    presentation::Messages messages([](unsigned total) { return total - 1; });
    messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR);
    const auto last = messages.show(presentation::Message::Dfa, presentation::Mode::Normal, 1);
    EXPECT_EQ(last.commentary,
              messages.variants(presentation::Message::Dfa, "default").back().text);
    EXPECT_NE(messages.show(presentation::Message::Dfa, presentation::Mode::Normal, 2).commentary,
              last.commentary);
}
TEST(Presentation, CommentaryCannotIssueCommandsOrAlterDiagnostics) {
    SimulatedController controller;
    const auto before = controller.snapshot();
    presentation::Messages messages(7);
    for (std::uint64_t i = 1; i < 200; ++i)
        (void)messages.show(presentation::Message::Dfa, presentation::Mode::UwU, i);
    EXPECT_EQ(controller.snapshot().revision, before.revision);
    EXPECT_EQ(controller.snapshot().events.size(), before.events.size());
    EXPECT_EQ(controller.snapshot().state, before.state);
}

TEST(Simulator, StoppedProfileCanBeDeletedButPendingOrArmedCannot) {
    SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-simplex").ok);
    EXPECT_FALSE(controller.remove_profile("profile-simplex").ok);
    controller.stop();
    controller.advance(300);
    EXPECT_TRUE(controller.remove_profile("profile-simplex").ok);
    EXPECT_EQ(profile(controller.snapshot(), "profile-simplex"), nullptr);
    controller.schedule("profile-duplex", 1000);
    EXPECT_FALSE(controller.remove_profile("profile-duplex").ok);
}
TEST(Simulator, SelectedScanRequiresKnownIdentityAndDocumentsScope) {
    SimulatedController controller;
    controller.scan(true, false, ScanScope::Selected, "unknown");
    EXPECT_FALSE(controller.snapshot().scanning);
    controller.scan(true, true, ScanScope::Selected, "vr-out");
    EXPECT_TRUE(controller.snapshot().scanning);
    EXPECT_TRUE(std::any_of(
        controller.snapshot().events.begin(), controller.snapshot().events.end(),
        [](const auto& event) { return event.detail.find("vr-out") != std::string::npos; }));
    controller.advance(3000);
    EXPECT_FALSE(controller.snapshot().scanning);
}

TEST(Simulator, HotplugRecoveryDoesNotRestartHealthyDuplexLeg) {
    SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-duplex").ok);
    controller.advance(300);
    auto device = *endpoint(controller.snapshot(), "mixer-in");
    device.availability = Availability::Unavailable;
    controller.update_endpoint(device);
    EXPECT_EQ(controller.snapshot().paths[1].state, State::Running);
    device.availability = Availability::Available;
    controller.update_endpoint(device);
    EXPECT_EQ(controller.snapshot().paths[0].state, State::Recovering);
    EXPECT_EQ(controller.snapshot().paths[1].state, State::Running);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().state, State::Running);
}
