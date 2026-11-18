#pragma once
#include "audiohole/plug_board/view.hpp"
#include <wx/menu.h>

namespace audiohole::plug_board::desktop {
// GUI-thread, process-local appearance. Native scrollbar input/accessibility
// stay with Windows; only their nonclient paint follows the resolved palette.
void prepare_native_appearance(const ViewAppearance&);
void apply_native_appearance(wxWindow&, const ViewAppearance&);
void apply_menu_appearance(wxMenu&, const ViewAppearance&);
} // namespace audiohole::plug_board::desktop
