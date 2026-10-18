#pragma once
#include <functional>
#include <wx/wx.h>

namespace audiohole::gui {
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
    auto* control = new wxButton(parent, wxID_ANY, title);
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

} // namespace audiohole::gui
