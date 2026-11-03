#include "presentation/text_layout.h"
#include <gtest/gtest.h>
using audiohole::presentation::keep_kaomoji_together;
TEST(PersonalityLayout, KeepsScreenshotKaomojiTogetherWithoutChangingSurroundingProse) {
    EXPECT_EQ(keep_kaomoji_together("Aww, finished awweady? (⁄ ⁄>⁄ω⁄<⁄ ⁄)"),
              "Aww, finished awweady? (⁄\u00a0⁄>⁄ω⁄<⁄\u00a0⁄)");
    EXPECT_EQ(keep_kaomoji_together("First (ง •̀_•́)ง then (╥ ﹏ ╥)."),
              "First (ง\u00a0•̀_•́)ง then (╥\u00a0﹏\u00a0╥).");
}
TEST(PersonalityLayout, LeavesFactsAndUnmatchedOrUnspacedTextUnchanged) {
    for (const auto* input : {"Buffer (simulated value)", "Save (without applying)",
                              "OwO >w< :3 (╥﹏╥)", "(incomplete face ", "a\nb", "ordinary prose"})
        EXPECT_EQ(keep_kaomoji_together(input), input);
    const auto long_prose = "(" + std::string(129, '*') + " more)";
    EXPECT_EQ(keep_kaomoji_together(long_prose), long_prose);
}
