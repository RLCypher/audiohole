#pragma once
#include <wx/dialog.h>
namespace audiohole::gui {
// YES selects the current Preview/tutorial environment; NO selects Test;
// CANCEL exits. This dialog never creates an engine or changes saved profiles.
class WelcomeDialog final : public wxDialog {
  public:
    explicit WelcomeDialog(bool first_time);
    bool has_norm_image() const {
        return has_norm_image_;
    }

  private:
    bool has_norm_image_ = false;
};
} // namespace audiohole::gui
