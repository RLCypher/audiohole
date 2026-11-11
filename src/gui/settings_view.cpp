#include "gui/controls.h"
#include "gui/frame.h"
#include "presentation/text_layout.h"
#include <wx/wrapsizer.h>

namespace audiohole::gui {
void Frame::build_settings() {
    auto* page = new wxScrolledWindow(notebook_);
    page->SetScrollRate(0, 16);
    auto* root = new wxBoxSizer(wxVERTICAL);
    std::vector<std::pair<wxStaticText*, wxString>> wrapping_notes;
    auto* columns = new wxFlexGridSizer(1, 2, FromDIP(12), FromDIP(12));
    columns->AddGrowableCol(0, 1);
    columns->AddGrowableCol(1, 1);
    auto* appearance_group = new wxStaticBoxSizer(wxVERTICAL, page, "Application & Tray");
    auto* appearance_box = appearance_group->GetStaticBox();
    appearance_box->SetName("Application & Tray settings");
    auto* startup_group = new wxStaticBoxSizer(wxVERTICAL, page, "Routing Activation");
    auto* startup_box = startup_group->GetStaticBox();
    startup_box->SetName("Routing Activation settings");
    columns->Add(appearance_group, 1, wxEXPAND);
    columns->Add(startup_group, 1, wxEXPAND);
    root->Add(columns, 0, wxEXPAND | wxALL, FromDIP(4));
    theme_choice_ =
        choices(appearance_box, appearance_group, "Appearance", {"Dark", "Light", "Follow System"});
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
    notifications_ = choices(appearance_box, appearance_group, "Tray notifications",
                             {"Important changes only", "All routine events", "None"});
    startup_ = choices(startup_box, startup_group, "Routing on startup",
                       {"Immediately", "After Delay", "Manual"});
    startup_target_ = choices(startup_box, startup_group, "Profile on startup", {});
    label(startup_box, startup_group, "Routing activation delay (seconds)");
    delay_ = new wxSpinCtrl(startup_box, wxID_ANY, "5", wxDefaultPosition, FromDIP(wxSize(75, -1)),
                            wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 300, 5);
    startup_group->Add(delay_, 0, wxALL, FromDIP(4));
    notifications_->SetSelection(preferences_->values().notifications);
    startup_->SetSelection((preferences_->values().startup_mode + 2) % 3);
    delay_->SetValue(preferences_->values().startup_delay_seconds);
    auto sync_delay = [this] {
        const bool delayed = startup_->GetSelection() == 1;
        delay_->SetValue(delayed ? preferences_->values().startup_delay_seconds : 0);
        delay_->Enable(delayed && controller_->snapshot().operating_mode == OperatingMode::Preview);
    };
    sync_delay();
    auto save_startup = [this, sync_delay] {
        auto next = preferences_->values();
        next.notifications = notifications_->GetSelection();
        next.startup_mode = (startup_->GetSelection() + 1) % 3;
        if (startup_->GetSelection() == 1)
            next.startup_delay_seconds = delay_->GetValue();
        const int target = startup_target_->GetSelection();
        if (target >= 0 && static_cast<std::size_t>(target) < profile_ids_.size())
            next.startup_profile_id = profile_ids_[static_cast<std::size_t>(target)];
        show_result(preferences_->update(next));
        sync_delay();
    };
    for (auto* choice : {notifications_, startup_, startup_target_})
        choice->Bind(wxEVT_CHOICE, [save_startup](wxCommandEvent&) { save_startup(); });
    delay_->Bind(wxEVT_SPINCTRL, [save_startup](wxSpinEvent&) { save_startup(); });
    delay_->Bind(wxEVT_TEXT_ENTER, [this, save_startup](wxCommandEvent&) {
        long value = 0;
        if (!delay_->GetTextValue().ToLong(&value) || value < 0 || value > 300) {
            show_result({false, "Enter a routing activation delay from 0 to 300 seconds."});
            return;
        }
        save_startup();
    });
    auto* row = new wxWrapSizer(wxHORIZONTAL);
    auto* startup_preview = button(startup_box, row, "Preview startup", [this] {
        if (startup_->GetSelection() == 2) {
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
                                  startup_->GetSelection() == 0 ? 0 : delay_->GetValue() * 1000));
        refresh();
    });
    auto* cancel_startup = button(startup_box, row, "Cancel pending activation", [this] {
        controller_->stop();
        present(presentation::Message::Cancelled);
        refresh();
    });
    startup_group->Add(row, 0, wxEXPAND);
    const bool preview_environment =
        controller_->snapshot().operating_mode == OperatingMode::Preview;
    for (auto* control :
         std::vector<wxWindow*>{startup_preview, cancel_startup, startup_, startup_target_, delay_})
        control->Enable(preview_environment);
    sync_delay();
    auto* windows_startup = new wxWrapSizer(wxHORIZONTAL);
    auto* launch = new wxCheckBox(appearance_box, wxID_ANY, "Start with Windows");
    auto* minimized = new wxCheckBox(appearance_box, wxID_ANY, "Start minimized");
    launch->SetValue(preferences_->values().start_with_windows);
    minimized->SetValue(preferences_->values().start_minimized);
    minimized->SetToolTip("Launch minimized using the saved Preview/Test environment, without the "
                          "environment prompt.");
    windows_startup->Add(launch, 0, wxALL, FromDIP(4));
    windows_startup->Add(minimized, 0, wxALL, FromDIP(4));
    appearance_group->Add(windows_startup, 0, wxEXPAND);
    auto save_launch = [this, launch, minimized](wxCommandEvent&) {
        auto next = preferences_->values();
        const bool previous = next.start_with_windows;
        if (launch->GetValue() != previous && !smoke_) {
            const auto result = configure_windows_startup(launch->GetValue());
            if (!result.ok) {
                launch->SetValue(previous);
                show_result(result);
                return;
            }
        }
        next.start_with_windows = launch->GetValue();
        next.start_minimized = minimized->GetValue();
        const auto result = preferences_->update(next);
        if (!result.ok) {
            if (!smoke_)
                (void)configure_windows_startup(previous);
            launch->SetValue(previous);
            minimized->SetValue(preferences_->values().start_minimized);
            show_result(result);
        }
    };
    launch->Bind(wxEVT_CHECKBOX, save_launch);
    minimized->Bind(wxEVT_CHECKBOX, save_launch);
    button(appearance_box, appearance_group, "Replay Tutorial", [this] {
        if (themed_message_box("AudioHole will restart in Tutorial mode", "Replay Tutorial?",
                               wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
            return;
        themed_message_box("Tutorial not yet implemented, please enjoy this placeholder message.",
                           wxS("AudioHole — Tutorial"), wxOK | wxICON_WARNING, this);
    });
    auto* personality_group = new wxStaticBoxSizer(wxVERTICAL, page, "Personality & animation");
    auto* personality_box = personality_group->GetStaticBox();
    personality_box->SetName("Personality & animation settings");
    personality_choice_ = choices(personality_box, personality_group, "Personality", {});
    personality_notice_ = label(personality_box, personality_group, "");
    auto* playback = new wxWrapSizer(wxHORIZONTAL);
    hold_expressions_ =
        new wxCheckBox(personality_box, wxID_ANY, "Hold expressions between events");
    animation_loops_ = new wxSpinCtrl(personality_box, wxID_ANY, "1", wxDefaultPosition,
                                      FromDIP(wxSize(75, -1)), wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER,
                                      1, 20, preferences_->values().animation_loops);
    animation_loops_->SetName("Animation loop count");
    loop_until_event_ = new wxCheckBox(personality_box, wxID_ANY, "Loop until next event");
    playback->Add(hold_expressions_, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(4));
    auto* loop_count = new wxBoxSizer(wxHORIZONTAL);
    label(personality_box, loop_count, "Animation loop count");
    loop_count->Add(animation_loops_, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(4));
    playback->Add(loop_count, 0, wxALIGN_CENTER_VERTICAL);
    playback->Add(loop_until_event_, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(4));
    low_motion_ = new wxCheckBox(personality_box, wxID_ANY, "Low motion");
    low_motion_->SetValue(preferences_->values().low_motion);
    low_motion_->SetToolTip(
        "Show the first animation frame. Windows reduced-motion settings also take precedence.");
    playback->Add(low_motion_, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(4));
    low_motion_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        auto next = preferences_->values();
        next.low_motion = low_motion_->GetValue();
        const auto result = preferences_->update(next);
        low_motion_->SetValue(preferences_->values().low_motion);
        if (!result.ok)
            show_result(result);
        apply_appearance();
        poll_avatar();
    });
    personality_group->Add(playback, 0, wxEXPAND);
    hold_expressions_->SetToolTip(
        "Hold the last frame after the final repeat. Otherwise hold it for two extra seconds. "
        "Warnings, new events and inactivity sleep can interrupt.");
    animation_loops_->SetToolTip("Total plays per reaction, 1–20; 1 plays once. Baseline "
                                 "idle/working animations keep their authored looping behavior.");
    loop_until_event_->SetToolTip("Repeat the reaction until another event. Overrides loop count "
                                  "and final-frame hold; warnings, identity changes and inactivity "
                                  "sleep still take priority. Reduced motion remains static.");
    auto sync_playback = [this] {
        const auto& values = preferences_->values();
        hold_expressions_->SetValue(values.hold_expressions);
        animation_loops_->SetValue(values.animation_loops);
        loop_until_event_->SetValue(values.loop_until_event);
        hold_expressions_->Enable(!values.loop_until_event);
        animation_loops_->Enable(!values.loop_until_event);
        avatar_host_->hold_expressions(values.hold_expressions);
        avatar_host_->reaction_playback(values.animation_loops, values.loop_until_event);
    };
    sync_playback();
    auto save_playback = [this, sync_playback] {
        long count = 0;
        if (!animation_loops_->GetTextValue().ToLong(&count) || count < 1 || count > 20) {
            themed_message_box("Enter a total play count from 1 to 20.", "Animation loop count",
                               wxOK | wxICON_INFORMATION, this);
            sync_playback();
            return;
        }
        auto next = preferences_->values();
        next.hold_expressions = hold_expressions_->GetValue();
        next.animation_loops = static_cast<int>(count);
        next.loop_until_event = loop_until_event_->GetValue();
        const auto result = preferences_->update(next);
        sync_playback();
        if (!result.ok)
            show_result(result);
        poll_avatar();
    };
    hold_expressions_->Bind(wxEVT_CHECKBOX, [save_playback](wxCommandEvent&) { save_playback(); });
    loop_until_event_->Bind(wxEVT_CHECKBOX, [save_playback](wxCommandEvent&) { save_playback(); });
    animation_loops_->Bind(wxEVT_SPINCTRL, [save_playback](wxSpinEvent&) { save_playback(); });
    animation_loops_->Bind(wxEVT_TEXT_ENTER, [save_playback](wxCommandEvent&) { save_playback(); });
    animation_loops_->Bind(wxEVT_KILL_FOCUS, [save_playback](wxFocusEvent& event) {
        save_playback();
        event.Skip();
    });
    personality_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        const int selected = personality_choice_->GetSelection();
        if (selected >= 0)
            select_personality(personality_ids_.at(static_cast<std::size_t>(selected)));
    });
    auto* catalogs = new wxWrapSizer(wxHORIZONTAL);
    button(personality_box, catalogs, "Reload personality files",
           [this] { reload_personalities(); });
    button(personality_box, catalogs, "Load custom personality…", [this] {
        wxFileDialog dialog(this, "Load personality", {}, {}, "JSON files (*.json)|*.json",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        theme_dialog(&dialog);
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
    personality_group->Add(catalogs, 0, wxEXPAND);
    personality_choice_->SetToolTip("Choose optional commentary. Vanilla keeps facts only; "
                                    "switching personality never changes routing.");
    root->Add(personality_group, 0, wxEXPAND | wxALL, FromDIP(4));
    auto* advanced = new wxStaticBoxSizer(wxVERTICAL, page, "Advanced");
    auto* box = advanced->GetStaticBox();
    box->SetName("Advanced settings");
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
    auto* environment_note =
        label(environment_box, preview_settings,
              controller_->snapshot().operating_mode == OperatingMode::Preview
                  ? "Active: Preview Mode. Environment changes take effect after restart."
                  : "Active: Test Mode (persistent configuration and real device scanning). "
                    "Environment changes take "
                    "effect after restart.");
    auto* data_note =
        label(environment_box, preview_settings,
              wxS("Test data: ") + wxString(preferences_->test_configuration().wstring()));
    for (auto* note : {environment_note, data_note}) {
        wrapping_notes.emplace_back(note, note->GetLabel());
        note->SetMinSize(wxSize(0, -1));
        note->SetToolTip(note->GetLabel());
    }
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
    root->Add(preview_settings, 0, wxEXPAND | wxALL, FromDIP(4));
    page->Bind(wxEVT_SIZE, [page, wrapping_notes, last_width = 0](wxSizeEvent& event) mutable {
        const int width = page->GetClientSize().x;
        if (width > 0 && width != last_width) {
            last_width = width;
            for (const auto& [note, original] : wrapping_notes) {
                note->SetLabel(original);
                note->Wrap(std::max(1, width - page->FromDIP(32)));
            }
            page->Layout();
            page->FitInside();
        }
        event.Skip();
    });
    page->SetSizer(root);
    notebook_->AddPage(page, "Settings");
    reload_personalities();
}
void Frame::testing_notice() {
    if (controller_->snapshot().operating_mode == OperatingMode::Test) {
        themed_message_box(
            "Test Mode saves isolated configuration and reads real Windows device evidence. "
            "Basic scans do not open experimental streams. Deep scans require consent and can "
            "interrupt audio. "
            "Routing, monitoring and Test Beep remain unavailable. External device JSON edits and "
            "filename renames can be read with Reload saved devices.",
            "AudioHole Test Mode", wxOK | wxICON_INFORMATION, this);
        return;
    }
    themed_message_box(
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
    agent_display_->present({});
    const auto* selected_package = messages_.package(personality_);
    avatar_host_->observe(controller_->snapshot(), operational_warning_visible_,
                          avatar::monotonic_ms());
    avatar_host_->select(selected_package, avatar::monotonic_ms());
    agent_display_->Show(avatar_host_->enabled());
    last_avatar_diagnostics_.clear();
    poll_avatar();
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
