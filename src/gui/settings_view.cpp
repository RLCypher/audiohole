#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
void Frame::build_settings() {
    auto* page = new wxScrolledWindow(notebook_);
    page->SetScrollRate(0, 16);
    auto* root = new wxBoxSizer(wxVERTICAL);
    personality_choice_ = choices(page, root, "Personality", {});
    personality_notice_ = label(page, root, "");
    personality_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        const int selected = personality_choice_->GetSelection();
        if (selected >= 0)
            select_personality(personality_ids_.at(static_cast<std::size_t>(selected)));
    });
    auto* catalogs = new wxBoxSizer(wxHORIZONTAL);
    button(page, catalogs, "Reload personality files", [this] { reload_personalities(); });
    button(page, catalogs, "Load custom personality…", [this] {
        wxFileDialog dialog(this, "Load personality", {}, {}, "JSON files (*.json)|*.json",
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
    theme_choice_ = choices(page, root, "Appearance", {"Dark", "Light", "Follow System"});
    theme_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { apply_appearance(); });
    theme_choice_->SetSelection(2);
    theme_choice_->SetToolTip("Follow Windows app appearance, or choose Light or Dark for this "
                              "session. Windows high contrast takes precedence.");
    personality_choice_->SetToolTip("Choose optional commentary. Vanilla keeps facts only; "
                                    "switching personality never changes routing.");
    notifications_ = choices(page, root, "Tray notifications",
                             {"Important changes only", "All routine events", "None"});
    startup_ = choices(page, root, "Startup", {"Manual", "Immediate", "Delayed"});
    startup_target_ = choices(page, root, "Startup target", {});
    label(page, root, "Startup delay (seconds)");
    delay_ = new wxSpinCtrl(page, wxID_ANY, "5", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                            0, 300, 5);
    root->Add(delay_, 0, wxALL, 3);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    button(page, row, "Preview startup", [this] {
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
    button(page, root, "Replay Tutorial", [this] {
        wxMessageBox("Tutorial not yet implemented, please enjoy this placeholder message.",
                     wxS("AudioHole — Tutorial"), wxOK | wxICON_WARNING, this);
    });
    auto* advanced = new wxStaticBoxSizer(wxVERTICAL, page, "Advanced");
    auto* box = advanced->GetStaticBox();
    dfa_ = button(box, advanced, "DFA", [this] { present_dfa(presentation::Message::Dfa); });
    dfa_->SetToolTip("Working as intended.");
    auto* slider_label = label(box, advanced, "DFA level");
    dfa_slider_ = new wxSlider(box, wxID_ANY, 0, 0, 1100);
    dfa_slider_->SetName("DFA level, zero to eleven");
    // A native floating-point spin editor truncates precision and can clamp raw
    // input before validation. Keep text and stepping separate so every path uses
    // the same exact decimal parser before changing the last valid value.
    auto* numeric = new wxBoxSizer(wxHORIZONTAL);
    dfa_value_ = new wxTextCtrl(box, wxID_ANY, "0.00", wxDefaultPosition, FromDIP(wxSize(100, -1)),
                                wxTE_PROCESS_ENTER);
    dfa_value_->SetName("DFA exact value, normalized to two decimal places");
    dfa_stepper_ = new wxSpinButton(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL);
    dfa_stepper_->SetRange(-1, 1);
    dfa_stepper_->SetValue(0);
    dfa_stepper_->SetName("Adjust DFA by one hundredth");
    dfa_feedback_ = label(box, advanced, "");
    advanced->Add(dfa_slider_, 0, wxEXPAND | wxALL, 4);
    numeric->Add(dfa_value_, 0, wxALIGN_CENTER_VERTICAL);
    numeric->Add(dfa_stepper_, 0, wxALIGN_CENTER_VERTICAL);
    advanced->Add(numeric, 0, wxALL, 4);
    dfa_slider_->SetToolTip(
        "Fiddle with this if you think it'll help. We're not making any promises.");
    dfa_value_->SetToolTip("Type your preferred amount of nonsense. Use a decimal point; extra "
                           "digits round half-up on "
                           "Enter; raw values must be between 0 and 11.");
    dfa_stepper_->SetToolTip(
        "Nudge the nonsense by 0.01. Values outside 0.00 to 11.00 are rejected, not clamped.");
    dfa_controls_ = {dfa_, slider_label, dfa_slider_, dfa_value_, dfa_stepper_, dfa_feedback_};
    // Tracking updates the visible number only. A release/keyboard settlement
    // selects at most one acknowledgment, never one per drag tick.
    dfa_slider_->Bind(wxEVT_SCROLL_THUMBTRACK, [this](wxScrollEvent&) {
        dfa_value_->ChangeValue(text(presentation::dfa_text(dfa_slider_->GetValue())));
    });
    dfa_slider_->Bind(wxEVT_SCROLL_CHANGED,
                      [this](wxScrollEvent&) { settle_dfa(dfa_slider_->GetValue()); });
    dfa_slider_->Bind(wxEVT_SCROLL_THUMBRELEASE,
                      [this](wxScrollEvent&) { settle_dfa(dfa_slider_->GetValue()); });
    dfa_stepper_->Bind(wxEVT_SPIN_UP, [this](wxSpinEvent& event) {
        adjust_dfa(1);
        event.Veto();
    });
    dfa_stepper_->Bind(wxEVT_SPIN_DOWN, [this](wxSpinEvent& event) {
        adjust_dfa(-1);
        event.Veto();
    });
    dfa_value_->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_UP)
            adjust_dfa(1);
        else if (event.GetKeyCode() == WXK_DOWN)
            adjust_dfa(-1);
        else
            event.Skip();
    });
    dfa_value_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { commit_dfa_text(); });
    // Text and arrows form one edit. Moving focus to the stepper must not first
    // reject/restore raw text and then accidentally increment the restored value.
    // The step handler validates the original text; leaving the composite commits.
    auto leave_numeric = [this](wxFocusEvent& event) {
        if (!IsBeingDeleted() && event.GetWindow() != dfa_value_ &&
            event.GetWindow() != dfa_stepper_)
            commit_dfa_text();
        event.Skip();
    };
    dfa_value_->Bind(wxEVT_KILL_FOCUS, leave_numeric);
    dfa_stepper_->Bind(wxEVT_KILL_FOCUS, leave_numeric);
    root->Add(advanced, 0, wxEXPAND | wxALL, 4);
    page->SetSizer(root);
    notebook_->AddPage(page, "Settings");
    reload_personalities();
}
void Frame::testing_notice() {
    wxMessageBox(
        "AudioHole Preview Build\n\n"
        "All devices, capabilities, routing, scans, activity and recovery in this version "
        "are simulated. No real devices are opened and no audio is captured or played. "
        "Test Beep is also simulated and emits no sound. No displayed state is evidence "
        "of a working hardware connection.\n\n"
        "The interface and personality messages preview the intended production experience. "
        "Profiles, edits and preferences last for this session only.\n\n"
        "Choose a profile, select source and destination, then Apply. "
        "Save keeps edits without changing active routing. Close hides to the tray; "
        "Exit stops and closes AudioHole.",
        wxS("AudioHole — Preview Build"), wxOK | wxICON_INFORMATION, this);
}

} // namespace audiohole::gui

namespace audiohole::gui {
void Frame::sync_personality_choices() {
    personality_choice_->Clear();
    personality_ids_ = {"default", "vanilla", "uwu"};
    personality_choice_->Append(wxS("Norm"));
    personality_choice_->Append(wxS("Vanilla — Strictly informational"));
    personality_choice_->Append(wxS("UwU"));
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
    if (dfa_feedback_)
        dfa_feedback_->SetLabel("");
    personality_ = messages_.available(requested) ? requested : "vanilla";
    // Presentation-only placeholder policy; future assets still use set_image().
    agent_display_->set_image(personality_ == "vanilla" ? canonical_logo() : placeholder_avatar());
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
            ? wxString{}
            : wxString(wxS("Selected personality file unavailable or invalid. Vanilla is active; "
                           "fix the file and Reload.")));
    if (technical_)
        present(presentation::Message::Personality);
    notebook_->GetPage(3)->Layout();
    static_cast<wxScrolledWindow*>(notebook_->GetPage(3))->FitInside();
}
// The raw decimal is range-checked before half-up rounding. Invalid text restores
// the previous normalized presentation value; it never reaches the slider/state.
void Frame::commit_dfa_text() {
    if (personality_ == "vanilla")
        return;
    const auto input = presentation::parse_dfa(utf8(dfa_value_->GetValue()));
    if (input.status == presentation::DfaInputStatus::Valid) {
        settle_dfa(input.hundredths);
        return;
    }
    dfa_value_->ChangeValue(text(presentation::dfa_text(dfa_hundredths_)));
    if (input.status == presentation::DfaInputStatus::Negative)
        present_dfa(presentation::Message::DfaNegative);
    else if (input.status == presentation::DfaInputStatus::AboveMaximum)
        present_dfa(presentation::Message::DfaAboveMaximum);
    else
        dfa_value_->SetToolTip(
            "Type your preferred nonsense from 0.00 to 11.00. Extra digits round half-up.");
}
void Frame::adjust_dfa(int delta) {
    if (personality_ == "vanilla")
        return;
    const auto input = presentation::parse_dfa(utf8(dfa_value_->GetValue()));
    if (input.status != presentation::DfaInputStatus::Valid) {
        commit_dfa_text();
        return;
    }
    settle_dfa(input.hundredths + delta);
}
void Frame::settle_dfa(int hundredths) {
    if (personality_ == "vanilla")
        return;
    if (hundredths < 0 || hundredths > 1100) {
        dfa_value_->ChangeValue(text(presentation::dfa_text(dfa_hundredths_)));
        present_dfa(hundredths < 0 ? presentation::Message::DfaNegative
                                   : presentation::Message::DfaAboveMaximum);
        return;
    }
    dfa_hundredths_ = hundredths;
    dfa_slider_->SetValue(hundredths);
    dfa_value_->ChangeValue(text(presentation::dfa_text(hundredths)));
    if (last_dfa_settled_ == hundredths)
        return;
    last_dfa_settled_ = hundredths;
    present_dfa(presentation::dfa_event(hundredths));
}
void Frame::present_dfa(presentation::Message event) {
    if (personality_ == "vanilla")
        return;
    if (!operational_warning_visible_) {
        dfa_feedback_->SetLabel("");
        present(event);
        return;
    }
    const auto copy = messages_.show(event, personality_, ++occurrence_);
    // Preserve the warning, its commentary policy and last_message_ in the header.
    // This local feedback still uses the same authored catalog/selection machinery.
    dfa_feedback_->SetLabel(
        text(copy.technical + " " + copy.commentary + " " + copy.fallback_detail));
    dfa_feedback_->Wrap(700);
    notebook_->GetPage(3)->Layout();
    static_cast<wxScrolledWindow*>(notebook_->GetPage(3))->FitInside();
}
} // namespace audiohole::gui
