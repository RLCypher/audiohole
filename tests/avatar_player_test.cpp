#include "audiohole/avatar/player.hpp"
#include <gtest/gtest.h>

namespace av = audiohole::avatar;
namespace {
std::shared_ptr<av::Clip> clip(const std::string& state, bool loop = false) {
    auto result = std::make_shared<av::Clip>();
    result->personality = "uwu";
    result->style = "chibi";
    result->requested = state;
    result->loop = loop;
    result->frames = {std::make_shared<av::Pixels>(), std::make_shared<av::Pixels>()};
    result->durations_ms = {100, 200};
    result->still = std::make_shared<av::Pixels>();
    return result;
}
av::Player player() {
    av::Player result;
    result.reset("uwu", "chibi", 0);
    result.context("idle", 1, 0, 0);
    result.install_baseline(clip("idle", true), 1, 0);
    return result;
}
TEST(AvatarReactionHold, KeepsFinalFrameThenResumesLatestBaselineAtDeadline) {
    // Includes a looping idle clip used as an owned reaction fallback.
    for (bool loop : {false, true}) {
        auto playback = player();
        auto reaction = clip("happy", loop);
        ASSERT_TRUE(playback.react(reaction, {"happy", "event", 1, 0, 2000, 10, 2000}, 0).accepted);
        EXPECT_EQ(playback.tick(99).index, 0);
        EXPECT_EQ(playback.tick(100).index, 1);
        for (int now : {300, 900, 2299}) {
            const auto held = playback.tick(now);
            EXPECT_TRUE(held.reaction);
            EXPECT_EQ(held.image, reaction->frames.back());
            EXPECT_EQ(held.index, 1);
        }
        const auto resumed = playback.tick(2300);
        EXPECT_FALSE(resumed.reaction);
        EXPECT_EQ(resumed.state, "idle");
        // Persistent idle time continues during the reaction and final hold.
        EXPECT_EQ(resumed.index, 1);
        EXPECT_EQ(playback.tick(2400).index, 0);
    }
}
TEST(AvatarReactionHold, WarningAndPersonalitySwitchInterruptTheHold) {
    auto playback = player();
    playback.hold_expressions(true);
    ASSERT_TRUE(
        playback.react(clip("happy"), {"happy", "event", 1, 0, 2000, 10, 2000}, 0).accepted);
    ASSERT_TRUE(playback.tick(400).reaction);
    ASSERT_TRUE(playback.context("concerned", 2, 70, 400, true).accepted);
    playback.install_baseline(clip("concerned", true), 2, 400);
    EXPECT_FALSE(playback.tick(400).reaction);
    EXPECT_EQ(playback.tick(400).state, "concerned");
    EXPECT_FALSE(
        playback.react(clip("happy"), {"happy", "stale", 1, 400, 2000, 10, 2000}, 400).accepted);

    playback = player();
    ASSERT_TRUE(
        playback.react(clip("happy"), {"happy", "event", 1, 0, 2000, 10, 2000}, 0).accepted);
    ASSERT_TRUE(playback.tick(400).reaction);
    playback.reset("default", "normal", 400);
    EXPECT_FALSE(playback.tick(400).reaction);
    EXPECT_FALSE(playback.tick(400).image);
}
TEST(AvatarReactionHold, HoldBetweenEventsReplaysTheNextEventAndCanBeDisabled) {
    for (bool reduced : {false, true}) {
        auto playback = player();
        playback.reduced_motion(reduced);
        playback.hold_expressions(true);
        auto reaction = clip("happy", true);
        ASSERT_TRUE(playback.react(reaction, {"happy", "first", 1, 0, 2000, 10, 2000}, 0).accepted);
        const auto held = playback.tick(10000);
        EXPECT_TRUE(held.reaction && held.holding);
        EXPECT_EQ(held.image, reduced ? reaction->still : reaction->frames.back());
        ASSERT_TRUE(
            playback.react(reaction, {"happy", "next", 1, 10000, 2000, 10, 2000}, 10000).accepted);
        EXPECT_FALSE(playback.tick(10000).holding);
        EXPECT_EQ(playback.tick(10000).index, 0);
        ASSERT_TRUE(playback.tick(20000).holding);
        playback.hold_expressions(false);
        EXPECT_FALSE(playback.tick(20000).reaction);
    }
}
TEST(AvatarReactionHold, StaticAndReducedMotionReactionsRetainTheirStillImage) {
    for (bool reduced : {false, true}) {
        auto playback = player();
        playback.reduced_motion(reduced);
        auto reaction = clip("happy");
        reaction->frames.resize(1);
        reaction->durations_ms.clear();
        reaction->behavior.static_hold_ms = 650;
        ASSERT_TRUE(playback.react(reaction, {"happy", "event", 1, 0, 2000, 10, 2000}, 0).accepted);
        const auto held = playback.tick(2649);
        EXPECT_TRUE(held.reaction);
        EXPECT_EQ(held.image, reduced ? reaction->still : reaction->frames.front());
        EXPECT_FALSE(playback.tick(2650).reaction);
    }
}
TEST(AvatarPlayback, CountsTotalPassesThenHoldsOnlyAfterTheLastPass) {
    auto playback = player();
    playback.reaction_playback(3, false);
    playback.hold_expressions(true);
    auto reaction = clip("happy");
    ASSERT_TRUE(playback.react(reaction, {"happy", "repeat", 1, 0, 2000, 10, 2000}, 0).accepted);
    EXPECT_EQ(playback.tick(300).index, 0);
    EXPECT_EQ(playback.tick(600).index, 0);
    EXPECT_FALSE(playback.tick(899).holding);
    EXPECT_TRUE(playback.tick(900).holding);
    EXPECT_EQ(playback.tick(10000).image, reaction->frames.back());
    playback.hold_expressions(false);
    EXPECT_FALSE(playback.tick(10000).reaction);
    EXPECT_THROW(playback.reaction_playback(0, false), std::invalid_argument);
    EXPECT_THROW(playback.reaction_playback(21, false), std::invalid_argument);
}
TEST(AvatarPlayback, ContinuousOverridesHoldAndYieldsToNextEventAndWarning) {
    auto playback = player();
    playback.reaction_playback(1, true);
    playback.hold_expressions(true);
    auto reaction = clip("happy");
    ASSERT_TRUE(playback.react(reaction, {"happy", "first", 1, 0, 2000, 10, 2000}, 0).accepted);
    const auto repeated = playback.tick(3000);
    EXPECT_TRUE(repeated.looping);
    EXPECT_FALSE(repeated.holding);
    EXPECT_EQ(repeated.index, 0);
    ASSERT_TRUE(
        playback.react(reaction, {"happy", "next", 1, 3000, 2000, 10, 2000}, 3000).accepted);
    EXPECT_EQ(playback.tick(3000).index, 0);
    ASSERT_TRUE(playback.context("concerned", 2, 70, 3100, true).accepted);
    EXPECT_FALSE(playback.tick(3100).reaction);
    playback = player();
    playback.reaction_playback(2, false);
    playback.reduced_motion(true);
    ASSERT_TRUE(playback.react(reaction, {"happy", "static", 1, 0, 2000, 10, 2000}, 0).accepted);
    EXPECT_EQ(playback.tick(500).image, reaction->still);
    EXPECT_FALSE(playback.tick(2600).reaction);
}
} // namespace
