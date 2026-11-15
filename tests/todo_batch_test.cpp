#include "model/latency_preferences.h"
#include "model/routing_service.h"
#include "presentation/device_events.h"
#include "presentation/messages.h"
#include <gtest/gtest.h>
#include <limits>
using namespace audiohole;
TEST(LatencyPreferences, EightValuesPreserveNullsFractionsAndIndependentEndpointInheritance) {
    Preferences p;
    p.common.sample_rate_hz = 48000;
    TimingValues values{35.5, 15.25, 10, {}, 7, {}, {}, 22};
    ASSERT_TRUE(set_timing_values(p, values).ok);
    EXPECT_EQ(timing_values(p), values);
    EXPECT_EQ(inherit_preferences(p.common, p.source).period_ms, 7);
    EXPECT_EQ(inherit_preferences(p.common, p.destination).period_ms, 10);
    TimingValues roundtrip;
    ASSERT_TRUE(decode_timing_clipboard(encode_timing_clipboard(values), roundtrip).ok);
    EXPECT_EQ(roundtrip, values);
    const auto original = p;
    values[3] = 1.5;
    EXPECT_FALSE(set_timing_values(p, values).ok);
    EXPECT_EQ(p, original);
    values[3].reset();
    values[0] = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(set_timing_values(p, values).ok);
    EXPECT_FALSE(decode_timing_clipboard("{\"format\":1,\"format\":2}", roundtrip).ok);
    EXPECT_FALSE(
        decode_timing_clipboard("{\"format\":\"audiohole-timing-settings\",\"schema_version\":1,"
                                "\"values\":[0,null,null,null,null,null,null,null]}",
                                roundtrip)
            .ok);
}
TEST(LatencyPreferences, PresetsUseMatchingEvidenceAndNeverInventDevicePeriods) {
    Preferences p;
    const auto unknown = timing_preset(2, nullptr, nullptr, p);
    EXPECT_FALSE(unknown[0]);
    EXPECT_FALSE(unknown[1]);
    EXPECT_FALSE(unknown[4]);
    EXPECT_FALSE(unknown[5]);
    Endpoint e;
    auto info = std::make_shared<DeviceInspection>();
    info->default_period_ms = 10;
    info->minimum_period_ms = 7;
    info->period_sample_rate_hz = 48000;
    info->period_channels = 2;
    info->period_sample_format = "float32";
    e.inspection = info;
    EXPECT_EQ(timing_preset(2, &e, &e, p)[4], 10);
    EXPECT_EQ(timing_preset(2, &e, &e, p, 22)[1], 15);
    EXPECT_FALSE(timing_preset(2, &e, &e, p, 10)[1]);
    EXPECT_EQ(timing_preset(0, &e, &e, p)[4], 7);
    p.source.sample_rate_hz = 44100;
    EXPECT_FALSE(timing_preset(2, &e, &e, p)[4]);
    EXPECT_EQ(timing_preset(2, &e, &e, p)[5], 10);
}
TEST(ErrorResponses, AuthoredPoolsHaveFifteenOwnedExpressionsAndNoImmediateRepeats) {
    presentation::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (const auto* id : {"default", "uwu"})
        for (const auto event :
             {presentation::Message::ErrorMinor, presentation::Message::ErrorMajor}) {
            const auto pool = messages.variants(event, id);
            ASSERT_EQ(pool.size(), 15u);
            const auto first = messages.show(event, id, 1);
            const auto again = messages.show(event, id, 1);
            const auto next = messages.show(event, id, 2);
            EXPECT_EQ(first.response_id, again.response_id);
            EXPECT_NE(first.response_id, next.response_id);
            EXPECT_FALSE(first.expression.empty());
            EXPECT_FALSE(first.technical.empty());
        }
}
TEST(DeviceNotices, HiddenTransitionsStaySilentAndUnchangedUnhideDoesNotInventConnection) {
    presentation::DeviceEvents events;
    Endpoint e;
    e.id = "hidden";
    e.hidden = true;
    e.availability = Availability::Unavailable;
    EXPECT_TRUE(events.observe({e}).empty());
    e.availability = Availability::Available;
    EXPECT_TRUE(events.observe({e}).empty());
    e.availability = Availability::Unavailable;
    EXPECT_TRUE(events.observe({e}).empty());
    e.hidden = false;
    EXPECT_TRUE(events.observe({e}).empty());
    e.availability = Availability::Available;
    ASSERT_EQ(events.observe({e}).size(), 1u);
}

TEST(RoutingProgress, CountsOnlyCompleteEnabledHolesFromFrozenIntent) {
    Snapshot state;
    Profile p;
    p.id = "profile";
    Hole h;
    h.id = "one";
    h.duplex = false;
    p.holes.push_back(h);
    h.id = "two";
    h.duplex = true;
    p.holes.push_back(h);
    h.id = "off";
    h.enabled = false;
    p.holes.push_back(h);
    state.routing_intent = std::make_shared<const Profile>(p);
    state.armed = true;
    PathStatus path;
    path.hole_id = "one";
    path.state = State::Running;
    state.paths.push_back(path);
    path.hole_id = "two";
    state.paths.push_back(path);
    EXPECT_EQ(routing_progress(state), "1/2 running");
    path.reverse = true;
    state.paths.push_back(path);
    EXPECT_EQ(routing_progress(state), "2/2 running");
    state.paths.back().state = State::Ready;
    EXPECT_EQ(routing_progress(state), "1/2 running");
    state.armed = false;
    EXPECT_TRUE(routing_progress(state).empty());
}
