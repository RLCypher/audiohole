// Numerical timing experiment only: no device provider, Windows API, audio I/O,
// real-time waiting, measurements of hardware, or production simulated fallback.
// Run from repository root after building the contract library:
// g++ -O2 -std=c++20 -Iengine/include -Iengine/src \
//   engine/research/packet_cadence_probe.cpp engine/src/stream_pipeline.cpp \
//   engine/build/libaudiohole_engine_contract.a -pthread -o /tmp/packet-probe
// /tmp/packet-probe
#include "stream_pipeline.hpp"
#include <array>
#include <iostream>
using namespace audiohole::engine;
using namespace audiohole::engine::detail;
int main() {
    for (int packet_ticks : {1, 2, 3, 4}) {
        StreamPipeline pipeline({48000, 1, SampleFormat::Float32},
                                {48000, 1, SampleFormat::Float32}, {}, false);
        std::array<float, 1920> input{};
        std::array<float, 480> output{};
        std::cout << "SOFTWARE ONLY, zero clock mismatch; capture packet "
                  << packet_ticks * 480 << " frames, render block 480 frames\n";
        for (int tick = 0; tick < 95000; ++tick) {
            if (tick % packet_ticks == 0) pipeline.push(input.data(), packet_ticks * 480);
            pipeline.render(output.data(), output.size());
        }
        const auto observed = pipeline.observe();
        std::cout << "underruns=" << *observed.underruns << " queue=" << *observed.queued_frames
                  << " correction=" << *observed.correction_ppm << " ppm\n";
    }
}
