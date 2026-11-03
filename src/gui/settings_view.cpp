#include "gui/controls.h"
#include "gui/frame.h"
#include "presentation/text_layout.h"

namespace audiohole::gui {
void Frame::build_settings() {
    auto* page = new wxScrolledWindow(notebook_);
    page->SetScrollRate(0, 16);
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* preview_settings = new wxStaticBoxSizer(wxVERTICAL, page, "Preview Build environment");
    auto* environment_box = preview_settings->GetStaticBox();
    preview_mode_ = new wxCheckBox(environment_box, wxID_ANY,
                                   "Preview Mode (load preview profiles and devices)");
    preview_mode_->SetValue(preferences_->values().preview_mode);
    preview_mode_->SetToolTip(
        "Select the environment for the next launch. Preview uses demonstration records; Test uses "
        "isolated persisted configuration. Restart is required.");
    show_preview_dialog_ =
        new wxCheckBox(environment_box, wxID_ANY, "Show preview dialog on startup");
    show_preview_dialog_->SetValue(preferences_->values().show_preview_dialog);
    show_preview_dialog_->SetToolTip(
        "Offer Preview Mode or Test Mode on startup. When disabled, the saved environment starts "
        "directly; Settings can still change the next launch.");
    preview_settings->Add(preview_mode_, 0, wxALL, 4);
    preview_settings->Add(show_preview_dialog_, 0, wxALL, 4);
    auto save_environment = [this](wxCommandEvent&) {
        auto next = preferences_->values();
        next.preview_mode = preview_mode_->GetValue();
        next.show_preview_dialog = show_preview_dialog_->GetValue();
        const auto result = preferences_->update(next);
        if (!result.ok) {
            preview_mode_->SetValue(preferences_->values().preview_mode);
            show_preview_dialog_->SetValue(preferences_->values().show_preview_dialog);
            show_result(result);
        }
    };
    preview_mode_->Bind(wxEVT_CHECKBOX, save_environment);
    show_preview_dialog_->Bind(wxEVT_CHECKBOX, save_environment);
    label(environment_box, preview_settings,
          controller_->snapshot().operating_mode == OperatingMode::Preview
              ? "Active: Preview Mode. Environment changes take effect after restart."
              : "Active: Test Mode (persistent configuration only). Environment changes take "
                "effect after restart.");
    label(environment_box, preview_settings,
          wxS("Test data: ") + wxString(preferences_->test_configuration().wstring()));
    auto* restart_button = button(environment_box, preview_settings, "Restart AudioHole", [this] {
        if (!discard())
            return;
        if (restart_)
            restart_();
        else
            show_result({false, "Restart is unavailable for this host."});
    });
    restart_button->SetToolTip(
        "Close this session cleanly and launch the saved environment after the current process "
        "exits. Saved settings/data are preserved; unsaved edits require confirmation.");
    root->Add(preview_settings, 0, wxEXPAND | wxALL, 4);
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
    theme_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        auto next = preferences_->values();
        next.appearance = theme_choice_->GetSelection() == 0   ? "dark"
                          : theme_choice_->GetSelection() == 1 ? "light"
                                                               : "system";
        const auto result = preferences_->update(next);
        if (!result.ok)
            show_result(result);
        apply_appearance();
    });
    theme_choice_->SetSelection(preferences_->values().appearance == "dark"    ? 0
                                : preferences_->values().appearance == "light" ? 1
                                                                               : 2);
    theme_choice_->SetToolTip("Follow Windows app appearance, or save a Light/Dark preference. "
                              "Windows high contrast takes precedence.");
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
    notifications_->SetSelection(preferences_->values().notifications);
    startup_->SetSelection(preferences_->values().startup_mode);
    delay_->SetValue(preferences_->values().startup_delay_seconds);
    auto save_startup = [this] {
        auto next = preferences_->values();
        next.notifications = notifications_->GetSelection();
        next.startup_mode = startup_->GetSelection();
        next.startup_delay_seconds = delay_->GetValue();
        const int target = startup_target_->GetSelection();
        if (target >= 0 && static_cast<std::size_t>(target) < profile_ids_.size())
            next.startup_profile_id = profile_ids_[static_cast<std::size_t>(target)];
        show_result(preferences_->update(next));
    };
    for (auto* choice : {notifications_, startup_, startup_target_})
        choice->Bind(wxEVT_CHOICE, [save_startup](wxCommandEvent&) { save_startup(); });
    delay_->Bind(wxEVT_SPINCTRL, [save_startup](wxSpinEvent&) { save_startup(); });
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* startup_preview = button(page, row, "Preview startup", [this] {
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
    auto* cancel_startup = button(page, row, "Cancel pending activation", [this] {
        controller_->stop();
        present(presentation::Message::Cancelled);
        refresh();
    });
    root->Add(row);
    const bool preview_environment =
        controller_->snapshot().operating_mode == OperatingMode::Preview;
    for (auto* control :
         std::vector<wxWindow*>{startup_preview, cancel_startup, startup_, startup_target_, delay_})
        control->Enable(preview_environment);
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
        "Preview profiles and devices last for this session only. Test configuration and "
        "Preview Build preferences are saved in their isolated data roots.\n\n"
        "Choose a profile, select source and destination, then Apply. "
        "Save keeps edits without changing active routing. Close hides to the tray; "
        "Exit stops and closes AudioHole.",
        wxS("AudioHole — Preview Build"), wxOK | wxICON_INFORMATION, this);
}

} // namespace audiohole::gui

namespace audiohole::gui {
void Frame::sync_personality_choices() {
    personality_choice_->Clear();
    personality_ids_.clear();
    for (const auto& item : messages_.personalities()) {
        personality_ids_.push_back(item.id);
        personality_choice_->Append(text(item.display_name));
    }
    personality_choice_->Enable(!personality_ids_.empty());
    const auto found = std::find(personality_ids_.begin(), personality_ids_.end(), personality_);
    personality_choice_->SetSelection(found == personality_ids_.end()
                                          ? wxNOT_FOUND
                                          : static_cast<int>(found - personality_ids_.begin()));
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
    personality_ = messages_.resolve_selection(requested);
    // Clear the old identity before loading any new resource. A failed or missing
    // preview leaves this package imageless; it never displays another character.
    agent_display_->clear_image();
    const auto* selected_package = messages_.package(personality_);
    bool show_image = false;
    if (selected_package && !selected_package->preview_image.empty()) {
        try {
            agent_display_->set_image(package_preview(selected_package->preview_image));
            show_image = true;
        } catch (const std::exception&) { /* Optional preview is disabled. */
        }
    }
    agent_display_->Show(show_image);
    const bool has_dfa = !messages_.variants(presentation::Message::Dfa, personality_).empty();
    personality_fallback_note_ =
        requested == personality_
            ? ""
            : (personality_.empty()
                   ? "No valid personality packages. Personality presentation disabled."
                   : "Selected personality unavailable. A complete available package is active; "
                     "repair and Reload to retry.");
    // A temporary missing package does not erase the saved desired stable ID.
    Result preference_result{true, {}};
    if (requested == personality_ && !personality_.empty()) {
        auto preference = preferences_->values();
        preference.personality_id = personality_;
        preference_result = preferences_->update(preference);
    }
    const auto found = std::find(personality_ids_.begin(), personality_ids_.end(), personality_);
    personality_choice_->SetSelection(found == personality_ids_.end()
                                          ? wxNOT_FOUND
                                          : static_cast<int>(found - personality_ids_.begin()));
    for (auto* control : dfa_controls_)
        control->Show(has_dfa);
    personality_notice_->SetLabel(text(personality_fallback_note_));
    if (!preference_result.ok)
        personality_notice_->SetLabel(text(preference_result.detail));
    if (technical_) {
        if (operational_warning_visible_) {
            // A personality switch clears the previous character's flavor but
            // cannot hide an authoritative warning with a low-priority greeting.
            commentary_copy_.clear();
            fallback_copy_ = personality_fallback_note_;
            layout_header();
        } else {
            present(presentation::Message::Personality);
        }
    }
    notebook_->GetPage(3)->Layout();
    static_cast<wxScrolledWindow*>(notebook_->GetPage(3))->FitInside();
}
// The raw decimal is range-checked before half-up rounding. Invalid text restores
// the previous normalized presentation value; it never reaches the slider/state.
void Frame::commit_dfa_text() {
    if (messages_.variants(presentation::Message::Dfa, personality_).empty())
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
    if (messages_.variants(presentation::Message::Dfa, personality_).empty())
        return;
    const auto input = presentation::parse_dfa(utf8(dfa_value_->GetValue()));
    if (input.status != presentation::DfaInputStatus::Valid) {
        commit_dfa_text();
        return;
    }
    settle_dfa(input.hundredths + delta);
}
void Frame::settle_dfa(int hundredths) {
    if (messages_.variants(presentation::Message::Dfa, personality_).empty())
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
    if (messages_.variants(presentation::Message::Dfa, personality_).empty())
        return;
    if (!operational_warning_visible_) {
        dfa_feedback_->SetLabel("");
        present(event);
        return;
    }
    const auto copy = messages_.show(event, personality_, ++occurrence_);
    // Preserve the warning, its commentary policy and last_message_ in the header.
    // This local feedback still uses the same authored catalog/selection machinery.
    dfa_feedback_->SetLabel(text(presentation::keep_kaomoji_together(
        copy.technical + " " + copy.commentary + " " + copy.fallback_detail)));
    dfa_feedback_->Wrap(700);
    notebook_->GetPage(3)->Layout();
    static_cast<wxScrolledWindow*>(notebook_->GetPage(3))->FitInside();
}
} // namespace audiohole::gui
