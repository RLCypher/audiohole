#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace audiohole::presentation {
// Exact decimal seconds, no floating-point rounding or silent precision loss.
std::optional<unsigned> milliseconds_from_seconds(std::string_view text);
std::string seconds_from_milliseconds(unsigned milliseconds);
} // namespace audiohole::presentation
