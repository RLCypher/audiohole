#include "gui/license_dialog.h"
#include "gui/branding.h"
#include "gui/branding_resources.h"
#include "gui/controls.h"
#include <wx/display.h>
#include <wx/modalhook.h>
#include <wx/radiobut.h>

namespace audiohole::gui {
LicenseDialog::LicenseDialog()
    : wxDialog(nullptr, wxID_ANY, "AudioHole License Agreement", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    SetIcons(application_icons());
    const auto resource =
        FindResourceW(nullptr, MAKEINTRESOURCEW(AUDIOHOLE_LICENSE_TEXT), RT_RCDATA);
    const auto handle = resource ? LoadResource(nullptr, resource) : nullptr;
    const auto* bytes = handle ? static_cast<const char*>(LockResource(handle)) : nullptr;
    const auto size = resource ? SizeofResource(nullptr, resource) : 0;
    if (!bytes || !size)
        throw std::runtime_error("The embedded AudioHole license agreement is unavailable.");
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticText(this, wxID_ANY, "Please read the following license agreement."), 0,
              wxALL, FromDIP(12));
    auto* agreement =
        new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(bytes, size), wxDefaultPosition,
                       FromDIP(wxSize(660, 380)), wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    agreement->SetName("AudioHole license agreement");
    root->Add(agreement, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    auto* accept = new wxRadioButton(this, wxID_ANY, "I accept the agreement", wxDefaultPosition,
                                     wxDefaultSize, wxRB_GROUP);
    auto* decline = new wxRadioButton(this, wxID_ANY, "I do not accept the agreement");
    accept->SetName("Accept license agreement");
    decline->SetName("Decline license agreement");
    decline->SetValue(true);
    root->Add(accept, 0, wxALL, FromDIP(8));
    root->Add(decline, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* start = new ActionButton(this, "Start AudioHole", 0, wxID_OK);
    start->SetName("Start AudioHole");
    start->Enable(false);
    auto* cancel = new ActionButton(this, "Cancel", 0, wxID_CANCEL);
    actions->AddStretchSpacer();
    actions->Add(start, 0, wxALL, FromDIP(6));
    actions->Add(cancel, 0, wxALL, FromDIP(6));
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(6));
    const auto update = [accept, start](wxCommandEvent&) { start->Enable(accept->GetValue()); };
    accept->Bind(wxEVT_RADIOBUTTON, update);
    decline->Bind(wxEVT_RADIOBUTTON, update);
    start->Bind(wxEVT_BUTTON, [this, accept](wxCommandEvent&) {
        if (accept->GetValue())
            EndModal(wxID_OK);
    });
    cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
    SetEscapeId(wxID_CANCEL);
    SetSizerAndFit(root);
    const wxDisplay display(static_cast<unsigned>(std::max(0, wxDisplay::GetFromWindow(this))));
    const auto area = display.GetClientArea();
    SetSize(wxSize(std::min(GetSize().x, area.width - FromDIP(30)),
                   std::min(GetSize().y, area.height - FromDIP(30))));
    SetMinSize(FromDIP(wxSize(320, 280)));
    // No owner window or saved app preference: consult Windows directly.
    theme_dialog(this, "system");
    Centre();
}
void smoke_license_dialog() {
    class Answer final : public wxModalDialogHook {
      public:
        bool accept = false, seen = false;
        int Enter(wxDialog* window) override {
            auto* dialog = dynamic_cast<LicenseDialog*>(window);
            if (!dialog)
                return wxID_NONE;
            seen = true;
            auto* yes = dynamic_cast<wxRadioButton*>(
                wxWindow::FindWindowByName("Accept license agreement", dialog));
            auto* no = dynamic_cast<wxRadioButton*>(
                wxWindow::FindWindowByName("Decline license agreement", dialog));
            auto* body = dynamic_cast<wxTextCtrl*>(
                wxWindow::FindWindowByName("AudioHole license agreement", dialog));
            auto* start = dialog->FindWindow(wxID_OK);
            if (!yes || !no || !body || !start || start->IsEnabled() || !no->GetValue() ||
                start->GetLabel() != "Start AudioHole" || !body->GetValue().Contains("GPLv3"))
                throw std::runtime_error(
                    "Portable license dialog lost its agreement, choices or acceptance gate.");
            yes->SetValue(true);
            no->SetValue(false);
            wxCommandEvent accepted(wxEVT_RADIOBUTTON, yes->GetId());
            yes->GetEventHandler()->ProcessEvent(accepted);
            if (!start->IsEnabled())
                throw std::runtime_error("Accepting the agreement did not enable Start AudioHole.");
            if (!accept) {
                no->SetValue(true);
                yes->SetValue(false);
                wxCommandEvent declined(wxEVT_RADIOBUTTON, no->GetId());
                no->GetEventHandler()->ProcessEvent(declined);
                if (start->IsEnabled())
                    throw std::runtime_error("Declining the agreement still permits startup.");
            }
            DWORD light = 1, size = sizeof(light);
            RegGetValueW(HKEY_CURRENT_USER,
                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                         L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
            HIGHCONTRASTW contrast{sizeof(contrast)};
            SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
            if (!(contrast.dwFlags & HCF_HIGHCONTRASTON) &&
                (dialog->GetBackgroundColour().GetLuminance() < 0.5) != !light)
                throw std::runtime_error("Portable license dialog ignores Windows appearance.");
            const auto id = accept ? wxID_OK : wxID_CANCEL;
            dialog->CallAfter([dialog, id] {
                auto* action = dialog->FindWindow(id);
                wxCommandEvent click(wxEVT_BUTTON, id);
                click.SetEventObject(action);
                action->GetEventHandler()->ProcessEvent(click);
            });
            return wxID_NONE;
        }
    };
    for (const bool accept : {false, true}) {
        LicenseDialog dialog;
        Answer answer;
        answer.accept = accept;
        answer.Register();
        const auto result = dialog.ShowModal();
        answer.Unregister();
        if (!answer.seen || result != (accept ? wxID_OK : wxID_CANCEL))
            throw std::runtime_error(
                "Portable license dialog ignored its acceptance or cancellation.");
    }
}
} // namespace audiohole::gui
