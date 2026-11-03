#include "presentation/messages.h"
#include "presentation/sleep_commentary.h"
#include <gtest/gtest.h>

namespace p = audiohole::presentation;
using Change = p::SleepCommentary::Change;
TEST(SleepCommentary, StartsWithSnoringThenBriefDreamsWithoutCatchupBursts) {
    p::SleepCommentary schedule([](unsigned) { return 0; });
    EXPECT_EQ(schedule.update(false, "uwu", true, 0), Change::None);
    EXPECT_EQ(schedule.update(true, "uwu", true, 100), Change::Rest);
    EXPECT_EQ(schedule.update(true, "uwu", true, 45099), Change::None);
    EXPECT_EQ(schedule.update(true, "uwu", true, 45100), Change::Dream);
    EXPECT_EQ(schedule.update(true, "uwu", true, 51099), Change::None);
    EXPECT_EQ(schedule.update(true, "uwu", true, 51100), Change::Rest);
    EXPECT_EQ(schedule.update(true, "uwu", true, 96099), Change::None);
    EXPECT_EQ(schedule.update(true, "uwu", true, 1000000), Change::Dream);
    EXPECT_EQ(schedule.update(true, "uwu", true, 1000000), Change::None);
}
TEST(SleepCommentary, WakeOrIdentityChangeCancelsTheOldDreamAndStartsFresh) {
    p::SleepCommentary schedule([](unsigned upper) { return upper - 1; });
    EXPECT_EQ(schedule.update(true, "default", true, 0), Change::Rest);
    EXPECT_EQ(schedule.update(true, "default", true, 89999), Change::None);
    EXPECT_EQ(schedule.update(true, "default", true, 90000), Change::Dream);
    EXPECT_EQ(schedule.update(false, "default", true, 90001), Change::Awake);
    EXPECT_EQ(schedule.update(false, "default", true, 200000), Change::None);
    EXPECT_EQ(schedule.update(true, "default", true, 200001), Change::Rest);
    EXPECT_EQ(schedule.update(true, "uwu", true, 200002), Change::Rest);
    EXPECT_EQ(schedule.update(true, "uwu", true, 290001), Change::None);
    EXPECT_EQ(schedule.update(true, "uwu", true, 290002), Change::Dream);
}
TEST(SleepCommentary, MissingOptionalDreamPoolStaysQuietWithoutRandomSelection) {
    p::SleepCommentary schedule([](unsigned) -> unsigned {
        ADD_FAILURE() << "Quiet sleep consumed randomness";
        return 0;
    });
    EXPECT_EQ(schedule.update(true, "vanilla", false, 0), Change::Rest);
    EXPECT_EQ(schedule.update(true, "vanilla", false, 1000000), Change::None);
    EXPECT_EQ(schedule.update(false, "vanilla", false, 1000001), Change::Awake);
}
TEST(SleepCommentary, DreamRepliesUseOwnedCatalogsAndHaveNoTechnicalClaim) {
    p::Messages messages(41);
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (const auto* id : {"default", "uwu"}) {
        const auto first = messages.show(p::Message::Dream, id, 1);
        const auto next = messages.show(p::Message::Dream, id, 2);
        EXPECT_TRUE(first.technical.empty());
        EXPECT_FALSE(first.commentary.empty());
        EXPECT_EQ(first.personality_id, id);
        EXPECT_EQ(first.expression, "sleeping");
        EXPECT_EQ(first.category, "dream");
        EXPECT_NE(first.response_id, next.response_id);
        EXPECT_EQ(messages.show(p::Message::Dream, id, 2).commentary, next.commentary);
    }
    EXPECT_TRUE(messages.variants(p::Message::Dream, "vanilla").empty());
}
