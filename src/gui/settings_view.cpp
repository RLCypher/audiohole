#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
void Frame::build_settings() {
    auto* page = new wxScrolledWindow(notebook_);
    page->SetScrollRate(0, 16);
    auto* root = new wxBoxSizer(wxVERTICAL);
    personality_choice_ = choices(
        page, root, "Personality — operational facts, warnings, and controls remain identical",
        {"Normal — Default Audio Engineer", "Vanilla — Strictly informational",
         "UwU — I know what you are"});
    personality_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        personality_ = static_cast<presentation::Mode>(personality_choice_->GetSelection());
        dfa_->Show(personality_ != presentation::Mode::Vanilla);
        present(presentation::Message::Personality);
        notebook_->GetPage(3)->Layout();
    });
    notifications_ = choices(page, root, "Tray notifications (session preference)",
                             {"Important changes only", "All routine events", "None"});
    startup_ = choices(page, root, "Startup preview (simulation only; no OS registration)",
                       {"Manual", "Immediate", "Delayed"});
    startup_target_ = choices(page, root, "Startup target", {});
    label(page, root,
          "Delay seconds (simulation; pending deadline stays fixed until explicitly rescheduled)");
    delay_ = new wxSpinCtrl(page, wxID_ANY, "5", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                            0, 300, 5);
    root->Add(delay_, 0, wxALL, 3);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    button(page, row, "Simulate startup", [this] {
        if (startup_->GetSelection() == 0) {
            controller_->stop();
            refresh();
            return;
        }
        const int i = startup_target_->GetSelection();
        if (i < 0) {
            show_result({false, "Select a startup profile."});
            return;
        }
        show_result(
            controller_->schedule(profile_ids_.at(static_cast<std::size_t>(i)),
                                  startup_->GetSelection() == 1 ? 0 : delay_->GetValue() * 1000));
        refresh();
    });
    button(page, row, "Cancel pending activation", [this] {
        controller_->stop();
        present(presentation::Message::Cancelled);
        refresh();
    });
    root->Add(row);
    button(page, root, "Run Introduction Again", [this] { introduction(); });
    label(page, root,
          "Close hides to tray; explicit Exit stops and exits. Minimize keeps ordinary taskbar "
          "behavior.\nNondefault close/tray preferences remain unavailable (O-003).\nProduction "
          "startup registration, JSON import/export, storage roots, and external API are "
          "unavailable.\nO-001 fallback health, O-002 switch preparation, O-004 schemas, O-005 "
          "synchronization,\nand O-006 storage roots remain Open. This build establishes no "
          "production compatibility contract.");
    auto* unavailable = new wxButton(
        page, wxID_ANY, wxS("Production persistence / synchronization policy — unavailable"));
    unavailable->Disable();
    root->Add(unavailable, 0, wxALL, 4);
    auto* advanced = new wxStaticBoxSizer(wxVERTICAL, page, "Advanced");
    dfa_ = button(advanced->GetStaticBox(), advanced, "DFA (Does Fuck-All)",
                  [this] { present(presentation::Message::Dfa); });
    label(advanced->GetStaticBox(), advanced,
          "DFA changes only this window's temporary acknowledgment text. It issues no controller "
          "command.");
    root->Add(advanced, 0, wxEXPAND | wxALL, 4);
    page->SetSizer(root);
    notebook_->AddPage(page, "Settings");
}
void Frame::introduction() {
    wxMessageBox("Welcome to the AudioHole GUI evaluation.\n\n1. Pick a saved profile or create "
                 "one.\n2. Choose capture and playback endpoints for each direction.\n3. Inspect "
                 "the visible format recommendation and buffer.\n4. Save for this session, then "
                 "Activate.\n5. Use Devices and simulated failures to test recovery.\n\nSIMULATION "
                 "ONLY: no audio or real device access. Test Beep emits NO SOUND.\nClose hides to "
                 "tray; Exit terminates. Rerunning this introduction preserves your session.",
                 wxS("AudioHole — Introduction"), wxOK | wxICON_INFORMATION, this);
}

} // namespace audiohole::gui
