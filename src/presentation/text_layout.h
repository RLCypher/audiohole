#pragma once
#include <string>
#include <string_view>

namespace audiohole::presentation {
// Render-only protection for bounded parenthesized kaomoji: internal ASCII
// spaces become NBSP, keeping wx/Windows from breaking the face across lines.
// Original catalog text is never changed. Ordinary parenthetical prose and
// incomplete/oversized spans remain ordinary wrappable text.
std::string keep_kaomoji_together(std::string_view text);
} // namespace audiohole::presentation
