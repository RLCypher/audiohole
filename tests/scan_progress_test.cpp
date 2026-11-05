#include "presentation/scan_progress.h"
#include <gtest/gtest.h>
using audiohole::presentation::ScanProgressText;
TEST(ScanProgress, NamesAndRealPlanCountsReplaceEndpointCodes) {
    ScanProgressText text;
    const std::string secret = "{transient-windows-code}";
    auto name = text.consume(
        {"scope_selection",
         secret,
         1,
         0,
         {{"device_name", "PS5 Mic"}, {"state", "active"}, {"capability_queries", "selected"}}});
    EXPECT_NE(name.find("PS5 Mic"), std::string::npos);
    EXPECT_EQ(name.find(secret), std::string::npos);
    text.consume({"deep_plan", {}, 0, 12, {{"devices", 2}}});
    auto device = text.consume({"deep_device_started",
                                secret,
                                0,
                                2,
                                {{"display_name", "PS5 Mic"}, {"direction", "capture"}}});
    EXPECT_NE(device.find("Device 1/2"), std::string::npos);
    EXPECT_NE(device.find("Capture"), std::string::npos);
    auto attempt = text.consume(
        {"deep_attempt_started",
         secret,
         0,
         4,
         {{"mode", "shared"},
          {"format",
           {{"sample_rate_hz", 48000}, {"channel_count", 2}, {"sample_format", "float32"}}}}});
    EXPECT_NE(attempt.find("Configuration 1/4"), std::string::npos);
    EXPECT_NE(attempt.find("48000 Hz"), std::string::npos);
    auto rejected = text.consume(
        {"deep_attempt_finished",
         secret,
         1,
         4,
         {{"classification", "failed"}, {"support", "unsupported"}, {"hresult", "0x88890008"}}});
    EXPECT_NE(rejected.find("candidate format unsupported"), std::string::npos);
    EXPECT_EQ(rejected.find(secret), std::string::npos);
    EXPECT_EQ(text.percent(), 8); // Actual completed configurations / batch plan.
    EXPECT_NE(
        text.consume({"deep_stage", "unknown-code", 0, 4, {{"api", "Cleanup"}, {"phase", "begin"}}})
            .find("Cleaning up"),
        std::string::npos);
    EXPECT_EQ(text.describe({"endpoint_query_failed", "Name unavailable", secret, {}}).find(secret),
              std::string::npos);
    EXPECT_NE(
        text.describe({"endpoint_query_failed", "Name unavailable", secret, {}}).find("PS5 Mic"),
        std::string::npos);
}
