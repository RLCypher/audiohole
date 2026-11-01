#include "presentation/icon_outline.h"
#include <gtest/gtest.h>

using namespace audiohole::presentation;
TEST(LogoOutline, PreservesArtworkAndLeavesInternalHoleAndLayerGapsTransparent) {
    RgbaImage source{16, 16, std::vector<std::uint8_t>(16 * 16 * 4)};
    // A hollow conduit with an antialiased edge and a transparent layer seam.
    for (int y = 3; y <= 12; ++y)
        for (int x = 3; x <= 12; ++x)
            if (y != 7 && (x == 3 || x == 12 || y == 3 || y == 12)) {
                const auto offset = static_cast<std::size_t>((y * 16 + x) * 4);
                source.pixels[offset] = 159;
                source.pixels[offset + 1] = 250;
                source.pixels[offset + 2] = 215;
                source.pixels[offset + 3] = x == 3 ? 80 : 255;
            }
    const auto output = outline_logo(source, {230, 83, 83}, 1);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const auto offset = static_cast<std::size_t>((y * 16 + x) * 4);
            if (source.pixels[offset + 3])
                for (int channel = 0; channel < 4; ++channel)
                    EXPECT_EQ(output.pixels[offset + channel], source.pixels[offset + channel]);
            if (x >= 4 && x <= 11 && y >= 4 && y <= 11)
                EXPECT_EQ(output.pixels[offset + 3], 0) << x << "," << y;
        }
    EXPECT_EQ(output.pixels[(8 * 16 + 2) * 4 + 3], 255);
    EXPECT_EQ(output.pixels[(8 * 16 + 2) * 4], 230);
    EXPECT_EQ(output.pixels[3], 0);
}
TEST(LogoOutline, RejectsInvalidDimensionsStorageAndStroke) {
    EXPECT_THROW(outline_logo({0, 16, {}}, {0, 0, 0}, 1), std::invalid_argument);
    EXPECT_THROW(outline_logo({513, 1, {}}, {0, 0, 0}, 1), std::invalid_argument);
    EXPECT_THROW(outline_logo({16, 16, {}}, {0, 0, 0}, 1), std::invalid_argument);
    EXPECT_THROW(outline_logo({1, 1, {0, 0, 0, 0}}, {0, 0, 0}, 0), std::invalid_argument);
    const RgbaImage empty{1, 1, {0, 0, 0, 0}};
    EXPECT_EQ(outline_logo(empty, {0, 0, 0}, 1).pixels, empty.pixels);
}
TEST(LogoOutline, UsesExistingAggregateStateWithoutClaimingReadyIsHealthy) {
    using audiohole::State;
    EXPECT_EQ(tray_colour(State::Ready).red, tray_colour(State::Stopped).red);
    EXPECT_EQ(tray_colour(State::Running).green, 175);
    EXPECT_EQ(tray_colour(State::Failed).red, 230);
    EXPECT_EQ(tray_colour(State::Degraded).red, 234);
    for (auto state : {State::Starting, State::Stopping, State::Recovering})
        EXPECT_EQ(tray_colour(state).blue, 210);
}
