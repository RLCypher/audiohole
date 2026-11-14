#pragma once
#include "gui/palette.h"
#include "model/controller.h"
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <wx/tglbtn.h>
#include <wx/wx.h>

namespace audiohole::gui {
// Keep Windows keyboard/accessibility behavior while painting matched selection
// foregrounds and surfaces for lists and popup/editable combo boxes.
class ThemedChoice final : public wxChoice {
  public:
    ThemedChoice(wxWindow*, wxWindowID = wxID_ANY);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool MSWOnMeasure(WXMEASUREITEMSTRUCT*) override;
    WXLRESULT MSWWindowProc(WXUINT, WXWPARAM, WXLPARAM) override;

  protected:
    WXDWORD MSWGetStyle(long, WXDWORD*) const override;
    wxSize DoGetBestSize() const override;

  private:
    wxBrush input_brush_;
};
class ThemedCombo final : public wxComboBox {
  public:
    ThemedCombo(wxWindow*, wxWindowID = wxID_ANY);
    std::vector<unsigned> disabled_items;
    bool item_enabled(unsigned item) const {
        return std::find(disabled_items.begin(), disabled_items.end(), item) ==
               disabled_items.end();
    }
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool MSWOnMeasure(WXMEASUREITEMSTRUCT*) override;
    WXLRESULT MSWWindowProc(WXUINT, WXWPARAM, WXLPARAM) override;

  protected:
    WXDWORD MSWGetStyle(long, WXDWORD*) const override;
    wxSize DoGetBestSize() const override;

  private:
    wxBrush input_brush_;
};
class ThemedList final : public wxListBox {
  public:
    using wxListBox::wxListBox;
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    std::vector<wxColour> status_colours;
};
// Native buttons retain Windows keyboard, command, and accessibility semantics.
// Only application-palette painting is replaced; high contrast uses wx/Windows.
class ActionButton final : public wxButton {
  public:
    ActionButton(wxWindow* parent, const wxString& title, long style = 0, wxWindowID id = wxID_ANY);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool use_native_colours = false;
    // Optional accent for the existing DPI-aware outline; high contrast stays native.
    wxColour outline_colour;
};
class NavigationButton final : public wxToggleButton {
  public:
    explicit NavigationButton(wxWindow* parent);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool use_native_colours = false;
};
// GUI-thread only. Helpers own no windows or GDI resources beyond a paint call.
// Generic wx dialogs avoid Windows' light-only message-box body. The palette
// follows Windows app appearance, retaining native high-contrast presentation.
void theme_dialog(wxWindow*);
bool confirm_exit(wxWindow* parent);
// Native scrollable fields consume the wheel only while they can move in that
// direction. Otherwise route to the first ancestor with remaining scroll range.
bool chain_field_wheel(wxWindow&, wxMouseEvent&);
void enable_field_wheel_chaining(wxWindow& root);
void forward_dropdown_wheel(wxWindow& control, wxMouseEvent& event);
// Protect native controls that change values before wx dispatches a wheel event.
// Parent windows still receive the gesture for page scrolling.
void prevent_wheel_adjustment(wxWindow& control);
int themed_message_box(const wxString& message, const wxString& caption = "AudioHole",
                       long style = wxOK, wxWindow* parent = nullptr);
wxString button_help(const wxString& title);
wxColour routing_colour(audiohole::State state);
// Decorative Material glyph next to the authoritative accessible text label.
void material_heading(wxWindow* parent, wxSizer*, const wxString& title, bool capture);
// Native sizer-owned panel with a restrained rounded outline. Children remain
// ordinary accessible controls; paint has no controller or personality behavior.
class SurfacePanel final : public wxPanel {
  public:
    SurfacePanel(wxWindow* parent, const wxString& role);
    bool use_system_colours = false;
    // Keep native children outside the rounded stroke, including at scaled DPI.
    void set_content_sizer(wxSizer* contents);
};
// Small construction helpers keep layout code consistent. Returned widgets are
// non-owning pointers; wx parent windows own them, including event bindings.
inline wxString text(const std::string& value) {
    return wxString::FromUTF8(value);
}
inline std::string utf8(const wxString& value) {
    return value.ToUTF8().data();
}
inline wxStaticText* label(wxWindow* parent, wxSizer* layout, const wxString& value) {
    auto* control = new wxStaticText(parent, wxID_ANY, value);
    const auto* row = dynamic_cast<wxBoxSizer*>(layout);
    const int alignment = row && row->GetOrientation() == wxHORIZONTAL
                              ? static_cast<int>(wxALIGN_CENTER_VERTICAL)
                              : static_cast<int>(wxEXPAND);
    layout->Add(control, 0, alignment | wxALL, parent->FromDIP(4));
    return control;
}
inline wxButton* button(wxWindow* parent, wxSizer* layout, const wxString& title,
                        std::function<void()> action) {
    auto* control = new ActionButton(parent, title);
    const auto* row = dynamic_cast<wxBoxSizer*>(layout);
    layout->Add(control, 0,
                wxALL |
                    (row && row->GetOrientation() == wxHORIZONTAL ? wxALIGN_CENTER_VERTICAL : 0),
                parent->FromDIP(4));
    control->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) { action(); });
    return control;
}
inline wxTextCtrl* field(wxWindow* parent, wxSizer* layout, const wxString& title, long style = 0) {
    label(parent, layout, title);
    auto* control = new wxTextCtrl(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, style);
    control->SetName(title); // Stable functional name for assistive technology.
    layout->Add(control, 0, wxEXPAND | wxALL, parent->FromDIP(4));
    return control;
}
inline wxChoice* choices(wxWindow* parent, wxSizer* layout, const wxString& title,
                         std::initializer_list<const char*> values) {
    label(parent, layout, title);
    auto* control = new ThemedChoice(parent);
    for (const auto* value : values)
        control->Append(wxString::FromUTF8(value));
    control->SetName(title);
    if (control->GetCount() > 0)
        control->SetSelection(0);
    const auto* row = dynamic_cast<wxBoxSizer*>(layout);
    const int alignment = row && row->GetOrientation() == wxHORIZONTAL
                              ? static_cast<int>(wxALIGN_CENTER_VERTICAL)
                              : static_cast<int>(wxEXPAND);
    layout->Add(control, 0, wxALL | alignment, parent->FromDIP(4));
    return control;
}

// Source copy is UTF-8, independent of the user's Windows ANSI code page.
// Literal overloads convert before a wxString constructor can use wxConvLibc.
inline wxStaticText* label(wxWindow* parent, wxSizer* layout, const char* value) {
    return label(parent, layout, wxString::FromUTF8(value));
}
inline wxButton* button(wxWindow* parent, wxSizer* layout, const char* title,
                        std::function<void()> action) {
    return button(parent, layout, wxString::FromUTF8(title), std::move(action));
}
inline wxTextCtrl* field(wxWindow* parent, wxSizer* layout, const char* title, long style = 0) {
    return field(parent, layout, wxString::FromUTF8(title), style);
}
inline wxChoice* choices(wxWindow* parent, wxSizer* layout, const char* title,
                         std::initializer_list<const char*> values) {
    return choices(parent, layout, wxString::FromUTF8(title), values);
}

} // namespace audiohole::gui
