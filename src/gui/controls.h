#pragma once
#include "gui/palette.h"
#include "model/controller.h"
#include <functional>
#include <string>
#include <utility>
#include <wx/tglbtn.h>
#include <wx/wx.h>

namespace audiohole::gui {
// Native buttons retain Windows keyboard, command, and accessibility semantics.
// Only application-palette painting is replaced; high contrast uses wx/Windows.
class ActionButton final : public wxButton {
  public:
    ActionButton(wxWindow* parent, const wxString& title);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool use_native_colours = false;
};
class NavigationButton final : public wxToggleButton {
  public:
    explicit NavigationButton(wxWindow* parent);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool use_native_colours = false;
};
// GUI-thread only. Helpers own no windows or GDI resources beyond a paint call.
wxString button_help(const wxString& title);
wxColour routing_colour(audiohole::State state);
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
    layout->Add(control, 0, wxEXPAND | wxALL, 4);
    return control;
}
inline wxButton* button(wxWindow* parent, wxSizer* layout, const wxString& title,
                        std::function<void()> action) {
    auto* control = new ActionButton(parent, title);
    layout->Add(control, 0, wxALL, 3);
    control->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) { action(); });
    return control;
}
inline wxTextCtrl* field(wxWindow* parent, wxSizer* layout, const wxString& title, long style = 0) {
    label(parent, layout, title);
    auto* control = new wxTextCtrl(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, style);
    control->SetName(title); // Stable functional name for assistive technology.
    layout->Add(control, 0, wxEXPAND | wxALL, 3);
    return control;
}
inline wxChoice* choices(wxWindow* parent, wxSizer* layout, const wxString& title,
                         std::initializer_list<const char*> values) {
    label(parent, layout, title);
    auto* control = new wxChoice(parent, wxID_ANY);
    for (const auto* value : values)
        control->Append(wxString::FromUTF8(value));
    control->SetName(title);
    if (control->GetCount() > 0)
        control->SetSelection(0);
    layout->Add(control, 0, wxEXPAND | wxALL, 3);
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
