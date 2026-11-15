#include "stream_pipeline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace audiohole::engine::detail {
namespace {
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "The validation data path requires lock-free 64-bit atomics.");
static_assert(std::atomic<std::int64_t>::is_always_lock_free);

float decode(const unsigned char* bytes, SampleFormat format) noexcept {
    if (format == SampleFormat::Float32) {
        float value;
        std::memcpy(&value, bytes, 4);
        return std::isfinite(value) ? value : 0.0f;
    }
    if (format == SampleFormat::PcmS16) {
        std::int16_t value;
        std::memcpy(&value, bytes, 2);
        return static_cast<float>(value) / 32768.0f;
    }
    if (format == SampleFormat::PcmS24Packed) {
        std::uint32_t bits =
            bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16);
        if (bits & 0x800000)
            bits |= 0xff000000;
        std::int32_t value;
        std::memcpy(&value, &bits, 4);
        return static_cast<float>(value) / 8388608.0f;
    }
    // WAVEFORMATEXTENSIBLE valid bits are left aligned. Both s32 and s24-in-32
    // consequently use the full container scale when decoded to float.
    std::int32_t value;
    std::memcpy(&value, bytes, 4);
    return static_cast<float>(static_cast<double>(value) / 2147483648.0);
}

void encode(unsigned char* bytes, float sample, SampleFormat format) noexcept {
    if (format == SampleFormat::Float32) {
        std::memcpy(bytes, &sample, 4);
        return;
    }
    const double value = std::isfinite(sample) ? std::clamp(double(sample), -1.0, 1.0) : 0.0;
    if (format == SampleFormat::PcmS16) {
        const auto integer =
            static_cast<std::int16_t>(std::clamp(value * 32768.0, -32768.0, 32767.0));
        std::memcpy(bytes, &integer, 2);
        return;
    }
    if (format == SampleFormat::PcmS24Packed || format == SampleFormat::PcmS24In32) {
        const auto integer =
            static_cast<std::int32_t>(std::clamp(value * 8388608.0, -8388608.0, 8388607.0));
        if (format == SampleFormat::PcmS24In32) {
            const auto aligned = integer * 256; // Multiplication avoids shifting a negative.
            std::memcpy(bytes, &aligned, 4);
        } else {
            const auto bits = static_cast<std::uint32_t>(integer);
            bytes[0] = static_cast<unsigned char>(bits);
            bytes[1] = static_cast<unsigned char>(bits >> 8);
            bytes[2] = static_cast<unsigned char>(bits >> 16);
        }
        return;
    }
    const auto integer =
        static_cast<std::int32_t>(std::clamp(value * 2147483648.0, -2147483648.0, 2147483647.0));
    std::memcpy(bytes, &integer, 4);
}
// Select once per callback; the compiler specializes the inner conversion
// loop for this fixed PCM container instead of branching for every sample.
template <class Kernel> void with_sample_format(SampleFormat format, Kernel&& kernel) noexcept {
    switch (format) {
    case SampleFormat::Float32:
        kernel.template operator()<SampleFormat::Float32>();
        break;
    case SampleFormat::PcmS16:
        kernel.template operator()<SampleFormat::PcmS16>();
        break;
    case SampleFormat::PcmS24Packed:
        kernel.template operator()<SampleFormat::PcmS24Packed>();
        break;
    case SampleFormat::PcmS24In32:
        kernel.template operator()<SampleFormat::PcmS24In32>();
        break;
    case SampleFormat::PcmS32:
        kernel.template operator()<SampleFormat::PcmS32>();
        break;
    }
}
} // namespace

StreamPipeline::StreamPipeline(StreamFormat source, StreamFormat destination,
                               const std::vector<ChannelRoute>& routes, bool experimental,
                               double target_milliseconds, bool explicit_target)
    : source_(source), destination_(destination), experimental_(experimental),
      explicit_target_(explicit_target) {
    const auto input = pcm_geometry(source);
    const auto output = pcm_geometry(destination);
    auto mapping = compile_mapping(source.channel_count, destination.channel_count, routes);
    if (!input.ok() || !output.ok() || !mapping.ok())
        throw std::invalid_argument("Invalid processing format or channel mapping.");
    mapping_ = *mapping.value;
    const auto indexes = mapping_.source_indexes();
    identity_map_ = source.channel_count == destination.channel_count;
    for (int channel = 0; channel < destination.channel_count; ++channel)
        identity_map_ = identity_map_ && indexes[channel] == channel;
    mono_pair_ = source.channel_count == 1 && destination.channel_count == 2 && indexes[0] == 0 &&
                 indexes[1] == 0;
    input_stride_ = input.value->block_align_bytes;
    output_stride_ = output.value->block_align_bytes;
    if (!std::isfinite(target_milliseconds) || target_milliseconds <= 0 ||
        target_milliseconds > 1000)
        throw std::invalid_argument("Queue target must be positive and at most 1000 ms.");
    // Prototype constants, NOT a production recommendation or latency promise:
    // at least 200-ms capacity and normally 20-ms target, at most 16 MiB/route.
    // An actual large endpoint block needs a proportionately larger target.
    // Capacity and target are separate; actual occupancy is reported to the host.
    const double capacity_ms = std::max(200.0, target_milliseconds * 2);
    capacity_ = std::max<std::uint32_t>(
        64, static_cast<std::uint32_t>(std::uint64_t(source.sample_rate_hz) * capacity_ms / 1000));
    target_ =
        static_cast<std::uint32_t>(std::ceil(source.sample_rate_hz * target_milliseconds / 1000));
    if (explicit_target && target_ < 2)
        throw std::invalid_argument("Queue target requires at least two source frames.");
    target_ = std::max(2u, target_);
    const auto samples = std::size_t(capacity_) * std::size_t(source.channel_count);
    if (samples > 4 * 1024 * 1024)
        throw std::invalid_argument("Prototype queue would exceed 16 MiB for this route.");
    queue_.resize(samples);
    active_target_.store(target_, std::memory_order_relaxed);
}

unsigned queue_target_milliseconds(const NegotiatedRoute& route, LatencyPreset preset) {
    // A 100-ms output callback cannot be serviced reliably by a 20-ms target.
    // Derive a conservative prototype target from both actual period domains,
    // using the preset floor when the period is unknown. Allocation is capacity.
    // Low reduces only the floor, not the two-period reserve or the observed
    // packet guard in render(). A 10-ms device can therefore still need 20 ms.
    std::uint64_t target = preset == LatencyPreset::Low ? 5 : 20;
    for (const auto* stream : {&route.source, &route.destination}) {
        const auto frames = stream->period_frames;
        const auto rate = stream->timing_sample_rate_hz;
        if (frames && rate && *rate > 0)
            target = std::max<std::uint64_t>(target, (2000ULL * *frames + *rate - 1) / *rate);
    }
    if (target > 1000)
        throw std::invalid_argument("Endpoint blocks exceed prototype queue-duration bounds.");
    return static_cast<unsigned>(target);
}

bool StreamPipeline::push(const void* input, std::size_t frames, bool silent) noexcept {
    if (frames == 0)
        return true;
    if ((!input && !silent) || frames > std::numeric_limits<std::uint32_t>::max())
        return false;
    const auto packet = static_cast<std::uint32_t>(frames);
    packet_min_.store(std::min(packet_min_.load(std::memory_order_relaxed), packet),
                      std::memory_order_relaxed);
    packet_max_.store(std::max(packet_max_.load(std::memory_order_relaxed), packet),
                      std::memory_order_relaxed);
    if (!explicit_target_ && !experimental_)
        recent_capture_peak_.store(capture_history_.record(packet, source_.sample_rate_hz),
                                   std::memory_order_relaxed);
    captured_.fetch_add(frames, std::memory_order_relaxed);
    if (frames > capacity_) {
        overruns_.fetch_add(1, std::memory_order_relaxed);
        capacity_insufficient_.store(true, std::memory_order_relaxed);
        return false;
    }
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    if (head - tail > capacity_ || frames > capacity_ - (head - tail)) {
        overruns_.fetch_add(1, std::memory_order_relaxed);
        return false; // Producer never overwrites unread consumer-owned frames.
    }
    const auto* bytes = static_cast<const unsigned char*>(input);
    const auto sample_bytes = input_stride_ / static_cast<std::size_t>(source_.channel_count);
    // At most two contiguous spans. The producer never calculates modulo per
    // frame/sample, and release of head still publishes only the complete packet.
    std::size_t written = 0;
    auto index = static_cast<std::uint32_t>(head % capacity_);
    with_sample_format(source_.sample_format, [&]<SampleFormat format>() noexcept {
        while (written < frames) {
            const auto count = std::min<std::size_t>(frames - written, capacity_ - index);
            auto* destination = queue_.data() + std::size_t(index) * source_.channel_count;
            const auto samples = count * source_.channel_count;
            if (silent)
                std::fill_n(destination, samples, 0.0f);
            else {
                const auto* source = bytes + written * input_stride_;
                for (std::size_t sample = 0; sample < samples; ++sample)
                    destination[sample] = decode(source + sample * sample_bytes, format);
            }
            written += count;
            index = 0;
        }
    });
    head_.store(head + frames, std::memory_order_release);
    return true;
}

bool StreamPipeline::would_starve(std::size_t frames) const noexcept {
    if (!primed_ || frames == 0)
        return false;
    const auto available =
        head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_relaxed);
    // Both candidate controllers clamp correction to +/-0.002. Include the
    // current fractional phase and interpolation lookahead. A false positive
    // can request one bounded adapter wait; a query never consumes/changes PCM.
    const double maximum_step =
        double(source_.sample_rate_hz) / destination_.sample_rate_hz * 1.002;
    return double(available) < std::ceil(phase_ + double(frames) * maximum_step) + 1;
}

std::size_t StreamPipeline::render(void* output, std::size_t frames,
                                   const RenderTiming& timing) noexcept {
    if (!output || frames == 0)
        return 0;
    auto* bytes = static_cast<unsigned char*>(output);
    const auto head = head_.load(std::memory_order_acquire);
    auto tail = tail_.load(std::memory_order_relaxed);
    const auto available = head - tail;
    if (frames > std::numeric_limits<std::uint32_t>::max())
        return 0; // Adapters supply bounded UINT32 frame counts; invalid private caller.
    const auto block = static_cast<std::uint32_t>(frames);
    render_min_.store(std::min(render_min_.load(std::memory_order_relaxed), block),
                      std::memory_order_relaxed);
    const auto largest_render = std::max(render_max_.load(std::memory_order_relaxed), block);
    render_max_.store(largest_render, std::memory_order_relaxed);
    // The experimental DLL did not tolerate guard reductions in stress tests;
    // preserve its original lifetime policy independently of kernel optimization.
    const bool recover_queue = !explicit_target_ && !experimental_;
    const auto recent_render =
        recover_queue ? render_history_.record(block, destination_.sample_rate_hz) : largest_render;
    // Engine periods are not actual capture packet sizes. In 0.1, periodic
    // 20-ms packets plus 10-ms render requests exhausted the margin even with
    // identical clocks. Reserve one observed capture packet, one render block
    // expressed in SOURCE frames, and interpolation lookahead. No RT resize.
    const auto source_frames = [&](std::uint32_t output_frames) noexcept {
        return static_cast<std::uint64_t>(std::ceil(double(output_frames) * source_.sample_rate_hz /
                                                    destination_.sample_rate_hz));
    };
    // A transient batch may grow the guard immediately, but need not set the
    // session's latency forever. Only recent source/render history determines
    // a reduction. Keep the negotiated period floor and drift-correction algorithm;
    // lowering a target never drops audio or forces a catch-up render burst.
    const auto capture_peak = recover_queue ? recent_capture_peak_.load(std::memory_order_relaxed)
                                            : packet_max_.load(std::memory_order_relaxed);
    auto desired_target =
        explicit_target_
            ? target_
            : std::max<std::uint64_t>(target_, capture_peak + source_frames(recent_render) + 2);
    if (!explicit_target_ && recovery_floor_)
        desired_target = std::max<std::uint64_t>(
            desired_target,
            std::max<std::uint64_t>(recovery_floor_, packet_max_.load(std::memory_order_relaxed) +
                                                         source_frames(largest_render) + 2));
    const auto effective_target =
        static_cast<std::uint32_t>(std::min<std::uint64_t>(desired_target, capacity_ - 2));
    if (desired_target > capacity_ - 2)
        capacity_insufficient_.store(true, std::memory_order_relaxed);
    active_target_.store(effective_target, std::memory_order_relaxed);
    if (recover_queue) {
        largest_auto_target_ = std::max(largest_auto_target_, effective_target);
        if (previous_target_ && effective_target < previous_target_) {
            automatic_reduced_ = true;
            queue_reductions_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (effective_target != previous_target_) {
        filtered_error_ = integral_ = 0;
        previous_target_ = effective_target;
    }
    if (!primed_ && available >= effective_target)
        primed_ = true;
    const double dt = double(frames) / destination_.sample_rate_hz;
    const double target_seconds = double(effective_target) / source_.sample_rate_hz;
    const double error_seconds = (double(available) - effective_target) / source_.sample_rate_hz;
    double correction = 0;
    if (primed_) {
        filtered_error_ += (error_seconds - filtered_error_) * (dt / (0.5 + dt));
        // These gains/limits are explicit validation settings. They have not
        // been selected as production policy or validated on Windows hardware.
        // Positive queue error must INCREASE input consumption per output frame.
        if (experimental_) {
            constexpr double omega = 0.06283185307179586; // illustrative 0.01-Hz DLL
            integral_ = std::clamp(integral_ + omega * omega * filtered_error_ * dt, -0.002, 0.002);
            correction = 1.4142135623730951 * omega * filtered_error_ + integral_;
        } else {
            const double normalized = filtered_error_ / target_seconds;
            constexpr double integral_gain = 0.0001;
            // q_dot = -correction and e = (q-target)/target give
            // s² + (Kp/target_seconds)s + Ki/target_seconds. Kp below gives
            // critical damping in that ideal model; 0.1's fixed Kp=.0005 was
            // underdamped at a 20-ms target. Packet jitter/hardware still need
            // validation; the smoothing pole is not part of this ideal model.
            const double proportional_gain = 2 * std::sqrt(integral_gain * target_seconds);
            const double candidate = std::clamp(integral_ + normalized * dt, -10.0, 10.0);
            const double unconstrained = proportional_gain * normalized + integral_gain * candidate;
            // Do not accumulate integral in the same direction as saturation.
            if (std::abs(unconstrained) <= 0.002 || unconstrained * normalized <= 0)
                integral_ = candidate;
            correction = proportional_gain * normalized + integral_gain * integral_;
        }
    } else {
        // Priming is not a clock-error observation. Carrying its deficit into
        // the controller biases the first running samples.
        filtered_error_ = integral_ = 0;
    }
    correction = std::clamp(correction, -0.002, 0.002);
    correction_ppb_.store(static_cast<std::int64_t>(correction * 1e9), std::memory_order_relaxed);
    const double step =
        (double(source_.sample_rate_hz) / destination_.sample_rate_hz) * (1.0 + correction);
    const auto output_sample_bytes = output_stride_ / std::size_t(destination_.channel_count);
    const auto indexes = mapping_.source_indexes();
    auto read_index = static_cast<std::uint32_t>(tail % capacity_);
    std::size_t produced = 0;
    bool starved = false;
    with_sample_format(destination_.sample_format, [&]<SampleFormat format>() noexcept {
        for (; primed_ && produced < frames; ++produced) {
            // phase_ is normalized to [0,1) after every frame. read_index is
            // tail modulo capacity, advanced once per consumed frame block.
            const float fraction = static_cast<float>(phase_);
            const auto lookahead = fraction == 0 ? 1u : 2u;
            if (head - tail < lookahead) {
                primed_ = false;
                underruns_.fetch_add(1, std::memory_order_relaxed);
                starved = true;
                phase_ = filtered_error_ = integral_ = 0;
                tail = head;
                break;
            }
            const auto next_index = read_index + 1 == capacity_ ? 0 : read_index + 1;
            const auto* current = queue_.data() + std::size_t(read_index) * source_.channel_count;
            const auto* next = queue_.data() + std::size_t(next_index) * source_.channel_count;
            const auto interpolate = [&](int channel) noexcept {
                const auto a = current[channel];
                const auto b = fraction == 0 ? a : next[channel];
                return a + (b - a) * fraction;
            };
            auto* output_frame = bytes + produced * output_stride_;
            if (identity_map_) {
                for (int channel = 0; channel < destination_.channel_count; ++channel)
                    encode(output_frame + channel * output_sample_bytes, interpolate(channel),
                           format);
            } else if (mono_pair_) {
                const auto sample = interpolate(0);
                encode(output_frame, sample, format);
                encode(output_frame + output_sample_bytes, sample, format);
            } else {
                // Compilation already validated widths/indexes. Unmapped outputs
                // remain silent; no repeated span/overlap validation per frame.
                for (int channel = 0; channel < destination_.channel_count; ++channel)
                    encode(output_frame + channel * output_sample_bytes,
                           indexes[channel] < 0 ? 0.0f : interpolate(indexes[channel]), format);
            }
            phase_ += step;
            const auto consumed = static_cast<std::uint64_t>(phase_);
            if (consumed > head - tail) {
                tail = head;
                primed_ = false;
                phase_ = filtered_error_ = integral_ = 0;
                underruns_.fetch_add(1, std::memory_order_relaxed);
                starved = true;
            } else {
                tail += consumed;
                phase_ -= double(consumed);
                // consumed cannot exceed capacity: only published frames may
                // be consumed, so one subtraction handles ring wraparound.
                read_index += static_cast<std::uint32_t>(consumed);
                if (read_index >= capacity_)
                    read_index -= capacity_;
            }
        }
    });
    // Priming/starvation is silent and counted; never repeat stale microphone
    // samples. The host distinguishes transferred frames from total render frames.
    std::memset(bytes + produced * output_stride_, 0, (frames - produced) * output_stride_);
    if (starved) {
        if (!explicit_target_ && automatic_reduced_ && !recovery_floor_) {
            recovery_floor_ = largest_auto_target_;
            queue_backoffs_.fetch_add(1, std::memory_order_relaxed);
        }
        const auto write = event_write_.load(std::memory_order_relaxed);
        const auto read = event_read_.load(std::memory_order_acquire);
        if (write - read < underrun_events_.size()) {
            auto& event = underrun_events_[write % underrun_events_.size()];
            event.sequence = underruns_.load(std::memory_order_relaxed);
            event.rendered_frame_index = rendered_.load(std::memory_order_relaxed) + produced;
            event.source_published_frames = head;
            event.source_frames_available_before = static_cast<std::uint32_t>(available);
            event.requested_output_frames = block;
            event.produced_output_frames = static_cast<std::uint32_t>(produced);
            event.controller_target_source_frames = effective_target;
            event.requested_correction_ppm = correction * 1e6;
            event.render_qpc_100ns = timing.render_qpc_100ns;
            event.last_capture_device_position_frames = timing.last_capture_device_position_frames;
            event.last_capture_qpc_100ns = timing.last_capture_qpc_100ns;
            event.capture_delivery_gap_last_us = timing.capture_delivery_gap_last_us;
            event.capture_packet_age_last_us = timing.capture_packet_age_last_us;
            event.native_padding_before_frames = timing.native_padding_before_frames;
            event.native_render_request_frames = timing.native_render_request_frames;
            event_write_.store(write + 1, std::memory_order_release);
        } else {
            dropped_events_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    tail_.store(tail, std::memory_order_release);
    transferred_.fetch_add(produced, std::memory_order_relaxed);
    rendered_.fetch_add(frames, std::memory_order_relaxed);
    return produced;
}

RouteObservation StreamPipeline::observe() {
    RouteObservation result;
    result.captured_frames = captured_.load(std::memory_order_relaxed);
    result.rendered_frames = rendered_.load(std::memory_order_relaxed);
    result.transferred_frames = transferred_.load(std::memory_order_relaxed);
    result.underruns = underruns_.load(std::memory_order_relaxed);
    result.overruns = overruns_.load(std::memory_order_relaxed);
    result.discontinuities = discontinuities_.load(std::memory_order_relaxed);
    result.startup_discontinuities = startup_discontinuities_.load(std::memory_order_relaxed);
    result.timestamp_errors = timestamp_errors_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    const auto head = head_.load(std::memory_order_acquire);
    result.queued_frames =
        static_cast<std::uint32_t>(std::min<std::uint64_t>(head - tail, capacity_));
    result.correction_ppm = double(correction_ppb_.load(std::memory_order_relaxed)) / 1000.0;
    result.controller_target_source_frames = active_target_.load(std::memory_order_relaxed);
    result.automatic_queue_reductions = queue_reductions_.load(std::memory_order_relaxed);
    result.automatic_queue_backoffs = queue_backoffs_.load(std::memory_order_relaxed);
    auto read = event_read_.load(std::memory_order_relaxed);
    const auto write = event_write_.load(std::memory_order_acquire);
    // At most the fixed mailbox capacity is consumed per observation. The audio
    // writer cannot reuse any of these slots before the release below.
    while (read < write) {
        last_observed_underrun_ = underrun_events_[read % underrun_events_.size()];
        ++read;
    }
    event_read_.store(read, std::memory_order_release);
    result.last_underrun = last_observed_underrun_;
    result.dropped_underrun_contexts = dropped_events_.load(std::memory_order_relaxed);
    if (packet_max_.load(std::memory_order_relaxed)) {
        result.capture_packet_min_frames = packet_min_.load(std::memory_order_relaxed);
        result.capture_packet_max_frames = packet_max_.load(std::memory_order_relaxed);
    }
    if (render_max_.load(std::memory_order_relaxed)) {
        result.render_request_min_frames = render_min_.load(std::memory_order_relaxed);
        result.render_request_max_frames = render_max_.load(std::memory_order_relaxed);
    }
    if (capacity_insufficient_.load(std::memory_order_relaxed))
        result.issues.push_back(
            {ErrorCode::UnsupportedRequirement, "fifo_capacity",
             "Observed packet/block size exceeds this route's preallocated safety margin."});
    return result;
}
} // namespace audiohole::engine::detail
