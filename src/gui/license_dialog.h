#pragma once
#include <wx/dialog.h>
namespace audiohole::gui {
class LicenseDialog final : public wxDialog {
  public:
    LicenseDialog();
};
// Explicit native smoke hosts only; no receipt or working-data access.
void smoke_license_dialog();
} // namespace audiohole::gui
