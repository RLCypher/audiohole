#pragma once

#include "audiohole/engine/backend.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>

namespace audiohole::engine::detail {

// Preserve the native code while distinguishing ownership refusal from a
// malformed format/buffer. No process-owner identity can be inferred from it.
inline Issue native_open_issue(const char* stage, std::uint32_t code) {
    std::ostringstream native;
    native << "0x" << std::hex << std::uppercase << code;
    const bool in_use = code == 0x8889000Au;
    const bool exclusive_disabled = code == 0x8889000Eu;
    const bool invalid_period = code == 0x88890020u;
    return {in_use                                       ? ErrorCode::ResourceConflict
            : code == 0x88890004u                        ? ErrorCode::UnavailableEndpoint
            : code == 0x88890008u || exclusive_disabled || invalid_period
                                                        ? ErrorCode::UnsupportedRequirement
                                                         : ErrorCode::BackendFailure,
            stage,
            std::string(stage) + " failed: HRESULT " + native.str() + "." +
                (in_use ? " Windows reports the endpoint is in use (AUDCLNT_E_DEVICE_IN_USE). "
                          "Check the endpoint's Allow applications to take exclusive control and "
                          "Give exclusive mode applications priority settings. Another exclusive "
                          "owner must release it before retrying; session listings do not identify "
                          "the owner. Stop and Start after resolving the conflict."
                        : exclusive_disabled
                              ? " Windows disallows exclusive access for this endpoint. Check "
                                "Allow applications to take exclusive control of this device."
                        : invalid_period
                              ? " The driver rejected the requested device processing interval."
                              : ""),
            native.str()};
}

// Initialization-only format selection, shared by native WASAPI and portable
// regressions. The live configured tuple remains first; positive scanner hints
// can fill Automatic fields after a format refusal. No saved override is ever
// changed. Stale hints are harmless because each candidate is queried now.
// Probe false means UNSUPPORTED_FORMAT only. Ownership, permission, invalidation
// and cancellation errors terminate selection instead of masquerading as a
// reason to cycle through formats. No audio thread/callback calls this helper.
template <class Probe>
Result<FormatHint> select_exclusive_format(const EndpointRequirements& required,
                                          const FormatHint& initial,
                                          std::span<const FormatHint> hints,
                                          std::stop_token stop, Probe&& probe) {
    std::vector<FormatHint> attempted;
    attempted.reserve(129);
    const auto count = std::min<std::size_t>(hints.size(), 128);
    for (std::size_t index = 0; index <= count; ++index) {
        if (stop.stop_requested())
            return {std::nullopt,
                    {{ErrorCode::Cancelled, "exclusive_format", "Format selection cancelled."}}};
        const auto& candidate = index == 0 ? initial : hints[index - 1];
        if (candidate.mode != AccessMode::Exclusive ||
            !pcm_geometry(candidate.format).ok() ||
            !check_format(required, candidate.format, "exclusive_format").ok() ||
            std::find(attempted.begin(), attempted.end(), candidate) != attempted.end())
            continue;
        attempted.push_back(candidate);
        auto supported = probe(candidate);
        if (!supported.ok())
            return {std::nullopt, std::move(supported.issues)};
        if (stop.stop_requested())
            return {std::nullopt,
                    {{ErrorCode::Cancelled, "exclusive_format", "Format selection cancelled."}}};
        if (*supported.value)
            return {candidate, {}};
    }
    return {std::nullopt, {native_open_issue("IsExclusiveFormatSupported", 0x88890008u)}};
}

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

// Explicit requests may round by one frame, never by an entire fundamental
// period. Values far from the advertised lattice remain unsupported.
inline std::optional<std::uint32_t> requested_shared_period(double milliseconds, int rate,
                                                            std::uint32_t fundamental,
                                                            std::uint32_t minimum,
                                                            std::uint32_t maximum) noexcept {
    if (!std::isfinite(milliseconds) || milliseconds <= 0 || rate <= 0 || !fundamental ||
        !minimum || minimum > maximum)
        return {};
    const double frames = milliseconds * rate / 1000;
    const double chosen = std::round(frames / fundamental) * fundamental;
    if (!std::isfinite(chosen) || chosen < minimum || chosen > maximum ||
        std::abs(chosen - frames) > 1.000001)
        return {};
    return static_cast<std::uint32_t>(chosen);
}

inline std::uint32_t render_ahead_target(const StreamConfiguration& stream,
                                         LatencyPreset) noexcept {
    const auto capacity = stream.allocated_buffer_frames.value_or(0);
    if (stream.applied_render_ahead_request_ms && stream.timing_sample_rate_hz) {
        const double requested =
            *stream.applied_render_ahead_request_ms * *stream.timing_sample_rate_hz / 1000;
        const double frames = std::max(1.0, std::round(requested));
        if (!std::isfinite(requested) || requested <= 0 || frames > capacity)
            return 0; // Opening diagnoses an unsupported explicit lead, without narrowing overflow.
        return static_cast<std::uint32_t>(frames);
    }
    // Exclusive packets must fill their allocation. Shared lead is independent
    // of capacity, including when a driver rounds a capacity request upward.
    if (stream.mode != AccessMode::Shared || !stream.timing_sample_rate_hz ||
        *stream.timing_sample_rate_hz <= 0)
        return capacity;
    // One actual engine period plus a 1-ms submission margin is an opt-in test
    // setting, not a production threshold. Allocation can remain larger while
    // unfilled capacity adds no intentional render lead. Late wakes can still
    // starve Windows; padding observations do not prove a hardware underrun.
    const auto margin = (std::uint64_t(*stream.timing_sample_rate_hz) + 999) / 1000;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        capacity, stream.period_frames ? std::uint64_t(*stream.period_frames) + margin
                                       : (21ULL * *stream.timing_sample_rate_hz + 999) / 1000));
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
    if (!awaiting_first_transfer || stream.mode != AccessMode::Shared || !stream.period_frames ||
        !*stream.period_frames || !stream.timing_sample_rate_hz ||
        *stream.timing_sample_rate_hz != stream.client_format.sample_rate_hz)
        return result;
    result.pipeline_frames = std::min(requested_frames, *stream.period_frames);
    result.prefix_silence_frames = requested_frames - result.pipeline_frames;
    return result;
}

} // namespace audiohole::engine::detail
