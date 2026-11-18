#include "audiohole/plug_board/presentation.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace audiohole::plug_board {
namespace {
TEST(PlugBoardLastSeen, CompactUnitsCoverMinuteHourDayWeekAndFourWeekBoundaries) {
    constexpr std::int64_t minute = 60, hour = 60 * minute, day = 24 * hour, week = 7 * day;
    for (const auto& [age, expected] :
         std::vector<std::pair<std::int64_t, std::string>>{{0, "just now"},
                                                           {59, "just now"},
                                                           {minute, "1m ago"},
                                                           {hour - 1, "59m ago"},
                                                           {hour, "1h ago"},
                                                           {hour + minute, "1h 1m ago"},
                                                           {2 * hour + 41 * minute, "2h 41m ago"},
                                                           {day - 1, "23h 59m ago"},
                                                           {day, "1d ago"},
                                                           {day + 23 * hour, "1d ago"},
                                                           {week - 1, "6d ago"},
                                                           {week, "1w ago"},
                                                           {2 * week - 1, "1w ago"},
                                                           {2 * week, "2w ago"},
                                                           {4 * week, "4w ago"},
                                                           {4 * week + 1, "a while ago"}})
        EXPECT_EQ(last_seen_text(age), "Last seen: " + expected) << age;
}
TEST(PlugBoardLastSeen, ElapsedTimeClampsNegativeValuesAndCannotOverflow) {
    EXPECT_EQ(last_seen_text(-1, -10), "Last seen: just now");
    EXPECT_EQ(last_seen_text(59, 1), "Last seen: 1m ago");
    EXPECT_EQ(last_seen_text(59 * 60, 60), "Last seen: 1h ago");
    EXPECT_EQ(last_seen_text(std::numeric_limits<std::int64_t>::max(),
                             std::numeric_limits<std::int64_t>::max()),
              "Last seen: a while ago");
}

class PlugBoardPlacement : public testing::Test {
  protected:
    BoardConfiguration board = make_library("board", utc_timestamp()).boards[0].working;
    EndpointInventory inventory;
    ApplicationView app;
    void SetUp() override {
        board.sockets.push_back({"music", "Music", {"logical-music"}});
        inventory.endpoints = {{"logical-music", "Music (RODECaster)", "windows-music", true},
                               {"logical-game", "Game (RODECaster)", "windows-game", true}};
        app.id = "spotify";
        app.activity = Activity::Active;
        app.observation_fresh = true;
        app.observation = ApplicationObservation{};
        app.observation->active_endpoint_ids = {"windows-music"};
    }
    ApplicationPlacement placement() const {
        return application_placement(board, app, inventory);
    }
};

TEST_F(PlugBoardPlacement, UnassignedAppFollowsItsActualOutputIntoAnExistingSocket) {
    const auto before = board.application_assignments;
    EXPECT_EQ(placement().socket_id, "music");
    EXPECT_EQ(placement().basis, PlacementBasis::ObservedOutput);
    EXPECT_EQ(board.application_assignments, before); // No implicit saved intent or setter.
}
TEST_F(PlugBoardPlacement, AnOutputWithoutACorrespondingSocketFallsBackToSystem) {
    app.observation->active_endpoint_ids = {"windows-game"};
    EXPECT_EQ(placement().socket_id, "system");
    EXPECT_EQ(placement().basis, PlacementBasis::SystemFallback);
}
TEST_F(PlugBoardPlacement, ExplicitSystemAssignmentWinsOverAnObservedMatch) {
    board.application_assignments[app.id] = "system";
    EXPECT_EQ(placement().socket_id, "system");
    EXPECT_EQ(placement().basis, PlacementBasis::SavedAssignment);
}
TEST_F(PlugBoardPlacement, AnExplicitAssignmentDoesNotMoveWithAnOldStream) {
    board.sockets.push_back({"game", "Game", {"logical-game"}});
    board.application_assignments[app.id] = "game";
    EXPECT_EQ(placement().socket_id, "game");
}
TEST_F(PlugBoardPlacement, HistoricalObservationsDoNotInferPlacementButKnownPartialPlaybackCan) {
    app.observation_fresh = false;
    EXPECT_EQ(placement().socket_id, "system");
    app.observation_fresh = true;
    app.activity = Activity::RecentlyActive;
    EXPECT_EQ(placement().socket_id, "system");
    app.activity = Activity::Active;
    app.observation->complete = false;
    EXPECT_EQ(placement().socket_id, "music");
}
TEST_F(PlugBoardPlacement, MultipleActiveOutputsRemainOneUnassignedSystemRow) {
    app.observation->active_endpoint_ids.push_back("windows-game");
    EXPECT_EQ(placement().socket_id, "system");
}
TEST_F(PlugBoardPlacement, DuplicateSocketTargetsHaveNoInferredPriority) {
    board.sockets.push_back({"second-music", "Second Music", {"logical-music"}});
    EXPECT_EQ(placement().socket_id, "system");
}
TEST_F(PlugBoardPlacement, UnavailableAmbiguousAndCaptureBindingsNeverMatch) {
    inventory.endpoints[0].available = false;
    EXPECT_EQ(placement().socket_id, "system");
    inventory.endpoints[0].available = true;
    inventory.endpoints[0].ambiguous = true;
    EXPECT_EQ(placement().socket_id, "system");
    inventory.endpoints[0].ambiguous = false;
    inventory.endpoints[0].playback = false;
    EXPECT_EQ(placement().socket_id, "system");
}
TEST_F(PlugBoardPlacement, FriendlyNamesDoNotSubstituteForAnExactBinding) {
    inventory.endpoints[0].windows_id = "different-windows-endpoint";
    inventory.endpoints[1].display_name = inventory.endpoints[0].display_name;
    EXPECT_EQ(placement().socket_id, "system");
}
} // namespace
} // namespace audiohole::plug_board
