#pragma once
#include <wx/wx.h>
namespace audiohole::gui {
// GUI-thread, process-local Windows appearance. Optional OS capabilities are
// resolved at runtime; high contrast retains the native system presentation.
void prepare_native_theme(bool dark, bool high_contrast);
void native_theme(wxWindow*, bool dark, bool high_contrast);
} // namespace audiohole::gui
