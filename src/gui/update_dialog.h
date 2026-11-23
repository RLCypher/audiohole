#pragma once
#include "model/update_policy.h"
#include <wx/dialog.h>
namespace audiohole::gui {
enum class UpdateChoice { Remind, OpenGithub, Skip, Install };
class UpdateDialog final : public wxDialog {
  public:
    UpdateDialog(wxWindow* parent, const updates::Release&, const std::string& current_version,
                 bool installed);
    UpdateChoice choice() const;
};
void smoke_update_dialog(wxWindow* parent);
// A successful manual check with no newer release. The explicit owner wording
// and Ok label are shared by every personality; errors use a separate dialog.
void show_no_update_dialog(wxWindow* parent);
} // namespace audiohole::gui
