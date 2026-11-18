#include "audiohole/plug_board/observations.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace audiohole::plug_board {
TEST(PlugBoardDisplayName, ExecutableSuffixIsRemovedOnlyForPresentation) {
    EXPECT_EQ(application_display_name("Selkie.exe"), "Selkie");
    EXPECT_EQ(application_display_name("Selkie.EXE"), "Selkie");
    EXPECT_EQ(application_display_name("Spotify"), "Spotify");
    EXPECT_EQ(application_display_name("Game.exe - lobby"), "Game.exe - lobby");
    EXPECT_EQ(application_display_name(".exe"), ".exe");
    const ApplicationIdentity identity{IdentityKind::Win32Image, "C:\\Games\\Selkie.exe"};
    (void)application_display_name("Selkie.exe");
    EXPECT_EQ(identity.value, "C:\\Games\\Selkie.exe");
}

TEST(PlugBoardInternalApps, AudioHoleProcessesAreRecognizedByIdentityNotDisplayName) {
    for (const auto* path : {"C:\\Apps\\AudioHole.exe", "C:/Apps/AUDIOHOLE-PlugBoard.EXE",
                             "C:/Apps/AudioHoleEngine.exe", "C:/Apps/plug_board_fixture.exe"})
        EXPECT_TRUE(is_audiohole_application({IdentityKind::Win32Image, path})) << path;
    EXPECT_TRUE(
        is_audiohole_application({IdentityKind::PackagedApplication, "AudioHole.Desktop_123!App"}));
    EXPECT_FALSE(is_audiohole_application({IdentityKind::Win32Image, "C:/Games/NotAudioHole.exe"}));
    EXPECT_FALSE(is_audiohole_application({IdentityKind::Unresolved, "AudioHole.exe"}));
}
namespace {

// A silent running session is Active without any peak/meter property. The fixture
// intentionally offers no amplitude field so classification cannot depend on it.
SessionObservation session(std::string instance, std::string endpoint, SessionState state,
                           std::string image = "C:/Apps/Discord/Discord.exe", unsigned pid = 100) {
    SessionObservation value;
    value.instance_id = std::move(instance);
    value.endpoint_id = std::move(endpoint);
    value.state = state;
    value.identity = {IdentityKind::Win32Image, image};
    value.process = ProcessToken{pid, 9000 + pid, image};
    value.display_name = "Application";
    return value;
}

TEST(PlugBoardObservation, OneApplicationAcrossActiveAndAbandonedEndpoints) {
    SessionSnapshot snapshot{{session("current", "B", SessionState::Active),
                              session("stale-a", "A", SessionState::Inactive),
                              session("expired-c", "C", SessionState::Expired)}};
    const auto applications = group_applications(snapshot);
    ASSERT_EQ(applications.size(), 1u);
    EXPECT_EQ(applications[0].sessions.size(), 3u);
    EXPECT_EQ(applications[0].active_endpoint_ids, std::vector<std::string>{"B"});
}

TEST(PlugBoardObservation, SimultaneousActiveEndpointsAreNotDiscarded) {
    const auto applications = group_applications(
        {{session("one", "B", SessionState::Active), session("two", "A", SessionState::Active)}});
    ASSERT_EQ(applications.size(), 1u);
    EXPECT_EQ(applications[0].active_endpoint_ids, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(compare_playback({applications[0].active_endpoint_ids}, "B"),
              PlaybackMatch::Multiple);
}

TEST(PlugBoardObservation, DuplicateCreationNotificationDoesNotDuplicateSession) {
    auto original = session("one", "A", SessionState::Active);
    const auto applications = group_applications({{original, original}});
    ASSERT_EQ(applications.size(), 1u);
    EXPECT_EQ(applications[0].sessions.size(), 1u);
}

TEST(PlugBoardObservation, LaterStateNotificationReplacesEnumeratedState) {
    auto first = session("one", "A", SessionState::Active);
    auto later = first;
    later.state = SessionState::Inactive;
    const auto applications = group_applications({{first, later}});
    ASSERT_EQ(applications.size(), 1u);
    EXPECT_EQ(applications[0].sessions.size(), 1u);
    EXPECT_TRUE(applications[0].active_endpoint_ids.empty());
    EXPECT_TRUE(observe_playback({{first, later}}, {*first.process}).active_endpoint_ids.empty());
}

TEST(PlugBoardObservation, CompletelyUnidentifiedSessionsAreNotMergedByEmptyMetadata) {
    SessionObservation first, second;
    const auto applications = group_applications({{first, second}});
    ASSERT_EQ(applications.size(), 2u);
    EXPECT_NE(applications[0].observation_key, applications[1].observation_key);
}

TEST(PlugBoardObservation, SameFilenameInDifferentInstallationsDoesNotMerge) {
    const auto applications = group_applications(
        {{session("one", "A", SessionState::Active, "C:/Vendor1/player.exe"),
          session("two", "A", SessionState::Active, "C:/Vendor2/player.exe", 200)}});
    EXPECT_EQ(applications.size(), 2u);
}

TEST(PlugBoardObservation, EveryVerifiedProcessIsRetainedForApplicationLevelAssignment) {
    const auto applications = group_applications(
        {{session("one", "A", SessionState::Active),
          session("two", "B", SessionState::Inactive, "C:/Apps/Discord/Discord.exe", 200)}});
    ASSERT_EQ(applications.size(), 1u);
    ASSERT_EQ(applications[0].verified_processes.size(), 2u);
    EXPECT_EQ(applications[0].verified_processes[0].pid, 100u);
    EXPECT_EQ(applications[0].verified_processes[1].pid, 200u);
}

TEST(PlugBoardObservation, PackagedApplicationsSharingAHostRemainDistinct) {
    auto first = session("one", "A", SessionState::Active);
    auto second = session("two", "A", SessionState::Active);
    first.identity = {IdentityKind::PackagedApplication, "Package_family!First"};
    second.identity = {IdentityKind::PackagedApplication, "Package_family!Second"};
    EXPECT_EQ(group_applications({{first, second}}).size(), 2u);
}

TEST(PlugBoardObservation, UnresolvedIdentitiesUseLiveOwnerNotDisplayName) {
    auto first = session("one", "A", SessionState::Active);
    auto duplicate_owner = session("two", "B", SessionState::Inactive);
    auto different_owner = session("three", "A", SessionState::Active, "unknown", 200);
    first.identity = duplicate_owner.identity = different_owner.identity = {};
    const auto applications = group_applications({{first, duplicate_owner, different_owner}});
    ASSERT_EQ(applications.size(), 2u);
    EXPECT_FALSE(applications[0].complete);
    EXPECT_TRUE(applications[0].verified_processes.empty());
}

TEST(PlugBoardObservation, CrossProcessCreatorIsNotACompleteAssignmentTarget) {
    auto shared = session("one", "A", SessionState::Active);
    shared.cross_process = true;
    const auto applications = group_applications({{shared}});
    ASSERT_EQ(applications.size(), 1u);
    EXPECT_TRUE(applications[0].verified_processes.empty());
    EXPECT_FALSE(applications[0].complete);
    EXPECT_FALSE(observe_playback({{shared}}, {*shared.process}).complete);
}

TEST(PlugBoardObservation, PidReuseCannotSupplyEvidenceForAnOlderProcessToken) {
    const auto current = session("one", "A", SessionState::Active);
    auto previous_process = *current.process;
    --previous_process.creation_time;
    const auto playback = observe_playback({{current}}, {previous_process});
    EXPECT_TRUE(playback.active_endpoint_ids.empty());
}

TEST(PlugBoardObservation, FailedEnumerationDoesNotConfirmAnEndpointOrAbsence) {
    const auto current = session("one", "A", SessionState::Active);
    SessionSnapshot incomplete{{current}, false};
    const auto playback = observe_playback(incomplete, {*current.process});
    EXPECT_EQ(compare_playback(playback, "A"), PlaybackMatch::Incomplete);
    EXPECT_FALSE(group_applications(incomplete)[0].complete);
}

TEST(PlugBoardObservation, NoActiveSessionsDoesNotReuseAnInactiveEndpoint) {
    const auto stopped = session("one", "A", SessionState::Inactive);
    const auto playback = observe_playback({{stopped}}, {*stopped.process});
    EXPECT_EQ(compare_playback(playback, "A"), PlaybackMatch::Unobserved);
}

TEST(PlugBoardActivity, SilentRunningIsActiveAndExactWindowBoundaryIsInactive) {
    EXPECT_EQ(classify_activity(true, {}, 5000, 300000), Activity::Active);
    EXPECT_EQ(classify_activity(false, 1000, 300999, 300000), Activity::RecentlyActive);
    EXPECT_EQ(classify_activity(false, 1000, 301000, 300000), Activity::Inactive);
    EXPECT_EQ(classify_activity(false, {}, 5000, 300000), Activity::Inactive);
}

TEST(PlugBoardActivity, InvalidWindowAndBackwardTimeDoNotInventRecentActivity) {
    EXPECT_EQ(classify_activity(false, 5001, 5000, 300000), Activity::Inactive);
    EXPECT_EQ(classify_activity(false, 4000, 5000, 0), Activity::Inactive);
    EXPECT_EQ(classify_activity(false, 4000, 5000, -1), Activity::Inactive);
    EXPECT_EQ(classify_activity(false, std::numeric_limits<std::int64_t>::min(),
                                std::numeric_limits<std::int64_t>::max(), 300000),
              Activity::Inactive);
}

} // namespace
} // namespace audiohole::plug_board
