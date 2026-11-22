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
} // namespace audiohole::gui
