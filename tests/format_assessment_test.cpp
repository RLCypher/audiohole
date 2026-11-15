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
    for (auto& endpoint : s.endpoints)
        endpoint.capabilities[0].format.representation = "pcm_s24_in_32";
    s.endpoints[0].capabilities[0].format.wave = WaveFormat{32, 24, 65534, 4, "PCM", 4, 192000};
    s.endpoints[1].capabilities[0].format.wave = WaveFormat{32, 24, 65534, 3, "PCM", 8, 384000};
    EXPECT_EQ(assess_formats(s, direction(), "wasapi").candidates.size(), 1u);
    s.endpoints[1].capabilities[0].format.wave->valid_bits = 32;
    EXPECT_TRUE(assess_formats(s, direction(), "wasapi").candidates.empty());
}
TEST(FormatAssessment, EqualChannelFormatsMatchDespiteDifferentNativeDescriptors) {
    auto s = mono_pair();
    auto& a = s.endpoints[0].capabilities[0].format;
    auto& b = s.endpoints[1].capabilities[0].format;
    a.channels = 2;
    a.wave = WaveFormat{32, 32, 3, {}, "", 8, 384000};
    b.wave = WaveFormat{32, 32, 65534, 3, "{00000003-0000-0010-8000-00aa00389b71}", 8, 384000};
    const auto result = assess_formats(s, direction(), "wasapi");
    ASSERT_EQ(result.candidates.size(), 1u);
    EXPECT_FALSE(result.candidates[0].dual_mono);
    EXPECT_NE(result.candidates[0].endpoint_label(true).find("2 ch"), std::string::npos);
    EXPECT_NE(result.candidates[0].endpoint_label(false).find("2 ch"), std::string::npos);
}
TEST(FormatAssessment, MultipleOtherwiseMatchingMonoFormatsHaveIndependentEndpointLabels) {
    auto s = mono_pair();
    for (auto& endpoint : s.endpoints)
        endpoint.capabilities.clear();
    for (int rate : {48000, 44100})
        for (const auto* sample : {"float32", "pcm_s16"}) {
            const int bits = std::string(sample) == "float32" ? 32 : 16;
            Format a{rate, 1, sample,
                     WaveFormat{bits,
                                bits,
                                std::string(sample) == "float32" ? 3 : 1,
                                {},
                                "",
                                bits / 8,
                                rate * bits / 8}};
            Format b{rate, 2, sample,
                     WaveFormat{bits, bits, 65534, 3, "native-guid", bits / 4, rate * bits / 4}};
            s.endpoints[0].capabilities.push_back({a, "Shared", FormatSupport::Supported});
            s.endpoints[1].capabilities.push_back({b, "Shared", FormatSupport::Supported});
        }
    const auto result = assess_formats(s, direction(), "wasapi");
    ASSERT_EQ(result.candidates.size(), 4u);
    for (const auto& candidate : result.candidates) {
        EXPECT_NE(candidate.endpoint_label(true).find("2 ch (dual mono)"), std::string::npos);
        EXPECT_EQ(candidate.endpoint_label(false).find("dual mono"), std::string::npos);
        EXPECT_NE(candidate.endpoint_label(false).find("2 ch"), std::string::npos);
        EXPECT_EQ(candidate.source.channels, 1);
        EXPECT_EQ(candidate.destination.channels, 2);
    }
    EXPECT_EQ(result.explanation.find("conversion remains unverified"), std::string::npos);
}
TEST(FormatAssessment, ChannelCountDifferencesAloneNeverHideMatchingPcmCandidates) {
    auto s = mono_pair();
    for (int capture : {1, 2, 6, 8})
        for (int playback : {1, 2, 6, 8}) {
            s.endpoints[0].capabilities[0].format.channels = capture;
            s.endpoints[1].capabilities[0].format.channels = playback;
            const auto result = assess_formats(s, direction(), "wasapi");
            ASSERT_EQ(result.candidates.size(), 1u) << capture << " → " << playback;
            EXPECT_EQ(result.candidates[0].dual_mono, capture == 1 && playback >= 2);
            EXPECT_EQ(result.candidates[0].source.channels, capture);
            EXPECT_EQ(result.candidates[0].destination.channels, playback);
        }
}
TEST(FormatAssessment, PartialNativeMetadataUsesTheKnownSampleRepresentation) {
    auto s = mono_pair();
    s.endpoints[0].capabilities[0].format.wave = WaveFormat{};
    s.endpoints[1].capabilities[0].format.wave =
        WaveFormat{32, 32, 65534, 3, "native-guid", 8, 384000};
    ASSERT_EQ(assess_formats(s, direction(), "wasapi").candidates.size(), 1u);
    s.endpoints[0].capabilities[0].format.wave->valid_bits = 16;
    EXPECT_TRUE(assess_formats(s, direction(), "wasapi").candidates.empty());
}
