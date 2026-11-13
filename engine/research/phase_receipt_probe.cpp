// Reproduce after the portable engine build, from the repository root:
// g++ -std=c++20 -O2 -Iengine/include -Iengine/src \
//   engine/research/phase_receipt_probe.cpp engine/build/libaudiohole_engine.a \
//   engine/build/libaudiohole_engine_contract.a -pthread -o /tmp/phase-probe
// /tmp/phase-probe
// Results for this probe do NOT reproduce the live 0.3 baseline FIFO failures.
#include "stream_pipeline.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace audiohole::engine;
using namespace audiohole::engine::detail;

// Necessary deterministic timing experiment for the reported real-device FIFO
// failures. No device emulation, physical clock inference, or hardware claims.
// Both virtual streams have exactly 48 kHz; only capture receipt scheduling
// varies around its ideal time. Data is available at callback receipt.
void run(const char* label, unsigned capture_frames, unsigned render_frames, double phase_ms,
         double jitter_ms, double jitter_cycle_s) {
    NegotiatedRoute route;
    route.source.period_frames = capture_frames;
    route.destination.period_frames = render_frames;
    route.source.timing_sample_rate_hz = route.destination.timing_sample_rate_hz = 48000;
    StreamPipeline pipeline({48000, 1, SampleFormat::Float32}, {48000, 1, SampleFormat::Float32},
                            {}, false, queue_target_milliseconds(route, LatencyPreset::Low));
    std::vector<float> input(capture_frames, .25f), output(render_frames);
    const double source_period = capture_frames / 48000.;
    const double destination_period = render_frames / 48000.;
    const auto capture_time = [&](std::uint64_t n) {
        const double ideal = (n + 1) * source_period;
        return ideal + phase_ms / 1000 +
               jitter_ms / 1000 * std::sin(2 * 3.141592653589793 * ideal / jitter_cycle_s);
    };
    std::uint64_t capture = 0, render = 0;
    double now = 0;
    bool accepted = true;
    while (now < 180) {
        const double next_capture = capture_time(capture);
        const double next_render = (render + 1) * destination_period;
        if (next_capture <= next_render) {
            now = next_capture;
            accepted &= pipeline.push(input.data(), input.size());
            ++capture;
        } else {
            now = next_render;
            pipeline.render(output.data(), output.size());
            ++render;
        }
    }
    const auto o = pipeline.observe();
    std::cout << label << " capture=" << capture_frames << " render=" << render_frames
              << " phase_ms=" << phase_ms << " jitter_ms=" << jitter_ms
              << " cycle_s=" << jitter_cycle_s << " accepted=" << accepted
              << " underruns=" << *o.underruns << " overruns=" << *o.overruns
              << " queue=" << *o.queued_frames << " ppm=" << *o.correction_ppm << '\n';
}
int main() {
    run("fixed-positive-phase", 480, 480, .15, 0, 1);
    run("fixed-negative-phase", 480, 480, -.15, 0, 1);
    run("boundary-crossing-fast", 480, 480, 0, .05, .07);
    run("boundary-crossing-slow", 480, 480, 0, .05, 20);
    run("boundary-nearby", 480, 480, .04, .05, .07);
    run("mixed-period-low", 336, 480, .15, .05, .07);
}
