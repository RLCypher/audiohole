#include "audiohole/engine/runtime.hpp"
#include "stream_pipeline.hpp"
#include "wasapi_policy.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace audiohole::engine;
using audiohole::engine::detail::StreamPipeline;

namespace {
int checks = 0;
void expect(bool condition, const char* reason) {
    ++checks;
    if (!condition)
        throw std::runtime_error(reason);
}

void processing() {
    // Exercise the actual conversion/FIFO path as well as the index kernel:
    // mono reaches both ears, stereo remains two independent signals, and a
    // retained manual one-ear map is honored only when the caller activates it.
    for (const int channels : {1, 2}) {
        StreamPipeline automatic({48000, channels, SampleFormat::Float32},
                                  {48000, 2, SampleFormat::Float32}, {}, false);
        std::vector<float> samples(4000 * channels, 0.25f);
        if (channels == 2)
            for (std::size_t frame = 0; frame < 4000; ++frame)
                samples[frame * 2 + 1] = -0.5f;
        expect(automatic.push(samples.data(), 2000) &&
                   automatic.push(samples.data() + 2000 * channels, 2000),
               "automatic channel fixture primes beyond the observed packet guard");
        std::array<float, 512> playback{};
        expect(automatic.render(playback.data(), 256) == 256, "automatic channel fixture renders");
        for (std::size_t frame = 0; frame < 256; ++frame)
            expect(playback[frame * 2] == 0.25f &&
                       playback[frame * 2 + 1] == (channels == 1 ? 0.25f : -0.5f),
                   "pipeline duplicates only mono, preserving stereo separation");
    }
    StreamPipeline pipeline({48000, 2, SampleFormat::Float32}, {48000, 4, SampleFormat::Float32},
                            {{0, 2}, {1, 3}}, false);
    std::vector<float> input(2000 * 2);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        input[i] = 0.25f;
        input[i + 1] = -0.5f;
    }
    expect(pipeline.push(input.data(), 2000), "capture queue accepts bounded frames");
    // More than one packet is needed to prime the packet-aware safety reserve.
    expect(pipeline.push(input.data(), 2000), "second packet permits guarded priming");
    std::vector<float> output(256 * 4, 99);
    pipeline.render(output.data(), 256);
    for (std::size_t i = 0; i < output.size(); i += 4)
        expect(output[i] == 0 && output[i + 1] == 0 && output[i + 2] == 0.25f &&
                   output[i + 3] == -0.5f,
               "mapped samples remain correct through interpolation");
    auto observed = pipeline.observe();
    expect(observed.transferred_frames == 256 && observed.captured_frames == 4000,
           "transferred and captured counters reflect processing");
    expect(!observed.measured_clock_drift_ppm, "actuator correction is not measured clock drift");
    expect(std::abs(*observed.correction_ppm) <= 2000, "prototype correction is bounded");

    for (const auto encoding : {SampleFormat::PcmS16, SampleFormat::PcmS24Packed,
                                SampleFormat::PcmS24In32, SampleFormat::PcmS32}) {
        // Guard bytes exercise packed-s24 and deliberately unaligned destinations.
        // No upstream shuffle/converter code is involved in this processing path.
        StreamPipeline converter({48000, 2, SampleFormat::Float32}, {48000, 3, encoding}, {},
                                 false);
        expect(converter.push(input.data(), 2000), "converter capture data accepted");
        expect(converter.push(input.data(), 2000), "converter has guarded priming data");
        const auto bytes = pcm_geometry({48000, 3, encoding}).value->block_align_bytes;
        std::vector<unsigned char> guarded(bytes * 256 + 2, 0xa5);
        converter.render(guarded.data() + 1, 256);
        expect(guarded.front() == 0xa5 && guarded.back() == 0xa5,
               "PCM writer stays inside destination extent");
        const auto sample_bytes = bytes / 3;
        for (std::size_t frame = 0; frame < 256; ++frame)
            for (std::size_t i = 0; i < sample_bytes; ++i)
                expect(guarded[1 + frame * bytes + 2 * sample_bytes + i] == 0,
                       "unmapped integer output remains exactly silent");
        StreamPipeline decoder({48000, 1, encoding}, {48000, 1, SampleFormat::Float32}, {}, false);
        std::vector<unsigned char> encoded(2000 * sample_bytes, 0);
        // 0.25 is exactly representable in all supported PCM containers.
        for (std::size_t frame = 0; frame < 2000; ++frame)
            encoded[frame * sample_bytes + (encoding == SampleFormat::PcmS16         ? 1
                                            : encoding == SampleFormat::PcmS24Packed ? 2
                                                                                     : 3)] = 0x20;
        expect(decoder.push(encoded.data(), 2000), "integer source bytes accepted");
        expect(decoder.push(encoded.data(), 2000), "integer decoder has priming reserve");
        std::array<float, 128> decoded{};
        decoder.render(decoded.data(), decoded.size());
        expect(decoded.front() == 0.25f && decoded.back() == 0.25f,
               "integer PCM decoding preserves the declared container alignment");
    }
    StreamPipeline silence({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32}, {},
                           true);
    std::array<float, 64> empty;
    empty.fill(9);
    silence.render(empty.data(), empty.size());
    expect(empty[0] == 0 && silence.observe().transferred_frames == 0,
           "priming emits silence and does not invent transferred frames");
    expect(silence.push(nullptr, 2000, true),
           "silent capture packets still represent valid frames");
    expect(silence.push(nullptr, 2000, true), "silent capture primes the packet guard");
    silence.render(empty.data(), empty.size());
    expect(silence.observe().transferred_frames == 64 && empty[0] == 0,
           "silent audio is valid processing rather than disconnection");
    std::vector<float> excessive(silence.capacity_frames(), 0.25f);
    expect(!silence.push(excessive.data(), excessive.size()) && silence.observe().overruns == 1,
           "full queue drops new frames without overwriting unread data");

    // A necessary software concurrency fixture: independent producer/consumer
    // exercise SPSC ownership. It is not a physical-clock or quality benchmark.
    StreamPipeline concurrent({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32},
                              {}, false);
    std::atomic<bool> done = false;
    std::jthread producer([&] {
        std::array<float, 48> block;
        block.fill(0.125f);
        for (int i = 0; i < 2000; ++i)
            (void)concurrent.push(block.data(), block.size());
        done.store(true, std::memory_order_release);
    });
    std::array<float, 48> block;
    while (!done.load(std::memory_order_acquire))
        concurrent.render(block.data(), block.size());
    producer.join();
    expect(concurrent.observe().queued_frames <= concurrent.capacity_frames(),
           "SPSC occupancy stays bounded");
    NegotiatedRoute negotiated;
    negotiated.source.timing_sample_rate_hz = 48000;
    negotiated.source.period_frames = 480;
    negotiated.destination.timing_sample_rate_hz = 48000;
    negotiated.destination.period_frames = 4800;
    expect(audiohole::engine::detail::queue_target_milliseconds(negotiated) == 200,
           "large actual output blocks increase queue target");
    StreamPipeline nominal({48000, 1, SampleFormat::Float32}, {44100, 1, SampleFormat::Float32}, {},
                           false);
    std::vector<float> priming(2000, 0.25f);
    (void)nominal.push(priming.data(), priming.size());
    (void)nominal.push(priming.data(), priming.size());
    std::array<float, 480> captured;
    captured.fill(0.25f);
    std::array<float, 441> rendered{};
    for (int i = 0; i < 100; ++i) {
        (void)nominal.push(captured.data(), captured.size());
        nominal.render(rendered.data(), rendered.size());
        expect(rendered.front() == 0.25f && rendered.back() == 0.25f,
               "nominal rate conversion retains a constant signal across blocks");
    }
}

void scanner_format_selection() {
    using namespace audiohole::engine::detail;
    const FormatHint configured{{48000, 2, SampleFormat::Float32}, AccessMode::Exclusive, 3};
    const FormatHint pcm{{48000, 2, SampleFormat::PcmS24In32}, AccessMode::Exclusive, 3};
    const FormatHint mono{{48000, 1, SampleFormat::PcmS16}, AccessMode::Exclusive, 4};
    auto shared = pcm;
    shared.mode = AccessMode::Shared;
    const std::vector<FormatHint> hints{configured, shared, pcm, mono};
    EndpointRequirements required;
    unsigned queries = 0;
    auto probe = [&](const FormatHint& candidate) -> Result<bool> {
        ++queries;
        return {candidate.format.sample_format != SampleFormat::Float32, {}};
    };
    auto chosen = select_exclusive_format(required, configured, hints, {}, probe);
    expect(chosen.ok() && *chosen.value == pcm && queries == 2,
           "unsupported live tuple uses a revalidated Exclusive scanner candidate");
    required.sample_format = SampleFormat::Float32;
    queries = 0;
    chosen = select_exclusive_format(required, configured, hints, {}, probe);
    expect(!chosen.ok() && queries == 1 && chosen.issues[0].native_code == "0x88890008",
           "scanner hint never weakens an explicit encoding requirement");
    required = {};
    required.channel_count = 1;
    queries = 0;
    chosen = select_exclusive_format(required, configured, hints, {}, probe);
    expect(chosen.ok() && *chosen.value == mono && queries == 1,
           "partial requirements filter each candidate without widening mono");
    required = {};
    queries = 0;
    chosen = select_exclusive_format(required, configured, hints, {},
        [&](const FormatHint&) -> Result<bool> {
            ++queries;
            return {std::nullopt, {native_open_issue("Query", 0x8889000Au)}};
        });
    expect(!chosen.ok() && queries == 1 && chosen.issues[0].code == ErrorCode::ResourceConflict,
           "ownership refusal does not trigger a format hunt");
    std::stop_source cancelled;
    cancelled.request_stop();
    queries = 0;
    chosen = select_exclusive_format(required, configured, hints, cancelled.get_token(), probe);
    expect(!chosen.ok() && queries == 0 && chosen.issues[0].code == ErrorCode::Cancelled,
           "Stop prevents further native format queries");
    std::vector<FormatHint> excessive(200, pcm);
    for (std::size_t i = 0; i < excessive.size(); ++i)
        excessive[i].format.sample_rate_hz = 8000 + static_cast<int>(i);
    queries = 0;
    chosen = select_exclusive_format(required, configured, excessive, {},
        [&](const FormatHint&) -> Result<bool> { ++queries; return {false, {}}; });
    expect(!chosen.ok() && queries == 129,
           "native format queries remain bounded even for an oversized host snapshot");
}

void packet_cadence_regression() {
    // Deterministic software timing only, never a device/hardware benchmark.
    // Equal 48-kHz clocks; capture batches 10/20/30/40 ms, render requests 10 ms.
    // In v0.1, packet batching alone caused periodic underruns with zero skew.
    for (bool experimental : {false, true}) {
        for (unsigned capture_ticks : {1u, 2u, 3u, 4u}) {
            StreamPipeline pipeline({48000, 1, SampleFormat::Float32},
                                    {48000, 1, SampleFormat::Float32}, {}, experimental);
            std::array<float, 1920> input{};
            std::array<float, 480> output{};
            bool accepted = true;
            for (unsigned tick = 0; tick < 12000; ++tick) {
                if (tick % capture_ticks == 0)
                    accepted = pipeline.push(input.data(), capture_ticks * 480) && accepted;
                pipeline.render(output.data(), output.size());
            }
            const auto observed = pipeline.observe();
            expect(accepted && observed.underruns == 0 && observed.overruns == 0,
                   "packet batching alone must not create underruns or overruns");
            expect(observed.capture_packet_max_frames == capture_ticks * 480 &&
                       observed.render_request_max_frames == 480,
                   "actual packet/block telemetry stays distinct from engine periods");
            expect(observed.controller_target_source_frames.value() >= capture_ticks * 480 + 482,
                   "observed packets determine the reserve rather than a global increase");
        }
    }
}

void automatic_queue_recovery() {
    // Deterministic audio-frame time. No wall-clock sleeps or hardware claims.
    // A single 100-ms startup batch must not set the next three minutes' target.
    for (const bool experimental : {false, true})
        for (const int source_rate : {44100, 48000}) {
            const unsigned captured = source_rate / 100;
            StreamPipeline pipeline({source_rate, 1, SampleFormat::Float32},
                                    {48000, 1, SampleFormat::Float32}, {}, experimental, 5);
            std::vector<float> input(10 * captured, 0.25f);
            std::array<float, 480> output{};
            expect(pipeline.push(input.data(), input.size()), "transient startup batch accepted");
            std::uint32_t high_target = 0, initial_queued = 0;
            for (unsigned tick = 0; tick < 18000; ++tick) {
                expect(pipeline.push(input.data(), captured),
                       "recovery never overruns its capacity");
                pipeline.render(output.data(), output.size());
                if (tick == 200) {
                    const auto observation = pipeline.observe();
                    high_target = *observation.controller_target_source_frames;
                    initial_queued = *observation.queued_frames;
                }
            }
            const auto recovered = pipeline.observe();
            if (experimental) {
                expect(recovered.controller_target_source_frames.value() >= high_target &&
                           recovered.automatic_queue_reductions == 0 &&
                           recovered.automatic_queue_backoffs == 0 && recovered.underruns == 0 &&
                           recovered.overruns == 0,
                       "experimental DLL retains its established lifetime queue guard");
                continue;
            }
            expect(high_target >= 11 * captured &&
                       recovered.controller_target_source_frames == 2 * captured + 2,
                   "recent frame history expires a transient guard in the correct rate domain");
            expect(recovered.capture_packet_max_frames == 10 * captured &&
                       recovered.automatic_queue_reductions.value() > 0,
                   "lifetime evidence survives automatic queue reductions");
            expect(recovered.underruns == 0 && recovered.overruns == 0 &&
                       recovered.queued_frames.value() + captured < initial_queued,
                   "steady transfer drains excess occupancy without dropping audio or starving");

            // An unexpected outage after a reduction must restore the conservative
            // margin and disable further attempts to shrink it for this session.
            for (unsigned tick = 0; tick < 100; ++tick)
                pipeline.render(output.data(), output.size());
            expect(pipeline.observe().automatic_queue_backoffs == 1,
                   "starvation after a reduction latches one conservative backoff");
            for (unsigned tick = 0; tick < 2000; ++tick) {
                pipeline.push(input.data(), captured);
                pipeline.render(output.data(), output.size());
            }
            const auto backed_off = pipeline.observe();
            expect(backed_off.controller_target_source_frames.value() >= high_target &&
                       backed_off.automatic_queue_backoffs == 1,
                   "a failed reduction retains the lifetime guard after recent windows expire");
        }
    // Recurrent batching stays in the recent history. The steady cadence tests
    // above and below exercise many ring wraps and independent block sizes.
    StreamPipeline recurring({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32},
                             {}, false, 5);
    // An 80-ms recurrent burst plus its reserve fits the fixed 200-ms capacity.
    // A recurrent 100-ms burst + 110-ms guard would exceed that capacity.
    std::array<float, 3840> input{};
    std::array<float, 480> output{};
    for (unsigned tick = 0; tick < 2000; ++tick) {
        if (tick % 200 == 0)
            recurring.push(input.data(), input.size());
        else if (tick % 200 >= 8)
            recurring.push(input.data(), output.size());
        recurring.render(output.data(), output.size());
    }
    const auto observed = recurring.observe();
    expect(observed.underruns == 0 && observed.overruns == 0 &&
               observed.automatic_queue_reductions == 0 &&
               observed.controller_target_source_frames.value() >= 4322,
           "recurring large batches never lose their reserve through peak expiry");

    StreamPipeline manual({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32}, {},
                          false, 1.25, true);
    manual.push(input.data(), input.size());
    for (unsigned tick = 0; tick < 100; ++tick)
        manual.render(output.data(), output.size());
    const auto explicit_request = manual.observe();
    expect(explicit_request.controller_target_source_frames == 60 &&
               explicit_request.automatic_queue_reductions == 0 &&
               explicit_request.automatic_queue_backoffs == 0,
           "explicit queue requests never participate in automatic recovery/backoff");
}

void low_latency_policy() {
    using namespace audiohole::engine::detail;
    expect(minimum_shared_period(48, 128, 480) == 144,
           "minimum period rounds UP to a permitted fundamental multiple");
    expect(!minimum_shared_period(0, 128, 480) && !minimum_shared_period(48, 128, 130) &&
               !minimum_shared_period(48, 0, 480) && !minimum_shared_period(48, 480, 128),
           "invalid period bounds cannot produce a guessed minimum");
    expect(requested_shared_period(10.001, 48000, 48, 144, 960) == 480,
           "sub-frame input precision rounds to a supported period");
    expect(!requested_shared_period(10.1, 48000, 48, 144, 960),
           "fundamental alignment cannot silently add several frames to an explicit period");
    StreamConfiguration stream;
    stream.allocated_buffer_frames = 1056;
    stream.period_frames = 480;
    stream.timing_sample_rate_hz = 48000;
    expect(render_ahead_target(stream, LatencyPreset::Baseline) == 528,
           "shared render lead follows cadence independently of allocation");
    expect(render_ahead_target(stream, LatencyPreset::Low) == 528,
           "low preset limits 10-ms shared rendering to 11-ms queued audio");
    stream.applied_render_ahead_request_ms = 22.0001;
    expect(render_ahead_target(stream, LatencyPreset::Low) == 1056,
           "sub-frame lead precision may round to the allocation boundary");
    stream.applied_render_ahead_request_ms = 22.1;
    expect(render_ahead_target(stream, LatencyPreset::Low) == 0,
           "a materially excessive explicit lead is rejected instead of clamped");
    stream.applied_render_ahead_request_ms.reset();
    stream.applied_buffer_request_ms = 22;
    expect(render_ahead_target(stream, LatencyPreset::Low) == 528,
           "explicit capacity does not inflate shared playback lead");
    stream.applied_buffer_request_ms.reset();
    stream.mode = AccessMode::Exclusive;
    expect(render_ahead_target(stream, LatencyPreset::Low) == 1056,
           "exclusive event packets retain the full buffer contract");
    stream.mode = AccessMode::Shared;
    stream.period_frames.reset();
    expect(render_ahead_target(stream, LatencyPreset::Low) == 1008,
           "unknown cadence uses a bounded automatic lead independent of capacity");
    stream.period_frames = 128;
    stream.timing_sample_rate_hz = 44100;
    expect(render_ahead_target(stream, LatencyPreset::Low) == 173,
           "one-ms safety margin rounds up in the endpoint timing domain");
    stream.allocated_buffer_frames = 128;
    expect(render_ahead_target(stream, LatencyPreset::Low) == 128,
           "submission target cannot exceed the actual allocation");
    // Assert extent and lead bounds over all possible padding values, including
    // impossible input beyond capacity. This exercises the native write policy
    // without pretending to simulate Windows scheduling or real hardware.
    for (std::uint32_t padding = 0; padding <= 1057; ++padding) {
        const auto fill = render_fill_frames(1056, padding, 528);
        expect(fill <= 1056 && (padding >= 528 ? fill == 0 : fill + padding == 528),
               "render writes stay bounded and never exceed the lead target");
    }
    NegotiatedRoute route;
    route.source.timing_sample_rate_hz = route.destination.timing_sample_rate_hz = 48000;
    route.source.period_frames = route.destination.period_frames = 128;
    expect(queue_target_milliseconds(route) == 20 &&
               queue_target_milliseconds(route, LatencyPreset::Low) == 6,
           "low queue uses actual periods while baseline preserves its 20-ms floor");
    route.source.period_frames = route.destination.period_frames = 480;
    expect(queue_target_milliseconds(route, LatencyPreset::Low) == 20,
           "low preset never removes the two-period safety reserve");

    for (const bool experimental : {false, true})
        for (const unsigned period : {128u, 240u, 480u})
            for (const unsigned capture_ticks : {1u, 2u, 4u}) {
                route.source.period_frames = route.destination.period_frames = period;
                StreamPipeline pipeline({48000, 1, SampleFormat::Float32},
                                        {48000, 1, SampleFormat::Float32}, {}, experimental,
                                        queue_target_milliseconds(route, LatencyPreset::Low));
                std::vector<float> input(period * capture_ticks, 0.25f), output(period);
                bool accepted = true;
                // Thirty virtual seconds per case, zero clock skew; small
                // callbacks and larger capture batches stress priming/reserve.
                for (unsigned tick = 0; tick < 30 * 48000 / period; ++tick) {
                    if (tick % capture_ticks == 0)
                        accepted &= pipeline.push(input.data(), input.size());
                    pipeline.render(output.data(), output.size());
                }
                const auto observed = pipeline.observe();
                expect(accepted && observed.underruns == 0 && observed.overruns == 0 &&
                           observed.transferred_frames.value() > 25 * 48000,
                       "low targets retain bounded, flowing zero-skew batched processing");
            }
}

void startup_refill_regression() {
    using namespace audiohole::engine::detail;
    StreamConfiguration stream;
    stream.client_format = {48000, 1, SampleFormat::Float32};
    stream.timing_sample_rate_hz = 48000;
    stream.period_frames = 480;
    stream.allocated_buffer_frames = 1056;
    const auto first = startup_render_plan(stream, 1056, true);
    expect(first.prefix_silence_frames == 576 && first.pipeline_frames == 480,
           "startup restores native lead without teaching a 22-ms FIFO request");
    const auto running = startup_render_plan(stream, 1056, false);
    expect(running.prefix_silence_frames == 0 && running.pipeline_frames == 1056,
           "a genuinely large running refill still reaches the FIFO safety guard");
    for (unsigned requested = 0; requested <= 1056; ++requested) {
        const auto plan = startup_render_plan(stream, requested, true);
        expect(plan.prefix_silence_frames + plan.pipeline_frames == requested &&
                   plan.pipeline_frames <= 480,
               "startup plan covers exactly the bounded native write extent");
    }
    stream.applied_buffer_request_ms = 22;
    expect(startup_render_plan(stream, 1056, true).pipeline_frames == 480,
           "capacity requests do not enlarge the startup processing block");
    stream.applied_buffer_request_ms.reset();
    stream.mode = AccessMode::Exclusive;
    expect(startup_render_plan(stream, 1056, true).pipeline_frames == 1056,
           "Exclusive retains complete event packets during startup");
    stream.mode = AccessMode::Shared;
    stream.period_frames.reset();
    expect(startup_render_plan(stream, 1056, true).pipeline_frames == 1056,
           "unknown periods never invent a smaller startup block");
    stream.period_frames = 480;
    stream.timing_sample_rate_hz = 44100;
    expect(startup_render_plan(stream, 1056, true).pipeline_frames == 1056,
           "startup block bounds must belong to the actual client frame domain");
    stream.timing_sample_rate_hz = 48000;

    for (bool experimental : {false, true}) {
        StreamPipeline pipeline(stream.client_format, stream.client_format, {}, experimental);
        std::array<float, 480> input;
        input.fill(.25f);
        std::vector<float> guarded(1058, 99);
        bool startup = true, accepted = true;
        std::uint64_t native_frames = 0, prefix_frames = 0;
        for (unsigned tick = 0; tick < 12000; ++tick) {
            accepted &= pipeline.push(input.data(), input.size());
            const unsigned requested = tick == 0 ? 1056 : 480;
            const auto plan = startup_render_plan(stream, requested, startup);
            std::fill(guarded.begin(), guarded.end(), 99);
            auto* output = guarded.data() + 1;
            std::fill_n(output, plan.prefix_silence_frames, 0);
            const auto produced =
                pipeline.render(output + plan.prefix_silence_frames, plan.pipeline_frames);
            if (produced)
                startup = false;
            native_frames += requested;
            prefix_frames += plan.prefix_silence_frames;
            expect(guarded.front() == 99 && guarded[requested + 1] == 99,
                   "startup prefix plus pipeline output stay inside native allocation");
            if (tick == 0)
                expect(std::all_of(output, output + 1056, [](float v) { return v == 0; }),
                       "initial priming covers both the prefix and unprimed PCM with silence");
        }
        auto o = pipeline.observe();
        expect(accepted && !startup && o.underruns == 0 && o.overruns == 0 &&
                   o.render_request_max_frames == 480 && o.controller_target_source_frames == 962,
               "startup-only full refill no longer leaves a permanent 1538-frame target");
        expect(prefix_frames == 576 && *o.rendered_frames + prefix_frames == native_frames,
               "native startup padding remains explicit instead of inflating PCM progress");
        // Once flowing, retain support/reserve for a real larger output demand.
        for (int i = 0; i < 3; ++i)
            expect(pipeline.push(input.data(), input.size()), "running refill reserve accepted");
        expect(pipeline.render(guarded.data() + 1, running.pipeline_frames) == 1056,
               "running full-buffer demand still transfers available source audio");
        o = pipeline.observe();
        expect(o.render_request_max_frames == 1056 && o.controller_target_source_frames == 1538 &&
                   o.underruns == 0,
               "runtime large-request safety remains intact after the startup fix");
    }
}

void underrun_context() {
    using namespace audiohole::engine::detail;
    StreamPipeline pipeline({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32},
                            {}, false);
    std::array<float, 480> input;
    input.fill(.25f);
    std::array<float, 480> output{};
    RenderTiming timing;
    // Fixed test metadata only: no Windows device or physical-clock simulation.
    timing.render_qpc_100ns = 123456789;
    timing.last_capture_device_position_frames = 960;
    timing.last_capture_qpc_100ns = 123450000;
    timing.capture_delivery_gap_last_us = 10012;
    timing.capture_packet_age_last_us = 145;
    timing.native_padding_before_frames = 576;
    timing.native_render_request_frames = 480;
    for (int i = 0; i < 3; ++i)
        expect(pipeline.push(input.data(), input.size()), "context fixture primes valid PCM");
    for (int i = 0; i < 5; ++i)
        pipeline.render(output.data(), output.size(), timing);
    const auto observed = pipeline.observe();
    expect(observed.underruns == 1 && observed.last_underrun.has_value(),
           "one starvation publishes one bounded event, not one event per silent callback");
    const auto& event = *observed.last_underrun;
    expect(event.sequence == 1 && event.source_published_frames == 1440 &&
               event.requested_output_frames == 480 && event.produced_output_frames < 480 &&
               event.controller_target_source_frames == 962 &&
               event.rendered_frame_index < *observed.rendered_frames,
           "last event preserves the actual failing block and frame-index context");
    expect(event.render_qpc_100ns == timing.render_qpc_100ns &&
               event.last_capture_device_position_frames == 960 &&
               event.last_capture_qpc_100ns == timing.last_capture_qpc_100ns &&
               event.native_padding_before_frames == 576 &&
               event.capture_delivery_gap_last_us == 10012 &&
               event.capture_packet_age_last_us == 145 && event.native_render_request_frames == 480,
           "native context copied into an event stays attached to that same render call");
    expect(pipeline.observe().last_underrun->sequence == 1,
           "event remains visible across polls slower than the supervisor");

    StreamPipeline bounded({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32}, {},
                           false, 1);
    std::array<float, 48> small{};
    std::array<float, 128> block{};
    // The consumer is deliberately paused: fill the eight-event SPSC mailbox,
    // then verify diagnostic overflow is counted and routing remains bounded.
    for (int event_index = 0; event_index < 12; ++event_index) {
        for (int packet = 0; packet < 4; ++packet)
            (void)bounded.push(small.data(), small.size());
        bounded.render(block.data(), block.size());
        bounded.render(block.data(), block.size());
    }
    const auto filled = bounded.observe();
    expect(filled.underruns == 12 && filled.last_underrun->sequence == 8 &&
               filled.dropped_underrun_contexts == 4 && filled.overruns == 0,
           "full telemetry mailbox drops four new contexts without blocking audio");
    for (int packet = 0; packet < 4; ++packet)
        (void)bounded.push(small.data(), small.size());
    bounded.render(block.data(), block.size());
    bounded.render(block.data(), block.size());
    const auto resumed = bounded.observe();
    expect(resumed.last_underrun->sequence == 13 && resumed.dropped_underrun_contexts == 4 &&
               !resumed.last_underrun->render_qpc_100ns,
           "reclaimed mailbox resumes publication; absent native timing stays absent");
}

void late_capture_regression() {
    using namespace audiohole::engine::detail;
    // Reproduce the live failure's QUEUE condition, not a Windows device: a
    // primed FIFO has less than a 480-frame output block, while the adapter still
    // has native playback headroom and the next real capture packet arrives.
    StreamPipeline immediate({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32},
                             {}, false);
    StreamPipeline waited({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32}, {},
                          false);
    std::array<float, 480> input, output{};
    input.fill(.25f);
    expect(!waited.would_starve(480), "startup priming does not request capture waits");
    for (auto* pipeline : {&immediate, &waited}) {
        for (int packet = 0; packet < 3; ++packet)
            expect(pipeline->push(input.data(), input.size()), "regression primes real PCM frames");
        expect(pipeline->render(output.data(), 480) == 480 &&
                   pipeline->render(output.data(), 480) == 480 &&
                   pipeline->render(output.data(), 48) == 48,
               "fixture drains into the observed near-starvation region without a prior underrun");
    }
    const auto before = waited.observe();
    expect(waited.would_starve(480) && waited.would_starve(480),
           "readiness detects an imminent block shortage without consuming samples");
    const auto after = waited.observe();
    expect(before.queued_frames == after.queued_frames &&
               before.transferred_frames == after.transferred_frames &&
               before.correction_ppm == after.correction_ppm,
           "readiness leaves FIFO, phase/controller command and progress unchanged");
    StreamConfiguration destination;
    destination.mode = AccessMode::Shared;
    destination.timing_sample_rate_hz = 48000;
    expect(capture_wait_milliseconds(destination, 576, true) == 1 &&
               capture_wait_milliseconds(destination, 143, true) == 0 &&
               capture_wait_milliseconds(destination, 144, true) == 1 &&
               capture_wait_milliseconds(destination, 576, false) == 0,
           "only sufficient measured native headroom permits one bounded capture wait");
    destination.mode = AccessMode::Exclusive;
    expect(capture_wait_milliseconds(destination, 576, true) == 0,
           "Exclusive event buffers retain their full-packet/no-padding-wait contract");
    destination.mode = AccessMode::Shared;
    destination.timing_sample_rate_hz.reset();
    expect(capture_wait_milliseconds(destination, 576, true) == 0,
           "unknown timing never invents a deadline budget");
    expect(immediate.render(output.data(), 480) < 480 && immediate.observe().underruns == 1,
           "immediate rendering reproduces starvation with the same bounded queue condition");
    expect(waited.push(input.data(), input.size()) && !waited.would_starve(480) &&
               waited.render(output.data(), 480) == 480,
           "arrival of the next capture packet permits a complete source-derived output block");
    expect(waited.observe().underruns == 0 && waited.observe().overruns == 0 &&
               output.front() == .25f && output.back() == .25f,
           "bounded deferral produces a complete valid block without FIFO starvation or overflow");
    // A genuine missing capture still follows normal starvation/re-priming;
    // this change must not conceal failure by pretending input arrived.
    for (int block = 0; block < 4; ++block)
        waited.render(output.data(), 480);
    expect(waited.observe().underruns == 1,
           "unavailable capture after a wait remains explicitly counted starvation");
}

struct FixtureState {
    std::atomic<bool> opening{false}, failed{false};
    std::atomic<int> stopped{0};
    bool block = false;
};
class FixtureSession final : public RouteSession {
  public:
    explicit FixtureSession(FixtureState& state) : state_(state) {}
    ~FixtureSession() override {
        stop();
    }
    NegotiatedRoute configuration() const override {
        NegotiatedRoute result;
        result.source.client_format =
            result.destination.client_format = {48000, 2, SampleFormat::Float32};
        return result;
    }
    Result<bool> start(std::stop_token) override {
        started_ = true;
        return {true, {}};
    }
    RouteObservation observe() override {
        RouteObservation result;
        if (state_.failed)
            result.issues.push_back(
                {ErrorCode::BackendFailure, "test-only", "Injected lifecycle failure"});
        else
            result.transferred_frames = started_ ? 100 : 0;
        return result;
    }
    void stop() noexcept override {
        if (!stopped_) {
            ++state_.stopped;
            stopped_ = true;
        }
    }

  private:
    FixtureState& state_;
    bool started_ = false, stopped_ = false;
};
class FixtureAdapter final : public BackendAdapter {
  public:
    explicit FixtureAdapter(FixtureState& state) : state_(state) {}
    BackendDescriptor descriptor() const override {
        return {BackendId::Wasapi, true, "TEST FIXTURE, NO AUDIO"};
    }
    Result<std::unique_ptr<RouteSession>> open(const BoundRoute&, std::stop_token stop) override {
        state_.opening = true;
        while (state_.block && !stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (stop.stop_requested())
            return {std::nullopt, {{ErrorCode::Cancelled, "test-only", "Cancelled fixture"}}};
        return {std::make_unique<FixtureSession>(state_), {}};
    }

  private:
    FixtureState& state_;
};

template <class Predicate> void until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Lifecycle fixture timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void lifecycle() {
    Profile profile{"profile-test", "Fixture", "", {}};
    Hole hole;
    hole.id = "hole-test";
    hole.forward.source = "capture-test";
    hole.forward.destination = "render-test";
    profile.holes.push_back(hole);
    BindingSnapshot bindings{
        1,
        {{"capture-test", "fixture-in", EndpointDirection::Capture, BindingState::Available},
         {"render-test", "fixture-out", EndpointDirection::Render, BindingState::Available}}};
    FixtureState state;
    std::vector<std::unique_ptr<BackendAdapter>> adapters;
    adapters.push_back(std::make_unique<FixtureAdapter>(state));
    Runtime runtime(std::move(adapters));
    const auto receipt = runtime.start(profile, bindings);
    expect(receipt.ok(), "start admission returns an operation ID");
    until([&] { return runtime.snapshot().state == State::Running; });
    expect(!runtime.start(profile, bindings).ok(), "running profile cannot be silently replaced");
    state.failed = true;
    until([&] { return runtime.snapshot().state == State::Failed; });
    expect(runtime.snapshot().armed, "failure preserves activation intent");
    runtime.stop();
    until([&] { return runtime.snapshot().state == State::Stopped; });
    expect(state.stopped == 1 && !runtime.snapshot().armed,
           "terminal Stop follows adapter cleanup");
    state.failed = false;
    state.opening = false;
    state.block = true;
    const auto next = runtime.start(profile, bindings);
    expect(next.ok() && *next.value > *receipt.value, "new session has a new generation");
    until([&] { return state.opening.load(); });
    runtime.stop();
    until([&] { return runtime.snapshot().state == State::Stopped; });
    expect(!runtime.snapshot().armed, "Stop during opening prevents late activation");
    runtime.stop();
    expect(runtime.snapshot().state == State::Stopped, "Stop is idempotent");
}

void explicit_timing_contract() {
    using namespace audiohole::engine::detail;
    EndpointRequirements required;
    required.buffer_ms = 1;
    required.period_ms = 10;
    required.render_ahead_ms = 10.5;
    StreamConfiguration stream;
    stream.client_format = {48000, 2, SampleFormat::Float32};
    stream.timing_sample_rate_hz = 48000;
    stream.allocated_buffer_frames = 1056;
    stream.applied_buffer_request_ms = 1;
    stream.period_frames = 480;
    stream.applied_period_request_ms = 10;
    stream.render_ahead_target_frames = 504;
    stream.applied_render_ahead_request_ms = 10.5;
    expect(check_stream(required, stream, "destination").ok(),
           "one-ms capacity can round up to 22 ms without increasing explicit cadence/lead");
    stream.period_frames = 528;
    expect(!check_stream(required, stream, "destination").ok(),
           "larger processing interval is rejected");
    stream.period_frames = 481;
    expect(check_stream(required, stream, "destination").ok(),
           "ordinary one-frame rounding is accepted");
    stream.render_ahead_target_frames = 1056;
    expect(!check_stream(required, stream, "destination").ok(), "larger playback lead is rejected");
    StreamPipeline pipeline({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32},
                            {}, false, 12.25, true);
    std::vector<float> packet(2000, 0.25f), output(480);
    expect(pipeline.push(packet.data(), packet.size()),
           "manual queue fixture accepts capture packet");
    pipeline.render(output.data(), output.size());
    expect(pipeline.target_frames() == 588 &&
               pipeline.observe().controller_target_source_frames == 588,
           "explicit fractional queue is not raised by a historical packet-size guard");
    NegotiatedRoute route;
    route.source.allocated_buffer_frames = route.destination.allocated_buffer_frames = 48000;
    route.source.timing_sample_rate_hz = route.destination.timing_sample_rate_hz = 48000;
    expect(queue_target_milliseconds(route) == 20,
           "unknown periods never derive queue occupancy from capacity");
}

void owner_thread_contract() {
    Runtime runtime;
    std::atomic<bool> rejected{false};
    std::jthread other([&] {
        try {
            (void)runtime.snapshot();
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    other.join();
    expect(rejected.load(), "wrong-thread commands cannot race controller state");
}
} // namespace

int main() {
    try {
        processing();
        scanner_format_selection();
        packet_cadence_regression();
        automatic_queue_recovery();
        const auto in_use = detail::native_open_issue("InitializeAudioStream", 0x8889000Au);
        expect(in_use.code == ErrorCode::ResourceConflict && in_use.native_code == "0x8889000A" &&
                   in_use.detail.find("Give exclusive mode applications priority") !=
                       std::string::npos,
               "native ownership refusal keeps its HRESULT and actionable feedback");
        const auto disabled = detail::native_open_issue("InitializeAudioStream", 0x8889000Eu);
        expect(disabled.code == ErrorCode::UnsupportedRequirement &&
                   disabled.detail.find("Allow applications") != std::string::npos,
               "exclusive permission refusal explains the endpoint setting");
        expect(detail::native_open_issue("InitializeAudioStream", 0x88890020u).code ==
                   ErrorCode::UnsupportedRequirement,
               "invalid device period is not misreported as an ownership conflict");
        low_latency_policy();
        startup_refill_regression();
        underrun_context();
        late_capture_regression();
        lifecycle();
        explicit_timing_contract();
        owner_thread_contract();
        std::cout << checks
                  << " portable processing/lifecycle checks passed; synthetic fixtures only.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
