#include "gui/frame.h"
#include "gui/controls.h"
#include <wx/stdpaths.h>

namespace audiohole::gui {
Frame::Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke)
    : wxFrame(nullptr, wxID_ANY, wxS("AudioHole — Simulated routing"), wxDefaultPosition,
              wxSize(1240, 900)),
      controller_(std::move(controller)), timer_(this), smoke_(smoke) {
    SetTitle(GetTitle() + wxS(" — build ") + text(AUDIOHOLE_BUILD_COMMIT));
    SetMinSize(wxSize(980, 740));
    personalities_directory_ =
        std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
            .parent_path() /
        "personalities";
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* banner = label(this, root,
                         "SIMULATION ONLY — authored devices and capabilities; NO AUDIO "
                         "capture or playback. Edits last for this session only.");
    banner->SetBackgroundColour(wxColour(255, 237, 177));
    status_ = label(this, root, "Stopped");
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    start_ = button(this, actions, "Start / Activate viewed profile", [this] { activate(false); });
    button(this, actions, "Arm Ready explicitly", [this] { activate(true); });
    stop_ = button(this, actions, "Stop Routing", [this] {
        controller_->stop();
        refresh();
    });
    button(this, actions, "Exit AudioHole", [this] { exit(); });
    root->Add(actions, 0, wxEXPAND);
    technical_ = label(this, root, "");
    commentary_ = label(this, root, "");
    fallback_notice_ = label(this, root, "");
    auto* navigation_row = new wxBoxSizer(wxHORIZONTAL);
    for (std::size_t index = 0; index < 4; ++index) {
        auto* tab = new wxToggleButton(this, wxID_ANY, "");
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
    std::string status = std::string("SIMULATED state: ") + audiohole::name(s.state) +
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
        status += " | Simulated scan: " + std::to_string(s.scan_percent) + "%";
    status_->SetLabel(text(status));
    std::string paths;
    for (const auto& p : s.paths)
        paths += p.hole_id + (p.reverse ? " reverse: " : " forward: ") + audiohole::name(p.state) +
                 " — " + p.detail + "\n";
    path_status_->SetLabel(text(paths.empty() ? "No active simulated directions." : paths));
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
        s.monitoring
            ? (running ? "SIMULATED source activity detected. Destination audibility is untested."
                       : "No simulated source activity. Silence does not establish disconnection.")
            : "Observation off. No real samples are inspected or stored.");
    // MSVC requires both branches to be wxString: its pointer conversions make
    // a mixed literal/wxString conditional ambiguous.
    beep_status_->SetLabel(s.beep_destination.empty()
                               ? wxString(wxS("Test Beep inactive. NO SOUND. No audibility or "
                                              "end-to-end result is established."))
                               : text("SIMULATED Test Beep active: " + s.beep_destination +
                                      ". NO SOUND. Cancel immediately if needed."));
    scan_progress_->SetValue(s.scan_percent);
    const auto query = log_search_->GetValue().Lower();
    std::string logs = "SIMULATED technical diagnostics — no hardware measurements\n";
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
                tray_->ShowBalloon(wxS("AudioHole — SIMULATION"), text(e.detail),
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
        for (int value : {-1100, 69, 420, 666, 670, 690, 1100}) {
            settle_dfa(value);
            require(dfa_slider_->GetValue() == value, "DFA slider/value mismatch");
        }
        require(controller_->snapshot().revision == before, "DFA slider changed controller state");
        require(dfa_->GetLabel() == "DFA", "DFA label changed");
        require(technical_->GetLabel() == "DFA Acknowledged", "DFA heading changed");
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
