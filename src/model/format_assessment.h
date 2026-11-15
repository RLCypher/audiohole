#pragma once
#include "model/controller.h"
#include <string_view>
namespace audiohole {
// Presentation-only candidates retain independent endpoint formats. They never
// become saved preferences or change engine requests merely by being displayed.
struct FormatCandidate {
    Format source, destination;
    bool dual_mono = false;
    std::string label() const;
};
struct FormatAssessment {
    std::vector<FormatCandidate> candidates;
    std::string explanation;
};
FormatAssessment assess_formats(const Snapshot&, const Direction&, std::string_view engine);
} // namespace audiohole
