#include "audiohole/engine/channel_mapping.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace audiohole::engine {
Result<ChannelMapping> compile_mapping(int source_channels, int destination_channels,
                                       const std::vector<ChannelRoute>& routes) {
    if (source_channels < 1 || source_channels > 256 || destination_channels < 1 ||
        destination_channels > 256 || routes.size() > 256)
        return {std::nullopt,
                {{ErrorCode::InvalidConfiguration, "channel_map",
                  "Stream widths must be 1..256; map has at most 256 routes."}}};
    ChannelMapping result;
    result.source_channels_ = source_channels;
    result.destination_channels_ = destination_channels;
    result.source_for_output_.fill(-1);
    if (routes.empty()) {
        for (int channel = 0; channel < std::min(source_channels, destination_channels); ++channel)
            result.source_for_output_[channel] = channel;
    } else {
        for (const auto& route : routes) {
            if (route.source < 0 || route.source >= source_channels || route.destination < 0 ||
                route.destination >= destination_channels ||
                result.source_for_output_[route.destination] != -1)
                return {std::nullopt,
                        {{ErrorCode::InvalidConfiguration, "channel_map",
                          "Map exceeds negotiated widths or assigns an output twice."}}};
            result.source_for_output_[route.destination] = route.source;
        }
    }
    return {result, {}};
}

bool ChannelMapping::process(std::span<const float> source, std::span<float> destination,
                             std::size_t frames) const noexcept {
    if (source_channels_ < 1 || destination_channels_ < 1)
        return false;
    if (frames > std::numeric_limits<std::size_t>::max() /
                     static_cast<std::size_t>(std::max(source_channels_, destination_channels_)))
        return false;
    const auto input_count = frames * static_cast<std::size_t>(source_channels_);
    const auto output_count = frames * static_cast<std::size_t>(destination_channels_);
    if (source.size() < input_count || destination.size() < output_count)
        return false;
    if (frames == 0)
        return true;
    // uintptr comparisons avoid relational comparison of unrelated pointers.
    // Counts are bounded by actual spans; also guard byte-range arithmetic.
    constexpr auto limit = std::numeric_limits<std::uintptr_t>::max();
    if (input_count > limit / sizeof(float) || output_count > limit / sizeof(float))
        return false;
    const auto input_begin = reinterpret_cast<std::uintptr_t>(source.data());
    const auto output_begin = reinterpret_cast<std::uintptr_t>(destination.data());
    const auto input_bytes = input_count * sizeof(float);
    const auto output_bytes = output_count * sizeof(float);
    if (input_begin > limit - input_bytes || output_begin > limit - output_bytes)
        return false;
    if (input_begin < output_begin + output_bytes && output_begin < input_begin + input_bytes)
        return false;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto input_offset = frame * static_cast<std::size_t>(source_channels_);
        const auto output_offset = frame * static_cast<std::size_t>(destination_channels_);
        for (int output = 0; output < destination_channels_; ++output) {
            const int input = source_for_output_[output];
            destination[output_offset + static_cast<std::size_t>(output)] =
                input < 0 ? 0.0f : source[input_offset + static_cast<std::size_t>(input)];
        }
    }
    return true;
}
} // namespace audiohole::engine
