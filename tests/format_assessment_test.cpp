#include "model/format_assessment.h"
#include <gtest/gtest.h>
using namespace audiohole;
namespace {
Snapshot mono_pair() {
    Snapshot s;
    s.endpoints = {{"mic", "Microphone", "", "", true}, {"out", "Playback", "", "", false}};
    s.endpoints[0].capabilities = {{{48000, 1, "float32"}, "Shared", FormatSupport::Supported}};
    s.endpoints[1].capabilities = {{{48000, 2, "float32"}, "Shared", FormatSupport::Supported}};
    return s;
}
Direction direction() {
    Direction d;
    d.source = "mic";
    d.destination = "out";
    return d;
}
} // namespace
TEST(FormatAssessment, MonoStereoCandidatePreservesNativeWidthsAndAllIntent) {
    const auto snapshot = mono_pair();
    const auto d = direction();
    const auto original = d;
    const auto result = assess_formats(snapshot, d, "automatic");
    ASSERT_EQ(result.candidates.size(), 1u);
    EXPECT_EQ(result.candidates[0].source.channels, 1);
    EXPECT_EQ(result.candidates[0].destination.channels, 2);
    EXPECT_TRUE(result.candidates[0].dual_mono);
    EXPECT_NE(result.candidates[0].label().find("2 ch (dual mono)"), std::string::npos);
    EXPECT_EQ(d, original);
    EXPECT_TRUE(recommend(snapshot, d).compatible.empty()); // Engine preparation is unchanged.
}
TEST(FormatAssessment, ExplicitMapAndEndpointRequirementsRemainAuthoritative) {
    auto s = mono_pair();
    auto d = direction();
    d.settings.preferences.channel_map = {{0, 0}};
    auto result = assess_formats(s, d, "wasapi");
    ASSERT_EQ(result.candidates.size(), 1u);
    EXPECT_FALSE(result.candidates[0].dual_mono);
    EXPECT_EQ(d.settings.preferences.channel_map, (std::vector<ChannelRoute>{{0, 0}}));
    d.settings.preferences.channel_map = {{1, 0}};
    EXPECT_TRUE(assess_formats(s, d, "wasapi").candidates.empty());
    d.settings.preferences.channel_map.clear();
    d.settings.preferences.common.channel_count = 2;
    EXPECT_TRUE(assess_formats(s, d, "wasapi").candidates.empty());
    d.settings.preferences.source.channel_count = 1;
    EXPECT_EQ(assess_formats(s, d, "wasapi").candidates.size(), 1u);
}
TEST(FormatAssessment, RemainingFormatDifferencesAndUnsupportedEvidenceAreSpecific) {
    auto s = mono_pair();
    auto& destination = s.endpoints[1].capabilities[0];
    destination.format.rate = 44100;
    auto result = assess_formats(s, direction(), "automatic");
    EXPECT_TRUE(result.candidates.empty());
    EXPECT_NE(result.explanation.find("Sample rate"), std::string::npos);
    destination.format.rate = 48000;
    destination.support = FormatSupport::Unknown;
    EXPECT_TRUE(assess_formats(s, direction(), "automatic").candidates.empty());
    destination.support = FormatSupport::Supported;
    s.endpoints[1].capabilities.push_back(
        {destination.format, "Shared", FormatSupport::Unsupported});
    EXPECT_TRUE(assess_formats(s, direction(), "automatic").candidates.empty());
}
TEST(FormatAssessment, SelectedBackendAvailabilityAndRepresentationAreChecked) {
    auto s = mono_pair();
    for (auto& e : s.endpoints)
        e.capabilities[0].format.representation = "pcm_s24_in_32";
    EXPECT_TRUE(assess_formats(s, direction(), "miniaudio").candidates.empty());
    EXPECT_EQ(assess_formats(s, direction(), "wasapi").candidates.size(), 1u);
    s.routing_backends = {{engine::BackendId::Wasapi, false, "Unavailable"},
                          {engine::BackendId::Miniaudio, true, "Available"}};
    EXPECT_TRUE(assess_formats(s, direction(), "automatic").candidates.empty());
    EXPECT_NE(assess_formats(s, direction(), "wasapi").explanation.find("unavailable"),
              std::string::npos);
    EXPECT_TRUE(assess_formats(s, direction(), "unknown").candidates.empty());
}
TEST(FormatAssessment, MonoAndStereoMasksCanDifferWithoutChangingPcmPrecision) {
    auto s = mono_pair();
    s.endpoints[0].capabilities[0].format.wave = WaveFormat{32, 24, 65534, 4, "PCM", 4, 192000};
    s.endpoints[1].capabilities[0].format.wave = WaveFormat{32, 24, 65534, 3, "PCM", 8, 384000};
    EXPECT_EQ(assess_formats(s, direction(), "wasapi").candidates.size(), 1u);
    s.endpoints[1].capabilities[0].format.wave->valid_bits = 32;
    EXPECT_TRUE(assess_formats(s, direction(), "wasapi").candidates.empty());
}
