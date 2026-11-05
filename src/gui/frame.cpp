#include "gui/frame.h"
#include "gui/controls.h"
#include "model/application_controller.h"
#include "model/configuration_controller.h"
#include "presentation/text_layout.h"
#include <cstdio>
#include <wx/display.h>
#include <wx/stdpaths.h>

namespace audiohole::gui {
Frame::Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke,
             std::shared_ptr<BuildPreferences> preferences, std::function<void()> restart)
    : wxFrame(nullptr, wxID_ANY, wxS("AudioHole"), wxDefaultPosition, wxSize(1000, 1000)),
      controller_(std::make_unique<ApplicationController>(std::move(controller))),
      preferences_(std::move(preferences)), restart_(std::move(restart)), timer_(this),
      avatar_timer_(this), smoke_(smoke) {
    if (!preferences_) {
        preferences_ = std::make_shared<BuildPreferences>(std::filesystem::path{}, true);
        preferences_->load();
    }
    personality_ = preferences_->values().personality_id;
    SetTitle(GetTitle() + wxS(" — build ") + text(AUDIOHOLE_BUILD_COMMIT));
    // A stable window property supports second-launch focus across build titles.
    // It is removed before the native window is destroyed (see the destructor).
    SetPropW(static_cast<HWND>(GetHandle()), L"AudioHole.GuiWindow", reinterpret_cast<HANDLE>(1));
    SetMinSize(FromDIP(wxSize(980, 680)));
    SetIcons(application_icons());
    personalities_directory_ =
        std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
            .parent_path() /
        "personalities";
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* header = new wxBoxSizer(wxHORIZONTAL);
    auto* information = new wxBoxSizer(wxVERTICAL);
    information->SetMinSize(FromDIP(wxSize(300, -1)));
    header->Add(information, 0, wxEXPAND | wxALL, FromDIP(10));
    auto* brand_row = new wxBoxSizer(wxHORIZONTAL);
    auto* brand_logo = new AgentDisplay(this, wxSize(64, 64));
    brand_row->Add(brand_logo, 0, wxALIGN_CENTER_VERTICAL);
    information->Add(brand_row, 0, wxEXPAND);
    auto* heading = label(this, brand_row, "AudioHole");
    heading->SetName("brand-wordmark");
    heading->SetFont(wxFont(wxFontInfo(20).Bold()));
    label(this, information, "Put your audio wherever you want.");
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
    status_row->Add(status_dot_, 0, wxALIGN_TOP | wxLEFT, FromDIP(4));
    // Neither label length nor wrapping may change the branding-column width.
    // Six reserved lines cover routing, profiles, startup, advisory and scan.
    status_ = new wxStaticText(this, wxID_ANY, "Routing: Stopped", wxDefaultPosition,
                               FromDIP(wxSize(280, 120)), wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    status_->SetMinSize(FromDIP(wxSize(280, 120)));
    status_row->Add(status_, 0, wxALL, FromDIP(4));
    information->Add(status_row, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
    auto* middle = new wxBoxSizer(wxVERTICAL);
    // This column follows the buttons' natural width, not the remaining window
    // width. The avatar follows this column, leaving spare space at the right.
    header->Add(middle, 0, wxEXPAND | wxALL, FromDIP(8));
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    routing_actions_ = actions;
    start_ = button(this, actions, "Start Routing", [this] { activate(false); });
    arm_ready_ = button(this, actions, "Arm Ready", [this] { activate(true); });
    stop_ = button(this, actions, "Stop Routing", [this] {
        controller_->stop();
        refresh();
    });
    button(this, actions, "Exit AudioHole", [this] { exit(); });
    // Align the card with the actual first/last button edges. Keep spacing only
    // between buttons, with DPI-scaled borders rather than outer side margins.
    for (auto* item : actions->GetChildren())
        item->SetBorder(FromDIP(3));
    actions->GetChildren().front()->SetFlag(wxTOP | wxBOTTOM | wxRIGHT);
    actions->GetChildren().back()->SetFlag(wxTOP | wxBOTTOM | wxLEFT);
    middle->Add(actions, 0, wxEXPAND);
    auto* comment_surface = new SurfacePanel(this, "commentary-card");
    comment_surface_ = comment_surface;
    comment_surface->SetMinSize(wxSize(actions->GetMinSize().x, FromDIP(156)));
    middle->Add(comment_surface, 0, wxEXPAND | wxTOP, FromDIP(8));
    auto* comment_layout = new wxBoxSizer(wxVERTICAL);
    personality_heading_ = new wxStaticText(comment_surface, wxID_ANY, "Norm", wxDefaultPosition,
                                            wxDefaultSize, wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    comment_layout->Add(personality_heading_, 0, wxEXPAND | wxALL, FromDIP(4));
    personality_heading_->SetMinSize(wxSize(0, -1));
    personality_heading_->SetFont(GetFont().Bold());
    commentary_panel_ = new wxScrolledWindow(comment_surface);
    commentary_panel_->SetScrollRate(0, FromDIP(12));
    commentary_panel_->SetMinSize(FromDIP(wxSize(220, 64)));
    comment_layout->Add(commentary_panel_, 1, wxEXPAND | wxALL, FromDIP(10));
    auto* messages = new wxBoxSizer(wxVERTICAL);
    commentary_panel_->SetSizer(messages);
    comment_surface->set_content_sizer(comment_layout);
    technical_ = label(commentary_panel_, messages, "");
    technical_->SetFont(GetFont().Bold());
    commentary_ = label(commentary_panel_, messages, "");
    fallback_notice_ = label(commentary_panel_, messages, "");
    // Existing commentary remains to the left of the replaceable agent image;
    // animation uses the shared avatar engine; branding stays independent.
    agent_display_ = new avatar::Panel(this);
    agent_display_->checkerboard(false);
    avatar::initialize_image_decoder();
    avatar_host_ = std::make_unique<presentation::AvatarHost>(avatar::native_decoder());
    agent_display_->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { bother(); });
    header->Add(agent_display_, 0, wxEXPAND | wxALL, FromDIP(6));
    root->Add(header, 0, wxEXPAND);
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
    CreateStatusBar();
    // The native Windows status bar ignores SetForegroundColour for its text.
    // Paint only its dark presentation; native fields, updates, tooltips and
    // resize hit testing stay in place. Light/high contrast use native painting.
    GetStatusBar()->Bind(wxEVT_PAINT, [this](wxPaintEvent& event) {
        if (!dark_appearance_) {
            event.Skip();
            return;
        }
        auto* bar = GetStatusBar();
        wxPaintDC dc(bar);
        dc.SetBackground(wxBrush(bar->GetBackgroundColour()));
        dc.Clear();
        dc.SetFont(bar->GetFont());
        dc.SetTextForeground(*wxWHITE);
        for (int field = 0; field < bar->GetFieldsCount(); ++field) {
            wxRect bounds;
            if (!bar->GetFieldRect(field, bounds))
                continue;
            bounds.Deflate(bar->FromDIP(3), 0);
            wxDCClipper clip(dc, bounds);
            const auto value =
                wxControl::Ellipsize(bar->GetStatusText(field), dc, wxELLIPSIZE_END, bounds.width);
            dc.DrawText(value, bounds.x, bounds.y + (bounds.height - dc.GetCharHeight()) / 2);
        }
    });
    SetSizer(root);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        // Coalesce native resize notifications; wxFrame first settles its client
        // and status-bar bounds. Never recursively resize while processing WM_SIZE.
        if (!header_layout_pending_) {
            header_layout_pending_ = true;
            CallAfter([this] {
                header_layout_pending_ = false;
                if (!IsBeingDeleted()) {
                    layout_header();
                    // Invalidate the old locations as well as newly exposed
                    // strips after native child moves; let Windows batch paint.
                    Refresh();
                }
            });
        }
        event.Skip();
    });
    tray_ = std::make_unique<Tray>(*this);
    initialize_shell_controls();
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            controller_->advance(100);
            if (device_change_pending_ && !controller_->snapshot().scanner.busy &&
                std::chrono::steady_clock::now() >= device_change_after_) {
                device_change_pending_ = false;
                ScannerActionRequest request;
                request.kind = ScannerActionKind::RefreshAvailability;
                (void)controller_->scanner_action(request);
            }
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
    select_profile(controller_->snapshot().profiles.empty()
                       ? ""
                       : controller_->snapshot().profiles.front().id);
    populate_devices();
    refresh();
    if (!operational_warning_visible_)
        present(presentation::Message::Ready);
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (event.ControlDown() && !event.AltDown() && event.GetKeyCode() == 'S' &&
            notebook_->GetSelection() == 0 && profile_save_->IsEnabled())
            save(false);
        else if (event.ControlDown() && event.GetKeyCode() == WXK_TAB)
            select_page((static_cast<std::size_t>(notebook_->GetSelection()) +
                         (event.ShiftDown() ? 3 : 1)) %
                            4,
                        true);
        else
            event.Skip();
    });
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            if (IsShown() && !IsIconized())
                poll_avatar();
        },
        avatar_timer_.GetId());
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        if (event.IsShown() && !IsIconized()) {
            avatar_host_->activity(avatar::monotonic_ms());
            poll_avatar();
            avatar_timer_.Start(16);
        } else
            avatar_timer_.Stop();
        event.Skip();
    });
    Bind(wxEVT_ICONIZE, [this](wxIconizeEvent& event) {
        if (!event.IsIconized() && IsShown()) {
            avatar_host_->activity(avatar::monotonic_ms());
            poll_avatar();
            avatar_timer_.Start(16);
        } else
            avatar_timer_.Stop();
        event.Skip();
    });
    // Prefer a square initial window when the fixed header and monitor permit
    // it. Short displays retain the required width and use the available height.
    const int display_index = wxDisplay::GetFromWindow(this);
    const auto work =
        wxDisplay(display_index == wxNOT_FOUND ? 0u : static_cast<unsigned>(display_index))
            .GetClientArea();
    const auto minimum = GetMinSize();
    const int preferred_edge = std::max({FromDIP(1000), minimum.x, minimum.y});
    const int edge = std::max(
        1, std::min({preferred_edge, work.width - FromDIP(20), work.height - FromDIP(20)}));
    SetSize(wxSize(std::max(minimum.x, edge), std::max(minimum.y, edge)));
    Centre();
    timer_.Start(100);
    wxEvtHandler::AddFilter(this);
}
int Frame::FilterEvent(wxEvent& event) {
    const auto type = event.GetEventType();
    if (type == wxEVT_MENU) {
        // Native popup-menu commands are not child-window mouse events.
        avatar_host_->activity(avatar::monotonic_ms());
        return wxEventFilter::Event_Skip;
    }
    if (type == wxEVT_KEY_DOWN || type == wxEVT_CHAR_HOOK || type == wxEVT_MOTION ||
        type == wxEVT_MOUSEWHEEL || type == wxEVT_LEFT_DOWN || type == wxEVT_RIGHT_DOWN ||
        type == wxEVT_MIDDLE_DOWN) {
        // Child mouse events do not bubble to Frame. Observe this window tree's
        // input without consuming it or counting paint/timer events as activity.
        auto* window = dynamic_cast<wxWindow*>(event.GetEventObject());
        if (window && (window == this || IsDescendant(window)))
            avatar_host_->activity(avatar::monotonic_ms());
    }
    return wxEventFilter::Event_Skip;
}
void Frame::stop_avatar() {
    avatar_timer_.Stop();
    if (avatar_host_)
        avatar_host_->shutdown();
    if (agent_display_)
        agent_display_->present({});
}
void Frame::poll_avatar() {
    if (!avatar_host_)
        return;
    const auto now = avatar::monotonic_ms();
    auto frame = avatar_host_->tick(now);
    if (frame.diagnostics != last_avatar_diagnostics_) {
        last_avatar_diagnostics_ = frame.diagnostics;
        for (const auto& line : frame.diagnostics) {
            avatar_diagnostics_.push_back("[avatar] " + line);
            if (avatar_diagnostics_.size() > 32)
                avatar_diagnostics_.erase(avatar_diagnostics_.begin());
        }
    }
    agent_display_->present(std::move(frame));
    const auto* package = messages_.package(personality_);
    const bool has_dreams = package && package->events.contains("dream");
    const auto change = sleep_commentary_.update(
        avatar_host_->enabled() && avatar_host_->baseline().expression == "sleeping" &&
            !operational_warning_visible_,
        personality_, has_dreams, now);
    using SleepChange = presentation::SleepCommentary::Change;
    if (change != SleepChange::None) {
        if (change == SleepChange::Awake)
            sleep_copy_.clear();
        else if (change == SleepChange::Dream) {
            // Select authored sleep text without presenting an event/reaction:
            // doing that would wake the avatar and overwrite its awake response.
            sleep_copy_ =
                messages_.show(presentation::Message::Dream, personality_, ++dream_occurrence_)
                    .commentary;
            if (sleep_copy_.empty())
                sleep_copy_ = "zzz....";
        } else
            sleep_copy_ = "zzz....";
        layout_header();
    }
}
void Frame::bother() {
    if (operational_warning_visible_ || personality_ == "vanilla" ||
        messages_.variants(presentation::Message::Bother, personality_).empty())
        return;
    const auto now = std::chrono::steady_clock::now();
    if (now < bother_after_)
        return;
    bother_after_ = now + std::chrono::milliseconds(350);
    present(presentation::Message::Bother);
}
void Frame::present(presentation::Message event) {
    operational_warning_visible_ = event == presentation::Message::ScanFailed ||
                                   event == presentation::Message::Disconnected ||
                                   event == presentation::Message::Recovering ||
                                   event == presentation::Message::Questionable ||
                                   event == presentation::Message::Invalid;
    last_message_ = event;
    const auto c = messages_.show(event, personality_, ++occurrence_);
    avatar_host_->observe(controller_->snapshot(), operational_warning_visible_,
                          avatar::monotonic_ms());
    avatar_host_->react(event, c, occurrence_, avatar::monotonic_ms());
    fallback_copy_ =
        personality_fallback_note_.empty() ? c.fallback_detail : personality_fallback_note_;
    technical_copy_ = event == presentation::Message::Bother ? std::string{} : c.technical;
    commentary_copy_ = c.commentary;
    layout_header();
}
void Frame::layout_header() {
    // Minimized native bounds are not a layout target; restore triggers layout.
    if (IsIconized())
        return;
    // Re-read native button metrics after DPI/font changes. Copy length never
    // participates in the commentary card's horizontal sizing.
    comment_surface_->SetMinSize(wxSize(routing_actions_->GetMinSize().x, FromDIP(156)));
    // Derive height solely from the branding and operations columns, excluding
    // the avatar to avoid a feedback loop. Never shrink it at compact widths.
    auto* header = agent_display_->GetContainingSizer();
    int header_height = 0;
    for (auto* item : header->GetChildren()) {
        if (item->GetWindow() == agent_display_)
            continue;
        item->CalcMin();
        header_height = std::max(header_height, item->GetMinSizeWithBorder().y);
    }
    agent_display_->fit_height(header_height - 2 * FromDIP(6));
    // Full-height artwork needs its real proportional width even at minimum
    // size. Increase the minimum instead of clipping or reducing the image.
    auto minimum = FromDIP(wxSize(980, 680));
    minimum.x = std::max(minimum.x, header->GetMinSize().x + GetSize().x - GetClientSize().x);
    if (GetMinSize() != minimum)
        SetMinSize(minimum);
    if (GetSize().x < minimum.x)
        SetSize(wxSize(minimum.x, GetSize().y));
    Layout(); // Allocate fixed branding/avatar regions before measuring text space.
    const int width = std::max(120, commentary_panel_->GetClientSize().x - FromDIP(24));
    auto wrap = [width](wxStaticText* control, const std::string& copy) {
        control->Show(!copy.empty());
        control->SetLabel(text(presentation::keep_kaomoji_together(copy)));
        control->Wrap(width);
    };
    status_->SetLabel(text(status_copy_));
    status_->SetToolTip(text(status_copy_));
    const bool asleep = !sleep_copy_.empty() && !operational_warning_visible_ &&
                        avatar_host_->enabled() &&
                        avatar_host_->baseline().expression == "sleeping";
    wrap(technical_, asleep ? std::string{} : technical_copy_);
    wrap(commentary_, asleep ? sleep_copy_ : commentary_copy_);
    wrap(fallback_notice_, asleep ? std::string{} : fallback_copy_);
    const auto* selected = messages_.package(personality_);
    personality_heading_->SetLabel(
        text(selected && !selected->character_name.empty() ? selected->character_name
             : selected && selected->id != "vanilla"       ? selected->display_name
                                                           : "Status"));
    commentary_panel_->Layout();
    commentary_panel_->FitInside();
    Layout();
}
void Frame::show_result(const audiohole::Result& result) {
    operational_warning_visible_ = !result.ok;
    avatar_host_->observe(controller_->snapshot(), operational_warning_visible_,
                          avatar::monotonic_ms());
    technical_copy_ = result.detail;
    layout_header();
    if (!result.ok && !smoke_)
        wxMessageBox(text(result.detail), wxS("AudioHole — action unavailable"),
                     wxOK | wxICON_WARNING, this);
}
bool Frame::discard() {
    if (!dirty_)
        return true;
    return wxMessageBox("Discard unsaved editor changes? Saved profiles and the active "
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
    avatar_host_->observe(s, operational_warning_visible_, avatar::monotonic_ms());
    const bool preview = s.operating_mode == OperatingMode::Preview;
    start_->Enable(s.capabilities.routing);
    arm_ready_->Enable(s.capabilities.routing);
    stop_->Enable(s.capabilities.routing);
    profile_apply_->Enable(s.capabilities.routing && !viewed_.empty());
    profile_save_->Enable(!viewed_.empty());
    profile_revert_->Enable(!viewed_.empty() && dirty_);
    profile_notes_->Enable(!viewed_.empty());
    hole_new_->Enable(!viewed_.empty());
    const auto edit_state =
        text(viewed_.empty()
                 ? std::string("No profile selected")
                 : draft_.display_name + (dirty_ ? " — Unsaved changes" : " — No pending edits"));
    if (profile_edit_state_->GetLabel() != edit_state) {
        profile_edit_state_->SetLabel(edit_state);
        profile_edit_state_->SetToolTip(edit_state);
    }
    if (s.scanner.inventory_revision != device_inventory_revision_) {
        device_inventory_revision_ = s.scanner.inventory_revision;
        capture_draft();
        populate_devices();
        if (hole_index_ >= 0) {
            const auto& h = draft_.holes.at(static_cast<std::size_t>(hole_index_));
            populate_direction(forward_, h.forward);
            populate_direction(reverse_, h.reverse);
        }
        update_beep_destinations();
    }
    const bool scanner_idle = !s.scanner.busy && !s.scanning;
    scan_start_->Enable(s.capabilities.scanning && scanner_idle);
    const bool selected_device = endpoint(s, device_id_) != nullptr;
    device_update_->Enable(s.capabilities.scanning && scanner_idle && selected_device);
    device_deep_->Enable(s.capabilities.scanning && scanner_idle && selected_device);
    for (auto* action : scanner_actions_)
        action->Enable(s.scanner.available && scanner_idle);
    scan_cancel_->Enable(s.capabilities.scanning && (s.scanning || s.scanner.busy));
    scan_mode_->Enable(s.capabilities.scanning && scanner_idle);
    scan_scope_->Enable(s.capabilities.scanning && scanner_idle);
    monitor_->Enable(s.capabilities.monitoring);
    activity_direction_->Enable(s.capabilities.monitoring && hole_index_ >= 0 &&
                                draft_.holes.at(static_cast<std::size_t>(hole_index_)).duplex);
    if (hole_index_ >= 0) {
        Direction forward, reverse;
        read_direction(forward_, forward);
        read_direction(reverse_, reverse);
        update_direction_details(forward_, forward);
        update_direction_details(reverse_, reverse);
    }
    const auto* active = audiohole::profile(s, s.active_profile);
    std::string status =
        (preview ? std::string("Preview Mode | Routing: ") + audiohole::name(s.state)
                 : std::string("Test Mode | Engine: unavailable")) +
        "\nActive: " + (active ? active->display_name : "none") +
        "\nViewed: " + draft_.display_name + (dirty_ ? " (unsaved edits)" : "");
    if (!s.pending_profile.empty())
        status += "\nStartup pending: " + std::to_string((s.pending_ms + 999) / 1000) + " s";
    std::vector<std::string> enabled_holes;
    for (const auto& path : s.paths)
        if (std::find(enabled_holes.begin(), enabled_holes.end(), path.hole_id) ==
            enabled_holes.end())
            enabled_holes.push_back(path.hole_id);
    if (enabled_holes.size() > 4)
        status += "\nExtended-range advisory: 5–8 enabled holes; hardware performance unverified.";
    if (s.scanning)
        status += "\nScan: " + std::to_string(s.scan_percent) + "%";
    if (status_copy_ != status) {
        status_copy_ = status;
        layout_header();
    }
    const auto errors = std::count_if(s.events.begin(), s.events.end(), [](const auto& event) {
        return event.type == "error" || event.type == "scan-failed";
    });
    const auto holes = draft_.holes.size();
    SetStatusText(text(std::string(preview ? "AudioHole (Preview Mode) | Engine: "
                                           : "AudioHole (Test Mode) | Engine: ") +
                       (preview ? name(s.state) : "unavailable") +
                       " | Devices: " + std::to_string(s.endpoints.size()) +
                       " | Profiles: " + std::to_string(s.profiles.size()) +
                       " | Viewed holes: " + std::to_string(holes) +
                       " | Warnings: unavailable | Log errors: " + std::to_string(errors)));
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
    beep_button_->SetLabel(s.beep_destination.empty() ? "Test Beep" : "Cancel Beep");
    const int beep_selection = destination_test_->GetSelection();
    const auto* beep_target =
        beep_selection >= 0 && static_cast<std::size_t>(beep_selection) < beep_ids_.size()
            ? endpoint(s, beep_ids_[static_cast<std::size_t>(beep_selection)])
            : nullptr;
    const bool beep_available = beep_target && !beep_target->capture &&
                                beep_target->availability == Availability::Available;
    beep_button_->Enable(s.capabilities.beep && (!s.beep_destination.empty() || beep_available));
    beep_button_->SetToolTip(
        !preview ? "Test Beep playback is unavailable in Test Mode because no audio backend is "
                   "installed."
        : s.beep_destination.empty()
            ? "Start a bounded test for the explicitly selected playback destination. This "
              "preview simulates the test interval and emits no sound."
            : "Cancel the active simulated destination test immediately. No second test starts.");
    if (s.scanner.busy && s.scan_percent < 0)
        scan_progress_->Pulse();
    else
        scan_progress_->SetValue(std::clamp(s.scan_percent, 0, 100));
    wxString scan_status;
    if (s.scanner.available) {
        scan_status = text(s.scanner.phase + " | " + s.scanner.result);
        if (!s.scanner.busy)
            scan_status += "\n" + text(s.scanner.detail).Left(240);
    } else
        scan_status = s.scanning ? "Preview scan in progress" : "No scan in progress";
    if (scan_state_->GetLabel() != scan_status) {
        scan_state_->SetLabel(scan_status);
        scan_state_->Wrap(900);
        scan_state_->GetParent()->Layout();
    }
    const auto query = log_search_->GetValue().Lower();
    std::string logs;
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
    if (severity_->GetSelection() == 0)
        for (const auto& line : avatar_diagnostics_)
            if (query.empty() || text(line).Lower().Contains(query))
                logs += line + "\n";
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
            if (e.type == "error" || e.type == "scan-failed" || e.severity == Severity::Error ||
                e.severity == Severity::Critical) {
                operational_warning_visible_ = true;
                avatar_host_->observe(s, true, avatar::monotonic_ms());
                technical_copy_ = e.detail;
                commentary_copy_.clear();
                layout_header();
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
        {
            auto menu = shell_menu();
            const int close = menu->FindItem("Close window"),
                      exit = menu->FindItem("Exit AudioHole");
            const int start = menu->FindItem("Start Routing"),
                      stop = menu->FindItem("Stop Routing");
            require(close != wxNOT_FOUND && exit != wxNOT_FOUND,
                    "Shared shell menu lacks separate Close and Exit actions");
            require(start != wxNOT_FOUND && stop != wxNOT_FOUND &&
                        menu->IsEnabled(start) == controller_->snapshot().capabilities.routing &&
                        menu->IsEnabled(stop) == controller_->snapshot().capabilities.routing,
                    "Taskbar/tray routing availability differs from the backend");
            require(menu->FindItem("Activate profile") != wxNOT_FOUND &&
                        menu->FindItem("Direction states (read-only)") != wxNOT_FOUND,
                    "Shared shell menu lost profile or direction controls");
            const auto system = GetSystemMenu(static_cast<HWND>(GetHandle()), FALSE);
            require(system &&
                        GetMenuState(system, shell_controls_command, MF_BYCOMMAND) !=
                            static_cast<UINT>(-1) &&
                        GetMenuState(system, shell_exit_command, MF_BYCOMMAND) !=
                            static_cast<UINT>(-1),
                    "Native window menu lacks AudioHole controls or Exit");
            const auto revision = controller_->snapshot().revision;
            std::uint32_t invalid = 999;
            COPYDATASTRUCT packet{shell_message_tag, sizeof(invalid), &invalid};
            require(SendMessageW(static_cast<HWND>(GetHandle()), WM_COPYDATA, 0,
                                 reinterpret_cast<LPARAM>(&packet)) == FALSE,
                    "Shell relay accepted an unknown action");
            packet.cbData = 0;
            require(SendMessageW(static_cast<HWND>(GetHandle()), WM_COPYDATA, 0,
                                 reinterpret_cast<LPARAM>(&packet)) == FALSE &&
                        controller_->snapshot().revision == revision,
                    "Malformed shell request changed the controller");
        }
        const auto check_profile_layout = [this, &require] {
            auto* page = notebook_->GetPage(0);
            page->Layout();
            require(dynamic_cast<wxScrolledWindow*>(profile_sidebar_) == nullptr,
                    "Profile column must not scroll as one section");
            require(page->GetClientRect().Contains(profile_actions_->GetRect()) &&
                        profile_actions_->GetRect().GetBottom() <
                            profile_sidebar_->GetRect().GetTop(),
                    "Profile save bar is clipped or overlaps the editor fields");
            for (auto* action : {profile_save_, profile_apply_, profile_revert_})
                require(action->IsShownOnScreen() &&
                            profile_actions_->GetClientRect().Contains(action->GetRect()),
                        "A profile save action is hidden or clipped");
            for (wxWindow* field : std::vector<wxWindow*>{profiles_, profile_notes_, holes_})
                require(field->GetParent() == profile_sidebar_ &&
                            profile_sidebar_->GetClientRect().Contains(field->GetRect()),
                        "An independently scrolling profile field is clipped");
        };
        layout_header();
        check_profile_layout();
        require(profile_new_->GetLabel() == "+" && hole_new_->GetLabel() == "+" &&
                    profile_new_->GetName() == "New profile" && hole_new_->GetName() == "Add hole",
                "Compact add controls lost their labels or accessible names");
        require(wxWindow::FindWindowByName("Profile display name", this) == nullptr &&
                    wxWindow::FindWindowByName("Hole display name", this) == nullptr,
                "Inline name fields remain instead of contextual rename");
        // Fill the actual native widgets well beyond their viewport. Their own
        // scrollbars must appear without moving or hiding the shared save bar.
        const auto original_profiles = profiles_->GetStrings();
        const auto original_holes = holes_->GetStrings();
        const int selected_profile = profiles_->GetSelection(),
                  selected_hole = holes_->GetSelection();
        const auto original_notes = profile_notes_->GetValue();
        const auto action_bounds = profile_actions_->GetRect();
        const auto overflow_revision = controller_->snapshot().revision;
        wxString many_notes;
        for (int i = 0; i < 200; ++i) {
            profiles_->Append(wxString::Format("Overflow profile %d", i));
            holes_->Append(wxString::Format("Overflow hole %d", i));
            many_notes += "A long profile note with its own scroll position.\n";
        }
        profile_notes_->ChangeValue(many_notes);
        check_profile_layout();
        for (wxWindow* field : std::vector<wxWindow*>{profiles_, profile_notes_, holes_})
            require((GetWindowLongPtrW(static_cast<HWND>(field->GetHandle()), GWL_STYLE) &
                     WS_VSCROLL) != 0,
                    "An overflowing profile field lacks its own native scrollbar");
        require(profile_actions_->GetRect() == action_bounds &&
                    controller_->snapshot().revision == overflow_revision,
                "Overflow moved Save or changed controller state");
        profiles_->Clear();
        profiles_->Append(original_profiles);
        profiles_->SetSelection(selected_profile);
        holes_->Clear();
        holes_->Append(original_holes);
        holes_->SetSelection(selected_hole);
        profile_notes_->ChangeValue(original_notes);
        if (controller_->snapshot().operating_mode == OperatingMode::Test) {
            require(controller_->snapshot().profiles.empty() &&
                        controller_->snapshot().endpoints.empty(),
                    "Test Mode leaked preview fixtures");
            require(!start_->IsEnabled() && !arm_ready_->IsEnabled() &&
                        controller_->snapshot().capabilities.scanning && !beep_button_->IsEnabled(),
                    "Test Mode enabled unsupported hardware controls");
            require(wxWindow::FindWindowByLabel("Forward failure", this) == nullptr &&
                        wxWindow::FindWindowByLabel("Reverse failure", this) == nullptr &&
                        wxWindow::FindWindowByLabel("Simulate Errors", this) == nullptr,
                    "Test Mode contains simulation controls");
            auto* create = profile_new_;
            require(create != nullptr, "New profile control missing");
            wxCommandEvent new_profile(wxEVT_BUTTON, create->GetId());
            create->GetEventHandler()->ProcessEvent(new_profile);
            require(!viewed_.empty(), "Test Mode could not create persistent profile");
            rename_profile("Restart persistence check");
            profile_notes_->ChangeValue("Native GUI Test data, saved without a routing backend.");
            require(profile_edit_state_->GetLabel().Contains("Unsaved changes"),
                    "Staged profile rename lacks a visible unsaved indicator");
            wxKeyEvent save_key(wxEVT_CHAR_HOOK);
            save_key.m_keyCode = 'S';
            save_key.SetControlDown(true);
            GetEventHandler()->ProcessEvent(save_key);
            require(!dirty_ && profile_edit_state_->GetLabel().Contains("No pending edits"),
                    "Ctrl+S failed to save the profile and clear the pending-edit indicator");
            draft_.holes.push_back(controller_->make_hole());
            load_holes();
            load_hole(0);
            const auto persistent_hole_id = draft_.holes.front().id;
            rename_hole("Persistent named hole");
            save(false);
            profile_notes_->ChangeValue("Other draft edits must remain unsaved");
            forward_.buffer->SetValue(14);
            acknowledge_buffer(forward_);
            require(forward_.buffer_apply->IsEnabled(),
                    "Test buffer preference Apply stayed disabled");
            wxCommandEvent buffer_apply(wxEVT_BUTTON, forward_.buffer_apply->GetId());
            forward_.buffer_apply->GetEventHandler()->ProcessEvent(buffer_apply);
            require(!forward_.buffer_apply->IsEnabled() && dirty_,
                    "Buffer-only save lost other staged edits");
            const auto saved_id = viewed_;
            ConfigurationController restarted(
                std::make_unique<TestJsonPersistence>(preferences_->test_configuration()));
            const auto* loaded = audiohole::profile(restarted.snapshot(), saved_id);
            require(loaded && loaded->display_name == "Restart persistence check" &&
                        loaded->holes.front().id == persistent_hole_id &&
                        loaded->holes.front().display_name == "Persistent named hole" &&
                        loaded->holes.front().forward.settings.buffer_ms == 14 &&
                        loaded->notes == "Native GUI Test data, saved without a routing backend.",
                    "Test buffer-only save did not persist or saved unrelated draft edits");
            require(!profile_apply_->IsEnabled(),
                    "Test Mode enabled Apply after creating a profile");
            if (!directory.empty()) {
                wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
                wxBitmap bitmap(GetClientSize());
                wxMemoryDC destination(bitmap);
                wxClientDC source(this);
                destination.Blit(0, 0, GetClientSize().x, GetClientSize().y, &source, 0, 0);
                destination.SelectObject(wxNullBitmap);
                bitmap.SaveFile(directory + "/test-mode.bmp", wxBITMAP_TYPE_BMP);
                std::ofstream result(std::filesystem::path(directory.ToStdWstring()) /
                                     "smoke-result.txt");
                result << "PASS: Test Mode isolation, real GUI configuration save/reload, "
                          "unavailable backend controls\n";
                result.flush();
                require(static_cast<bool>(result), "Test Mode evidence write failed");
            }
            return true;
        }
        require(capture_devices_ && playback_devices_ && capture_devices_ != playback_devices_,
                "Devices must have separate capture and playback lists");
        const auto hidden_id = draft_.holes.front().forward.source;
        require(!hidden_id.empty(), "Preview source fixture is missing");
        const auto before_hide = draft_.holes.front().forward;
        ScannerActionRequest hide;
        hide.kind = ScannerActionKind::SetHidden;
        hide.logical_id = hidden_id;
        hide.hidden = true;
        scanner_action(hide);
        require(capture_devices_->FindString("----- Hidden devices -----") != wxNOT_FOUND,
                "Hidden capture device lacks a divider");
        require(std::find(forward_.sources.begin(), forward_.sources.end(), hidden_id) ==
                    forward_.sources.end(),
                "Hidden device remains a configurator choice");
        Direction retained;
        read_direction(forward_, retained);
        require(retained.source == before_hide.source &&
                    retained.destination == before_hide.destination,
                "Hiding a device discarded a saved routing reference");
        hide.hidden = false;
        scanner_action(hide);
        require(capture_devices_->FindString("----- Hidden devices -----") == wxNOT_FOUND,
                "Empty hidden section remained after unhide");
        const auto face = wxString::FromUTF8("(⁄\xC2\xA0⁄>⁄ω⁄<⁄\xC2\xA0⁄)");
        commentary_->SetLabel(text(presentation::keep_kaomoji_together("Prefix (⁄ ⁄>⁄ω⁄<⁄ ⁄)")));
        wxClientDC face_measure(commentary_);
        face_measure.SetFont(commentary_->GetFont());
        commentary_->Wrap(face_measure.GetTextExtent("Prefix (⁄ ").GetWidth());
        require(commentary_->GetLabel().Contains(face),
                "Kaomoji split across native wrapped lines");
        layout_header();
        const auto await_avatar = [this, &require] {
            // Smoke runs synchronously on the GUI thread: no timer event can
            // consume the worker's completed load until we explicitly poll it.
            const auto revision = controller_->snapshot().revision;
            const auto deadline = avatar::monotonic_ms() + 8000;
            while (avatar::monotonic_ms() < deadline) {
                poll_avatar();
                const auto& frame = agent_display_->presentation();
                if (frame.image && frame.personality == personality_) {
                    require(controller_->snapshot().revision == revision,
                            "Waiting for avatar loading advanced controller state");
                    return;
                }
                wxMilliSleep(5);
            }
            std::string detail = "Avatar failed to load the selected owned pack: " + personality_;
            for (const auto& diagnostic : agent_display_->presentation().diagnostics)
                detail += "\n" + diagnostic;
            throw std::runtime_error(detail);
        };
        await_avatar();
        const auto draft_before_menu = draft_;
        const auto routing_before_menu = controller_->snapshot();
        const auto menu_revision = controller_->snapshot().revision;
        auto profile_context = profile_menu(viewed_);
        require(profile_context->FindItem(wxID_EDIT) && profile_context->FindItem(wxID_DUPLICATE) &&
                    profile_context->FindItem(wxID_DELETE) && draft_ == draft_before_menu &&
                    controller_->snapshot().revision == menu_revision,
                "Opening profile context actions modified staged or controller state");
        const int hole_before_menu = hole_index_;
        const bool dirty_before_menu = dirty_;
        const auto original_hole =
            draft_before_menu.holes.at(static_cast<std::size_t>(hole_before_menu));
        add_hole(); // The context target is deliberately not the selected row.
        auto hole_context = hole_menu(original_hole.id);
        wxCommandEvent duplicate(wxEVT_MENU, wxID_DUPLICATE);
        hole_context->ProcessEvent(duplicate);
        require(draft_.holes.size() == draft_before_menu.holes.size() + 2,
                "Hole context menu did not duplicate its target");
        const auto duplicate_id = draft_.holes.back().id;
        require(duplicate_id != original_hole.id &&
                    draft_.holes.back().forward == original_hole.forward &&
                    draft_.holes.back().reverse == original_hole.reverse,
                "Duplicating a hole reused its identity or changed directional configuration");
        const auto& after_navigation = controller_->snapshot();
        require(after_navigation.state == routing_before_menu.state &&
                    after_navigation.armed == routing_before_menu.armed &&
                    after_navigation.active_profile == routing_before_menu.active_profile &&
                    after_navigation.pending_profile == routing_before_menu.pending_profile &&
                    after_navigation.profiles == routing_before_menu.profiles &&
                    after_navigation.paths.size() == routing_before_menu.paths.size(),
                "Contextual hole editing changed routing or saved profiles");
        // Adding/selecting a hole stops passive observation, which legitimately
        // advances the controller revision. Measure rename from its own start.
        const auto rename_revision = after_navigation.revision;
        rename_hole("Context-renamed hole");
        require(draft_.holes.back().id == duplicate_id,
                "Contextual hole rename changed its logical identity");
        require(hole_heading_->GetLabel() == "Context-renamed hole",
                "Contextual hole rename did not update the editor heading");
        require(controller_->snapshot().revision == rename_revision,
                "Contextual hole rename changed controller state");
        draft_ = draft_before_menu;
        dirty_ = dirty_before_menu;
        load_holes();
        load_hole(hole_before_menu);
        refresh();
        const auto check_button_help = [&](const auto& self, wxWindow* window) -> void {
            if (dynamic_cast<wxButton*>(window))
                require(window->GetToolTip() && !window->GetToolTip()->GetTip().empty(),
                        "An action or navigation button lacks explanatory hover help");
            for (auto* child : window->GetChildren())
                self(self, child);
        };
        check_button_help(check_button_help, this);
        for (auto* tab : navigation_)
            require(tab->GetToolTip() == nullptr, "Navigation must not have generic hover text");
        require(!forward_.buffer_apply->IsEnabled() && !reverse_.buffer_apply->IsEnabled(),
                "Unchanged buffer preferences must keep Apply disabled");
        require(console_->GetToolTip() == nullptr, "Diagnostic log must not have hover text");
        const auto target = std::find(beep_ids_.begin(), beep_ids_.end(), "vr-out");
        require(target != beep_ids_.end(), "Preview beep destination missing");
        destination_test_->SetSelection(static_cast<int>(target - beep_ids_.begin()));
        refresh();
        require(beep_button_->IsEnabled(), "Available beep target must enable the test button");
        const auto beep_width = beep_button_->GetSize().x;
        wxCommandEvent beep_click(wxEVT_BUTTON, beep_button_->GetId());
        beep_button_->GetEventHandler()->ProcessEvent(beep_click);
        require(controller_->snapshot().beep_destination == "vr-out" &&
                    beep_button_->GetLabel() == "Cancel Beep",
                "Beep label must follow the controller's active test state");
        require(beep_button_->GetSize().x == beep_width, "Beep label changes button width");
        beep_button_->GetEventHandler()->ProcessEvent(beep_click);
        require(controller_->snapshot().beep_destination.empty() &&
                    beep_button_->GetLabel() == "Test Beep",
                "Second click must cancel the current beep");
        beep_button_->GetEventHandler()->ProcessEvent(beep_click);
        controller_->advance(1000);
        refresh();
        require(controller_->snapshot().beep_destination.empty() &&
                    beep_button_->GetLabel() == "Test Beep" &&
                    beep_button_->GetSize().x == beep_width,
                "Natural beep completion must restore the fixed-width idle button");
        show_result({false, "Operational warning retained."});
        const auto warning_occurrence = occurrence_;
        bother();
        require(technical_copy_ == "Operational warning retained." &&
                    occurrence_ == warning_occurrence,
                "Bother must not replace an operational warning");
        select_personality("uwu");
        require(technical_copy_ == "Operational warning retained." && commentary_copy_.empty(),
                "Personality switch concealed a warning or retained another character's flavor");
        select_personality("default");
        present(presentation::Message::Ready);
        const auto bother_revision = controller_->snapshot().revision;
        const auto bother_dfa = dfa_hundredths_;
        bother_after_ = {};
        bother();
        require(wxWindow::FindWindowByLabel("Bother", this) == nullptr && technical_copy_.empty() &&
                    !technical_->IsShown() && last_message_ == presentation::Message::Bother &&
                    controller_->snapshot().revision == bother_revision &&
                    dfa_hundredths_ == bother_dfa,
                "Bother must remain independent of controller and DFA state");
        const auto bother_occurrence = occurrence_;
        bother_after_ = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        bother();
        require(occurrence_ == bother_occurrence, "Bother ignored its click cooldown");
        bother_after_ = {};
        require(GetPropW(static_cast<HWND>(GetHandle()), L"AudioHole.GuiWindow") != nullptr,
                "Stable second-launch window identity missing");
        require(personality_choice_->GetString(0) == wxS("Norm") &&
                    personality_choice_->GetString(2) == wxS("UwU"),
                "Canonical personality labels changed");
        require(theme_choice_->GetSelection() == 2, "Follow System is not the initial theme");
        // Exercise the real sizers: changing status text must never resize or
        // move the commentary card or reduce the full-height avatar.
        const auto initial_client = GetClientSize();
        const auto initial_status = status_copy_;
        const auto resize_occurrence = occurrence_;
        SetSize(GetMinSize());
        layout_header();
        check_profile_layout();
        wxSize previous_card_size = wxDefaultSize;
        wxSize previous_avatar_size = wxDefaultSize;
        for (const auto size :
             {wxSize(980, 680), wxSize(1000, 1000), wxSize(980, 740), wxSize(1250, 850),
              wxSize(1536, 1024), wxSize(1250, 850), wxSize(980, 740)}) {
            SetClientSize(FromDIP(size));
            status_copy_ = "Routing: Stopped\nActive: none\nViewed: short";
            layout_header();
            check_profile_layout();
            const auto card = comment_surface_->GetRect();
            status_copy_ = "Routing: Recovering\nActive: " + std::string(250, 'A') +
                           "\nViewed: " + std::string(250, 'V') + "\nScan: 95%";
            layout_header();
            require(comment_surface_->GetRect() == card,
                    "Status changes move or resize the personality card");
            require(card.GetHeight() == FromDIP(156), "Commentary card height is not bounded");
            const auto* exit_button = routing_actions_->GetChildren().back()->GetWindow();
            require(card.GetLeft() == start_->GetRect().GetLeft() &&
                        card.GetRight() == exit_button->GetRect().GetRight(),
                    "Commentary card does not match the routing button row edges");
            require(previous_card_size == wxDefaultSize || card.GetSize() == previous_card_size,
                    "Window resizing changes the commentary card size");
            previous_card_size = card.GetSize();
            const auto avatar = agent_display_->GetRect();
            require(previous_avatar_size == wxDefaultSize ||
                        avatar.GetSize() == previous_avatar_size,
                    "Window resizing reduces the full-height avatar");
            previous_avatar_size = avatar.GetSize();
            const auto* header = agent_display_->GetContainingSizer();
            require(avatar.GetTop() == header->GetPosition().y + FromDIP(6) &&
                        avatar.GetHeight() == header->GetSize().y - 2 * FromDIP(6),
                    "Avatar does not fill the header height");
            require(static_cast<long long>(avatar.GetWidth()) * 768 >=
                        static_cast<long long>(avatar.GetHeight()) * 512,
                    "Avatar width constrains its rendered height");
            require(agent_display_->GetPosition().x - card.GetRight() - 1 ==
                        FromDIP(8) + FromDIP(6),
                    "Avatar no longer follows the commentary card with a fixed gap");
            require(card.GetRight() <= agent_display_->GetPosition().x &&
                        agent_display_->GetRect().GetRight() < GetClientSize().x &&
                        agent_display_->GetRect().GetBottom() < navigation_[0]->GetPosition().y,
                    "Header regions overlap or avatar reaches navigation");
            require(personality_heading_->GetPosition().x >= FromDIP(12) &&
                        personality_heading_->GetPosition().y >= FromDIP(12),
                    "Personality heading covers the rounded corner");
        }
        status_copy_ = initial_status;
        SetClientSize(initial_client);
        layout_header();
        require(occurrence_ == resize_occurrence, "Resizing reselected personality commentary");
        require(status_dot_->IsShown(), "Routing status indicator missing");
        // The warning/Bother checks above switched packs after the first wait.
        // Resizing does not tick playback or publish the newly loaded frame.
        await_avatar();
        require(static_cast<bool>(agent_display_->presentation().image) &&
                    canonical_logo().HasAlpha(),
                "Agent logo or transparency missing");
        // Exact samples from the immutable source verify WIC channel order and
        // alpha without claiming that a desktop screenshot has been reviewed.
        const auto& logo = canonical_logo();
        require(logo.GetAlpha(0, 0) == 0 && logo.GetRed(256, 128) == 158 &&
                    logo.GetGreen(256, 128) == 251 && logo.GetBlue(256, 128) == 212 &&
                    logo.GetRed(256, 256) == 238 && logo.GetGreen(256, 256) == 158 &&
                    logo.GetBlue(256, 256) == 251,
                "Logo source colors or alpha changed during decoding");
        require(GetIcon().IsOk(), "Window application icon missing");
        TrayIconCache icon_cache;
        for (int size : {16, 32, 48})
            for (auto state : {State::Stopped, State::Ready, State::Running, State::Degraded,
                               State::Failed, State::Starting, State::Recovering, State::Stopping})
                require(icon_cache.get(size, state).IsOk(), "Tray logo render failed");
        const auto cached = icon_cache.size();
        (void)icon_cache.get(32, State::Running);
        require(icon_cache.size() == cached, "Unchanged tray state regenerated an icon");
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
        auto active_profile_context = profile_menu(viewed_);
        require(!active_profile_context->FindItem(wxID_DELETE)->IsEnabled(),
                "Context menu enabled deletion of an armed active profile");
        const auto before_avatar_ticks = controller_->snapshot().revision;
        wxTimerEvent avatar_tick(avatar_timer_);
        for (int tick = 0; tick < 10; ++tick)
            GetEventHandler()->ProcessEvent(avatar_tick);
        require(controller_->snapshot().revision == before_avatar_ticks,
                "Avatar timer advanced controller state or generated operational events");
        require(!hold_expressions_->GetValue(), "Expression hold must be opt-in");
        wxCommandEvent expression_hold(wxEVT_CHECKBOX, hold_expressions_->GetId());
        for (bool hold : {true, false}) {
            hold_expressions_->SetValue(hold);
            hold_expressions_->GetEventHandler()->ProcessEvent(expression_hold);
            require(preferences_->values().hold_expressions == hold &&
                        controller_->snapshot().revision == before_avatar_ticks,
                    "Expression hold checkbox failed to save or changed routing state");
        }
        forward_.buffer->SetValue(13);
        acknowledge_buffer(forward_);
        require(forward_.buffer_apply->IsEnabled(),
                "Edited buffer did not enable its Apply button");
        wxCommandEvent buffer_apply(wxEVT_BUTTON, forward_.buffer_apply->GetId());
        forward_.buffer_apply->GetEventHandler()->ProcessEvent(buffer_apply);
        require(!forward_.buffer_apply->IsEnabled() && !dirty_ &&
                    controller_->snapshot().paths[0].applied_buffer_ms == 13 &&
                    controller_->snapshot().paths[1].applied_buffer_ms == 6,
                "Buffer Apply failed confirmation or changed its sibling direction");
        duplex_->SetValue(false);
        wxCommandEvent duplex_change(wxEVT_CHECKBOX, duplex_->GetId());
        duplex_->GetEventHandler()->ProcessEvent(duplex_change);
        require(last_message_ == presentation::Message::Simplex &&
                    !commentary_->GetLabel().empty() && controller_->snapshot().paths.size() == 2,
                "Unchecking Duplex lacks acknowledgment or changed the live plan");
        duplex_->SetValue(true);
        duplex_->GetEventHandler()->ProcessEvent(duplex_change);
        auto* save_device = wxWindow::FindWindowByLabel("Save device", this);
        require(save_device != nullptr, "Save device action missing");
        wxCommandEvent device_save(wxEVT_BUTTON, save_device->GetId());
        save_device->GetEventHandler()->ProcessEvent(device_save);
        require(last_message_ == presentation::Message::DeviceSaved &&
                    !commentary_->GetLabel().empty(),
                "Save device acknowledgment missing");
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
        dfa_value_->ChangeValue(wxS("6.70"));
        wxCommandEvent enter(wxEVT_TEXT_ENTER, dfa_value_->GetId());
        dfa_value_->GetEventHandler()->ProcessEvent(enter);
        require(dfa_slider_->GetValue() == 670 &&
                    last_message_ == presentation::Message::DfaSixSeven,
                "Typed DFA Enter did not commit the text and move the slider");
        require(technical_->GetLabel() == "DFA Acknowledged", "DFA heading changed");
        dfa_value_->ChangeValue(wxS("5.55"));
        dfa_value_->GetEventHandler()->ProcessEvent(enter);
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
        dfa_value_->ChangeValue(wxS("12.00"));
        dfa_value_->GetEventHandler()->ProcessEvent(enter);
        require(dfa_slider_->GetValue() == 555 && dfa_value_->GetValue() == wxS("5.55") &&
                    last_message_ == presentation::Message::DfaAboveMaximum &&
                    controller_->snapshot().revision == before,
                "Out-of-range DFA input changed saved state or lacked validation feedback");
        dfa_value_->ChangeValue(wxS("1.337"));
        dfa_value_->GetEventHandler()->ProcessEvent(enter);
        require(dfa_value_->GetValue() == wxS("1.34") && dfa_slider_->GetValue() == 134 &&
                    last_message_ == presentation::Message::Dfa134,
                "Extra decimal precision did not normalize before Easter egg matching");
        for (const auto& invalid : {wxS("-0.001"), wxS("11.001"), wxS("999")}) {
            dfa_value_->ChangeValue(invalid);
            dfa_value_->GetEventHandler()->ProcessEvent(enter);
            require(dfa_hundredths_ == 134 && dfa_slider_->GetValue() == 134 &&
                        dfa_value_->GetValue() == wxS("1.34") &&
                        controller_->snapshot().revision == before,
                    "Rejected raw DFA input changed the previous valid value");
        }
        dfa_value_->ChangeValue(wxS("11.001"));
        wxFocusEvent focus_to_stepper(wxEVT_KILL_FOCUS, dfa_value_->GetId());
        focus_to_stepper.SetWindow(dfa_stepper_);
        dfa_value_->GetEventHandler()->ProcessEvent(focus_to_stepper);
        require(dfa_value_->GetValue() == wxS("11.001"),
                "Composite focus transition replaced raw input before stepper validation");
        wxSpinEvent step(wxEVT_SPIN_UP, dfa_stepper_->GetId());
        dfa_stepper_->GetEventHandler()->ProcessEvent(step);
        require(dfa_hundredths_ == 134 && last_message_ == presentation::Message::DfaAboveMaximum,
                "DFA stepper bypassed raw range validation");
        show_result({false, "Operational warning retained during DFA validation."});
        const auto warning = technical_copy_;
        dfa_value_->ChangeValue(wxS("11.001"));
        dfa_value_->GetEventHandler()->ProcessEvent(enter);
        require(technical_copy_ == warning && operational_warning_visible_ &&
                    !dfa_feedback_->GetLabel().empty(),
                "DFA validation suppressed a warning");
        present(presentation::Message::Ready);
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
            require(personality_heading_->GetLabel() == (i == 1   ? "Status"
                                                         : i == 2 ? "Horu"
                                                                  : "Norm"),
                    "Agent heading uses a mode label instead of its character/status name");
            await_avatar();
            require(agent_display_->presentation().personality == personality_,
                    "Selected built-in avatar retained a different identity");
            require(dfa_->IsShown() == (i != 1) && dfa_slider_->IsShown() == (i != 1),
                    "DFA visibility mismatch");
            require(dfa_value_->IsShown() == (i != 1) && dfa_stepper_->IsShown() == (i != 1) &&
                        dfa_feedback_->IsShown() == (i != 1),
                    "DFA numeric controls visible in Vanilla");
            if (i == 1) {
                const auto vanilla_occurrence = occurrence_;
                const int previous = dfa_hundredths_;
                dfa_value_->ChangeValue(wxS("-0.001"));
                commit_dfa_text();
                require(occurrence_ == vanilla_occurrence && dfa_hundredths_ == previous,
                        "Vanilla accepted DFA input or selected validation commentary");
                dfa_value_->ChangeValue(text(presentation::dfa_text(previous)));
            }
        }
        const auto desired_personality = preferences_->values().personality_id;
        select_personality("missing-catalog");
        require(preferences_->values().personality_id == desired_personality,
                "Missing package erased the saved desired personality identity");
        require(personality_ == "default" && dfa_->IsShown() && dfa_slider_->IsShown(),
                "Unavailable catalog did not select the entire preferred built-in package");
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
        require(navigation_[0]->GetLabel() == wxS("[Profiles]") &&
                    navigation_[1]->GetLabel() == notebook_->GetPageText(1),
                "Selected tab lacks textual distinction");
        const auto scan_options_revision = controller_->snapshot().revision;
        scan_mode_->SetSelection(1);
        wxCommandEvent level_change(wxEVT_CHOICE, scan_mode_->GetId());
        scan_mode_->GetEventHandler()->ProcessEvent(level_change);
        require(last_message_ == presentation::Message::ScanLevelChanged,
                "Testing level acknowledgment missing");
        scan_mode_->SetSelection(0); // Avoid consent dialogs in unattended smoke.
        scan_scope_->SetSelection(1);
        wxCommandEvent scope_change(wxEVT_CHOICE, scan_scope_->GetId());
        scan_scope_->GetEventHandler()->ProcessEvent(scope_change);
        require(last_message_ == presentation::Message::ScanScopeChanged &&
                    controller_->snapshot().revision == scan_options_revision,
                "Scan scope acknowledgment missing or options started controller work");
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
        auto* clear_log = wxWindow::FindWindowByLabel("Clear diagnostics log", this);
        require(clear_log != nullptr, "Clear diagnostics action missing");
        wxCommandEvent clear(wxEVT_BUTTON, clear_log->GetId());
        clear_log->GetEventHandler()->ProcessEvent(clear);
        require(controller_->snapshot().events.empty() && console_->GetValue().empty() &&
                    last_message_ == presentation::Message::DiagnosticsCleared &&
                    controller_->snapshot().state == audiohole::State::Running,
                "Clear diagnostics failed or interrupted routing");
        refresh();
        require(console_->GetValue().empty() &&
                    last_message_ == presentation::Message::DiagnosticsCleared,
                "Cleared diagnostics reappeared on refresh");
        scan_start_->GetEventHandler()->ProcessEvent(scan);
        controller_->advance(3000);
        refresh();
        require(last_message_ == presentation::Message::ScanCompleted &&
                    !console_->GetValue().empty(),
                "New events missing after diagnostics clear");
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
        // CTest captures stderr even for the Windows GUI executable. Keep the
        // assertion in its failed-test output as well as the downloadable file.
        std::fprintf(stderr, "GUI smoke FAIL: %s\n", e.what());
        std::fflush(stderr);
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream out(std::filesystem::path(directory.ToStdWstring()) / "smoke-result.txt");
            out << "FAIL: " << e.what() << '\n';
        }
        return false;
    }
}

} // namespace audiohole::gui
