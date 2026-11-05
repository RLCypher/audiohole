#include "gui/seconds_field.h"
#include "presentation/scan_text.h"
#include <algorithm>
namespace audiohole::gui {
SecondsField::SecondsField(wxWindow* parent, unsigned minimum_ms, unsigned maximum_ms,
                           unsigned initial_ms)
    : wxPanel(parent), minimum_(minimum_ms), maximum_(maximum_ms) {
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    text_ = new wxTextCtrl(this, wxID_ANY,
                           wxString::FromUTF8(presentation::seconds_from_milliseconds(initial_ms)),
                           wxDefaultPosition, wxSize(80, -1));
    auto* step = new wxSpinButton(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL);
    step->SetRange(-1, 1);
    step->SetValue(0);
    row->Add(text_, 1, wxEXPAND);
    row->Add(step, 0, wxEXPAND);
    SetSizer(row);
    const auto help = wxString::FromUTF8(
        "Seconds, up to three decimal places. Arrows adjust by 0.001 s. Range: " +
        presentation::seconds_from_milliseconds(minimum_ms) + "–" +
        presentation::seconds_from_milliseconds(maximum_ms) + " s.");
    SetToolTip(help);
    text_->SetToolTip(help);
    step->SetToolTip(help);
    step->SetName("Adjust seconds by one millisecond");
    step->Bind(wxEVT_SPIN_UP, [this, step](wxSpinEvent&) {
        nudge(1);
        step->SetValue(0);
    });
    step->Bind(wxEVT_SPIN_DOWN, [this, step](wxSpinEvent&) {
        nudge(-1);
        step->SetValue(0);
    });
}
std::optional<unsigned> SecondsField::milliseconds() const {
    const auto value = presentation::milliseconds_from_seconds(text_->GetValue().ToUTF8().data());
    return value && *value >= minimum_ && *value <= maximum_ ? value : std::nullopt;
}
void SecondsField::nudge(int step) {
    if (const auto value = milliseconds()) {
        const auto next = std::clamp(static_cast<int>(*value) + step, static_cast<int>(minimum_),
                                     static_cast<int>(maximum_));
        text_->ChangeValue(wxString::FromUTF8(
            presentation::seconds_from_milliseconds(static_cast<unsigned>(next))));
    }
}
} // namespace audiohole::gui
