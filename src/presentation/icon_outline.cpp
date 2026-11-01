#include "presentation/icon_outline.h"
#include <algorithm>
#include <stdexcept>

namespace audiohole::presentation {
OutlineColour tray_colour(audiohole::State state) {
    switch (state) {
    case State::Running:
        return {46, 175, 110};
    case State::Degraded:
        return {234, 158, 52};
    case State::Failed:
        return {230, 83, 83};
    case State::Starting:
    case State::Stopping:
    case State::Recovering:
        return {75, 146, 210};
    default:
        return {145, 154, 168};
    }
}
RgbaImage outline_logo(const RgbaImage& image, OutlineColour colour, int stroke) {
    if (image.width < 1 || image.height < 1 || image.width > 512 || image.height > 512 ||
        stroke < 1 || stroke > 8 ||
        image.pixels.size() != static_cast<std::size_t>(image.width * image.height * 4))
        throw std::invalid_argument("Invalid logo dimensions, storage or stroke");
    struct Span {
        int left = -1, right = -1;
    };
    std::vector<Span> spans(static_cast<std::size_t>(image.height));
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x)
            if (image.pixels[static_cast<std::size_t>((y * image.width + x) * 4 + 3)] != 0) {
                auto& span = spans[static_cast<std::size_t>(y)];
                if (span.left < 0)
                    span.left = x;
                span.right = x;
            }
    int previous = -1;
    for (int y = 0; y < image.height; ++y) {
        if (spans[static_cast<std::size_t>(y)].left < 0)
            continue;
        if (previous >= 0) {
            const auto above = spans[static_cast<std::size_t>(previous)];
            const auto below = spans[static_cast<std::size_t>(y)];
            for (int gap = previous + 1; gap < y; ++gap) {
                const int offset = gap - previous, distance = y - previous;
                spans[static_cast<std::size_t>(gap)] = {
                    above.left + (below.left - above.left) * offset / distance,
                    above.right + (below.right - above.right) * offset / distance};
            }
        }
        previous = y;
    }
    auto inside = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= image.width || y >= image.height)
            return false;
        const auto span = spans[static_cast<std::size_t>(y)];
        return span.left >= 0 && x >= span.left && x <= span.right;
    };
    RgbaImage output = image;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const auto offset = static_cast<std::size_t>((y * image.width + x) * 4);
            if (inside(x, y) || image.pixels[offset + 3] != 0)
                continue;
            bool near = false;
            for (int dy = -stroke; dy <= stroke && !near; ++dy)
                for (int dx = -stroke; dx <= stroke; ++dx)
                    if (dx * dx + dy * dy <= stroke * stroke && inside(x + dx, y + dy)) {
                        near = true;
                        break;
                    }
            if (near) {
                output.pixels[offset] = colour.red;
                output.pixels[offset + 1] = colour.green;
                output.pixels[offset + 2] = colour.blue;
                output.pixels[offset + 3] = 255;
            }
        }
    }
    return output;
}
} // namespace audiohole::presentation
