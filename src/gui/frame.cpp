#include "gui/frame.h"
#include "gui/controls.h"
#include <cmath>
#include <wx/stdpaths.h>

namespace audiohole::gui {
Frame::Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke)
    : wxFrame(nullptr, wxID_ANY, wxS("AudioHole"), wxDefaultPosition, wxSize(1240, 900)),
      controller_(std::move(controller)), timer_(this), smoke_(smoke) {
    SetTitle(GetTitle() + wxS(" — build ") + text(AUDIOHOLE_BUILD_COMMIT));
    // A stable window property supports second-launch focus across build titles.
    // It is removed before the native window is destroyed (see the destructor).
    SetPropW(static_cast<HWND>(GetHandle()), L"AudioHole.GuiWindow", reinterpret_cast<HANDLE>(1));
    SetMinSize(wxSize(980, 740));
    personalities_directory_ =
        std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
            .parent_path() /
        "personalities";
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* heading = label(this, root, "AudioHole");
    heading->SetFont(wxFont(wxFontInfo(20).Bold()));
    auto* status_row = new wxBoxSizer(wxHORIZONTAL);
    status_dot_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(22, 22)));
    status_dot_->SetMinSize(FromDIP(wxSize(22, 22)));
    status_dot_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    status_dot_->Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxPaintDC dc(status_dot_);
        dc.SetBackground(wxBrush(status_dot_->GetBackgroundColour()));
        dc.Clear();
        dc.SetPen(wxPen(status_dot_->GetForegroundColour()));
        dc.SetBrush(wxBrush(status_dot_->GetForegroundColour()));
        const auto size = status_dot_->GetClientSize();
        dc.DrawCircle(size.x / 2, size.y / 2, status_dot_->FromDIP(5));
    });
    status_row->Add(status_dot_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    status_ = label(this, status_row, "Stopped");
    root->Add(status_row, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    start_ = button(this, actions, "Start Routing", [this] { activate(false); });
    button(this, actions, "Arm Ready", [this] { activate(true); });
    stop_ = button(this, actions, "Stop Routing", [this] {
        controller_->stop();
        refresh();
    });
    button(this, actions, "Exit AudioHole", [this] { exit(); });
    root->Add(actions, 0, wxEXPAND);
    technical_ = label(this, root, "");
    technical_->SetFont(GetFont().Bold());
    commentary_ = label(this, root, "");
    fallback_notice_ = label(this, root, "");
    auto* navigation_row = new wxBoxSizer(wxHORIZONTAL);
    for (std::size_t index = 0; index < 4; ++index) {
        auto* tab = new NavigationButton(this);
        tab->SetMinSize(wxSize(190, 36));
        navigation_.push_back(tab);
        navigation_row->Add(tab, 1, wxEXPAND | wxALL, 3);
        tab->Bind(wxEVT_TOGGLEBUTTON, [this, index](wxCommandEvent&) { select_page(index); });
        tab->Bind(wxEVT_KEY_DOWN, [this, index](wxKeyEvent& event) {
            if (event.GetKeyCode() == WXK_LEFT)
                select_page((index + 3) % 4, true);
            else if (event.GetKeyCode() == WXK_RIGHT)
                select_page((index + 1) % 4, true);
            else if (event.GetKeyCode() == WXK_HOME)
                select_page(0, true);
            else if (event.GetKeyCode() == WXK_END)
                select_page(3, true);
            else
                event.Skip();
        });
    }
    root->Add(navigation_row, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);
    notebook_ = new wxSimplebook(this, wxID_ANY);
    root->Add(notebook_, 1, wxEXPAND | wxALL, 6);
    build_profiles();
    build_devices();
    build_scan();
    build_settings();
    SetSizer(root);
    Centre();
    tray_ = std::make_unique<Tray>(*this);
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            controller_->advance(100);
            refresh();
        },
        timer_.GetId());
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
        if (!e.CanVeto()) {
            controller_->stop();
            e.Skip();
            return;
        }
        if (smoke_) {
            e.Veto();
            exit();
            return;
        }
        if (tray_->IsIconInstalled()) {
            end_observation();
            Hide();
            e.Veto();
        } else {
            e.Veto();
            wxMessageBox(
                "The tray icon is unavailable. Use Exit AudioHole to stop and close safely.",
                "AudioHole", wxOK | wxICON_INFORMATION, this);
        }
    });
    Bind(wxEVT_ICONIZE, [this](wxIconizeEvent& e) {
        if (e.IsIconized())
            end_observation();
        e.Skip();
    });
    notebook_->Bind(wxEVT_BOOKCTRL_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
        if (e.GetSelection() != 0)
            end_observation();
        update_navigation();
        e.Skip();
    });
    apply_appearance();
    select_profile("profile-simplex");
    populate_devices();
    refresh();
    present(presentation::Message::Ready);
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (event.ControlDown() && event.GetKeyCode() == WXK_TAB)
            select_page((static_cast<std::size_t>(notebook_->GetSelection()) +
                         (event.ShiftDown() ? 3 : 1)) %
                            4,
                        true);
        else
            event.Skip();
    });
    timer_.Start(100);
}
void Frame::present(presentation::Message event) {
    last_message_ = event;
    const auto c = messages_.show(event, personality_, ++occurrence_);
    fallback_notice_->SetLabel(
        text(personality_fallback_note_.empty() ? c.fallback_detail : personality_fallback_note_));
    technical_->SetLabel(text(c.technical));
    commentary_->SetLabel(text(c.commentary));
    technical_->Wrap(std::max(500, GetClientSize().x - 35));
    commentary_->Wrap(std::max(500, GetClientSize().x - 35));
    Layout();
}
void Frame::show_result(const audiohole::Result& result) {
    technical_->SetLabel(text(result.detail));
    technical_->Wrap(std::max(500, GetClientSize().x - 35));
    if (!result.ok && !smoke_)
        wxMessageBox(text(result.detail), wxS("AudioHole — action unavailable"),
                     wxOK | wxICON_WARNING, this);
}
bool Frame::discard() {
    if (!dirty_)
        return true;
    return wxMessageBox("Discard unsaved editor changes? Saved session profiles and the active "
                        "route will be retained.",
                        "Unsaved changes", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                        this) == wxYES;
}
void Frame::end_observation() {
    controller_->monitor(false);
    controller_->cancel_beep();
    monitor_->SetValue(false);
}
// GUI timer polling does not create presentation events. A controller sequence
// number gates commentary, so repainting/telemetry cannot consume more variants.
void Frame::refresh() {
    const auto& s = controller_->snapshot();
    const auto* active = audiohole::profile(s, s.active_profile);
    std::string status = std::string{} + audiohole::name(s.state) +
                         " | Active: " + (active ? active->display_name : "none") +
                         " | Viewed: " + draft_.display_name + (dirty_ ? " (unsaved edits)" : "");
    if (!s.pending_profile.empty())
        status += " | Startup pending: " + std::to_string((s.pending_ms + 999) / 1000) + " s";
    std::vector<std::string> enabled_holes;
    for (const auto& path : s.paths)
        if (std::find(enabled_holes.begin(), enabled_holes.end(), path.hole_id) ==
            enabled_holes.end())
            enabled_holes.push_back(path.hole_id);
    if (enabled_holes.size() > 4)
        status += " | Extended-range advisory: 5–8 enabled holes; hardware performance unverified.";
    if (s.scanning)
        status += " | Scan: " + std::to_string(s.scan_percent) + "%";
    status_->SetLabel(text(status));
    update_status_indicator();
    std::string paths;
    for (const auto& p : s.paths)
        paths += p.hole_id + (p.reverse ? " reverse: " : " forward: ") + audiohole::name(p.state) +
                 " — " + p.detail + "\n";
    path_status_->SetLabel(text(paths.empty() ? "No active directions." : paths));
    path_status_->Wrap(620);
    const std::string observed_hole =
        hole_index_ >= 0 ? draft_.holes.at(static_cast<std::size_t>(hole_index_)).id : "";
    const bool observe_reverse = activity_direction_->GetSelection() == 1;
    const bool running = std::any_of(s.paths.begin(), s.paths.end(), [&](const auto& path) {
        return path.hole_id == observed_hole && path.reverse == observe_reverse &&
               path.state == audiohole::State::Running;
    });
    monitor_->SetValue(s.monitoring);
    activity_->SetLabel(
        s.monitoring ? (running ? "Source activity detected. Destination audibility is untested."
                                : "No source activity. Silence does not establish disconnection.")
                     : "Observation off.");
    // MSVC requires both branches to be wxString: its pointer conversions make
    // a mixed literal/wxString conditional ambiguous.
    beep_status_->SetLabel(
        s.beep_destination.empty()
            ? wxString(wxS("Test Beep inactive."))
            : text("Test Beep active: " + s.beep_destination + ". Cancel immediately if needed."));
    scan_progress_->SetValue(s.scan_percent);
    const auto query = log_search_->GetValue().Lower();
    std::string logs = "Technical diagnostics\n";
    for (const auto& e : s.events) {
        if (severity_->GetSelection() == 1 && e.type != "error" && e.type != "scan-failed")
            continue;
        if (severity_->GetSelection() == 2 && e.type != "state" && e.type != "activated" &&
            e.type != "stopped")
            continue;
        const std::string line =
            std::to_string(e.sequence) + " [" + e.type + "] " + e.detail + "\n";
        if (query.empty() || text(line).Lower().Contains(query))
            logs += line;
    }
    if (logs != last_log_) {
        console_->ChangeValue(text(logs));
        last_log_ = logs;
    }
    for (const auto& e : s.events)
        if (e.sequence > seen_event_) {
            seen_event_ = e.sequence;
            bool routine = true;
            presentation::Message message = presentation::Message::Ready;
            if (e.type == "scan-start")
                message = presentation::Message::ScanStarted;
            else if (e.type == "scan-complete")
                message = presentation::Message::ScanCompleted;
            else if (e.type == "scan-cancelled")
                message = presentation::Message::ScanCancelled;
            else if (e.type == "scan-failed")
                message = presentation::Message::ScanFailed;
            else if (e.type == "activated")
                message = presentation::Message::Activated;
            else if (e.type == "stopped")
                message = presentation::Message::Stopped;
            else if (e.type == "pending")
                message = presentation::Message::Pending;
            else if (e.type == "reconnected")
                message = presentation::Message::Reconnected;
            else if (e.type == "disconnected")
                message = presentation::Message::Disconnected;
            else if (e.type == "recovering")
                message = presentation::Message::Recovering;
            else if (e.type == "beep-complete")
                message = presentation::Message::BeepComplete;
            else if (e.type == "state" && s.state == audiohole::State::Running)
                message = presentation::Message::Started;
            else
                routine = false;
            if (routine)
                present(message);
            if (e.type == "error" || e.type == "scan-failed") {
                technical_->SetLabel(text(e.detail));
                commentary_->SetLabel("");
            }
            if (!smoke_ && tray_->IsIconInstalled() && notifications_->GetSelection() != 2 &&
                (e.type == "error" ||
                 (notifications_->GetSelection() == 1 &&
                  (e.type == "reconnected" || e.type == "disconnected" || e.type == "stopped")))) {
                tray_->ShowBalloon(wxS("AudioHole"), text(e.detail),
                                   e.type == "error" ? wxICON_WARNING : wxICON_INFORMATION, 3000);
            }
        }
    tray_->update(s);
}
bool Frame::smoke(const wxString& directory) {
    try {
        auto require = [](bool ok, const char* detail) {
            if (!ok)
                throw std::runtime_error(detail);
        };
        const auto check_button_help = [&](const auto& self, wxWindow* window) -> void {
            if (dynamic_cast<wxButton*>(window) || dynamic_cast<wxToggleButton*>(window))
                require(window->GetToolTip() && !window->GetToolTip()->GetTip().empty(),
                        "An action or navigation button lacks explanatory hover help");
            for (auto* child : window->GetChildren())
                self(self, child);
        };
        check_button_help(check_button_help, this);
        require(GetPropW(static_cast<HWND>(GetHandle()), L"AudioHole.GuiWindow") != nullptr,
                "Stable second-launch window identity missing");
        require(theme_choice_->GetSelection() == 2, "Follow System is not the initial theme");
        require(status_dot_->IsShown(), "Routing status indicator missing");
        require(notebook_->GetPageCount() == 4, "Expected four native GUI pages");
        require(wxWindow::FindWindowByLabel(wxS("Export visible diagnostics…"), this) != nullptr,
                "UTF-8 control copy changed under the Windows locale");
        select_profile("profile-duplex");
        wxCommandEvent start(wxEVT_BUTTON, start_->GetId());
        start_->GetEventHandler()->ProcessEvent(start);
        controller_->advance(300);
        refresh();
        require(controller_->snapshot().state == audiohole::State::Running,
                "GUI Start did not reach Running");
        require(controller_->snapshot().paths.size() == 2, "Duplex directions missing");
        require(messages_.available("default") && messages_.available("uwu"),
                "Packaged personality files unavailable or invalid");
        const auto before = controller_->snapshot().revision;
        wxCommandEvent dfa(wxEVT_BUTTON, dfa_->GetId());
        dfa_->GetEventHandler()->ProcessEvent(dfa);
        dfa_->GetEventHandler()->ProcessEvent(dfa);
        require(controller_->snapshot().revision == before, "DFA changed controller state");
        for (int value : {0, 69, 420, 666, 670, 690, 1100}) {
            settle_dfa(value);
            require(dfa_slider_->GetValue() == value, "DFA slider/value mismatch");
        }
        require(controller_->snapshot().revision == before, "DFA slider changed controller state");
        require(dfa_->GetLabel() == "DFA", "DFA label changed");
        require(dfa_slider_->GetMin() == 0 && dfa_slider_->GetMax() == 1100,
                "DFA range must be zero to eleven");
        // Change only the edit field, reproducing typed input with a stale
        // spin-control cache; dispatch Enter through the actual child control.
        dfa_value_->GetText()->ChangeValue(wxS("6.70"));
        wxCommandEvent enter(wxEVT_TEXT_ENTER, dfa_value_->GetText()->GetId());
        dfa_value_->GetText()->GetEventHandler()->ProcessEvent(enter);
        require(dfa_slider_->GetValue() == 670 &&
                    last_message_ == presentation::Message::DfaSixSeven,
                "Typed DFA Enter did not commit the text and move the slider");
        require(technical_->GetLabel() == "DFA Acknowledged", "DFA heading changed");
        dfa_value_->GetText()->ChangeValue(wxS("5.55"));
        dfa_value_->GetText()->GetEventHandler()->ProcessEvent(enter);
        require(dfa_slider_->GetValue() == 555, "Ordinary typed DFA value did not move the slider");
        require(last_message_ == presentation::Message::Dfa && !commentary_->GetLabel().empty(),
                "Ordinary DFA adjustment lacks a pool acknowledgment");
        const auto ordinary_commentary = commentary_->GetLabel();
        settle_dfa(556);
        require(last_message_ == presentation::Message::Dfa &&
                    commentary_->GetLabel() != ordinary_commentary &&
                    controller_->snapshot().revision == before,
                "Ordinary DFA adjustments repeat commentary or affect controller state");
        settle_dfa(555);
        const auto dfa_occurrence = occurrence_;
        settle_dfa(555);
        require(occurrence_ == dfa_occurrence, "Duplicate DFA settlement repeated acknowledgment");
        dfa_value_->GetText()->ChangeValue(wxS("12.00"));
        dfa_value_->GetText()->GetEventHandler()->ProcessEvent(enter);
        require(dfa_slider_->GetValue() == 555 && std::abs(dfa_value_->GetValue() - 5.55) < 0.005 &&
                    occurrence_ == dfa_occurrence && controller_->snapshot().revision == before,
                "Out-of-range DFA input changed the controls, commentary or controller");
        forward_.buffer->SetValue(forward_.buffer->GetValue() + 1);
        const auto buffer_before = controller_->snapshot().revision;
        wxSpinEvent buffer_change(wxEVT_SPINCTRL, forward_.buffer->GetId());
        forward_.buffer->GetEventHandler()->ProcessEvent(buffer_change);
        require(last_message_ == presentation::Message::BufferChanged,
                "Buffer edit missing top callout");
        const auto buffer_occurrence = occurrence_;
        forward_.buffer->GetEventHandler()->ProcessEvent(buffer_change);
        require(occurrence_ == buffer_occurrence,
                "Duplicate buffer notification repeated commentary");
        require(controller_->snapshot().revision == buffer_before && dirty_,
                "Buffer acknowledgment changed live state or lost staged edit");
        for (int i = 0; i < 3; ++i) {
            personality_choice_->SetSelection(i);
            wxCommandEvent change(wxEVT_CHOICE, personality_choice_->GetId());
            personality_choice_->GetEventHandler()->ProcessEvent(change);
            require(dfa_->IsShown() == (i != 1) && dfa_slider_->IsShown() == (i != 1),
                    "DFA visibility mismatch");
        }
        select_personality("missing-catalog");
        require(personality_ == "vanilla" && !dfa_->IsShown() && !dfa_slider_->IsShown(),
                "Unavailable catalog did not fall back to Vanilla");
        require(!fallback_notice_->GetLabel().empty(),
                "Whole-personality fallback lacks explanation");
        select_personality("default");
        const auto appearance_before = controller_->snapshot().revision;
        for (int theme = 0; theme < 3; ++theme) {
            theme_choice_->SetSelection(theme);
            wxCommandEvent changed(wxEVT_CHOICE, theme_choice_->GetId());
            theme_choice_->GetEventHandler()->ProcessEvent(changed);
            require(controller_->snapshot().revision == appearance_before,
                    "Appearance changed controller state");
        }
        theme_choice_->SetSelection(0);
        apply_appearance();
        select_page(0);
        require(navigation_[0]->GetValue() && !navigation_[1]->GetValue(),
                "Selected navigation state mismatch");
        require(navigation_[0]->GetLabel().Contains("[selected]"),
                "Selected tab lacks textual distinction");
        wxCommandEvent scan(wxEVT_BUTTON, scan_start_->GetId());
        scan_start_->GetEventHandler()->ProcessEvent(scan);
        require(last_message_ == presentation::Message::ScanStarted,
                "Scan start missing top callout");
        require(!commentary_->GetLabel().empty(), "Scan start lacks personality commentary");
        const auto scan_heading = technical_->GetLabel();
        const auto commentary_before = commentary_->GetLabel();
        controller_->advance(100);
        refresh();
        require(commentary_->GetLabel() == commentary_before,
                "Scan polling regenerated commentary");
        wxCommandEvent cancel(wxEVT_BUTTON, scan_cancel_->GetId());
        scan_cancel_->GetEventHandler()->ProcessEvent(cancel);
        require(last_message_ == presentation::Message::ScanCancelled,
                "Scan cancel missing top callout");
        scan_start_->GetEventHandler()->ProcessEvent(scan);
        controller_->advance(3000);
        refresh();
        require(last_message_ == presentation::Message::ScanCompleted,
                "Scan completion missing top callout");
        require(technical_->GetLabel() != scan_heading && !commentary_->GetLabel().empty(),
                "Scan completion did not update information and commentary");
        controller_->monitor(true);
        notebook_->SetSelection(1);
        require(!controller_->snapshot().monitoring, "Monitor survived panel closure");
        wxCommandEvent stop(wxEVT_BUTTON, stop_->GetId());
        stop_->GetEventHandler()->ProcessEvent(stop);
        controller_->advance(200);
        refresh();
        require(controller_->snapshot().state == audiohole::State::Stopped,
                "GUI Stop did not disarm");
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            for (std::size_t i = 0; i < notebook_->GetPageCount(); ++i) {
                notebook_->SetSelection(i);
                Layout();
                Update();
                wxYield();
                wxBitmap bitmap(GetClientSize());
                wxMemoryDC dest(bitmap);
                wxClientDC source(this);
                dest.Blit(0, 0, GetClientSize().x, GetClientSize().y, &source, 0, 0);
                dest.SelectObject(wxNullBitmap);
                // Capture availability depends on the Windows desktop/session. This is evidence for
                // human review, not a visual correctness assertion.
                bitmap.SaveFile(directory + wxString::Format("/page-%zu.bmp", i),
                                wxBITMAP_TYPE_BMP);
            }
            std::ofstream result(std::filesystem::path(directory.ToStdWstring()) /
                                 "smoke-result.txt");
            result << "PASS: native widgets, controller Start/Stop, duplex, personality/DFA, "
                      "monitor closure\n";
            result.flush();
            if (!result)
                throw std::runtime_error("Could not write GUI smoke result evidence");
        }
        return true;
    } catch (const std::exception& e) {
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream out(std::filesystem::path(directory.ToStdWstring()) / "smoke-result.txt");
            out << "FAIL: " << e.what();
        }
        return false;
    }
}

} // namespace audiohole::gui
