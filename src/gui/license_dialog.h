#pragma once
#include <wx/dialog.h>
#include <wx/textctrl.h>
namespace audiohole::gui {
// Native inspection of the complete rendered range, including body text.
bool license_ink_is_readable(wxTextCtrl&, const wxColour& foreground);
class LicenseDialog final : public wxDialog {
  public:
    // Acceptance gates first launch; a parent opens a read-only license viewer
    // following that parent's theme. Neither mode writes an acceptance receipt.
    explicit LicenseDialog(wxWindow* parent = nullptr);
};
// Explicit native smoke hosts only; no receipt or working-data access.
void smoke_license_dialog();
} // namespace audiohole::gui
