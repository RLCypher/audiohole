#pragma once

#include "audiohole/engine/backend.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>

namespace audiohole::engine::detail {

// Pure arithmetic shared by native opening and portable regression tests. No
// Windows interfaces or synthetic endpoint implementation are needed to verify
// legal period selection, frame-domain rounding and bounded render writes.
inline std::optional<std::uint32_t> minimum_shared_period(std::uint32_t fundamental,
                                                          std::uint32_t minimum,
                                                          std::uint32_t maximum) noexcept {
    if (!fundamental || !minimum || minimum > maximum)
        return std::nullopt;
    const auto rounded = (std::uint64_t(minimum) + fundamental - 1) / fundamental * fundamental;
    if (rounded > maximum)
        return std::nullopt;
    return static_cast<std::uint32_t>(rounded);
}

inline std::uint32_t render_ahead_target(const StreamConfiguration& stream,
                                         LatencyPreset preset) noexcept {
    const auto capacity = stream.allocated_buffer_frames.value_or(0);
    // Explicit endpoint buffers retain baseline submission behavior. Unknown
    // periods also retain it rather than guessing a scheduling deadline.
    if (preset != LatencyPreset::Low || stream.mode != AccessMode::Shared ||
        stream.applied_buffer_request_ms || !stream.period_frames || !*stream.period_frames ||
        !stream.timing_sample_rate_hz || *stream.timing_sample_rate_hz <= 0)
        return capacity;
    // One actual engine period plus a 1-ms submission margin is an opt-in test
    // setting, not a production threshold. Allocation can remain larger while
    // unfilled capacity adds no intentional render lead. Late wakes can still
    // starve Windows; padding observations do not prove a hardware underrun.
    const auto margin = (std::uint64_t(*stream.timing_sample_rate_hz) + 999) / 1000;
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(capacity, std::uint64_t(*stream.period_frames) + margin));
}

inline std::uint32_t render_fill_frames(std::uint32_t capacity, std::uint32_t padding,
                                        std::uint32_t target) noexcept {
    // The caller diagnoses impossible padding; arithmetic never underflows or
    // requests more than the currently free allocation, even for bad input.
    target = std::min(target, capacity);
    return padding < target ? target - padding : 0;
}

struct RenderPlan {
    std::uint32_t prefix_silence_frames = 0;
    std::uint32_t pipeline_frames = 0;
};

// Only spend EXISTING Shared-mode padding on a short wait for capture. The
// 1-ms wait and 2-ms submission reserve are validation scheduling constants,
// not a new FIFO target, user buffer override or guaranteed Windows deadline.
// Exclusive event packets have no padding-derived safety budget and never wait.
inline std::uint32_t capture_wait_milliseconds(const StreamConfiguration& destination,
                                               std::uint32_t padding,
                                               bool source_would_starve) noexcept {
    if (!source_would_starve || destination.mode != AccessMode::Shared ||
        !destination.timing_sample_rate_hz || *destination.timing_sample_rate_hz <= 0)
        return 0;
    const auto rate = std::uint64_t(*destination.timing_sample_rate_hz);
    return std::uint64_t(padding) * 1000 >= rate * 3 ? 1 : 0;
}

// While no captured PCM has yet been transferred, a depleted native buffer can
// ask for its full allocation (22 ms in the wired run), not one routine period.
// Restore the requested native lead, but make the surplus a counted SILENT
// prefix and ask the pipeline for at most one period at the end of that buffer.
// This avoids teaching the FIFO controller a permanent startup-only demand.
// After the first valid transfer, the full real demand again reaches the FIFO
// guard unchanged. Explicit buffers, Exclusive and unknown timing retain their
// previous contract. This is priming, not routine sample insertion/recovery.
inline RenderPlan startup_render_plan(const StreamConfiguration& stream,
                                      std::uint32_t requested_frames,
                                      bool awaiting_first_transfer) noexcept {
    RenderPlan result{0, requested_frames};
    if (!awaiting_first_transfer || stream.mode != AccessMode::Shared ||
        stream.applied_buffer_request_ms || !stream.period_frames || !*stream.period_frames ||
        !stream.timing_sample_rate_hz ||
        *stream.timing_sample_rate_hz != stream.client_format.sample_rate_hz)
        return result;
    result.pipeline_frames = std::min(requested_frames, *stream.period_frames);
    result.prefix_silence_frames = requested_frames - result.pipeline_frames;
    return result;
}

} // namespace audiohole::engine::detail
