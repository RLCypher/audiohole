#include "gui/controls.h"
#include "gui/frame.h"
#include <cmath>

namespace audiohole::gui {
void Frame::build_settings() {
    auto* page = new wxScrolledWindow(notebook_);
    page->SetScrollRate(0, 16);
    auto* root = new wxBoxSizer(wxVERTICAL);
    personality_choice_ = choices(
        page, root,
        "Personality — technical facts, warnings, and controls remain built-in Vanilla information",
        {});
    personality_notice_ = label(page, root, "");
    personality_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        const int selected = personality_choice_->GetSelection();
        if (selected >= 0)
            select_personality(personality_ids_.at(static_cast<std::size_t>(selected)));
    });
    auto* catalogs = new wxBoxSizer(wxHORIZONTAL);
    button(page, catalogs, "Reload personality files", [this] { reload_personalities(); });
    button(page, catalogs, "Load custom personality…", [this] {
        wxFileDialog dialog(this, "Load authored personality", {}, {}, "JSON files (*.json)|*.json",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK)
            return;
        const auto loaded =
            messages_.load_file(std::filesystem::path(dialog.GetPath().ToStdWstring()));
        sync_personality_choices();
        select_personality(loaded.ok ? loaded.id : "vanilla");
        personality_notice_->SetLabel(text(loaded.detail));
        notebook_->GetPage(3)->Layout();
        static_cast<wxScrolledWindow*>(notebook_->GetPage(3))->FitInside();
    });
    root->Add(catalogs);
    label(page, root,
          "Commentary files: personalities/ beside audiohole.exe. Load/reload explicitly; no "
          "runtime scripts or network retrieval. Missing events use Vanilla individually.");
    theme_choice_ =
        choices(page, root, "Appearance (session only)",
                {"Dark (testing default)", "Light", "Follow System (final-build default)"});
    theme_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { apply_appearance(); });
    label(page, root,
          "Windows high contrast takes precedence. Windows-owned dialogs and menus retain system "
          "styling. Preferences are session-only in this evaluation build.");
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
    auto* box = advanced->GetStaticBox();
    dfa_ = button(box, advanced, "DFA", [this] { present(presentation::Message::Dfa); });
    dfa_->SetToolTip("Working as intended.");
    auto* slider_label = label(box, advanced, "DFA slider — presentation only");
    dfa_slider_ = new wxSlider(box, wxID_ANY, 0, -1100, 1100);
    dfa_slider_->SetName("DFA slider, minus eleven to plus eleven");
    dfa_value_ = new wxSpinCtrlDouble(box, wxID_ANY, "0.00", wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, -11.0, 11.0, 0.0, 0.01);
    dfa_value_->SetDigits(2);
    dfa_value_->SetName("DFA exact value, two decimal places");
    advanced->Add(dfa_slider_, 0, wxEXPAND | wxALL, 4);
    advanced->Add(dfa_value_, 0, wxALL, 4);
    auto* helper = label(
        box, advanced,
        "Placebo controls. No routing, repair, configuration, diagnostic, or persistent effect.");
    dfa_controls_ = {dfa_, slider_label, dfa_slider_, dfa_value_, helper};
    // Tracking updates the visible number only. A release/keyboard settlement
    // selects at most one special acknowledgment, never one per drag tick.
    dfa_slider_->Bind(wxEVT_SCROLL_THUMBTRACK, [this](wxScrollEvent&) {
        dfa_value_->SetValue(static_cast<double>(dfa_slider_->GetValue()) / 100.0);
    });
    dfa_slider_->Bind(wxEVT_SCROLL_CHANGED,
                      [this](wxScrollEvent&) { settle_dfa(dfa_slider_->GetValue()); });
    dfa_slider_->Bind(wxEVT_SCROLL_THUMBRELEASE,
                      [this](wxScrollEvent&) { settle_dfa(dfa_slider_->GetValue()); });
    dfa_value_->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) {
        settle_dfa(static_cast<int>(std::lround(dfa_value_->GetValue() * 100.0)));
    });
    dfa_value_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        settle_dfa(static_cast<int>(std::lround(dfa_value_->GetValue() * 100.0)));
    });
    dfa_value_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
        settle_dfa(static_cast<int>(std::lround(dfa_value_->GetValue() * 100.0)));
        event.Skip();
    });
    root->Add(advanced, 0, wxEXPAND | wxALL, 4);
    page->SetSizer(root);
    notebook_->AddPage(page, "Settings");
    reload_personalities();
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

namespace audiohole::gui {
void Frame::sync_personality_choices() {
    personality_choice_->Clear();
    personality_ids_ = {"default", "vanilla", "uwu"};
    personality_choice_->Append(wxS("Normal — Default Audio Engineer"));
    personality_choice_->Append(wxS("Vanilla — Strictly informational"));
    personality_choice_->Append(wxS("UwU — I know what you are"));
    for (const auto& item : messages_.personalities()) {
        if (item.id == "default" || item.id == "uwu")
            continue;
        personality_ids_.push_back(item.id);
        personality_choice_->Append(text(item.display_name + " [" + item.id + "]"));
    }
    const auto found = std::find(personality_ids_.begin(), personality_ids_.end(), personality_);
    personality_choice_->SetSelection(
        found == personality_ids_.end() ? 1 : static_cast<int>(found - personality_ids_.begin()));
}
void Frame::reload_personalities() {
    const auto warnings = messages_.load_directory(personalities_directory_);
    sync_personality_choices();
    const auto requested = personality_;
    select_personality(requested);
    if (!warnings.empty()) {
        personality_notice_->SetLabel(
            text(warnings.front() + " Fix the file and Reload to retry."));
        personality_notice_->Wrap(950);
    }
}
void Frame::select_personality(const std::string& requested) {
    personality_ = messages_.available(requested) ? requested : "vanilla";
    personality_fallback_note_ = requested == personality_
                                     ? ""
                                     : "Selected personality file unavailable or invalid. Vanilla "
                                       "is active; fix the file and Reload.";
    const auto found = std::find(personality_ids_.begin(), personality_ids_.end(), personality_);
    personality_choice_->SetSelection(
        found == personality_ids_.end() ? 1 : static_cast<int>(found - personality_ids_.begin()));
    for (auto* control : dfa_controls_)
        control->Show(personality_ != "vanilla");
    personality_notice_->SetLabel(
        requested == personality_
            ? wxString(wxS("Optional commentary only. Vanilla remains deterministic; selecting it "
                           "hides DFA."))
            : wxString(wxS("Selected personality file unavailable or invalid. Vanilla is active; "
                           "fix the file and Reload.")));
    if (technical_)
        present(presentation::Message::Personality);
    notebook_->GetPage(3)->Layout();
    static_cast<wxScrolledWindow*>(notebook_->GetPage(3))->FitInside();
}
void Frame::settle_dfa(int hundredths) {
    if (personality_ == "vanilla")
        return;
    hundredths = std::clamp(hundredths, -1100, 1100);
    dfa_slider_->SetValue(hundredths);
    dfa_value_->SetValue(static_cast<double>(hundredths) / 100.0);
    if (last_dfa_settled_ == hundredths)
        return;
    last_dfa_settled_ = hundredths;
    const auto event = presentation::dfa_event(hundredths);
    if (event != presentation::Message::Dfa)
        present(event);
}
} // namespace audiohole::gui
