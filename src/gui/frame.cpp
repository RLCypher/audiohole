#include "gui/frame.h"
#include "gui/controls.h"

namespace audiohole::gui {
Frame::Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke)
    : wxFrame(nullptr, wxID_ANY, wxS("AudioHole — Simulated routing"), wxDefaultPosition,
              wxSize(1240, 900)),
      controller_(std::move(controller)), timer_(this), smoke_(smoke) {
    SetMinSize(wxSize(980, 740));
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
    notebook_ = new wxNotebook(this, wxID_ANY);
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
    notebook_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
        if (e.GetSelection() != 0)
            end_observation();
        e.Skip();
    });
    select_profile("profile-simplex");
    populate_devices();
    refresh();
    present(presentation::Message::Ready);
    timer_.Start(100);
}
void Frame::present(presentation::Message event) {
    const auto c = messages_.show(event, personality_, ++occurrence_);
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
        wxMessageBox(text(result.detail), "AudioHole — action unavailable", wxOK | wxICON_WARNING,
                     this);
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
    beep_status_->SetLabel(
        s.beep_destination.empty()
            ? "Test Beep inactive. NO SOUND. No audibility or end-to-end result is established."
            : text("SIMULATED Test Beep active: " + s.beep_destination +
                   ". NO SOUND. Cancel immediately if needed."));
    scan_progress_->SetValue(s.scan_percent);
    const auto query = log_search_->GetValue().Lower();
    std::string logs = "SIMULATED technical diagnostics — no hardware measurements\n";
    for (const auto& e : s.events) {
        if (severity_->GetSelection() == 1 && e.type != "error")
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
            if (e.type == "activated")
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
            if (e.type == "error") {
                technical_->SetLabel(text(e.detail));
                commentary_->SetLabel("");
            }
            if (!smoke_ && tray_->IsIconInstalled() && notifications_->GetSelection() != 2 &&
                (e.type == "error" ||
                 (notifications_->GetSelection() == 1 &&
                  (e.type == "reconnected" || e.type == "disconnected" || e.type == "stopped")))) {
                tray_->ShowBalloon("AudioHole — SIMULATION", text(e.detail),
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
        select_profile("profile-duplex");
        wxCommandEvent start(wxEVT_BUTTON, start_->GetId());
        start_->GetEventHandler()->ProcessEvent(start);
        controller_->advance(300);
        refresh();
        require(controller_->snapshot().state == audiohole::State::Running,
                "GUI Start did not reach Running");
        require(controller_->snapshot().paths.size() == 2, "Duplex directions missing");
        const auto before = controller_->snapshot().revision;
        wxCommandEvent dfa(wxEVT_BUTTON, dfa_->GetId());
        dfa_->GetEventHandler()->ProcessEvent(dfa);
        dfa_->GetEventHandler()->ProcessEvent(dfa);
        require(controller_->snapshot().revision == before, "DFA changed controller state");
        for (int i = 0; i < 3; ++i) {
            personality_choice_->SetSelection(i);
            wxCommandEvent change(wxEVT_CHOICE, personality_choice_->GetId());
            personality_choice_->GetEventHandler()->ProcessEvent(change);
            require(dfa_->IsShown() == (i != 1), "DFA visibility mismatch");
        }
        personality_choice_->SetSelection(0);
        personality_ = presentation::Mode::Normal;
        dfa_->Show();
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
