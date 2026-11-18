#include "dialogs.hpp"
#include <wx/generic/filedlgg.h>
#include <wx/generic/msgdlgg.h>
#include <wx/modalhook.h>
#include <wx/weakref.h>

namespace audiohole::plug_board::desktop {
namespace {
void theme_library_choices(wxWindow& parent) {
    // The framework's generic file control keeps a pointer to its plain
    // wxChoice for filter state. Keep that model control hidden and replace its
    // visible sizer slot with our themed, accessible choice. Forward selections
    // to the original handler so file-type filtering remains framework-owned.
    std::vector<wxWindow*> children;
    for (auto* child : parent.GetChildren())
        children.push_back(child);
    for (auto* child : children) {
        auto* original = dynamic_cast<wxChoice*>(child);
        auto* sizer = parent.GetSizer();
        if (original && !dynamic_cast<Choice*>(original) && original->IsShown() && sizer &&
            sizer->GetItem(original, true)) {
            auto* visible = new Choice(&parent);
            visible->Set(original->GetStrings());
            visible->SetSelection(original->GetSelection());
            visible->SetFont(original->GetFont());
            visible->SetMinSize(original->GetMinSize());
            visible->SetName("File type");
            visible->SetToolTip("Choose which file types are shown.");
            sizer->Replace(original, visible, true);
            original->Hide();
            const wxWeakRef<wxChoice> destination(original);
            visible->Bind(wxEVT_CHOICE, [destination](wxCommandEvent& event) {
                if (!destination || event.GetSelection() < 0 ||
                    static_cast<unsigned>(event.GetSelection()) >= destination->GetCount())
                    return;
                destination->SetSelection(event.GetSelection());
                wxCommandEvent forwarded(wxEVT_CHOICE, destination->GetId());
                forwarded.SetEventObject(destination.get());
                forwarded.SetInt(event.GetSelection());
                destination->GetEventHandler()->ProcessEvent(forwarded);
            });
        }
        theme_library_choices(*child);
    }
    parent.Layout();
}
class DialogThemeScope final : public wxModalDialogHook {
  public:
    explicit DialogThemeScope(Appearance value) : appearance(std::move(value)) {
        Register();
    }

  private:
    Appearance appearance;
    int Enter(wxDialog* dialog) override {
        auto colors = appearance;
        const auto* top = dialog->GetParent() ? wxGetTopLevelParent(dialog->GetParent()) : nullptr;
        if (top && top->GetBackgroundColour().IsOk() &&
            top->GetBackgroundColour() != colors.surface)
            colors = appearance_for_window(dialog->GetParent());
        if (auto* native = dynamic_cast<wxMessageDialog*>(dialog)) {
            // Library-authored file/validation errors also use wxMessageDialog.
            // Re-present the same decision, including its safe default and IDs,
            // instead of allowing a light-only Windows message-box body.
            wxGenericMessageDialog replacement(native->GetParent(), native->GetMessage(),
                                               native->GetCaption(),
                                               native->GetMessageDialogStyle());
            replacement.SetExtendedMessage(native->GetExtendedMessage());
            replacement.SetYesNoCancelLabels(native->GetYesLabel(), native->GetNoLabel(),
                                             native->GetCancelLabel());
            replacement.SetOKCancelLabels(native->GetOKLabel(), native->GetCancelLabel());
            return replacement.ShowModal();
        }
        dialog->Bind(wxEVT_INIT_DIALOG, [dialog, colors](wxInitDialogEvent& event) {
            theme_library_choices(*dialog);
            apply_appearance(*dialog, colors);
            event.Skip();
        });
        if (dialog->GetHandle()) {
            theme_library_choices(*dialog);
            apply_appearance(*dialog, colors);
        }
        return wxID_NONE;
    }
};
} // namespace
int show_themed_dialog(wxDialog& dialog, const Appearance& appearance) {
    DialogThemeScope scope(appearance);
    return dialog.ShowModal();
}
int show_message(wxWindow* parent, const wxString& message, const wxString& title, long style,
                 const Appearance& appearance) {
    wxGenericMessageDialog dialog(parent, message, title, style);
    dialog.SetName("Plug Board message dialog");
    return show_themed_dialog(dialog, appearance);
}
std::optional<std::size_t> choose_item(wxWindow* parent, const wxString& title,
                                       const std::vector<std::string>& choices,
                                       const Appearance& appearance) {
    wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition, parent->FromDIP(wxSize(460, 330)),
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    dialog.SetName("Plug Board selection dialog");
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* list = new ListBox(&dialog);
    list->SetName("Available choices");
    list->SetToolTip("Select one item, then choose OK or double-click it.");
    for (const auto& choice : choices)
        list->Append(wxString::FromUTF8(choice));
    if (!choices.empty())
        list->SetSelection(0);
    layout->Add(list, 1, wxEXPAND | wxALL, dialog.FromDIP(12));
    layout->Add(dialog.CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxALL,
                dialog.FromDIP(12));
    dialog.SetSizer(layout);
    dialog.FindWindow(wxID_OK)->Enable(!choices.empty());
    list->Bind(wxEVT_LISTBOX_DCLICK, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_OK); });
    dialog.CentreOnParent();
    list->SetFocus();
    if (show_themed_dialog(dialog, appearance) != wxID_OK || list->GetSelection() < 0)
        return {};
    return static_cast<std::size_t>(list->GetSelection());
}
std::optional<wxString> choose_file(wxWindow* parent, const wxString& title,
                                    const wxString& directory, const wxString& filename,
                                    const wxString& wildcard, long style,
                                    const Appearance& appearance) {
    // The generic browser is part of the app's palette, including its nested
    // overwrite/error dialogs. An OS file picker can ignore an explicit app
    // theme when it differs from Windows' own appearance.
    wxGenericFileDialog dialog(parent, title, directory, filename, wildcard, style);
    dialog.SetName("Plug Board file dialog");
    if (show_themed_dialog(dialog, appearance) != wxID_OK)
        return {};
    return dialog.GetPath();
}
} // namespace audiohole::plug_board::desktop
