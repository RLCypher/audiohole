#include "gui/welcome.h"
#include "gui/branding.h"
#include "gui/controls.h"
#include <wx/display.h>
#include <wx/stdpaths.h>
#include <wx/wrapsizer.h>
namespace audiohole::gui {
WelcomeDialog::WelcomeDialog(bool first_time)
    : wxDialog(nullptr, wxID_ANY, "Welcome to AudioHole!", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    SetIcons(application_icons());
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* content = new wxBoxSizer(wxHORIZONTAL);
    auto* norm = new AgentDisplay(this, wxSize(210, 310));
    norm->SetName("Norm welcome artwork");
    try {
        const auto path =
            std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
                .parent_path() /
            "personalities" / "default" / "preview" / "placeholder.png";
        norm->set_image(package_preview(path));
        has_norm_image_ = norm->has_image();
    } catch (const std::exception&) {
        norm->SetToolTip(
            "Norm artwork is unavailable; restore the companion personalities folder.");
    }
    content->Add(norm, 0, wxEXPAND | wxALL, FromDIP(12));
    const auto copy = wxString::FromUTF8(R"(Welcome to AudioHole!

Plug it in. Put your audio wherever you want.

AudioHole connects your audio devices together. Pick a source, pick a destination, and we'll handle the complicated bits.

Think of it like a Chinese finger trap for audio devices.

Yes, that stupid little woven tube you spent 15 tickets on at Unnamed Pizza Arcade™ when you were six, which confused you for exactly four minutes and left you with a lifelong distrust of novelty woven bamboo trinkets.

We've consulted our engineering department and he says that this analogy is technically accurate.

We've consulted our marketing department and have been informed that we don't have a marketing department.

But I digress, we've worked very hard to recreate that formative childhood experience, but now you've got microphones, mixers, headphones, and probably a USB interface Windows insists doesn't exist.

If you know your way around a hole or two, jump right in. Otherwise, we'll show you how to handle your first one.

It's easier than it looks. Usually.)");
    auto* introduction =
        new wxTextCtrl(this, wxID_ANY, copy, wxDefaultPosition, FromDIP(wxSize(550, 440)),
                       wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP | wxBORDER_NONE);
    introduction->SetName("AudioHole welcome introduction");
    introduction->SetInsertionPoint(0);
    content->Add(introduction, 1, wxEXPAND | wxALL, FromDIP(12));
    root->Add(content, 1, wxEXPAND);
    auto* actions = new wxWrapSizer(wxHORIZONTAL);
    for (const auto& option :
         {std::pair{wxID_YES, "It's my first time"}, std::pair{wxID_NO, "Show me them holes!"},
          std::pair{wxID_CANCEL, "I'm not ready yet"}}) {
        auto* button = new ActionButton(this, option.second, 0, option.first);
        button->SetToolTip(
            option.first == wxID_YES ? "Learn with Norm in a fictional tutorial. Your real devices "
                                       "and profiles stay untouched."
            : option.first == wxID_NO
                ? "Open your saved working environment with real device scanning and audio routing."
                : "Close AudioHole without starting a session.");
        actions->Add(button, 0, wxALL, FromDIP(5));
        button->Bind(wxEVT_BUTTON, [this, id = option.first](wxCommandEvent&) { EndModal(id); });
        if (option.first == (first_time ? wxID_YES : wxID_NO))
            button->SetDefault();
    }
    root->Add(actions, 0, wxALIGN_RIGHT | wxALL, FromDIP(8));
    SetSizerAndFit(root);
    const wxDisplay display(static_cast<unsigned>(std::max(0, wxDisplay::GetFromWindow(this))));
    const auto area = display.GetClientArea();
    const int width = std::max(300, area.width - FromDIP(30));
    const int height = std::max(260, area.height - FromDIP(30));
    introduction->SetMinSize(wxSize(0, FromDIP(140)));
    norm->SetMinSize(wxSize(std::min(FromDIP(210), width / 4), 0));
    SetMinSize(wxSize(std::min(FromDIP(650), width), std::min(FromDIP(390), height)));
    SetSize(wxSize(std::min(GetSize().x, width), std::min(GetSize().y, height)));
    SetEscapeId(wxID_CANCEL);
    theme_dialog(this);
    Centre();
}
} // namespace audiohole::gui
