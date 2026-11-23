#include "gui/license_dialog.h"
#include "gui/branding.h"
#include "gui/branding_resources.h"
#include "gui/controls.h"
#include <cstring>
#include <richedit.h>
#include <string_view>
#include <wx/display.h>
#include <wx/modalhook.h>
#include <wx/radiobut.h>

namespace audiohole::gui {
namespace {
// Rich Edit streams the embedded bytes directly, preserving the RTF codepage and
// Unicode escapes. The resource outlives this synchronous GUI-thread operation.
struct RtfInput {
    std::string_view remaining;
};
DWORD CALLBACK read_rtf(DWORD_PTR cookie, LPBYTE buffer, LONG requested, LONG* copied) {
    if (requested < 0)
        return ERROR_INVALID_PARAMETER;
    auto& input = *reinterpret_cast<RtfInput*>(cookie);
    const auto count = std::min(input.remaining.size(), static_cast<std::size_t>(requested));
    std::memcpy(buffer, input.remaining.data(), count);
    input.remaining.remove_prefix(count);
    *copied = static_cast<LONG>(count);
    return 0;
}
void load_license(wxTextCtrl& control) {
    const auto resource =
        FindResourceW(nullptr, MAKEINTRESOURCEW(AUDIOHOLE_LICENSE_RTF), RT_RCDATA);
    const auto handle = resource ? LoadResource(nullptr, resource) : nullptr;
    const auto* bytes = handle ? static_cast<const char*>(LockResource(handle)) : nullptr;
    const auto size = resource ? SizeofResource(nullptr, resource) : 0;
    if (!bytes || !size || !control.IsRich())
        throw std::runtime_error("The embedded AudioHole RTF license is unavailable.");
    RtfInput input{{bytes, size}};
    if (!input.remaining.starts_with("{\\rtf1"))
        throw std::runtime_error("The embedded AudioHole license is not an RTF document.");
    EDITSTREAM stream{};
    stream.dwCookie = reinterpret_cast<DWORD_PTR>(&input);
    stream.pfnCallback = read_rtf;
    SendMessageW(static_cast<HWND>(control.GetHandle()), EM_STREAMIN, SF_RTF,
                 reinterpret_cast<LPARAM>(&stream));
    if (stream.dwError || control.IsEmpty())
        throw std::runtime_error("Cannot display the AudioHole RTF license.");
    control.SetInsertionPoint(0);
    control.ShowPosition(0);
}
} // namespace
bool license_ink_is_readable(wxTextCtrl& control, const wxColour& foreground) {
    const auto window = static_cast<HWND>(control.GetHandle());
    CHARRANGE saved{}, all{0, -1};
    SendMessageW(window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&saved));
    SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&all));
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    SendMessageW(window, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&saved));
    return (format.dwMask & CFM_COLOR) && !(format.dwEffects & CFE_AUTOCOLOR) &&
           format.crTextColor == RGB(foreground.Red(), foreground.Green(), foreground.Blue()) &&
           foreground != control.GetBackgroundColour();
}
LicenseDialog::LicenseDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "AudioHole License Agreement", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    const bool acceptance_required = parent == nullptr;
    SetName(acceptance_required ? "AudioHole license acceptance" : "AudioHole license viewer");
    SetIcons(application_icons());
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticText(this, wxID_ANY, "Please read the following license agreement."), 0,
              wxALL, FromDIP(12));
    auto* agreement =
        new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, FromDIP(wxSize(660, 380)),
                       wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP | wxTE_RICH2);
    load_license(*agreement);
    agreement->SetName("AudioHole license agreement");
    root->Add(agreement, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    if (acceptance_required) {
        auto* accept = new wxRadioButton(this, wxID_ANY, "I accept the agreement",
                                         wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
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
    } else {
        auto* close = new ActionButton(this, "Close", 0, wxID_CLOSE);
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); });
        close->SetDefault();
        root->Add(close, 0, wxALIGN_RIGHT | wxALL, FromDIP(12));
        SetEscapeId(wxID_CLOSE);
        SetAffirmativeId(wxID_CLOSE);
    }
    SetSizerAndFit(root);
    const wxDisplay display(static_cast<unsigned>(std::max(0, wxDisplay::GetFromWindow(this))));
    const auto area = display.GetClientArea();
    SetSize(wxSize(std::min(GetSize().x, area.width - FromDIP(30)),
                   std::min(GetSize().y, area.height - FromDIP(30))));
    SetMinSize(FromDIP(wxSize(320, 280)));
    // First launch follows Windows; the read-only viewer follows its app owner.
    theme_dialog(this, acceptance_required ? "system" : "");
    // Preserve all typography and paragraphs while overriding document ink for
    // legibility in the current theme, including Windows high contrast.
    CHARFORMAT2W ink{};
    ink.cbSize = sizeof(ink);
    ink.dwMask = CFM_COLOR;
    const auto foreground = GetForegroundColour();
    ink.crTextColor = RGB(foreground.Red(), foreground.Green(), foreground.Blue());
    if (!SendMessageW(static_cast<HWND>(agreement->GetHandle()), EM_SETCHARFORMAT, SCF_ALL,
                      reinterpret_cast<LPARAM>(&ink)) ||
        !license_ink_is_readable(*agreement, GetForegroundColour()))
        throw std::runtime_error("Cannot apply readable theme colors to the AudioHole license.");
    if (parent)
        CentreOnParent();
    else
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
            wxTextAttr heading;
            if (!body->IsRich() || body->IsEditable() || !body->GetValue().Contains("TL;DR") ||
                !body->GetValue().Contains("fnord") || body->GetValue().Contains("{\\rtf1") ||
                !body->GetStyle(0, heading) || heading.GetFontWeight() != wxFONTWEIGHT_BOLD ||
                heading.GetTextColour() != dialog->GetForegroundColour() ||
                !license_ink_is_readable(*body, dialog->GetForegroundColour()))
                throw std::runtime_error("License RTF formatting or theme ink was not rendered.");
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
