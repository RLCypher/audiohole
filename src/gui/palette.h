#pragma once
#include <wx/colour.h>
namespace audiohole::gui::palette {
// Application surfaces/accents only. Never apply to source branding or avatars.
inline wxColour mint() {
    return {80, 237, 178};
}
inline wxColour magenta() {
    return {238, 158, 251};
}
inline wxColour charcoal() {
    return {10, 19, 25};
}
inline wxColour input() {
    return {21, 35, 43};
}
inline wxColour selection(bool dark) {
    return dark ? wxColour(18, 54, 78) : wxColour(204, 232, 255);
}
} // namespace audiohole::gui::palette
