#pragma once
#include "model/controller.h"
#include <cstdint>
#include <vector>

namespace audiohole::presentation {
// Owned, tightly packed, straight-alpha RGBA pixels; no OS or GUI resources.
struct RgbaImage {
    int width{}, height{};
    std::vector<std::uint8_t> pixels;
};
struct OutlineColour {
    std::uint8_t red, green, blue;
};
// Maps only the authoritative aggregate state. Ready is neutral waiting, never
// healthy; transitions are blue. Does not infer health or inspect audio paths.
OutlineColour tray_colour(audiohole::State state);
// Adds an exterior-only stroke without changing any source pixel with alpha > 0.
// The supplied artwork is one stacked conduit: its horizontal alpha envelope
// deliberately closes the internal opening, wireframe and gaps between layers.
// Empty rows between visible rows interpolate the envelope; outer padding stays
// empty. This is logo-specific silhouette extraction, not a general sprite hull.
// Dimensions must be 1..512, storage exact, and stroke 1..8 physical pixels.
// Throws invalid_argument on invalid inputs. Allocation/work is GUI-thread only,
// never real-time safe; callers cache the result instead of invoking per paint.
RgbaImage outline_logo(const RgbaImage& image, OutlineColour colour, int stroke);
} // namespace audiohole::presentation
