#pragma once
#include <wx/wx.h>
namespace audiohole::gui {
// GUI-thread, process-local Windows appearance. Optional OS capabilities are
// resolved at runtime; high contrast retains the native system presentation.
void prepare_native_theme(bool dark, bool high_contrast);
void native_theme(wxWindow*, bool dark, bool high_contrast);
// Keep native single-line editing/caret/selection semantics while centering the
// edit client row inside the taller application field. Spin buddies are included.
void center_input_text(wxWindow&);
// wx's default screen rectangle starts at the client origin. Centered native
// fields have padding above that origin; outlines/alignment need the outer box.
wxRect field_screen_rect(const wxWindow&);
} // namespace audiohole::gui
