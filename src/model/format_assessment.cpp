#include "model/format_assessment.h"
#include <algorithm>
namespace audiohole {
namespace {
std::string representation(const Format& format) {
    return format.representation == "PCM16" || format.representation == "int16"
               ? "pcm_s16"
               : format.representation;
}
bool matches(const FormatCapability& candidate, const EndpointPreferences& p) {
    const auto& f = candidate.format;
    return (!p.sample_rate_hz || *p.sample_rate_hz == f.rate) &&
           (!p.channel_count || *p.channel_count == f.channels) &&
           (!p.sample_format || *p.sample_format == representation(f)) &&
           (!p.mode || *p.mode == candidate.mode);
}
std::vector<FormatCapability> evidence(const Endpoint& endpoint, const EndpointPreferences& p) {
    auto candidates = endpoint.capabilities;
    if (candidates.empty() && !endpoint.inspection)
        for (const auto& f : endpoint.formats)
            candidates.push_back({f, "Shared", FormatSupport::Supported});
    const auto all = candidates;
    std::erase_if(candidates, [&](const auto& c) {
        return c.support != FormatSupport::Supported ||
               (c.mode != "Shared" && c.mode != "Exclusive") || !matches(c, p) ||
               std::any_of(all.begin(), all.end(), [&](const auto& other) {
                   return other.format == c.format && other.mode == c.mode &&
                          other.support == FormatSupport::Unsupported;
               });
    });
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.mode == "Shared" && b.mode != "Shared";
    });
    return candidates;
}
bool supported_representation(const Format& f, engine::BackendId backend) {
    const auto sample = representation(f);
    return sample == "float32" || sample == "pcm_s16" || sample == "pcm_s32" ||
           (backend != engine::BackendId::Miniaudio &&
            (sample == "pcm_s24_packed" || sample == "pcm_s24_in_32"));
}
} // namespace
std::string FormatCandidate::label() const {
    if (source == destination)
        return source.label();
    if (dual_mono)
        return std::to_string(destination.rate) + " Hz / 2 ch (dual mono) / " +
               destination.representation;
    return "Capture: " + source.label() + " → Playback: " + destination.label();
}
FormatAssessment assess_formats(const Snapshot& snapshot, const Direction& direction,
                                std::string_view requested) {
    FormatAssessment result;
    const auto* source = endpoint(snapshot, direction.source);
    const auto* destination = endpoint(snapshot, direction.destination);
    if (!source || !destination) {
        result.explanation = "Select a capture source and playback destination.";
        return result;
    }
    auto backend = engine::BackendId::Automatic;
    if (requested == "wasapi")
        backend = engine::BackendId::Wasapi;
    else if (requested == "miniaudio")
        backend = engine::BackendId::Miniaudio;
    else if (requested == "experimental")
        backend = engine::BackendId::Experimental;
    else if (requested != "automatic") {
        result.explanation = "Selected routing engine is unknown.";
        return result;
    }
    if (!snapshot.routing_backends.empty()) {
        const auto selected = engine::select_backend(backend, snapshot.routing_backends);
        if (!selected.ok()) {
            result.explanation = "Selected routing engine is unavailable in this build.";
            return result;
        }
        backend = *selected.value;
    } else if (snapshot.capabilities.real_engine) {
        result.explanation = "Routing engine capabilities have not been reported yet.";
        return result;
    }
    const auto& preferences = direction.settings.preferences;
    const auto sources =
        evidence(*source, inherit_preferences(preferences.common, preferences.source));
    const auto destinations =
        evidence(*destination, inherit_preferences(preferences.common, preferences.destination));
    if (sources.empty() || destinations.empty()) {
        result.explanation = std::string(sources.empty() ? "Capture" : "Playback") +
                             " format evidence is missing, unsupported, or does not match the "
                             "explicit format/access requirements.";
        return result;
    }
    bool engine_format = false, rate_pair = false, encoding_pair = false, precision_pair = false,
         width_pair = false;
    for (const auto& capture : sources)
        for (const auto& playback : destinations) {
            const auto& a = capture.format;
            const auto& b = playback.format;
            if (!supported_representation(a, backend) || !supported_representation(b, backend)) {
                continue;
            }
            engine_format = true;
            if (a.rate != b.rate)
                continue;
            rate_pair = true;
            if (representation(a) != representation(b))
                continue;
            encoding_pair = true;
            if (a.wave && b.wave &&
                (a.wave->container_bits != b.wave->container_bits ||
                 a.wave->valid_bits != b.wave->valid_bits))
                continue;
            precision_pair = true;
            // Only this known bridge is added to exact tuple candidates. Other
            // rate/representation/layout conversions remain specifically unverified.
            const bool mono_stereo = a.channels == 1 && b.channels == 2 && a.rate == b.rate &&
                                     representation(a) == representation(b) &&
                                     (!a.wave || !b.wave ||
                                      (a.wave->container_bits == b.wave->container_bits &&
                                       a.wave->valid_bits == b.wave->valid_bits));
            if (a != b && !mono_stereo)
                continue;
            width_pair = true;
            if (std::any_of(preferences.channel_map.begin(), preferences.channel_map.end(),
                            [&](const auto& map) {
                                return map.source < 0 || map.source >= a.channels ||
                                       map.destination < 0 || map.destination >= b.channels;
                            }))
                continue;
            if (std::none_of(result.candidates.begin(), result.candidates.end(),
                             [&](const auto& c) { return c.source == a && c.destination == b; }))
                result.candidates.push_back({a, b, mono_stereo && preferences.channel_map.empty()});
        }
    if (result.candidates.empty()) {
        if (!engine_format)
            result.explanation =
                "Selected engine does not support a reported sample representation. "
                "Miniaudio supports float32, pcm_s16 and pcm_s32; native WASAPI additionally "
                "supports pcm_s24_packed and pcm_s24_in_32.";
        else if (!rate_pair)
            result.explanation =
                "Sample rate conversion remains unverified: no matching reported rates.";
        else if (!encoding_pair)
            result.explanation = "Sample representation conversion remains unverified: no matching "
                                 "reported encodings.";
        else if (!precision_pair)
            result.explanation = "PCM precision conversion remains unverified: reported container "
                                 "or valid-bit widths differ.";
        else if (!width_pair)
            result.explanation = "Channel count or layout conversion remains unverified for the "
                                 "reported endpoint pair.";
        else
            result.explanation =
                "Explicit channel map exceeds the reported capture or playback channel count.";
    } else {
        const bool bridge =
            std::any_of(result.candidates.begin(), result.candidates.end(), [](const auto& c) {
                return c.source.channels == 1 && c.destination.channels == 2;
            });
        result.explanation =
            bridge ? "Compatible mono capture → stereo playback candidate. Capture stays 1 ch; "
                     "playback stays 2 ch. Dual mono duplicates the mono signal; an explicit "
                     "channel map "
                     "stays authoritative. "
                   : "Compatible format candidates from available endpoint evidence. ";
        result.explanation += "Reported compatibility is not a verified running route or measured "
                              "latency. Channel layouts remain unknown where not reported.";
    }
    return result;
}
} // namespace audiohole
