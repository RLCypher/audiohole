#include "gui/update_dialog.h"
#include "gui/branding.h"
#include "gui/controls.h"
#include <wx/modalhook.h>
#include <wx/wrapsizer.h>
namespace audiohole::gui {
UpdateDialog::UpdateDialog(wxWindow* parent, const updates::Release& release,
                           const std::string& current, bool installed)
    : wxDialog(parent, wxID_ANY, "AudioHole update", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    const auto running = updates::parse_version(current);
    const auto offered = updates::parse_version(release.version);
    if (!running || !offered)
        throw std::invalid_argument("Invalid update popup version.");
    SetName("AudioHole update available");
    SetIcons(application_icons());
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(this, wxID_ANY, "A new version of AudioHole is available!");
    heading->SetFont(GetFont().Bold());
    layout->Add(heading, 0, wxALL, FromDIP(12));
    auto* current_label =
        new wxStaticText(this, wxID_ANY, "Current version: " + text(running->display()));
    current_label->SetName("Current update version");
    layout->Add(current_label, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    auto* available =
        new wxStaticText(this, wxID_ANY, "Update available: " + text(offered->display()));
    available->SetName("Available update version");
    layout->Add(available, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    if (!release.notes.empty()) {
        layout->Add(new wxStaticText(this, wxID_ANY, "What's new:"), 0, wxLEFT | wxRIGHT,
                    FromDIP(12));
        auto* notes = new wxTextCtrl(this, wxID_ANY, text(release.notes), wxDefaultPosition,
                                     FromDIP(wxSize(580, 150)),
                                     wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
        notes->SetName("Update changelog");
        layout->Add(notes, 1, wxEXPAND | wxALL, FromDIP(12));
    }
    auto* actions = new wxWrapSizer(wxHORIZONTAL);
    const auto action = [&](const char* label, int id, bool enabled = true) {
        auto* control = new ActionButton(this, label, 0, id);
        control->SetName(label);
        control->UnsetToolTip();
        control->Enable(enabled);
        control->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent&) { EndModal(id); });
        actions->Add(control, 0, wxALL, FromDIP(5));
        return control;
    };
    if (installed)
        action("Update AudioHole", wxID_APPLY, release.installer.has_value());
    action("Open Github", wxID_OPEN);
    action("Remind me later", wxID_CANCEL)->SetDefault();
    action("Skip this one", wxID_IGNORE);
    layout->Add(actions, 0, wxEXPAND | wxALL, FromDIP(7));
    if (installed && !release.installer) {
        auto* explanation = new wxStaticText(
            this, wxID_ANY,
            "Automatic installation isn't available for this release. Use Open Github to update.");
        explanation->Wrap(FromDIP(580));
        layout->Add(explanation, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    }
    SetEscapeId(wxID_CANCEL);
    SetSizerAndFit(layout);
    SetMinSize(FromDIP(wxSize(320, 220)));
    theme_dialog(this);
    CentreOnParent();
}
UpdateChoice UpdateDialog::choice() const {
    switch (GetReturnCode()) {
    case wxID_APPLY:
        return UpdateChoice::Install;
    case wxID_OPEN:
        return UpdateChoice::OpenGithub;
    case wxID_IGNORE:
        return UpdateChoice::Skip;
    default:
        return UpdateChoice::Remind;
    }
}
void smoke_update_dialog(wxWindow* parent) {
    class Answer final : public wxModalDialogHook {
      public:
        int answer = wxID_CANCEL;
        bool installed = false, seen = false;
        int Enter(wxDialog* raw) override {
            auto* dialog = dynamic_cast<UpdateDialog*>(raw);
            if (!dialog)
                return wxID_NONE;
            seen = true;
            auto* current = wxWindow::FindWindowByName("Current update version", dialog);
            auto* offered = wxWindow::FindWindowByName("Available update version", dialog);
            auto* notes =
                dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("Update changelog", dialog));
            if (!current || current->GetLabel() != "Current version: v0.1" || !offered ||
                offered->GetLabel() != "Update available: v0.1.1" || !notes ||
                notes->GetValue() != "A short release changelog." || notes->IsEditable())
                throw std::runtime_error("Update popup lost versions or the read-only changelog.");
            for (const auto* name : {"Open Github", "Remind me later", "Skip this one"}) {
                auto* button = wxWindow::FindWindowByName(name, dialog);
                if (!button || button->GetToolTip())
                    throw std::runtime_error("Update popup buttons must have no tooltip.");
            }
            auto* automatic = wxWindow::FindWindowByName("Update AudioHole", dialog);
            if (bool(automatic) != installed || (automatic && automatic->GetToolTip()))
                throw std::runtime_error(
                    "Automatic update action must be installed-only without a tooltip.");
            if (current->GetForegroundColour() != dialog->GetParent()->GetForegroundColour())
                throw std::runtime_error("Update popup ignored the app theme.");
            dialog->CallAfter([dialog, id = answer] {
                wxCommandEvent click(wxEVT_BUTTON, id);
                dialog->FindWindow(id)->GetEventHandler()->ProcessEvent(click);
            });
            return wxID_NONE;
        }
    };
    const updates::Release offered{
        "0.1.1", "A short release changelog.",
        updates::Installer{"https://github.com/RLCypher/audiohole/releases/download/v0.1.1/"
                           "Install%20AudioHole%20v0.1.1.exe",
                           std::string(64, '0'), 123}};
    for (bool installed : {false, true})
        for (const auto [id, expected] : {std::pair{wxID_CANCEL, UpdateChoice::Remind},
                                          std::pair{wxID_OPEN, UpdateChoice::OpenGithub},
                                          std::pair{wxID_IGNORE, UpdateChoice::Skip},
                                          std::pair{wxID_APPLY, UpdateChoice::Install}}) {
            if (!installed && id == wxID_APPLY)
                continue;
            UpdateDialog dialog(parent, offered, "0.1.0", installed);
            Answer hook;
            hook.answer = id;
            hook.installed = installed;
            hook.Register();
            dialog.ShowModal();
            hook.Unregister();
            if (!hook.seen || dialog.choice() != expected)
                throw std::runtime_error("Update popup returned the wrong action.");
        }
}
} // namespace audiohole::gui
