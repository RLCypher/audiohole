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

TEST(ScanProgress, InactiveDevicesNeverClaimLiveCapabilityQueries) {
    ScanProgressText text;
    for (const auto* state : {"not_present", "unplugged", "disabled", "unknown"}) {
        const auto discovery = text.consume({"scope_selection",
                                             "private-id",
                                             1,
                                             0,
                                             {{"device_name", "Katana"},
                                              {"direction", "capture"},
                                              {"state", state},
                                              {"capability_queries", "metadata_only"}}});
        EXPECT_NE(discovery.find("Katana"), std::string::npos);
        EXPECT_NE(discovery.find("cached metadata only"), std::string::npos);
        EXPECT_NE(discovery.find("live capability queries skipped"), std::string::npos);
        EXPECT_EQ(discovery.find("selected for live"), std::string::npos);
        const auto observation =
            text.consume({"selected_endpoint_observation", "private-id", 0, 1, {{"state", state}}});
        EXPECT_NE(observation.find("Cached metadata observation"), std::string::npos);
        EXPECT_NE(observation.find(state), std::string::npos);
        EXPECT_EQ(observation.find("private-id"), std::string::npos);
    }
}

TEST(ScanProgress, LookupFailuresKeepApiStateAndErrorWithoutExposingRawIds) {
    ScanProgressText text;
    audiohole::scanner::Endpoint endpoint;
    endpoint.endpoint_id = "private-id";
    endpoint.direction = "render";
    endpoint.state = "not_present";
    const auto diagnostic = audiohole::scanner::endpoint_lookup_failure(
        endpoint, "IMMDevice::OpenPropertyStore", "0x80070490", "Metadata unavailable");
    const auto absent =
        text.consume({"absent_endpoint_observation", endpoint.endpoint_id, 1, 1, diagnostic});
    EXPECT_NE(absent.find("Absent historical endpoint"), std::string::npos);
    EXPECT_NE(absent.find("no live tests attempted"), std::string::npos);
    EXPECT_NE(absent.find("state: not_present"), std::string::npos);
    EXPECT_NE(absent.find("IMMDevice::OpenPropertyStore"), std::string::npos);
    EXPECT_NE(absent.find("0x80070490"), std::string::npos);
    EXPECT_EQ(absent.find(endpoint.endpoint_id), std::string::npos);
    endpoint.state = "active";
    const auto failure = audiohole::scanner::endpoint_lookup_failure(
        endpoint, "IMMDevice::OpenPropertyStore", "0x80070005", "Access denied");
    const auto failed =
        text.consume({"endpoint_lookup_failed", endpoint.endpoint_id, 1, 1, failure});
    EXPECT_NE(failed.find("Endpoint lookup failed"), std::string::npos);
    EXPECT_NE(failed.find("state: active"), std::string::npos);
    EXPECT_NE(failed.find("0x80070005"), std::string::npos);
    EXPECT_EQ(failed.find("Absent historical endpoint"), std::string::npos);
}
