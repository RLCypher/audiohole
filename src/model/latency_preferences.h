#pragma once
#include "model/controller.h"
#include <array>
#include <string_view>
namespace audiohole {
// Direction-local configuration only. Ordering matches the two rows in the
// tuning view. A null slot is Automatic (0..3) or inheritance (4..7).
using TimingValues = std::array<std::optional<double>, 8>;
TimingValues timing_values(const Preferences&);
Result set_timing_values(Preferences&, const TimingValues&);
Result parse_timing_values(const std::array<std::string, 8>&, TimingValues&);
std::string encode_timing_clipboard(const TimingValues&);
Result decode_timing_clipboard(std::string_view, TimingValues&);
// Presets are starting requests, not measured guarantees. Supported periods
// are filled only from matching scanner evidence; unknowns remain Automatic.
TimingValues timing_preset(unsigned index, const Endpoint*, const Endpoint*, const Preferences&,
                           std::optional<double> playback_capacity_ms = {});
} // namespace audiohole
