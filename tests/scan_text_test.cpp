#include "presentation/scan_text.h"
#include <gtest/gtest.h>
#include <limits>
using namespace audiohole::presentation;
TEST(ScanSeconds, ExactDecimalsRoundTripWithoutFloatingPointOrTrailingZeroes) {
    for (const unsigned ms : {0u, 1u, 250u, 1000u, 1001u, 1125u, 1250u, 2000u, 30000u, 3600000u,
                              std::numeric_limits<unsigned>::max()})
        EXPECT_EQ(milliseconds_from_seconds(seconds_from_milliseconds(ms)), ms);
    EXPECT_EQ(seconds_from_milliseconds(2000), "2");
    EXPECT_EQ(seconds_from_milliseconds(2001), "2.001");
    EXPECT_EQ(seconds_from_milliseconds(1250), "1.25");
    EXPECT_EQ(milliseconds_from_seconds("2.1"), 2100u);
    EXPECT_EQ(milliseconds_from_seconds("2.010"), 2010u);
}
TEST(ScanSeconds, RejectsExcessPrecisionNondecimalAndOverflowInput) {
    for (const auto* text : {"", "-1", "+2", ".5", "2.", "2.0001", "1e3", "nan", "inf", " 2", "2 s",
                             "4294967.296", "999999999999999999999"})
        EXPECT_FALSE(milliseconds_from_seconds(text)) << text;
}
