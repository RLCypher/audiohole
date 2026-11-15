// Standalone, opt-in CPU microbenchmark / deterministic output fingerprint.
// No devices, scheduling claims, audio-quality score or timing pass threshold.
#include "stream_pipeline.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace audiohole::engine;
using audiohole::engine::detail::StreamPipeline;
namespace {
struct Case {
    std::string name;
    StreamFormat source, destination;
    std::vector<ChannelRoute> map;
};
std::uint64_t hash_bytes(std::uint64_t hash, const void* input, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(input);
    for (std::size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * 1099511628211ULL;
    return hash;
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--queue-recovery") {
        StreamPipeline pipeline({48000, 1, SampleFormat::Float32},
                                {48000, 1, SampleFormat::Float32}, {}, false, 5);
        std::vector<float> input(4800, 0.25f), output(480);
        pipeline.push(input.data(), input.size());
        std::cout << "audio_seconds,target_frames,queued_frames,underruns\n";
        for (unsigned tick = 0; tick < 18000; ++tick) {
            pipeline.push(input.data(), output.size());
            pipeline.render(output.data(), output.size());
            if (tick % 100 == 0) {
                const auto observation = pipeline.observe();
                std::cout << tick / 100 << ',' << *observation.controller_target_source_frames
                          << ',' << *observation.queued_frames << ',' << *observation.underruns
                          << '\n';
            }
        }
        return 0;
    }
    const bool verify = argc == 2 && std::string_view(argv[1]) == "--verify";
    constexpr unsigned block = 128;
    std::vector<Case> cases{
        {"float-stereo", {48000, 2, SampleFormat::Float32}, {48000, 2, SampleFormat::Float32}, {}},
        {"float-mono-pair",
         {48000, 1, SampleFormat::Float32},
         {48000, 2, SampleFormat::Float32},
         {}},
        {"pcm16-stereo", {48000, 2, SampleFormat::PcmS16}, {48000, 2, SampleFormat::PcmS16}, {}},
        {"explicit-map",
         {48000, 4, SampleFormat::Float32},
         {48000, 3, SampleFormat::Float32},
         {{3, 0}, {0, 2}}},
        {"rate-conversion",
         {44100, 2, SampleFormat::Float32},
         {48000, 2, SampleFormat::Float32},
         {}},
        {"pcm24-containers",
         {48000, 2, SampleFormat::PcmS24Packed},
         {48000, 2, SampleFormat::PcmS24In32},
         {}}};
    if (verify) {
        const SampleFormat formats[]{SampleFormat::Float32, SampleFormat::PcmS16,
                                     SampleFormat::PcmS24Packed, SampleFormat::PcmS24In32,
                                     SampleFormat::PcmS32};
        for (const auto source : formats)
            for (const auto destination : formats)
                for (const auto rate : {44100, 96000})
                    for (const auto channels : {1, 2, 4})
                        cases.push_back({"pair-" + std::to_string(cases.size()),
                                         {rate, channels, source},
                                         {48000, channels == 4 ? 3 : 2, destination},
                                         channels == 4 ? std::vector<ChannelRoute>{{3, 0}, {0, 2}}
                                                       : std::vector<ChannelRoute>{}});
    }
    for (const auto& c : cases) {
        StreamPipeline pipeline(c.source, c.destination, c.map, false, 20, true);
        const auto in_stride = pcm_geometry(c.source).value->block_align_bytes;
        const auto out_stride = pcm_geometry(c.destination).value->block_align_bytes;
        std::vector<unsigned char> input(2048 * in_stride),
            output(2 * block * out_stride + 2, 0xa5);
        // Reproducible nonconstant samples, including both PCM signs. Floating
        // cases also exercise silent capture and nonfinite sanitization below.
        std::uint32_t random = 0x13579bdu;
        for (auto& byte : input) {
            random = random * 1664525u + 1013904223u;
            byte = static_cast<unsigned char>(random >> 24);
        }
        if (c.source.sample_format == SampleFormat::Float32)
            for (unsigned i = 0; i < 2048u * c.source.channel_count; ++i) {
                const float sample = verify && i % 19 == 0 ? std::numeric_limits<float>::quiet_NaN()
                                     : verify && i % 23 == 0
                                         ? std::numeric_limits<float>::infinity()
                                     : verify && i % 29 == 0 ? -0.0f
                                                             : std::sin(float(i) * 0.071f) * 1.25f;
                std::memcpy(input.data() + i * sizeof(float), &sample, sizeof(sample));
            }
        pipeline.push(input.data(), 1200);
        std::uint64_t input_clock = 0;
        auto transfer = [&](unsigned iteration) {
            const unsigned sizes[]{1, 17, 128, 256};
            const auto rendered = verify ? sizes[iteration % 4] : block;
            input_clock += std::uint64_t(c.source.sample_rate_hz) * rendered;
            const auto captured = input_clock / c.destination.sample_rate_hz;
            input_clock %= c.destination.sample_rate_hz;
            const bool silent = verify && iteration % 23 == 0;
            if (!(verify && iteration % 47 == 0) &&
                !pipeline.push(silent ? nullptr : input.data(), captured, silent))
                throw std::runtime_error("Benchmark overflowed its bounded queue");
            if (verify)
                std::fill(output.begin() + 1 + rendered * out_stride, output.end(), 0xa5);
            pipeline.render(output.data() + 1, rendered);
            if (output.front() != 0xa5 || output.back() != 0xa5 ||
                (verify && !std::all_of(output.begin() + 1 + rendered * out_stride, output.end(),
                                        [](auto byte) { return byte == 0xa5; })))
                throw std::runtime_error("Processing exceeded the output buffer");
            return rendered;
        };
        for (unsigned i = 0; i < 256; ++i)
            transfer(i);
        const unsigned iterations = verify ? 2000 : 20000;
        std::uint64_t hash = 14695981039346656037ULL;
        const auto start = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < iterations; ++i) {
            const auto rendered = transfer(i);
            if (verify)
                hash = hash_bytes(hash, output.data() + 1, rendered * out_stride);
        }
        const auto elapsed =
            std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start)
                .count();
        const auto observation = pipeline.observe();
        if (verify)
            std::cout << c.name << ' ' << hash << ' ' << *observation.transferred_frames << ' '
                      << *observation.underruns << ' ' << *observation.queued_frames << '\n';
        else
            std::cout << c.name << ' ' << elapsed / (iterations * block) << '\n';
    }
}
