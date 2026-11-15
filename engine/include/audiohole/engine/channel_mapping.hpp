#pragma once

#include "profile.hpp"
#include <array>
#include <span>

namespace audiohole::engine {

// Fixed-size, immutable mapping compiled on the control thread. An output has
// one source or -1 for silence. Automatic mono duplicates into the first output
// pair; otherwise matching indexes pass through. No implicit sums or downmix.
// Repeated source indexes intentionally permit explicit duplication.
class ChannelMapping {
  public:
    int source_channels() const noexcept {
        return source_channels_;
    }
    int destination_channels() const noexcept {
        return destination_channels_;
    }

    // Bounded processing kernel for interleaved float32 frames at ONE rate.
    // No allocation, logging, locks, filesystem or GUI calls. The caller owns
    // distinct nonoverlapping buffers and clocks/SRC; this function is not a
    // router or drift compensator. False means invalid extents/overlap; output
    // is untouched. Empty spans with zero frames are accepted.
    bool process(std::span<const float> source, std::span<float> destination,
                 std::size_t frames) const noexcept;

  private:
    friend Result<ChannelMapping> compile_mapping(int, int, const std::vector<ChannelRoute>&);
    int source_channels_ = 0;
    int destination_channels_ = 0;
    std::array<int, 256> source_for_output_{};
};

// Revalidate against negotiated stream widths. Empty routes compile the
// documented default index map; explicit routes leave other outputs silent.
Result<ChannelMapping> compile_mapping(int source_channels, int destination_channels,
                                       const std::vector<ChannelRoute>& routes);

} // namespace audiohole::engine
