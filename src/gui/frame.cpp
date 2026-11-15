#include "gui/frame.h"
#include "gui/controls.h"
#include "gui/native_theme.h"
#include "gui/welcome.h"
#include "model/application_controller.h"
#include "model/configuration_bundle.h"
#include "model/configuration_controller.h"
#include "model/routing_service.h"
#include "presentation/text_layout.h"
#include <cstdio>
#include <wx/display.h>
#include <wx/hyperlink.h>
#include <wx/modalhook.h>
#include <wx/stdpaths.h>

namespace audiohole::gui {
Frame::Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke,
             std::shared_ptr<BuildPreferences> preferences, std::function<void()> restart)
    : wxFrame(nullptr, wxID_ANY, wxS("AudioHole"), wxDefaultPosition, wxSize(1000, 1000)),
      controller_(std::make_unique<ApplicationController>(std::move(controller))),
      preferences_(std::move(preferences)), restart_(std::move(restart)), timer_(this),
      avatar_timer_(this), tuning_timer_(this), smoke_(smoke) {
    if (!preferences_) {
        preferences_ = std::make_shared<BuildPreferences>(std::filesystem::path{}, true);
        preferences_->load();
    }
    if (controller_->snapshot().operating_mode == OperatingMode::Tutorial)
        tutorial_ = std::make_unique<Tutorial>();
    controller_->audio_worker_policy(preferences_->values().scheduling_task,
                                     preferences_->values().audio_thread_priority);
    personality_ = tutorial_ ? "default" : preferences_->values().personality_id;
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            if (hole_index_ >= 0 && !operational_warning_visible_)
                present((tuning_reaction_reverse_ ? reverse_ : forward_).advanced_tuning->GetValue()
                            ? presentation::Message::AdvancedTuningEnabled
                            : presentation::Message::AutomaticRestored);
        },
        tuning_timer_.GetId());
    // A stable window property supports second-launch focus and shell actions.
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
    information->SetMinSize(FromDIP(wxSize(tutorial_ ? 320 : 280, -1)));
    header->Add(information, 0, wxEXPAND | wxALL, FromDIP(10));
    auto* brand_row = new wxBoxSizer(wxHORIZONTAL);
    auto* brand_logo = new AgentDisplay(this, wxSize(80, 80));
    brand_row->Add(brand_logo, 0, wxALIGN_CENTER_VERTICAL);
    information->Add(brand_row, 0, wxEXPAND);
    auto* heading = label(this, brand_row, "Audio");
    brand_row->GetItem(heading)->SetFlag(wxALIGN_BOTTOM | wxLEFT);
    auto* hole_wordmark = label(this, brand_row, "Hole");
    hole_wordmark->SetName("brand-wordmark-hole");
    hole_wordmark->SetFont(wxFont(wxFontInfo(26).Bold()));
    brand_row->GetItem(hole_wordmark)->SetFlag(wxALIGN_BOTTOM);
    brand_row->GetItem(hole_wordmark)->SetBorder(0);
    heading->SetName("brand-wordmark");
    heading->SetFont(wxFont(wxFontInfo(26).Bold()));
    auto* subtitle = label(this, information, "Put your audio wherever you want.");
    subtitle->SetFont(wxFont(wxFontInfo(12)));
    information->AddSpacer(FromDIP(10));
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
    // Reserve wrapped lines for routing, profiles, startup, advisory and scan.
    status_ = new wxStaticText(this, wxID_ANY, "Routing: Stopped", wxDefaultPosition, wxDefaultSize,
                               wxST_NO_AUTORESIZE);
    status_->SetMinSize(wxSize(0, status_->GetCharHeight() * (tutorial_ ? 9 : 5)));
    status_row->Add(status_, 1, wxEXPAND | wxALL, FromDIP(4));
    information->Add(status_row, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
    auto* middle = new wxBoxSizer(wxVERTICAL);
    // Branding and the centered avatar have reserved regions. The dialogue and
    // equal-width action buttons grow together into the remaining header width.
    header->Add(middle, 1, wxEXPAND | wxALL, FromDIP(8));
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    routing_actions_ = actions;
    start_ = button(this, actions, "Start Routing", [this] { activate(false); });
    stop_ = button(this, actions, "Stop Routing", [this] {
        controller_->stop();
        refresh();
    });
    button(this, actions, "Exit AudioHole", [this] { request_exit(); });
    // Align the card with the actual first/last button edges. Keep spacing only
    // between buttons, with DPI-scaled borders rather than outer side margins.
    for (auto* item : actions->GetChildren()) {
        item->SetBorder(FromDIP(3));
        item->SetProportion(1);
        item->SetFlag(wxEXPAND | wxALL);
    }
    actions->GetChildren().front()->SetFlag(wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT);
    actions->GetChildren().back()->SetFlag(wxEXPAND | wxTOP | wxBOTTOM | wxLEFT);
    middle->Add(actions, 0, wxEXPAND);
    auto* comment_surface = new SurfacePanel(this, "commentary-card");
    comment_surface_ = comment_surface;
    comment_surface->SetMinSize(wxSize(actions->GetMinSize().x, FromDIP(156)));
    middle->Add(comment_surface, tutorial_ ? 0 : 1, wxEXPAND | wxTOP, FromDIP(8));
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
    avatar_region_ = new wxPanel(this);
    avatar_region_->SetMinSize(FromDIP(wxSize(tutorial_ ? 260 : 120, -1)));
    avatar_layout_ = new wxBoxSizer(wxVERTICAL);
    avatar_region_->SetSizer(avatar_layout_);
    agent_display_ = new avatar::Panel(avatar_region_);
    agent_display_->checkerboard(false);
    avatar::initialize_image_decoder();
    avatar_host_ = std::make_unique<presentation::AvatarHost>(avatar::native_decoder());
    agent_display_->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { bother(); });
    avatar_layout_->Add(agent_display_, 1, wxALIGN_CENTER_HORIZONTAL);
    if (tutorial_) {
        tutorial_choices_panel_ = new wxPanel(avatar_region_);
        tutorial_choices_panel_->SetName("Tutorial choices at Norm's feet");
        tutorial_choices_panel_->SetMinSize(wxSize(0, FromDIP(84)));
        tutorial_choices_panel_->SetMaxSize(wxSize(-1, FromDIP(84)));
        tutorial_choices_layout_ = new wxBoxSizer(wxVERTICAL);
        tutorial_choices_panel_->SetSizer(tutorial_choices_layout_);
        avatar_layout_->Add(tutorial_choices_panel_, 0, wxEXPAND);
    }
    header->Add(avatar_region_, 0, wxEXPAND | wxALL, FromDIP(6));
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
    for (auto* split : {profile_columns_, profile_rows_, profile_lower_rows_, device_columns_,
                        device_rows_, performance_split_, scan_output_split_}) {
        split->Bind(wxEVT_SPLITTER_SASH_POS_CHANGED, [split](wxSplitterEvent& event) {
            event.Skip();
            split->CallAfter([split] {
                if (!split->IsBeingDeleted())
                    RedrawWindow(static_cast<HWND>(split->GetHandle()), nullptr, nullptr,
                                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN |
                                     RDW_UPDATENOW);
            });
        });
    }
    if (tutorial_) {
        auto disable_files = [&](const auto& self, wxWindow* parent) -> void {
            if (auto* action = dynamic_cast<wxButton*>(parent)) {
                const auto caption = action->GetLabel();
                if (caption.StartsWith("Export") || caption.StartsWith("Import"))
                    action->Disable();
            }
            for (auto* child : parent->GetChildren())
                self(self, child);
        };
        disable_files(disable_files, this);
    }
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
    CallAfter([this] {
        if (IsBeingDeleted())
            return;
        reset_layout();
    });
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
            if (closing_ && !controller_->snapshot().routing_pending &&
                controller_->snapshot().state == State::Stopped)
                finish_exit();
        },
        timer_.GetId());
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
        if (!e.CanVeto()) {
            controller_->stop();
            e.Skip();
            return;
        }
        if (tutorial_) {
            e.Veto();
            request_exit();
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
            themed_message_box(
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
        end_observation();
        update_navigation();
        tutorial_event("navigation");
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
            notebook_->GetSelection() == 0 && !bundle_generation_ && profile_edits_pending())
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
            avatar_timer_.Start(11);
        } else {
            avatar_timer_.Stop();
            tutorial_scroll_duration_ = 0;
        }
        event.Skip();
    });
    Bind(wxEVT_ICONIZE, [this](wxIconizeEvent& event) {
        if (!event.IsIconized() && IsShown()) {
            avatar_host_->activity(avatar::monotonic_ms());
            poll_avatar();
            avatar_timer_.Start(11);
        } else {
            avatar_timer_.Stop();
            tutorial_scroll_duration_ = 0;
        }
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
    if (tutorial_ && type == wxEVT_CHAR_HOOK &&
        static_cast<wxKeyEvent&>(event).GetKeyCode() == WXK_F1) {
        tutorial_->repeat();
        render_tutorial();
        return wxEventFilter::Event_Processed;
    }
    if (type == wxEVT_MOUSEWHEEL) {
        auto* window = dynamic_cast<wxWindow*>(event.GetEventObject());
        if (tutorial_ && window &&
            (window == profile_editor_ || profile_editor_->IsDescendant(window)))
            tutorial_scroll_duration_ = 0;
        if (window && notebook_->GetPage(0)->IsDescendant(window) &&
            chain_field_wheel(*window, static_cast<wxMouseEvent&>(event)))
            return wxEventFilter::Event_Processed;
        for (auto* control = window; control && control != this; control = control->GetParent()) {
            if (dynamic_cast<wxChoice*>(control) || dynamic_cast<wxComboBox*>(control) ||
                control == dfa_slider_ || control == dfa_value_ || control == dfa_stepper_) {
                forward_dropdown_wheel(*control, static_cast<wxMouseEvent&>(event));
                return wxEventFilter::Event_Processed;
            }
        }
    }
    if (bundle_generation_ &&
        (type == wxEVT_KEY_DOWN || type == wxEVT_CHAR_HOOK || type == wxEVT_CHAR ||
         type == wxEVT_LEFT_DOWN || type == wxEVT_LEFT_DCLICK || type == wxEVT_RIGHT_DOWN)) {
        auto* window = dynamic_cast<wxWindow*>(event.GetEventObject());
        if (window &&
            (window == notebook_->GetPage(0) || notebook_->GetPage(0)->IsDescendant(window)))
            return wxEventFilter::Event_Processed;
    }
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
void Frame::exit() {
    if (closing_)
        return;
    closing_ = true;
    controller_->stop();
    end_observation();
    const auto& state = controller_->snapshot();
    if (state.capabilities.real_engine &&
        (state.routing_pending || state.state != State::Stopped)) {
        start_->Disable();
        notebook_->Disable();
        technical_copy_ = "Stopping routing before closing. Waiting for stream cleanup.";
        commentary_copy_.clear();
        layout_header();
        timer_.Start(100);
        return;
    }
    finish_exit();
}
void Frame::finish_exit() {
    timer_.Stop();
    tuning_timer_.Stop();
    stop_avatar();
    if (tray_)
        tray_->RemoveIcon();
    Destroy();
}
void Frame::request_exit() {
    if (closing_)
        return;
    if (tutorial_ && !tutorial_->complete()) {
        tutorial_->remind("B011");
        render_tutorial();
    }
    if (confirm_exit(this)) {
        if (tutorial_ && tutorial_->complete() && !tutorial_marker_saved_ && !finish_tutorial())
            return;
        exit();
    } else if (tutorial_) {
        tutorial_->dismiss_reminder();
        render_tutorial();
    }
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
    if (tutorial_) {
        avatar_host_->activity(now);
        if (advance_tutorial_scroll(now))
            render_tutorial();
    }
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
    if (tutorial_) {
        tutorial_->click(avatar::monotonic_ms());
        render_tutorial();
        return;
    }
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
    if (tutorial_ || (event == presentation::Message::ErrorMinor && operational_warning_visible_))
        return;
    const bool device_notice = event == presentation::Message::DeviceConnected ||
                               event == presentation::Message::DeviceDisconnected ||
                               event == presentation::Message::DeviceUnavailable;
    if (!device_notice)
        device_event_copy_.clear();
    operational_warning_visible_ =
        event == presentation::Message::ErrorMajor || event == presentation::Message::ScanFailed ||
        event == presentation::Message::Disconnected ||
        event == presentation::Message::Recovering ||
        event == presentation::Message::Questionable || event == presentation::Message::Invalid;
    last_message_ = event;
    const auto c = messages_.show(event, personality_, ++occurrence_);
    avatar_host_->observe(controller_->snapshot(), operational_warning_visible_,
                          avatar::monotonic_ms());
    avatar_host_->react(event, c, occurrence_, avatar::monotonic_ms());
    fallback_copy_ =
        personality_fallback_note_.empty() ? c.fallback_detail : personality_fallback_note_;
    technical_copy_ = event == presentation::Message::Bother ? std::string{} : c.technical;
    if (device_notice && !device_event_copy_.empty())
        technical_copy_ = device_event_copy_;
    commentary_copy_ = c.commentary;
    layout_header();
}
void Frame::layout_header() {
    // Minimized native bounds are not a layout target; restore triggers layout.
    if (IsIconized())
        return;
    // Re-read native button metrics after DPI/font changes. Copy length never
    // participates in the commentary card's horizontal sizing.
    comment_surface_->SetMinSize(
        wxSize(routing_actions_->GetMinSize().x, FromDIP(tutorial_ ? 266 : 156)));
    // Derive height solely from the branding and operations columns, excluding
    // the avatar to avoid a feedback loop. Never shrink it at compact widths.
    auto* header = avatar_region_->GetContainingSizer();
    int header_height = 0;
    for (auto* item : header->GetChildren()) {
        if (item->GetWindow() == avatar_region_)
            continue;
        item->CalcMin();
        header_height = std::max(header_height, item->GetMinSizeWithBorder().y);
    }
    const int avatar_height = header_height - 2 * FromDIP(6);
    agent_display_->fit_height(avatar_height - (tutorial_ ? FromDIP(84) : 0));
    if (!tutorial_)
        avatar_region_->SetMinSize(
            wxSize(std::max(FromDIP(120), agent_display_->GetMinSize().x), avatar_height));
    // Full-height artwork needs its real proportional width even at minimum
    // size. Increase the minimum instead of clipping or reducing the image.
    auto minimum = FromDIP(wxSize(980, 680));
    if (GetSizer() && profile_rows_ && notebook_ && !navigation_.empty()) {
        constrain_profile_rows();
        int borders = 2 * profile_columns_->GetBorderSize();
        if (const auto* item = GetSizer()->GetItem(notebook_))
            borders += item->GetBorder() * 2;
        auto* page_layout = notebook_->GetPage(0)->GetSizer();
        if (const auto* item = page_layout->GetItem(profile_actions_))
            borders += item->GetBorder() * 2;
        if (const auto* item = page_layout->GetItem(profile_columns_))
            borders += item->GetBorder() * 2;
        minimum.y =
            std::max(minimum.y,
                     header_height + navigation_[0]->GetContainingSizer()->GetMinSize().y +
                         profile_actions_->GetEffectiveMinSize().y + profile_rows_->GetMinSize().y +
                         borders + GetSize().y - GetClientSize().y);
    }
    minimum.x = std::max(minimum.x, header->GetMinSize().x + GetSize().x - GetClientSize().x);
    if (GetMinSize() != minimum)
        SetMinSize(minimum);
    if (GetSize().x < minimum.x || GetSize().y < minimum.y)
        SetSize(wxSize(std::max(minimum.x, GetSize().x), std::max(minimum.y, GetSize().y)));
    Layout(); // Allocate fixed branding/avatar regions before measuring text space.
    constrain_profile_rows();
    const int width = std::max(120, commentary_panel_->GetClientSize().x - FromDIP(24));
    auto wrap = [width](wxStaticText* control, const std::string& copy) {
        control->Show(!copy.empty());
        control->SetLabel(text(presentation::keep_kaomoji_together(copy)));
        control->Wrap(width);
    };
    status_->SetLabel(text(status_copy_));
    status_->Wrap(std::max(100, status_->GetClientSize().x));
    status_->SetToolTip(text(status_copy_));
    const bool asleep = !sleep_copy_.empty() && !operational_warning_visible_ &&
                        avatar_host_->enabled() &&
                        avatar_host_->baseline().expression == "sleeping";
    wrap(technical_, asleep ? std::string{} : technical_copy_);
    technical_->SetToolTip(text(technical_copy_));
    if (!tutorial_ && !operational_warning_visible_ && !asleep &&
        technical_->GetLabel().Freq('\n') > 1) {
        const auto lines = wxSplit(technical_->GetLabel(), '\n');
        wxClientDC dc(technical_);
        dc.SetFont(technical_->GetFont());
        technical_->SetLabel(lines[0] + "\n" +
                             wxControl::Ellipsize(lines[1] + "…", dc, wxELLIPSIZE_END, width));
    }
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
void Frame::show_result(const audiohole::Result& result, bool operational) {
    if (tutorial_) {
        if (!result.ok)
            tutorial_->remind(dirty_ ? "B007" : "B006");
        render_tutorial();
        return;
    }
    // Input/file-action failures are recoverable locally. They do not authorize
    // a persistent operational warning or suppress cosmetic interactions.
    if (!result.ok && !operational) {
        if (!operational_warning_visible_) {
            present(presentation::Message::ErrorMinor);
            technical_copy_ = result.detail;
            layout_header();
        }
        if (!smoke_)
            themed_message_box(text(result.detail), "AudioHole — action unavailable",
                               wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (result.ok && operational_warning_visible_ && !operational)
        return;
    if (!result.ok)
        present(presentation::Message::ErrorMajor);
    operational_warning_visible_ = !result.ok;
    avatar_host_->observe(controller_->snapshot(), operational_warning_visible_,
                          avatar::monotonic_ms());
    technical_copy_ = result.detail;
    layout_header();
    if (!result.ok && !smoke_)
        themed_message_box(text(result.detail), wxS("AudioHole — action unavailable"),
                           wxOK | wxICON_WARNING, this);
}
bool Frame::discard() {
    if (!dirty_)
        return true;
    return themed_message_box("Discard unsaved editor changes? Saved profiles and the active "
                              "route will be retained.",
                              "Unsaved changes", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                              this) == wxYES;
}
void Frame::end_observation() {
    controller_->monitor(false);
    controller_->cancel_beep();
    active_check_ = nullptr;
}
// GUI timer polling does not create presentation events. A controller sequence
// number gates commentary, so repainting/telemetry cannot consume more variants.
void Frame::refresh() {
    const auto& s = controller_->snapshot();
    if (bundle_generation_ && s.scanner.completed_generation == bundle_generation_) {
        const bool inspecting = bundle_inspecting_, importing = bundle_importing_;
        bundle_generation_ = 0;
        bundle_inspecting_ = bundle_importing_ = false;
        if (s.scanner.result == "completed" && inspecting && s.scanner.import_plan) {
            const auto plan = s.scanner.import_plan;
            if (themed_message_box(text(plan->summary), "Import configuration bundle?",
                                   wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) == wxYES) {
                ScannerActionRequest request;
                request.kind = ScannerActionKind::ImportBundle;
                request.confirmed = true;
                request.import_plan = plan;
                const auto result = controller_->scanner_action(request);
                if (!result.ok)
                    show_result(result);
                else {
                    bundle_generation_ = controller_->snapshot().scanner.generation;
                    bundle_importing_ = true;
                }
            }
        } else if (s.scanner.result == "completed" && importing) {
            const auto id = profile(s, viewed_)  ? viewed_
                            : s.profiles.empty() ? std::string{}
                                                 : s.profiles.front().id;
            select_profile(id);
            return;
        } else if (s.scanner.result != "completed")
            show_result({false, s.scanner.detail});
    }
    if (!tutorial_)
        avatar_host_->observe(s, operational_warning_visible_, avatar::monotonic_ms());
    const bool preview = s.operating_mode != OperatingMode::Test;
    start_->Enable(s.capabilities.routing &&
                   (!s.capabilities.real_engine ||
                    (!s.scanner.busy && !s.monitoring && s.beep_destination.empty())) &&
                   !s.routing_pending && (!s.armed || s.capabilities.simulated) &&
                   s.state != State::Stopping);
    stop_->Enable(s.capabilities.routing);
    profile_apply_->Enable(s.capabilities.routing && !viewed_.empty() &&
                           (s.capabilities.simulated ||
                            (!s.armed && !s.routing_pending && s.state != State::Stopping)));
    if (!loading_)
        dirty_ = profile_edits_pending();
    profile_save_->Enable(!viewed_.empty() && dirty_ && !bundle_generation_);
    profile_revert_->Enable(!viewed_.empty() && dirty_ && !bundle_generation_);
    profile_notes_->SetEditable(!viewed_.empty());
    hole_new_->Enable(!viewed_.empty());
    profile_remove_->Enable(!viewed_.empty() && !(s.armed && viewed_ == s.active_profile) &&
                            viewed_ != s.pending_profile);
    hole_remove_->Enable(hole_index_ >= 0);
    profiles_folder_->Enable(!preview);
    devices_folder_->Enable(!preview);
    const auto edit_state =
        text(viewed_.empty()
                 ? std::string("No profile selected")
                 : draft_.display_name + (dirty_ ? " — Unsaved changes" : " — No pending edits"));
    if (profile_edit_state_->GetLabel() != edit_state) {
        profile_edit_state_->SetLabel(edit_state);
        profile_edit_state_->SetToolTip(edit_state);
    }
    if (device_save_generation_ && s.scanner.completed_generation == device_save_generation_) {
        device_save_generation_ = 0;
        if (s.scanner.result == "completed") {
            device_dirty_ = false;
            loaded_device_id_.clear();
            present(presentation::Message::DeviceSaved);
        }
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
        end_observation();
    }
    const bool scanner_idle = !s.scanner.busy && !s.scanning;
    scan_start_->Enable(s.capabilities.scanning && scanner_idle &&
                        (s.capabilities.simulated ||
                         (!s.armed && !s.routing_pending && s.state != State::Stopping)));
    const bool selected_device = endpoint(s, device_id_) != nullptr;
    if (!loading_ && selected_device && loaded_device_id_ == device_id_) {
        const auto* selected = endpoint(s, device_id_);
        device_dirty_ = utf8(device_name_->GetValue()) != selected->display_name ||
                        utf8(device_notes_->GetValue()) != selected->notes;
    }
    device_save_->Enable(selected_device && scanner_idle && device_dirty_);
    device_name_->SetEditable(selected_device && scanner_idle);
    device_notes_->SetEditable(selected_device && scanner_idle);
    for (auto* action : scanner_actions_)
        action->Enable(s.scanner.available && scanner_idle);
    scan_cancel_->Enable(s.capabilities.scanning && (s.scanning || s.scanner.busy));
    scan_mode_->Enable(s.capabilities.scanning && scanner_idle);
    scan_scope_->Enable(s.capabilities.scanning && scanner_idle);
    if (hole_index_ >= 0) {
        auto forward = draft_.holes.at(static_cast<std::size_t>(hole_index_)).forward;
        auto reverse = draft_.holes.at(static_cast<std::size_t>(hole_index_)).reverse;
        read_direction(forward_, forward);
        read_direction(reverse_, reverse);
        update_direction_details(forward_, forward);
        update_direction_details(reverse_, reverse);
    }
    if (holes_->GetCount() == draft_.holes.size()) {
        auto* list = static_cast<ThemedList*>(holes_);
        std::vector<wxColour> colours;
        for (const auto& hole : draft_.holes) {
            const bool failed = std::any_of(s.paths.begin(), s.paths.end(), [&](const auto& path) {
                return path.hole_id == hole.id && path.state == State::Failed;
            });
            colours.push_back(failed         ? wxColour(230, 70, 70)
                              : hole.enabled ? wxColour(40, 170, 105)
                                             : wxColour(145, 154, 168));
        }
        if (colours != list->status_colours) {
            list->status_colours = std::move(colours);
            list->Refresh();
        }
    }
    const auto* active = audiohole::profile(s, s.active_profile);
    std::string status =
        (tutorial_ ? std::string("Tutorial Mode | Pretend routing: ") + audiohole::name(s.state)
         : preview ? std::string("Preview Mode | Routing: ") + audiohole::name(s.state)
                   : (s.capabilities.routing ? std::string("Routing: ") + audiohole::name(s.state)
                                             : std::string("Engine: unavailable"))) +
        "\nActive: " + (active ? active->display_name : "none") +
        "\nViewed: " + draft_.display_name + (dirty_ ? " (unsaved edits)" : "");
    if (s.capabilities.real_engine && s.armed)
        status = "Routing: " + routing_progress(s) + status.substr(status.find('\n'));
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
    SetStatusText(text(std::string(tutorial_ ? "AudioHole (Tutorial Mode) | Simulated: "
                                   : preview ? "AudioHole (Preview Mode) | Engine: "
                                             : "AudioHole | Engine: ") +
                       ((preview || s.capabilities.routing) ? name(s.state) : "unavailable") +
                       " | Devices: " + std::to_string(s.endpoints.size()) +
                       " | Profiles: " + std::to_string(s.profiles.size()) +
                       " | Viewed holes: " + std::to_string(holes) +
                       " | Warnings: unavailable | Log errors: " + std::to_string(errors)));
    update_status_indicator();
    std::string paths;
    for (const auto& p : s.paths)
        paths += p.hole_id + (p.reverse ? " return: " : " forward: ") + audiohole::name(p.state) +
                 " — " + p.detail + "\n";
    if (s.routing_status)
        paths = routing_diagnostics(*s.routing_status, s.routing_intent.get());
    if (!s.routing_admission_issues.empty())
        paths = routing_issues(s.routing_admission_issues);
    const auto rendered_paths = text(paths.empty() ? "No active directions." : paths);
    if (path_status_->GetValue() != rendered_paths) {
        const auto handle = static_cast<HWND>(path_status_->GetHandle());
        const auto first = SendMessageW(handle, EM_GETFIRSTVISIBLELINE, 0, 0);
        long begin = 0, end = 0;
        path_status_->GetSelection(&begin, &end);
        path_status_->Freeze();
        path_status_->ChangeValue(rendered_paths);
        const auto length = path_status_->GetLastPosition();
        path_status_->SetSelection(std::min(begin, length), std::min(end, length));
        SendMessageW(handle, EM_LINESCROLL, 0,
                     first - SendMessageW(handle, EM_GETFIRSTVISIBLELINE, 0, 0));
        path_status_->Thaw();
    }
    refresh_device_checks();
    if (s.scanner.busy && s.scan_percent < 0)
        scan_progress_->Pulse();
    else
        scan_progress_->SetValue(std::clamp(s.scan_percent, 0, 100));
    wxString scan_status;
    if (s.scanner.busy && !scanner_clock_active_)
        scanner_clock_started_ = std::chrono::steady_clock::now();
    scanner_clock_active_ = s.scanner.busy;
    if (s.scanner.available) {
        scan_status = text(s.scanner.phase + " | " + s.scanner.result);
        if (s.scanner.busy)
            scan_status +=
                wxString::Format(" | Elapsed %lld s",
                                 static_cast<long long>(
                                     std::chrono::duration_cast<std::chrono::seconds>(
                                         std::chrono::steady_clock::now() - scanner_clock_started_)
                                         .count()));
        scan_status += "\n" + text(s.scanner.detail);
    } else
        scan_status = s.scanning ? (tutorial_ ? "Fictional tutorial scan in progress"
                                              : "Preview scan in progress")
                                 : "No scan in progress";
    if (scan_state_->GetValue() != scan_status) {
        scan_state_->ChangeValue(scan_status);
        scan_state_->SetToolTip(scan_status);
        scan_state_->ShowPosition(0);
    }
    const auto query = log_search_->GetValue().Lower();
    std::string logs;
    for (const auto& e : s.events) {
        if (severity_->GetSelection() == 1 && e.type != "error" && e.type != "scan-failed" &&
            e.type != "scanner-action-failed" && e.type != "configuration-save-failed")
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
        update_console(text(logs));
        last_log_ = logs;
    }
    for (const auto& e : s.events)
        if (e.sequence > seen_event_) {
            seen_event_ = e.sequence;
            if (tutorial_)
                continue;
            bool routine = true;
            presentation::Message message = presentation::Message::Ready;
            if (e.type == "scan-start")
                message = presentation::Message::ScanStarted;
            else if (e.type == "scan-complete")
                message = presentation::Message::ScanCompleted;
            else if (e.type == "scan-cancelled")
                message = presentation::Message::ScanCancelled;
            else if (e.type == "scan-failed")
                routine = false;
            else if (e.type == "manual-override-applied")
                message = presentation::Message::ManualOverrideApplied;
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
            if ((e.type == "scanner-action-failed" || e.type == "configuration-save-failed" ||
                 e.type == "action-rejected") &&
                !operational_warning_visible_) {
                present(presentation::Message::ErrorMinor);
                technical_copy_ = e.detail;
                layout_header();
            }
            if (e.type == "scanner-action-failed" && !smoke_)
                themed_message_box(text(e.detail), "Device / export operation", wxOK | wxICON_ERROR,
                                   this);
            if (e.type == "error" || e.type == "scan-failed" || e.severity == Severity::Error ||
                e.severity == Severity::Critical) {
                const bool major = e.type == "error" || e.severity == Severity::Critical;
                if (major || !operational_warning_visible_) {
                    present(major ? presentation::Message::ErrorMajor
                                  : presentation::Message::ErrorMinor);
                    technical_copy_ = e.detail;
                    layout_header();
                }
            }
            if (!smoke_ && !tutorial_ && tray_->IsIconInstalled() &&
                notifications_->GetSelection() != 2 &&
                (e.type == "error" ||
                 (notifications_->GetSelection() == 1 &&
                  (e.type == "reconnected" || e.type == "disconnected" || e.type == "stopped")))) {
                tray_->ShowBalloon(wxS("AudioHole"), text(e.detail),
                                   e.type == "error" ? wxICON_WARNING : wxICON_INFORMATION, 3000);
            }
        }
    const auto device_notices = device_events_.observe(s.endpoints);
    if (!device_notices.empty() && !operational_warning_visible_) {
        device_event_copy_.clear();
        for (const auto& notice : device_notices) {
            if (!device_event_copy_.empty())
                device_event_copy_ += " ";
            device_event_copy_ += notice.text;
        }
        present(device_notices.back().message);
    }
    tray_->update(s);
    if (tutorial_) {
        std::vector<std::string> observed;
        for (const auto& event : s.events)
            if (event.sequence > tutorial_seen_event_) {
                observed.push_back(event.type);
                tutorial_seen_event_ = event.sequence;
            }
        for (const auto& event : observed)
            tutorial_event(event);
        tutorial_->tick(avatar::monotonic_ms());
        render_tutorial();
    }
}
bool Frame::smoke(const wxString& directory) {
    try {
        auto require = [](bool ok, const char* detail) {
            if (!ok)
                throw std::runtime_error(detail);
        };
        require(GetTitle() == "AudioHole", "Window title must omit the build identifier");
        const auto check_centered_input = [](wxWindow* control) {
            // Collapsed panes can temporarily assign one-pixel native bounds.
            // Each direction is checked again with its tuning pane visible.
            if (!control->IsShownOnScreen())
                return;
            const auto handle = static_cast<HWND>(control->MSWGetFocusHWND());
            RECT outer{}, client{};
            GetWindowRect(handle, &outer);
            ::GetClientRect(handle, &client);
            POINT origin{};
            ::ClientToScreen(handle, &origin);
            const auto dc = GetDC(handle);
            const auto saved = SaveDC(dc);
            SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(handle, WM_GETFONT, 0, 0)));
            TEXTMETRICW metric{};
            GetTextMetricsW(dc, &metric);
            RestoreDC(dc, saved);
            ReleaseDC(handle, dc);
            const auto position = SendMessageW(handle, EM_POSFROMCHAR, 0, 0);
            const int text_y = position == -1 ? 0 : static_cast<short>(HIWORD(position));
            const int actual = origin.y + text_y;
            const int expected = outer.top + (outer.bottom - outer.top - metric.tmHeight) / 2;
            const LONG padding_point =
                std::max(2L, (outer.bottom - outer.top - metric.tmHeight) / 4);
            for (LONG y : {outer.top + padding_point, outer.bottom - padding_point - 1})
                if (SendMessageW(handle, WM_NCHITTEST, 0,
                                 MAKELPARAM((outer.left + outer.right) / 2, y)) != HTCLIENT)
                    throw std::runtime_error("Centered field padding stopped accepting input: " +
                                             utf8(control->GetName()));
            if ((GetWindowLongPtrW(handle, GWL_STYLE) & ES_MULTILINE) ||
                std::abs(actual - expected) > 1 || client.bottom - client.top < metric.tmHeight)
                throw std::runtime_error("Input text is not centered in its single-line field: " +
                                         utf8(control->GetName()) +
                                         " actual=" + std::to_string(actual) +
                                         " expected=" + std::to_string(expected) +
                                         " window=" + std::to_string(outer.bottom - outer.top) +
                                         " client=" + std::to_string(client.bottom - client.top) +
                                         " font=" + std::to_string(metric.tmHeight) +
                                         " text_y=" + std::to_string(text_y));
        };
        const auto check_endpoint_heights = [this] {
            for (auto* direction : {&forward_, &reverse_})
                for (auto* field : {direction->source, direction->destination}) {
                    if (!field->HasFlag(wxCB_READONLY) ||
                        (GetWindowLongPtrW(static_cast<HWND>(field->GetHandle()), GWL_STYLE) &
                         CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST)
                        throw std::runtime_error("Endpoint selector still permits text editing");
                    if (field->IsShownOnScreen() &&
                        field->GetSize().y < field->GetCharHeight() + FromDIP(12))
                        throw std::runtime_error(
                            "Endpoint dropdown collapsed: " + utf8(field->GetName()) +
                            " height=" + std::to_string(field->GetSize().y) +
                            " best=" + std::to_string(field->GetBestSize().y));
                }
        };
        {
            WelcomeDialog welcome(true, preferences_->values().appearance);
            require(welcome.has_norm_image(), "Welcome Norm image missing");
            require(welcome.FindWindow(wxID_YES)->GetLabel() == "It's my first time" &&
                        welcome.FindWindow(wxID_NO)->GetLabel() == "Show me them holes!" &&
                        welcome.FindWindow(wxID_CANCEL)->GetLabel() == "I'm not ready yet",
                    "Welcome button labels/launch targets changed");
        }
        apply_appearance(); // The separate welcome follows Windows' dialog theme.
        require(theme_choice_->GetSelection() == 2 && preferences_->values().appearance == "system",
                "Fresh appearance must follow the Windows system theme");
        require(startup_->IsEnabled() && startup_target_->IsEnabled(),
                "Routing Activation preferences must be editable in both environments");
        {
            const auto original = preferences_->values();
            const auto revision = controller_->snapshot().revision;
            wxCommandEvent mode(wxEVT_CHOICE, startup_->GetId());
            for (int selection : {0, 1, 2, 1}) {
                startup_->SetSelection(selection);
                startup_->GetEventHandler()->ProcessEvent(mode);
                require(delay_->IsEnabled() == (selection == 1) &&
                            (selection == 1 || delay_->GetValue() == 0),
                        "Routing delay enable/value does not match activation mode");
            }
            require(controller_->snapshot().revision == revision,
                    "Saving startup preferences attempted a routing operation");
            require(preferences_->update(original).ok, "Could not restore startup preferences");
            startup_->SetSelection((original.startup_mode + 2) % 3);
            startup_->GetEventHandler()->ProcessEvent(mode);
        }
        {
            class CancelExit final : public wxModalDialogHook {
              public:
                bool asked = false;
                int Enter(wxDialog* dialog) override {
                    if (dialog->GetTitle() != "Exit AudioHole?")
                        return wxID_NONE;
                    asked = true;
                    return wxID_NO;
                }
            } hook;
            const auto before = controller_->snapshot().revision;
            hook.Register();
            request_exit();
            hook.Unregister();
            require(hook.asked && !IsBeingDeleted() && controller_->snapshot().revision == before,
                    "Cancel on Exit must leave the session running and unchanged");
        }
        {
            const auto initial = theme_choice_->GetSelection();
            const auto revision = controller_->snapshot().revision;
            wxMouseEvent wheel(wxEVT_MOUSEWHEEL);
            wheel.SetEventObject(theme_choice_);
            wheel.m_wheelRotation = -WHEEL_DELTA;
            wheel.m_wheelDelta = WHEEL_DELTA;
            wheel.m_linesPerAction = 3;
            require(FilterEvent(wheel) == wxEventFilter::Event_Processed &&
                        theme_choice_->GetSelection() == initial &&
                        controller_->snapshot().revision == revision,
                    "Wheel gesture changed a dropdown or controller state");
            theme_choice_->GetEventHandler()->ProcessEvent(wheel);
            const auto point = theme_choice_->ClientToScreen(wxPoint(2, 2));
            SendMessageW(static_cast<HWND>(theme_choice_->GetHandle()), WM_MOUSEWHEEL,
                         MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(point.x, point.y));
            require(theme_choice_->GetSelection() == initial,
                    "Native wheel changed a dropdown selection");
        }
        {
            const auto value = dfa_hundredths_;
            const auto slider = dfa_slider_->GetValue();
            const auto entered = dfa_value_->GetValue();
            const auto revision = controller_->snapshot().revision;
            const auto occurrence = occurrence_;
            for (auto* control : std::vector<wxWindow*>{dfa_slider_, dfa_value_, dfa_stepper_}) {
                for (int rotation : {-WHEEL_DELTA, WHEEL_DELTA}) {
                    wxMouseEvent wheel(wxEVT_MOUSEWHEEL);
                    wheel.SetEventObject(control);
                    wheel.m_wheelRotation = rotation;
                    wheel.m_wheelDelta = WHEEL_DELTA;
                    wheel.m_linesPerAction = 3;
                    require(FilterEvent(wheel) == wxEventFilter::Event_Processed,
                            "DFA wheel gesture was not intercepted");
                    const auto point = control->ClientToScreen(wxPoint(2, 2));
                    SendMessageW(static_cast<HWND>(control->GetHandle()), WM_MOUSEWHEEL,
                                 MAKEWPARAM(0, rotation), MAKELPARAM(point.x, point.y));
                }
            }
            require(dfa_hundredths_ == value && dfa_slider_->GetValue() == slider &&
                        dfa_value_->GetValue() == entered && occurrence_ == occurrence &&
                        controller_->snapshot().revision == revision,
                    "Wheel changed DFA values, commentary or controller state");
            require(dfa_stepper_->GetSize().y == dfa_value_->GetSize().y,
                    "DFA arrows must match the numeric editor height");
        }
        auto exercise_information = [&] {
            const auto window_size = GetSize();
            const auto original = draft_.holes.at(static_cast<std::size_t>(hole_index_));
            require(!forward_.advanced_tuning->GetValue(),
                    "Information regression needs Automatic policy");
            for (bool duplex : {false, true}) {
                reverse_.group->ShowItems(duplex);
                for (auto* widgets : {&forward_, &reverse_}) {
                    if (widgets == &reverse_ && !duplex)
                        continue;
                    auto candidate = widgets == &forward_ ? original.forward : original.reverse;
                    const int height = widgets->information->GetMinSize().y;
                    for (const auto* destination : {"legacy-out", "vr-out", ""}) {
                        candidate.source = "mixer-in";
                        candidate.destination = destination;
                        populate_direction(*widgets, candidate);
                        auto effective = candidate;
                        effective.settings = Settings{};
                        const auto explanation =
                            text(assess_formats(controller_->snapshot(), effective,
                                                draft_.holes[hole_index_].routing_engine)
                                     .explanation);
                        require(widgets->information->GetSize().y == height &&
                                    widgets->information->GetMaxSize().y == height &&
                                    !widgets->information->IsEditable() &&
                                    widgets->information->HasFlag(wxTE_WORDWRAP) &&
                                    widgets->information->GetValue().Contains(explanation),
                                "Direction information resized, lost evidence or became editable");
                        require(widgets->group->GetStaticBox()->GetScreenRect().Contains(
                                    widgets->information->GetScreenRect()),
                                "Direction information extends outside its group before a tuning "
                                "toggle");
                        require(GetSize() == window_size && !forward_.advanced_tuning->GetValue(),
                                "Endpoint information changed window size or enabled tuning");
                    }
                }
            }
            load_hole(hole_index_); // Restore the original draft's endpoints and duplex visibility.
            require(reverse_.group->GetStaticBox()->GetLabel() == "Return" &&
                        reverse_.tuning_group->GetStaticBox()->GetLabel() == "Return tuning",
                    "Duplex captions must say Return");
        };
        auto exercise_tuning = [&] {
            exercise_information();
            require(hole_index_ >= 0 && !forward_.advanced_tuning->GetValue() &&
                        forward_.tuning->IsCollapsed() && reverse_.tuning->IsCollapsed(),
                    "New holes must start with Automatic tuning and collapsed direction panels");
            require(forward_.advanced_tuning->GetLabel() == "Use advanced tuning" &&
                        routing_engine_->GetCount() == 4 &&
                        routing_engine_->GetString(1) == "WASAPI" &&
                        routing_engine_->GetString(2) == "Miniaudio" &&
                        routing_engine_->GetString(3) == "Experimental",
                    "Routing/tuning choices are incomplete");
            require(forward_.tuning->GetParent() == forward_.group->GetStaticBox() &&
                        reverse_.tuning->GetParent() == reverse_.group->GetStaticBox() &&
                        forward_.tuning != reverse_.tuning,
                    "Direction tuning must belong to its own endpoint group");
            require(wxWindow::FindWindowByLabel(wxS("Fallback device — unavailable"), this) ==
                        nullptr,
                    "Fallback device placeholder remains in advanced tuning");
            require(forward_.mode->GetString(1) == "Force exclusive mode" &&
                        forward_.mode->GetString(2) == "Force shared mode" &&
                        forward_.clock->GetCount() == 3,
                    "Forced mode or clock choices missing");
            for (auto* widgets : {&forward_, &reverse_}) {
                const auto& items = widgets->group->GetChildren();
                auto find = [&](wxWindow* window) {
                    return std::find_if(items.begin(), items.end(), [&](wxSizerItem* item) {
                        return item->IsWindow() && item->GetWindow() == window;
                    });
                };
                require(std::distance(items.begin(), find(widgets->exclusive_preferred)) <
                            std::distance(items.begin(), find(widgets->information)),
                        "Exclusive preference must precede its direction information");
                require(widgets->tuning->GetPane()
                                ->GetSizer()
                                ->GetItem(static_cast<std::size_t>(0))
                                ->GetSizer()
                                ->GetItem(static_cast<std::size_t>(0))
                                ->GetWindow() == widgets->advanced_tuning,
                        "Use advanced tuning must lead the disclosure header row");
            }
            const auto original = draft_.holes.at(static_cast<std::size_t>(hole_index_));
            const auto saved_profiles = controller_->snapshot().profiles;
            for (auto* widgets : {&forward_, &reverse_}) {
                for (bool enabled : {false, true}) {
                    widgets->advanced_tuning->SetValue(enabled);
                    tuning_changed(*widgets);
                    for (int selection : {1, 2, 0}) {
                        const auto sibling = widgets == &forward_
                                                 ? draft_.holes[hole_index_].reverse
                                                 : draft_.holes[hole_index_].forward;
                        widgets->mode->SetSelection(selection);
                        wxCommandEvent mode(wxEVT_CHOICE, widgets->mode->GetId());
                        widgets->mode->GetEventHandler()->ProcessEvent(mode);
                        const auto& edited = widgets == &forward_
                                                 ? draft_.holes[hole_index_].forward
                                                 : draft_.holes[hole_index_].reverse;
                        const auto expected =
                            selection == 0 ? std::optional<std::string>{}
                                           : std::optional<std::string>(selection == 1 ? "Exclusive"
                                                                                       : "Shared");
                        require(widgets->mode->GetSelection() == selection &&
                                    edited.settings.preferences.common.mode == expected &&
                                    edited.advanced_tuning == enabled,
                                "Operating mode edit was discarded or changed tuning eligibility");
                        require((widgets == &forward_
                                     ? draft_.holes[hole_index_].reverse
                                     : draft_.holes[hole_index_].forward) == sibling,
                                "Operating mode edit changed the sibling direction");
                    }
                }
            }
            require(controller_->snapshot().profiles == saved_profiles,
                    "Editing operating mode silently saved a profile");
            tuning_timer_.Stop();
            draft_.holes[hole_index_] = original;
            load_hole(hole_index_);
            const auto revision = controller_->snapshot().revision;
            const auto occurrence = occurrence_;
            const auto before = draft_.holes.at(static_cast<std::size_t>(hole_index_));
            const auto window_size = GetSize();
            auto disclose = [&](DirectionWidgets& widgets, bool expanded) {
                widgets.tuning->Collapse(!expanded);
                wxCollapsiblePaneEvent event(widgets.tuning, widgets.tuning->GetId(), !expanded);
                widgets.tuning->GetEventHandler()->ProcessEvent(event);
                require(GetSize() == window_size, "Direction disclosure resized the window");
            };
            for (auto* widgets : {&forward_, &reverse_}) {
                for (auto* section : widgets->sections)
                    require(section->IsCollapsed(),
                            "Advanced tuning subsection must initially be collapsed");
                for (auto* section : widgets->sections) {
                    section->Expand();
                    wxCollapsiblePaneEvent event(section, section->GetId(), false);
                    section->GetEventHandler()->ProcessEvent(event);
                    require(GetSize() == window_size,
                            "Advanced subsection resized the application window");
                }
            }
            disclose(forward_, true);
            for (auto* control :
                 std::vector<wxWindow*>{forward_.rate, forward_.channels, forward_.source_channels,
                                        forward_.destination_channels, forward_.representation,
                                        forward_.channel_map, forward_.buffer})
                require(control->GetSize().y == forward_.format->GetSize().y,
                        "Advanced format/buffer field height differs from dropdown height");
            for (auto* control : forward_.timings)
                if (control)
                    require(control->GetSize().y == forward_.format->GetSize().y,
                            "Advanced timing field height differs from dropdown height");
            for (auto* direction : {&forward_, &reverse_}) {
                for (auto* control : std::vector<wxWindow*>{
                         direction->rate, direction->channels, direction->source_channels,
                         direction->destination_channels, direction->channel_map,
                         direction->buffer})
                    check_centered_input(control);
                for (auto* control : direction->timings)
                    if (control)
                        check_centered_input(control);
            }
            {
                const auto original = draft_.holes[hole_index_].forward;
                forward_.rate->SetValue("44100");
                require(forward_.format->GetStringSelection() == "Manual",
                        "Typing a partial format override did not select Manual");
                forward_.format->SetSelection(1);
                wxCommandEvent preset(wxEVT_CHOICE, forward_.format->GetId());
                forward_.format->GetEventHandler()->ProcessEvent(preset);
                require(forward_.rate->GetValue() == "44100",
                        "Selecting Manual discarded typed format values");
                capture_draft();
                const auto edited = draft_.holes[hole_index_].forward;
                populate_direction(forward_, edited);
                require(forward_.format->GetStringSelection() == "Manual",
                        "Refreshing a partial format override reverted Manual to Automatic");
                forward_.format->SetSelection(0);
                forward_.format->GetEventHandler()->ProcessEvent(preset);
                capture_draft();
                require(
                    !draft_.holes[hole_index_].forward.settings.preferences.common.sample_rate_hz &&
                        draft_.holes[hole_index_].forward.settings.preferences.channel_map ==
                            original.settings.preferences.channel_map,
                    "Automatic did not clear format overrides or changed the explicit map");
                draft_.holes[hole_index_].forward = original;
                forward_.manual_format = false;
                populate_direction(forward_, original);
            }
            require(forward_.buffer->GetSize().y == forward_.buffer_apply->GetSize().y &&
                        forward_.buffer->GetPosition().y == forward_.buffer_apply->GetPosition().y,
                    "Buffer input and Apply button must have matching heights and alignment");
            require(reverse_.tuning->IsCollapsed() && !forward_.advanced_tuning->GetValue(),
                    "Opening Forward tuning opened Return or changed eligibility");
            wxCommandEvent toggle(wxEVT_CHECKBOX, forward_.advanced_tuning->GetId());
            auto choose = [&](bool enabled) {
                forward_.advanced_tuning->SetValue(enabled);
                forward_.advanced_tuning->GetEventHandler()->ProcessEvent(toggle);
                reverse_.advanced_tuning->SetValue(enabled);
                wxCommandEvent return_toggle(wxEVT_CHECKBOX, reverse_.advanced_tuning->GetId());
                reverse_.advanced_tuning->GetEventHandler()->ProcessEvent(return_toggle);
                require(!forward_.tuning->IsCollapsed() && reverse_.tuning->IsCollapsed() &&
                            GetSize() == window_size,
                        "Tuning eligibility changed disclosure or frame geometry");
            };
            forward_.advanced_tuning->SetValue(true);
            forward_.advanced_tuning->GetEventHandler()->ProcessEvent(toggle);
            require(draft_.holes.at(hole_index_).forward.advanced_tuning &&
                        !draft_.holes.at(hole_index_).reverse.advanced_tuning,
                    "Forward tuning checkbox changed the Return policy");
            forward_.exclusive_preferred->SetValue(true);
            wxCommandEvent exclusive(wxEVT_CHECKBOX, forward_.exclusive_preferred->GetId());
            forward_.exclusive_preferred->GetEventHandler()->ProcessEvent(exclusive);
            require(draft_.holes.at(hole_index_).forward.exclusive_mode_preferred &&
                        !draft_.holes.at(hole_index_).reverse.exclusive_mode_preferred,
                    "Forward exclusive preference changed Return");
            forward_.exclusive_preferred->SetValue(false);
            forward_.exclusive_preferred->GetEventHandler()->ProcessEvent(exclusive);
            choose(true);
            forward_.buffer->SetValue(19);
            reverse_.buffer->SetValue(23);
            reverse_.group->ShowItems(true);
            reverse_.tuning_group->ShowItems(true);
            for (int pass = 0; pass < 3; ++pass) {
                for (auto* widgets : {&forward_, &reverse_}) {
                    widgets->tuning->Expand();
                    layout_direction(*widgets);
                    for (auto* input : std::vector<wxWindow*>{
                             widgets->rate, widgets->channels, widgets->source_channels,
                             widgets->destination_channels, widgets->channel_map, widgets->buffer})
                        check_centered_input(input);
                    for (auto* input : widgets->timings)
                        if (input)
                            check_centered_input(input);
                    if (widgets->clock->GetSize().y < widgets->mode->GetBestSize().y ||
                        widgets->clock->GetSize().y < widgets->clock->GetCharHeight() + FromDIP(12))
                        throw std::runtime_error(
                            "Clock dropdown height: " + utf8(widgets->clock->GetName()) +
                            " actual=" + std::to_string(widgets->clock->GetSize().y) +
                            " best=" + std::to_string(widgets->clock->GetBestSize().y) +
                            " mode=" + std::to_string(widgets->mode->GetSize().y) +
                            " pane=" + std::to_string(widgets->tuning->GetPane()->GetSize().y) +
                            " font=" + std::to_string(widgets->clock->GetCharHeight()));
                    widgets->tuning->Collapse();
                    layout_direction(*widgets);
                }
            }
            reverse_.group->ShowItems(before.duplex);
            reverse_.tuning_group->ShowItems(before.duplex);
            disclose(forward_, true);
            disclose(reverse_, false);
            forward_.clock->SetSelection(1);
            reverse_.clock->SetSelection(2);
            capture_draft();
            choose(false);
            require(draft_.holes.at(hole_index_).forward.settings.buffer_ms == 19 &&
                        draft_.holes.at(hole_index_).reverse.settings.buffer_ms == 23 &&
                        draft_.holes.at(hole_index_)
                                .forward.settings.preferences.preferred_clock_device == "capture" &&
                        draft_.holes.at(hole_index_)
                                .reverse.settings.preferences.preferred_clock_device == "playback",
                    "Disabling tuning discarded independent directional choices");
            choose(true);
            require(controller_->snapshot().revision == revision && occurrence_ == occurrence,
                    "Tuning changed operational state or emitted immediate commentary");
            tuning_timer_.Stop();
            capture_draft();
            const auto retained = draft_.holes.at(hole_index_);
            disclose(forward_, false);
            disclose(reverse_, true);
            capture_draft();
            require(draft_.holes.at(hole_index_) == retained && forward_.tuning->IsCollapsed(),
                    "Directional disclosure changed saved intent or its sibling panel");
            disclose(reverse_, false);
            draft_.holes.at(hole_index_) = before;
            load_hole(hole_index_);
            forward_.advanced_tuning->SetValue(true);
            forward_.advanced_tuning->GetEventHandler()->ProcessEvent(toggle);
            reverse_.advanced_tuning->SetValue(true);
            wxCommandEvent return_toggle(wxEVT_CHECKBOX, reverse_.advanced_tuning->GetId());
            reverse_.advanced_tuning->GetEventHandler()->ProcessEvent(return_toggle);
            tuning_timer_.Stop();
            for (auto* widgets : {&forward_, &reverse_}) {
                for (auto* section : widgets->sections)
                    section->Collapse();
                layout_direction(*widgets);
            }
        };
        auto decline_delete = [&](wxListBox* list, wxButton* remove = nullptr) {
            capture_draft();
            class Decline final : public wxModalDialogHook {
              public:
                bool asked = false;
                int Enter(wxDialog* dialog) override {
                    if (dialog->GetTitle() == "Confirm profile deletion" ||
                        dialog->GetTitle() == "Confirm hole removal") {
                        asked = true;
                        return wxID_NO;
                    }
                    return wxID_NONE;
                }
            } hook;
            hook.Register();
            const auto before = controller_->snapshot().profiles;
            const auto draft = draft_;
            if (remove) {
                wxCommandEvent click(wxEVT_BUTTON, remove->GetId());
                remove->GetEventHandler()->ProcessEvent(click);
            } else {
                wxKeyEvent key(wxEVT_KEY_DOWN);
                key.m_keyCode = WXK_DELETE;
                key.SetEventObject(list);
                list->GetEventHandler()->ProcessEvent(key);
            }
            require(hook.asked && controller_->snapshot().profiles == before && draft_ == draft,
                    "Delete controls must use confirmation and preserve state when declined");
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
        require(scan_scope_->GetCount() == 2 &&
                    scan_scope_->FindString("Selected device") == wxNOT_FOUND,
                "Scanning page still offers per-device scope");
        const bool persistent_folders =
            controller_->snapshot().operating_mode == OperatingMode::Test;
        require(profiles_folder_->IsEnabled() == persistent_folders &&
                    devices_folder_->IsEnabled() == persistent_folders,
                "Data-folder actions must match the active persistent Test environment");
        require(profile_remove_->GetLabel() == wxS("−") && hole_remove_->GetLabel() == wxS("−") &&
                    profile_remove_->GetName() == "Delete profile" &&
                    hole_remove_->GetName() == "Delete hole",
                "Minus controls must have readable functional names");
        {
            auto* page = notebook_->GetPage(2);
            page->Layout();
            scan_output_split_->UpdateSize();
            const auto text = scan_state_->GetValue();
            const auto size = scan_state_->GetSize();
            const auto console_size = console_->GetSize();
            scan_state_->ChangeValue("One line");
            page->Layout();
            scan_state_->ChangeValue("First line\nSecond line\nThird line");
            page->Layout();
            require(scan_state_->GetSize() == size && console_->GetSize() == console_size,
                    "Scan status changes resized the console");
            scan_state_->ChangeValue(text);
            const auto window_size = GetSize();
            require(performance_panel_->GetParent() == profile_editor_->GetParent() &&
                        performance_panel_->GetParent() != profile_editor_,
                    "Performance diagnostics must be outside the scrolling hole editor");
            performance_split_->UpdateSize();
            const int sash = performance_split_->GetSashPosition();
            performance_split_->SetSashPosition(sash - FromDIP(24));
            require(performance_split_->GetSashPosition() != sash,
                    "Performance diagnostics divider cannot resize the pinned panel");
            performance_split_->SetSashPosition(sash);
            const auto position = performance_panel_->GetPosition();
            profile_editor_->Scroll(0, 100);
            profile_editor_->GetParent()->Layout();
            require(performance_panel_->GetPosition() == position && GetSize() == window_size,
                    "Scrolling the hole editor moved the pinned diagnostics panel");
            profile_editor_->Scroll(0, 0);
        }
        {
            auto* deep = wxWindow::FindWindowByName("Deep scan settings", this);
            auto* layout = scan_start_->GetParent()->GetSizer();
            int actions_position = -1, settings_position = -1;
            for (unsigned i = 0; i < layout->GetItemCount(); ++i) {
                const auto* item = layout->GetItem(i);
                if (item->GetSizer() == scan_start_->GetContainingSizer())
                    actions_position = static_cast<int>(i);
                if (item->GetWindow() == deep)
                    settings_position = static_cast<int>(i);
            }
            require(actions_position >= 0 && settings_position > actions_position,
                    "Scan actions must precede Deep settings");
            const int selected = hole_index_;
            load_hole(-1);
            require(hole_notes_->IsEnabled() && !hole_notes_->IsEditable() &&
                        !forward_.source->IsEnabled() && !forward_.buffer_apply->IsEnabled() &&
                        forward_.source_detail->IsEnabled() && forward_.information->IsEnabled() &&
                        forward_.group->GetStaticBox()->IsEnabled(),
                    "Empty hole editor must keep themed notes and crisp labels without enabling "
                    "inputs");
            Direction unknown_mode;
            unknown_mode.settings.mode = "Future mode (unrecognized)";
            populate_direction(forward_, unknown_mode);
            Direction retained_mode;
            forward_.advanced_tuning->SetValue(
                true); // Explicitly read manual inputs for this check.
            read_direction(forward_, retained_mode);
            require(!retained_mode.settings.preferences.common.mode &&
                        retained_mode.settings.mode == "Automatic",
                    "A transient legacy mode overrode current inherited preferences");
            load_hole(selected);
        }
        require(device_evidence_->HasFlag(wxTE_WORDWRAP) &&
                    !device_evidence_->HasFlag(wxTE_DONTWRAP),
                "Device evidence does not wrap");
        deep_duration_->editor()->ChangeValue("2.001");
        require(deep_duration_->milliseconds() == 2001,
                "Fractional seconds lost millisecond precision");
        deep_duration_->editor()->ChangeValue("2.0001");
        require(!deep_duration_->milliseconds(), "Excess seconds precision was silently rounded");
        deep_duration_->editor()->ChangeValue("2");
        const auto retained_technical = technical_copy_, retained_commentary = commentary_copy_;
        const auto warning_before_input = operational_warning_visible_;
        show_result({false, "Invalid numeric field fixture"});
        require(operational_warning_visible_ == warning_before_input &&
                    (warning_before_input ? technical_copy_ == retained_technical &&
                                                commentary_copy_ == retained_commentary
                                          : technical_copy_ == "Invalid numeric field fixture" &&
                                                !commentary_copy_.empty()),
                "Minor error lost its technical detail/personality response or replaced a major "
                "warning");
        wxCommandEvent continuous_event(wxEVT_CHECKBOX, loop_until_event_->GetId());
        loop_until_event_->SetValue(true);
        loop_until_event_->GetEventHandler()->ProcessEvent(continuous_event);
        require(preferences_->values().loop_until_event && !animation_loops_->IsEnabled() &&
                    !hold_expressions_->IsEnabled(),
                "Continuous playback did not override hold/count controls");
        loop_until_event_->SetValue(false);
        loop_until_event_->GetEventHandler()->ProcessEvent(continuous_event);
        require(!preferences_->values().loop_until_event && animation_loops_->IsEnabled() &&
                    hold_expressions_->IsEnabled(),
                "Disabling continuous playback did not restore hold/count controls");
        const auto check_profile_layout = [this, &require] {
            auto* page = notebook_->GetPage(0);
            page->Layout();
            require(dynamic_cast<wxScrolledWindow*>(profile_sidebar_) == nullptr,
                    "Profile column must not scroll as one section");
            require(page->GetClientRect().Contains(profile_actions_->GetRect()) &&
                        profile_actions_->GetScreenRect().GetBottom() <
                            profile_sidebar_->GetScreenRect().GetTop(),
                    "Profile save bar is clipped or overlaps the editor fields");
            for (auto* action : {profile_save_, profile_apply_, profile_revert_})
                require(action->IsShownOnScreen() &&
                            profile_actions_->GetClientRect().Contains(action->GetRect()),
                        "A profile save action is hidden or clipped");
            for (wxWindow* field : std::vector<wxWindow*>{profiles_, profile_notes_, holes_}) {
                if (!profile_sidebar_->IsDescendant(field) ||
                    !profile_sidebar_->GetScreenRect().Contains(field->GetScreenRect())) {
                    const auto sidebar = profile_sidebar_->GetScreenRect(),
                               bounds = field->GetScreenRect();
                    throw std::runtime_error(
                        "Profile field clipped: " + utf8(field->GetName()) +
                        "; client=" + std::to_string(GetClientSize().x) + "x" +
                        std::to_string(GetClientSize().y) +
                        "; sidebar=" + std::to_string(sidebar.x) + "," + std::to_string(sidebar.y) +
                        "," + std::to_string(sidebar.width) + "," + std::to_string(sidebar.height) +
                        "; field=" + std::to_string(bounds.x) + "," + std::to_string(bounds.y) +
                        "," + std::to_string(bounds.width) + "," + std::to_string(bounds.height) +
                        "; window-min=" + std::to_string(GetMinSize().y) +
                        "; rows-min=" + std::to_string(profile_rows_->GetMinSize().y) +
                        "; rows-height=" + std::to_string(profile_rows_->GetClientSize().y) +
                        "; outer-sash=" + std::to_string(profile_rows_->GetSashPosition()) +
                        "; lower-height=" + std::to_string(profile_lower_rows_->GetClientSize().y) +
                        "; lower-sash=" + std::to_string(profile_lower_rows_->GetSashPosition()));
                }
            }
        };
        auto check_reset_layout = [&] {
            const auto window = GetSize();
            const auto saved = controller_->snapshot().profiles;
            auto* reset = wxWindow::FindWindowByLabel("Reset layout to default", this);
            require(reset, "Settings is missing Reset layout to default");
            auto click_reset = [&] {
                wxCommandEvent event(wxEVT_BUTTON, reset->GetId());
                reset->GetEventHandler()->ProcessEvent(event);
                Layout();
            };
            click_reset();
            std::vector<int> positions;
            for (auto* split :
                 {profile_columns_, profile_rows_, profile_lower_rows_, performance_split_,
                  device_columns_, device_rows_, scan_output_split_}) {
                positions.push_back(split->GetSashPosition());
            }
            for (auto* split :
                 {profile_columns_, profile_rows_, profile_lower_rows_, performance_split_,
                  device_columns_, device_rows_, scan_output_split_})
                split->SetSashPosition(split->GetSashPosition() + FromDIP(16));
            click_reset();
            unsigned index = 0;
            for (auto* split :
                 {profile_columns_, profile_rows_, profile_lower_rows_, performance_split_,
                  device_columns_, device_rows_, scan_output_split_})
                if (std::abs(split->GetSashPosition() - positions[index++]) > 1)
                    throw std::runtime_error("Reset splitter " + utf8(split->GetName()) + ": " +
                                             std::to_string(positions[index - 1]) + " -> " +
                                             std::to_string(split->GetSashPosition()));
            require(GetSize() == window && controller_->snapshot().profiles == saved,
                    "Reset layout resized the window or changed saved profiles");
            auto lines = [&](wxTextCtrl* control, int count) {
                const int actual = control->GetClientSize().y,
                          target = count * control->GetCharHeight();
                if (actual < target || actual > target + FromDIP(24))
                    throw std::runtime_error("Default field height: " + utf8(control->GetName()) +
                                             " actual=" + std::to_string(actual) +
                                             " target=" + std::to_string(target));
            };
            lines(profile_notes_, 4);
            lines(device_notes_, 3);
            RECT text_area{};
            SendMessageW(static_cast<HWND>(device_notes_->GetHandle()), EM_GETRECT, 0,
                         reinterpret_cast<LPARAM>(&text_area));
            require(text_area.bottom - text_area.top >= 3 * device_notes_->GetCharHeight(),
                    "Device Notes does not fit three complete native edit rows");
            lines(path_status_, 2);
            require(std::abs(capture_devices_->GetParent()->GetScreenRect().GetRight() -
                             devices_folder_->GetScreenRect().GetRight()) <= 2 &&
                        std::abs(playback_devices_->GetParent()->GetScreenRect().GetRight() -
                                 devices_folder_->GetScreenRect().GetRight()) <= 2,
                    "Device panels do not align with the folder toolbar button by default");
        };
        layout_header();
        check_profile_layout();
        check_endpoint_heights();
        require(profile_new_->GetLabel() == "+" && hole_new_->GetLabel() == "+" &&
                    profile_new_->GetName() == "New Profile" && hole_new_->GetName() == "Add hole",
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
        // A short desktop can put both rows at their minimum sizes, leaving no
        // legal sash travel. Verify movement with deliberate spare space, then
        // restore the original compact layout and verify its bounds again.
        const auto workspace_client = GetClientSize();
        const std::array<wxSplitterWindow*, 6> dividers{profile_columns_,    profile_rows_,
                                                        profile_lower_rows_, device_columns_,
                                                        device_rows_,        scan_output_split_};
        std::array<int, 6> original_sashes{};
        for (std::size_t i = 0; i < dividers.size(); ++i)
            original_sashes[i] = dividers[i]->GetSashPosition();
        SetClientSize(FromDIP(wxSize(1000, 1000)));
        layout_header();
        profile_rows_->SetSashPosition(profile_rows_->GetClientSize().y / 3);
        profile_lower_rows_->SetSashPosition(
            std::max(FromDIP(72), profile_lower_rows_->GetClientSize().y -
                                      profile_notes_->GetCharHeight() * 3 - FromDIP(36)));
        auto move_divider = [this](wxSplitterWindow* split) {
            split->UpdateSize();
            const int before = split->GetSashPosition();
            split->SetSashPosition(before + FromDIP(12));
            // A legal minimum or maximum clamps movement in one direction.
            if (split->GetSashPosition() == before)
                split->SetSashPosition(before - FromDIP(12));
            if (split->GetSashPosition() == before)
                throw std::runtime_error(
                    utf8(split->GetName()) + " cannot be adjusted; sash=" + std::to_string(before) +
                    ", client=" + std::to_string(split->GetClientSize().x) + "x" +
                    std::to_string(split->GetClientSize().y) +
                    ", minimum pane=" + std::to_string(split->GetMinimumPaneSize()));
            return before;
        };
        auto check_blank_list = [&](wxListBox* list) {
            const auto handle = static_cast<HWND>(list->GetHandle());
            RECT last{};
            int begin = FromDIP(4);
            if (list->GetCount() && SendMessageW(handle, LB_GETITEMRECT, list->GetCount() - 1,
                                                 reinterpret_cast<LPARAM>(&last)) != LB_ERR)
                begin = std::max(begin, static_cast<int>(last.bottom) + FromDIP(3));
            // Paint pending native damage only. A forced full invalidation here
            // would erase the very stale frame trails this regression checks.
            UpdateWindow(handle);
            const auto dc = GetDC(handle);
            require(dc != nullptr, "Cannot inspect resized list client pixels");
            const auto colour = list->GetBackgroundColour();
            const auto expected = RGB(colour.Red(), colour.Green(), colour.Blue());
            bool clean = true;
            for (int y = begin; y < list->GetClientSize().y - FromDIP(4); ++y)
                clean = clean && GetPixel(dc, list->GetClientSize().x / 2, y) == expected;
            ReleaseDC(handle, dc);
            require(clean, "Resizing left border trails inside a profile/hole list");
        };
        for (auto* split : {profile_columns_, profile_rows_, profile_lower_rows_}) {
            const int before = move_divider(split);
            check_profile_layout();
            check_blank_list(profiles_);
            check_blank_list(holes_);
            split->SetSashPosition(before);
            check_blank_list(profiles_);
            check_blank_list(holes_);
        }
        require(profile_notes_->GetValue() == original_notes &&
                    controller_->snapshot().revision == overflow_revision,
                "Resizing profile fields changed editor or saved state");
        notebook_->SetSelection(1);
        notebook_->GetPage(1)->Layout();
        device_rows_->SetSashPosition(std::max(FromDIP(72), device_rows_->GetClientSize().y -
                                                                device_notes_->GetCharHeight() * 3 -
                                                                FromDIP(36)));
        for (auto* split : {device_columns_, device_rows_}) {
            const int before = move_divider(split);
            require(notebook_->GetPage(1)->GetScreenRect().Contains(device_save_->GetScreenRect()),
                    "Resizing device fields hid Save");
            split->SetSashPosition(before);
        }
        notebook_->SetSelection(2);
        notebook_->GetPage(2)->Layout();
        for (auto* control :
             std::vector<wxWindow*>{device_search_, device_name_, log_search_, delay_,
                                    animation_loops_, dfa_value_, dfa_stepper_})
            require(control->GetSize().y == theme_choice_->GetSize().y,
                    "Device/Scan/Settings editable field height differs from dropdown height");
        for (auto* control : std::vector<wxWindow*>{device_search_, device_name_, log_search_,
                                                    delay_, animation_loops_, dfa_value_})
            check_centered_input(control);
        {
            const auto original_status = scan_state_->GetValue();
            scan_output_split_->SetSashPosition(scan_output_split_->GetClientSize().y / 3);
            move_divider(scan_output_split_);
            const auto chosen_sash = scan_output_split_->GetSashPosition();
            const auto status_size = scan_state_->GetSize();
            const auto console_size = console_->GetSize();
            const wxString long_status = "Scan status detail " + wxString('x', 600);
            scan_state_->ChangeValue("Short status");
            notebook_->GetPage(2)->Layout();
            scan_state_->ChangeValue(long_status + "\nSecond line\nThird line\nFourth line");
            notebook_->GetPage(2)->Layout();
            require(scan_state_->GetValue().Contains(long_status) &&
                        scan_output_split_->GetSashPosition() == chosen_sash &&
                        scan_state_->GetSize() == status_size &&
                        console_->GetSize() == console_size,
                    "Scan status lost details or changed the chosen status/log heights");
            scan_state_->ChangeValue(original_status);
        }
        {
            auto* settings = notebook_->GetPage(3);
            auto* appearance =
                wxWindow::FindWindowByName("Application and Tray settings", settings);
            auto* startup = wxWindow::FindWindowByName("Routing Activation settings", settings);
            auto* personality =
                wxWindow::FindWindowByName("Personality and Animation settings", settings);
            auto* advanced = wxWindow::FindWindowByName("Advanced settings", settings);
            require(appearance && startup && personality && advanced &&
                        theme_choice_->GetParent() == appearance &&
                        notifications_->GetParent() == appearance &&
                        startup_->GetParent() == startup && delay_->GetParent() == startup &&
                        hold_expressions_->GetParent() == personality &&
                        animation_loops_->GetParent() == personality,
                    "Settings controls escaped their functional groups");
            auto* layout = settings->GetSizer();
            require(layout->GetItemCount() == 3 && layout->GetItem(std::size_t{0})->IsSizer(),
                    "Settings no longer starts with the two-column layout");
            auto* columns =
                dynamic_cast<wxFlexGridSizer*>(layout->GetItem(std::size_t{0})->GetSizer());
            require(
                columns && columns->GetCols() == 2 && columns->GetItemCount() == 2 &&
                    personality->GetContainingSizer() == layout->GetItem(1U)->GetSizer() &&
                    advanced->GetContainingSizer() == layout->GetItem(2U)->GetSizer() &&
                    wxWindow::FindWindowByName("Restart AudioHole", settings)->GetParent() ==
                        appearance &&
                    wxWindow::FindWindowByLabel("Preview Build environment", settings) == nullptr &&
                    wxWindow::FindWindowByLabel("Preview Mode (load preview profiles and devices)",
                                                settings) == nullptr,
                "Settings must contain Personality and Advanced, with Restart in Application and "
                "Tray, without the "
                "environment panel");
            const auto previous_page = notebook_->GetSelection();
            notebook_->SetSelection(3);
            settings->Layout();
            auto* update = wxWindow::FindWindowByLabel("Update", appearance);
            auto* restart = wxWindow::FindWindowByName("Restart AudioHole", appearance);
            require(update && restart && update->IsShownOnScreen() &&
                        update->GetContainingSizer() == restart->GetContainingSizer() &&
                        update->GetScreenRect().GetLeft() > restart->GetScreenRect().GetRight(),
                    "Update must appear beside Restart AudioHole");
            if (!(dfa_slider_->GetContainingSizer() == dfa_value_->GetContainingSizer() &&
                  field_screen_rect(*dfa_slider_).GetLeft() >
                      field_screen_rect(*dfa_stepper_).GetRight() &&
                  std::abs(field_screen_rect(*dfa_slider_).y + dfa_slider_->GetSize().y / 2 -
                           field_screen_rect(*dfa_value_).y - dfa_value_->GetSize().y / 2) <=
                      FromDIP(1))) {
                auto geometry = [](wxWindow* control) {
                    const auto rect = field_screen_rect(*control);
                    return std::to_string(rect.x) + "," + std::to_string(rect.y) + "," +
                           std::to_string(rect.width) + "," + std::to_string(rect.height);
                };
                throw std::runtime_error("DFA row geometry: slider=" + geometry(dfa_slider_) +
                                         " editor=" + geometry(dfa_value_) +
                                         " stepper=" + geometry(dfa_stepper_) + " same sizer=" +
                                         std::to_string(dfa_slider_->GetContainingSizer() ==
                                                        dfa_value_->GetContainingSizer()));
            }
            class UpdateDialogCheck final : public wxModalDialogHook {
              public:
                bool seen = false;
                wxColour foreground;
                wxString capture_path;
                int Enter(wxDialog* dialog) override {
                    if (dialog->GetTitle() != "AudioHole Update")
                        return wxID_NONE;
                    seen = true;
                    auto* version = wxWindow::FindWindowByName("Current AudioHole version", dialog);
                    auto* link = dynamic_cast<wxHyperlinkCtrl*>(
                        wxWindow::FindWindowByName("AudioHole GitHub", dialog));
                    auto* close = dialog->FindWindow(wxID_CLOSE);
                    if (!version || !version->GetLabel().Contains(text(AUDIOHOLE_VERSION)) ||
                        !link || link->GetURL() != "https://github.com/RLCypher/audiohole" ||
                        !close || close->GetLabel() != "Close" ||
                        dialog->GetEscapeId() != wxID_CLOSE ||
                        version->GetForegroundColour() != foreground ||
                        close->GetForegroundColour() != foreground ||
                        link->GetNormalColour() != foreground)
                        throw std::runtime_error(
                            "Update dialog lost its version, link, Close action or theme");
                    dialog->CallAfter([dialog, close, path = capture_path] {
                        if (!path.empty()) {
                            wxFileName::Mkdir(wxFileName(path).GetPath(), wxS_DIR_DEFAULT,
                                              wxPATH_MKDIR_FULL);
                            dialog->Update();
                            wxBitmap bitmap(dialog->GetClientSize());
                            wxMemoryDC destination(bitmap);
                            wxClientDC source(dialog);
                            destination.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &source,
                                             0, 0);
                            destination.SelectObject(wxNullBitmap);
                            if (!bitmap.SaveFile(path, wxBITMAP_TYPE_BMP))
                                throw std::runtime_error(
                                    "Could not capture Update dialog evidence");
                        }
                        wxCommandEvent click(wxEVT_BUTTON, close->GetId());
                        close->GetEventHandler()->ProcessEvent(click);
                    });
                    return wxID_NONE;
                }
            } hook;
            const auto appearance_before = theme_choice_->GetSelection();
            for (int choice : {0, 1}) {
                theme_choice_->SetSelection(choice);
                wxCommandEvent theme(wxEVT_CHOICE, theme_choice_->GetId());
                theme_choice_->GetEventHandler()->ProcessEvent(theme);
                hook.foreground = choice == 0 ? *wxWHITE : *wxBLACK;
                hook.capture_path =
                    directory.empty()
                        ? wxString{}
                        : directory + (choice == 0 ? "/update-dark.bmp" : "/update-light.bmp");
                hook.seen = false;
                const auto before = controller_->snapshot().revision;
                const auto draft_before = draft_;
                hook.Register();
                wxCommandEvent click(wxEVT_BUTTON, update->GetId());
                update->GetEventHandler()->ProcessEvent(click);
                hook.Unregister();
                require(hook.seen && controller_->snapshot().revision == before &&
                            draft_ == draft_before,
                        "Update failed to open its dialog or changed routing/editor state");
            }
            theme_choice_->SetSelection(appearance_before);
            wxCommandEvent restore_theme(wxEVT_CHOICE, theme_choice_->GetId());
            theme_choice_->GetEventHandler()->ProcessEvent(restore_theme);
            notebook_->SetSelection(previous_page);
        }
        {
            const auto original_console = console_->GetValue();
            wxString lines;
            for (int i = 0; i < 200; ++i)
                lines += wxString::Format("Smoke console line %04d: incremental evidence\n", i);
            console_->ChangeValue("");
            update_console(lines);
            const auto handle = static_cast<HWND>(console_->GetHandle());
            const auto first_visible = [handle] {
                return static_cast<long>(SendMessageW(handle, EM_GETFIRSTVISIBLELINE, 0, 0));
            };
            const long tail = first_visible();
            require(tail > 50, "Console did not follow initial output to the bottom");
            SendMessageW(handle, EM_LINESCROLL, 0, -50);
            const long reading = first_visible();
            require(reading < tail, "Console could not scroll up to earlier evidence");
            lines += "Smoke console appended evidence\n";
            update_console(lines);
            require(first_visible() == reading, "New console output moved a scrolled-up reader");
            // Bounded logs can lose old lines. Preserve the same visible content,
            // including Windows CRLF/native-position differences.
            lines = lines.Mid(lines.Find("Smoke console line 0040:"));
            update_console(lines);
            require(first_visible() == reading - 40,
                    "Console lost its reading position when old log lines were evicted");
            console_->ShowPosition(console_->GetLastPosition());
            lines += "Smoke console resumed follow\n";
            update_console(lines);
            require(first_visible() > reading - 40,
                    "Returning to the console bottom did not resume follow");
            console_->ChangeValue(original_console);
            console_->ShowPosition(console_->GetLastPosition());
        }
        notebook_->SetSelection(0);
        SetClientSize(workspace_client);
        layout_header();
        for (std::size_t i = 0; i < dividers.size(); ++i)
            dividers[i]->SetSashPosition(original_sashes[i]);
        check_profile_layout();
        // Check both environments before the Test-only branch returns. Include
        // the control identity so future omissions are immediately actionable.
        const auto check_button_help = [&](const auto& self, wxWindow* window) -> void {
            if (dynamic_cast<wxButton*>(window) &&
                (!window->GetToolTip() || window->GetToolTip()->GetTip().empty()))
                throw std::runtime_error("Button lacks explanatory hover help: label='" +
                                         utf8(window->GetLabel()) + "', name='" +
                                         utf8(window->GetName()) + "'");
            for (auto* child : window->GetChildren())
                self(self, child);
        };
        check_button_help(check_button_help, this);
        if (controller_->snapshot().operating_mode == OperatingMode::Test) {
            require(controller_->snapshot().profiles.empty() &&
                        controller_->snapshot().endpoints.empty(),
                    "Test Mode leaked preview fixtures");
            require(!start_->IsEnabled() && controller_->snapshot().capabilities.scanning &&
                        !forward_.playback_check.action->IsEnabled() &&
                        !forward_.capture_check.action->IsEnabled() &&
                        wxWindow::FindWindowByLabel("Arm Ready", this) == nullptr,
                    "Test Mode enabled unsupported hardware controls");
            require(wxWindow::FindWindowByLabel("Forward failure", this) == nullptr &&
                        wxWindow::FindWindowByLabel("Return failure", this) == nullptr &&
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
            exercise_tuning();
            rename_hole("Persistent named hole");
            save(false);
            {
                const auto original_notes = profile_notes_->GetValue();
                const auto original_revision = controller_->snapshot().revision;
                profile_notes_->AppendText("\nStaged note");
                require(controller_->snapshot().revision == original_revision &&
                            profile_notes_->HasFlag(wxTE_MULTILINE) &&
                            hole_notes_->HasFlag(wxTE_MULTILINE),
                        "Editing notes unexpectedly saved or lost multiline input");
                forward_.rate->SetValue("44100");
                wxCommandEvent enter(wxEVT_TEXT_ENTER, forward_.rate->GetId());
                forward_.rate->GetEventHandler()->ProcessEvent(enter);
                wxTheApp->ProcessPendingEvents();
                const auto* saved = profile(controller_->snapshot(), viewed_);
                require(
                    saved &&
                        saved->holes.front().forward.settings.preferences.common.sample_rate_hz ==
                            44100 &&
                        !dirty_,
                    "Enter did not save profile tuning and staged notes");
                profile_notes_->ChangeValue(original_notes);
                forward_.rate->SetValue("");
                forward_.rate->GetEventHandler()->ProcessEvent(enter);
                wxTheApp->ProcessPendingEvents();
                require(!profile(controller_->snapshot(), viewed_)
                                ->holes.front()
                                .forward.settings.preferences.common.sample_rate_hz &&
                            !dirty_,
                        "Enter did not save an inherited Automatic value");
            }
            decline_delete(profiles_);
            decline_delete(holes_);
            decline_delete(profiles_, profile_remove_);
            decline_delete(holes_, hole_remove_);
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
                std::make_unique<ProfileDirectoryPersistence>(preferences_->test_configuration()));
            const auto* loaded = audiohole::profile(restarted.snapshot(), saved_id);
            require(loaded && loaded->display_name == "Restart persistence check" &&
                        loaded->holes.front().id == persistent_hole_id &&
                        loaded->holes.front().display_name == "Persistent named hole" &&
                        loaded->holes.front().forward.settings.buffer_ms == 14 &&
                        loaded->notes == "Native GUI Test data, saved without a routing backend.",
                    "Test buffer-only save did not persist or saved unrelated draft edits");
            require(!profile_apply_->IsEnabled(),
                    "Test Mode enabled Apply after creating a profile");
            check_endpoint_heights();
            check_reset_layout();
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
        for (const bool capture : {true, false}) {
            const auto found = std::find_if(
                controller_->snapshot().endpoints.begin(), controller_->snapshot().endpoints.end(),
                [capture](const auto& e) { return e.capture == capture; });
            require(found != controller_->snapshot().endpoints.end(),
                    "Device rename needs both directions");
            const auto original = *found;
            auto rename_device = [&](const std::string& name, bool cancel) {
                class ConfirmDevice final : public wxModalDialogHook {
                  public:
                    std::string name;
                    bool cancel = false, asked = false;
                    int Enter(wxDialog* dialog) override {
                        auto* edit = dynamic_cast<wxTextEntryDialog*>(dialog);
                        if (!edit || edit->GetTitle() != "Rename device")
                            return wxID_NONE;
                        asked = true;
                        edit->SetValue(text(name));
                        return cancel ? wxID_CANCEL : wxID_OK;
                    }
                } hook;
                hook.name = name;
                hook.cancel = cancel;
                hook.Register();
                auto menu = device_menu(original.id);
                require(menu && menu->FindItem(wxID_EDIT) && menu->IsEnabled(wxID_EDIT),
                        "Device context menu lacks Rename");
                wxCommandEvent event(wxEVT_MENU, wxID_EDIT);
                menu->ProcessEvent(event);
                hook.Unregister();
                require(hook.asked, "Device context Rename did not open its native dialog");
            };
            rename_device("Cancelled device rename", true);
            const auto* cancelled = endpoint(controller_->snapshot(), original.id);
            require(cancelled && cancelled->display_name == original.display_name &&
                        cancelled->windows_name == original.windows_name &&
                        cancelled->notes == original.notes &&
                        cancelled->capture == original.capture,
                    "Cancelling Rename changed a device");
            rename_device(original.display_name + " renamed", false);
            const auto* saved = endpoint(controller_->snapshot(), original.id);
            require(saved && saved->display_name == original.display_name + " renamed" &&
                        saved->windows_name == original.windows_name &&
                        saved->notes == original.notes && saved->capture == original.capture,
                    "Device Rename changed identity/notes or did not save the selected record");
            rename_device(original.display_name, false);
        }

        {
            const auto* selected = endpoint(controller_->snapshot(), device_id_);
            require(selected != nullptr, "Enter-save device fixture missing");
            const auto original = *selected;
            device_name_->SetValue("Enter-save device fixture");
            wxCommandEvent enter(wxEVT_TEXT_ENTER, device_name_->GetId());
            device_name_->GetEventHandler()->ProcessEvent(enter);
            wxTheApp->ProcessPendingEvents();
            const auto* saved = endpoint(controller_->snapshot(), original.id);
            require(saved && saved->display_name == "Enter-save device fixture" &&
                        saved->windows_name == original.windows_name && !device_dirty_,
                    "Enter failed to save a device name or changed identity");
            device_name_->SetValue(text(original.display_name));
            device_name_->GetEventHandler()->ProcessEvent(enter);
            wxTheApp->ProcessPendingEvents();
            require(endpoint(controller_->snapshot(), original.id)->display_name ==
                        original.display_name,
                    "Enter-save device fixture did not restore its name");
        }
        decline_delete(profiles_);
        decline_delete(holes_);
        decline_delete(profiles_, profile_remove_);
        decline_delete(holes_, hole_remove_);
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
        for (auto* tab : navigation_)
            require(tab->GetToolTip() == nullptr, "Navigation must not have generic hover text");
        require(!forward_.buffer_apply->IsEnabled() && !reverse_.buffer_apply->IsEnabled(),
                "Unchanged buffer preferences must keep Apply disabled");
        exercise_tuning();
        save(false);
        require(!profile_save_->IsEnabled(), "Unchanged saved profile must keep Save disabled");
        const auto saved_notes = profile_notes_->GetValue();
        profile_notes_->SetValue(saved_notes + " edited");
        refresh();
        require(profile_save_->IsEnabled(), "Editing profile notes did not enable Save");
        profile_notes_->SetValue(saved_notes);
        refresh();
        require(!profile_save_->IsEnabled(), "Undoing a profile edit left Save enabled");
        require(console_->GetToolTip() == nullptr, "Diagnostic log must not have hover text");
        auto* beep_button = forward_.playback_check.action;
        refresh();
        require(forward_.playback_check.target == "vr-out" && beep_button->IsEnabled(),
                "Forward playback check must target its assigned endpoint");
        const auto beep_width = beep_button->GetSize().x;
        wxCommandEvent beep_click(wxEVT_BUTTON, beep_button->GetId());
        beep_button->GetEventHandler()->ProcessEvent(beep_click);
        require(controller_->snapshot().beep_destination == "vr-out" &&
                    beep_button->GetLabel() == "Cancel beep",
                "Playback card did not start the exact assigned endpoint");
        require(!forward_.capture_check.action->IsEnabled(),
                "Another diagnostic must wait for the current check to finish");
        beep_button->GetEventHandler()->ProcessEvent(beep_click);
        require(controller_->snapshot().beep_destination.empty() &&
                    beep_button->GetLabel() == "Test beep" &&
                    beep_button->GetSize().x == beep_width,
                "Cancel beep failed or changed the button width");
        beep_button->GetEventHandler()->ProcessEvent(beep_click);
        controller_->advance(1000);
        refresh();
        require(controller_->snapshot().beep_destination.empty() &&
                    beep_button->GetLabel() == "Test beep",
                "Natural beep completion left the check active");
        auto* capture_button = forward_.capture_check.action;
        wxCommandEvent capture_click(wxEVT_BUTTON, capture_button->GetId());
        capture_button->GetEventHandler()->ProcessEvent(capture_click);
        require(controller_->snapshot().monitoring &&
                    controller_->snapshot().monitor_source ==
                        draft_.holes[hole_index_].forward.source &&
                    forward_.capture_check.meter->GetValue() > 0,
                "Capture check failed to publish the explicit source and simulated level");
        select_page(1, true);
        require(!controller_->snapshot().monitoring &&
                    controller_->snapshot().beep_destination.empty(),
                "Changing tabs left a device diagnostic running");
        const auto previous_device = device_id_;
        require(select_device("mixer-in"), "Could not select the device capture test target");
        wxCommandEvent device_capture(wxEVT_BUTTON, device_check_.action->GetId());
        device_check_.action->GetEventHandler()->ProcessEvent(device_capture);
        require(controller_->snapshot().monitor_source == "mixer-in" &&
                    device_check_.meter->IsShown(),
                "Devices capture check did not use its selected endpoint");
        select_page(2, true);
        require(!controller_->snapshot().monitoring,
                "Leaving Devices for Scanning kept capture open");
        select_page(1, true);
        require(select_device("vr-out"), "Could not select the device playback test target");
        device_check_.action->GetEventHandler()->ProcessEvent(device_capture);
        require(
            controller_->snapshot().beep_destination == "vr-out" && !device_check_.meter->IsShown(),
            "Devices playback check did not use its selected endpoint or retained capture meter");
        select_page(3, true);
        require(controller_->snapshot().beep_destination.empty(),
                "Leaving Devices for Settings kept playback open");
        select_device(previous_device);
        select_page(0, true);
        show_result({false, "Operational warning retained."}, true);
        const auto warning_occurrence = occurrence_;
        const auto warning_commentary = commentary_copy_;
        present(presentation::Message::ErrorMinor);
        require(technical_copy_ == "Operational warning retained." &&
                    commentary_copy_ == warning_commentary && occurrence_ == warning_occurrence &&
                    operational_warning_visible_,
                "A minor error displaced an unresolved operational failure");
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
        {
            const auto revision = controller_->snapshot().revision;
            require(show_onboarding_greeting(OnboardingOutcome::Completed) &&
                        personality_ == "default" && technical_copy_.empty() &&
                        commentary_copy_.find("Training wheels are off") != std::string::npos &&
                        !tutorial_choices_panel_,
                    "Norm completion greeting is missing or introduced tutorial controls");
            select_personality("uwu");
            require(!show_onboarding_greeting(OnboardingOutcome::Completed) &&
                        personality_ == "uwu",
                    "Completion greeting switched a saved Horu personality to Norm");
            require(show_onboarding_greeting(OnboardingOutcome::Skipped) &&
                        commentary_copy_.starts_with("Ehehe~") && personality_ == "uwu",
                    "Horu skip greeting did not match the working personality");
            select_personality("vanilla");
            require(!show_onboarding_greeting(OnboardingOutcome::Skipped),
                    "Vanilla displayed an unauthored onboarding greeting");
            select_personality("default");
            require(show_onboarding_greeting(OnboardingOutcome::Skipped) &&
                        commentary_copy_.starts_with("Oh, skipping the tutorial?"),
                    "Norm skip greeting did not match the working personality");
            require(controller_->snapshot().revision == revision,
                    "Working greeting issued a controller command");
            present(presentation::Message::Ready);
        }
        // Exercise the real sizers: changing status text must never resize or
        // move the commentary card or reduce the full-height avatar.
        const auto initial_client = GetClientSize();
        const auto initial_status = status_copy_;
        const auto resize_occurrence = occurrence_;
        SetSize(GetMinSize());
        layout_header();
        check_profile_layout();
        wxSize previous_card_size = wxDefaultSize;
        int previous_header_width = 0;
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
            require(card.GetHeight() >= FromDIP(156) && card.GetHeight() < FromDIP(240),
                    "Operation-mode commentary card lost its compact bounded height");
            require(navigation_[0]->GetPosition().y - card.GetBottom() <= FromDIP(16),
                    "Empty header space remains between the personality card and tabs");
            const auto* exit_button = routing_actions_->GetChildren().back()->GetWindow();
            require(card.GetLeft() == start_->GetRect().GetLeft() &&
                        card.GetRight() == exit_button->GetRect().GetRight(),
                    "Commentary card does not match the routing button row edges");
            if (previous_header_width && GetClientSize().x > previous_header_width)
                require(card.GetWidth() > previous_card_size.x,
                        "A wider window did not widen the personality panel");
            if (previous_header_width && GetClientSize().x < previous_header_width)
                require(card.GetWidth() < previous_card_size.x,
                        "A narrower window did not resize the personality panel");
            previous_header_width = GetClientSize().x;
            previous_card_size = card.GetSize();
            const auto avatar = wxRect(ScreenToClient(agent_display_->GetScreenPosition()),
                                       agent_display_->GetSize());
            const auto region = wxRect(ScreenToClient(avatar_region_->GetScreenPosition()),
                                       avatar_region_->GetSize());
            require(previous_avatar_size == wxDefaultSize ||
                        avatar.GetSize() == previous_avatar_size,
                    "Window resizing reduces the full-height avatar");
            previous_avatar_size = avatar.GetSize();
            require(avatar.GetTop() == region.GetTop() &&
                        avatar.GetHeight() == region.GetHeight() &&
                        avatar.GetTop() < start_->GetPosition().y &&
                        navigation_[0]->GetPosition().y - avatar.GetBottom() <= FromDIP(16),
                    "Operation-mode avatar does not occupy the full compact header height");
            if (static_cast<long long>(avatar.GetWidth()) * 768 <
                static_cast<long long>(avatar.GetHeight()) * 512)
                throw std::runtime_error("Avatar width constrains its rendered height: avatar=" +
                                         std::to_string(avatar.width) + "x" +
                                         std::to_string(avatar.height) +
                                         ", region=" + std::to_string(region.width) + "x" +
                                         std::to_string(region.height));
            require(region.GetLeft() - card.GetRight() - 1 == FromDIP(8) + FromDIP(6),
                    "Avatar region no longer follows the widened personality panel");
            require(std::abs(avatar.GetLeft() * 2 + avatar.GetWidth() - region.GetLeft() * 2 -
                             region.GetWidth()) <= 1,
                    "Avatar is not centered in its right-hand region");
            require(card.GetRight() <= region.GetLeft() && avatar.GetRight() < GetClientSize().x &&
                        avatar.GetBottom() < navigation_[0]->GetPosition().y,
                    "Header regions overlap or clip the avatar");
            require(personality_heading_->GetPosition().x >= FromDIP(12) &&
                        personality_heading_->GetPosition().y >= FromDIP(12),
                    "Personality heading covers the rounded corner");
        }
        {
            select_page(0);
            const auto information = forward_.information->GetValue();
            const auto view = profile_editor_->GetViewStart();
            const auto revision = controller_->snapshot().revision;
            auto* field = forward_.information;
            wxString wheel_fixture = "Short information that fits.";
            auto wheel = [&](int rotation) {
                require(field->GetValue() == wheel_fixture,
                        "Wheel fixture was replaced before the native gesture");
                const auto point = field->ClientToScreen(wxPoint(2, 2));
                // The edit procedure and container handoff finish in SendMessage.
                // Yielding here lets the normal refresh replace our temporary
                // evidence text before its scroll position can be asserted.
                SendMessageW(static_cast<HWND>(field->GetHandle()), WM_MOUSEWHEEL,
                             MAKEWPARAM(0, rotation), MAKELPARAM(point.x, point.y));
                require(field->GetValue() == wheel_fixture,
                        "Native wheel changed the information fixture's contents");
            };
            auto field_position = [&] { return field->GetScrollPos(wxVERTICAL); };
            field->ChangeValue(wheel_fixture);
            profile_editor_->Scroll(0, 1);
            const int initial = profile_editor_->GetViewStart().y;
            wheel(-WHEEL_DELTA);
            require(profile_editor_->GetViewStart().y > initial,
                    "A partially filled information field trapped the downward wheel");
            const int down = profile_editor_->GetViewStart().y;
            wheel(WHEEL_DELTA);
            require(profile_editor_->GetViewStart().y < down,
                    "A partially filled information field trapped the upward wheel");
            wxString overflow;
            for (int line = 0; line < 80; ++line)
                overflow += wxString::Format("Scrollable evidence line %d\n", line);
            wheel_fixture = overflow;
            field->ChangeValue(wheel_fixture);
            field->ShowPosition(0);
            profile_editor_->Scroll(0, 1);
            const auto outer = profile_editor_->GetViewStart();
            wheel(-WHEEL_DELTA);
            require(field_position() > 0 && profile_editor_->GetViewStart() == outer,
                    "An overflowing field did not consume its available downward scroll");
            // Revealing the final character can leave one scrollable line at
            // fractional DPI/font heights. Use the actual native scroll limit.
            SendMessageW(static_cast<HWND>(field->GetHandle()), WM_VSCROLL, SB_BOTTOM, 0);
            profile_editor_->Scroll(0, 1);
            const auto before_bottom = profile_editor_->GetViewStart();
            const int bottom = field_position();
            // Force the CI race: a runnable refresh must wait until we have
            // asserted the gesture and restored controller-owned information.
            const auto refresh_completed = std::make_shared<bool>(false);
            CallAfter([this, refresh_completed] {
                refresh();
                *refresh_completed = true;
            });
            wheel(-WHEEL_DELTA);
            if (field_position() != bottom ||
                profile_editor_->GetViewStart().y <= before_bottom.y) {
                SCROLLINFO native{};
                native.cbSize = sizeof(native);
                native.fMask = SIF_ALL;
                GetScrollInfo(static_cast<HWND>(field->GetHandle()), SB_VERT, &native);
                throw std::runtime_error(
                    "A field at its bottom failed to hand the wheel to the editor: field=" +
                    std::to_string(bottom) + "->" + std::to_string(field_position()) +
                    ", outer=" + std::to_string(before_bottom.y) + "->" +
                    std::to_string(profile_editor_->GetViewStart().y) + ", native_max=" +
                    std::to_string(native.nMax) + ", page=" + std::to_string(native.nPage));
            }
            const auto at_bottom = profile_editor_->GetViewStart();
            wheel(WHEEL_DELTA);
            require(field_position() < bottom && profile_editor_->GetViewStart() == at_bottom,
                    "An overflowing field did not consume upward scrolling away from its bottom");
            SendMessageW(static_cast<HWND>(field->GetHandle()), WM_VSCROLL, SB_TOP, 0);
            wheel(WHEEL_DELTA);
            require(field_position() == 0 && profile_editor_->GetViewStart().y < at_bottom.y,
                    "A field at its top failed to hand the wheel to the editor");
            require(!*refresh_completed,
                    "Wheel assertions unexpectedly dispatched the queued application refresh");
            field->ChangeValue(information);
            profile_editor_->Scroll(view);
            wxTheApp->ProcessPendingEvents();
            require(*refresh_completed && field->GetValue() == information,
                    "Queued refresh did not preserve restored controller-owned information");
            require(controller_->snapshot().revision == revision,
                    "Scrolling information modified controller state");
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
        {
            const auto original = draft_;
            const auto saved_source = draft_.holes[0].forward.source;
            const auto return_source = draft_.holes[0].reverse.source;
            wxCommandEvent return_toggle(wxEVT_CHECKBOX, duplex_->GetId());
            for (bool enabled : {false, true}) {
                duplex_->SetValue(enabled);
                duplex_->GetEventHandler()->ProcessEvent(return_toggle);
                const auto reserved =
                    std::find(forward_.sources.begin(), forward_.sources.end(), return_source);
                require(reserved != forward_.sources.end() &&
                            static_cast<ThemedCombo*>(forward_.source)
                                    ->item_enabled(reserved - forward_.sources.begin()) != enabled,
                        "Return channel toggle did not update endpoint reservations");
            }
            const auto entry =
                std::find(reverse_.sources.begin(), reverse_.sources.end(), saved_source);
            require(entry != reverse_.sources.end(),
                    "Assigned endpoint disappeared instead of being grayed");
            const auto index = static_cast<int>(entry - reverse_.sources.begin());
            require(!static_cast<ThemedCombo*>(reverse_.source)->item_enabled(index),
                    "Forward endpoint remains available for reuse in Return");
            reverse_.source->SetSelection(index);
            wxCommandEvent duplicate(wxEVT_COMBOBOX, reverse_.source->GetId());
            reverse_.source->GetEventHandler()->ProcessEvent(duplicate);
            require(draft_ == original, "Selecting a grayed endpoint changed the hole");
            forward_.source->SetSelection(static_cast<int>(forward_.sources.size()) - 1);
            wxCommandEvent clear(wxEVT_COMBOBOX, forward_.source->GetId());
            forward_.source->GetEventHandler()->ProcessEvent(clear);
            require(draft_.holes[0].forward.source.empty() && forward_.source->GetValue().empty(),
                    "Clear endpoint did not leave a blank assignment");
            const auto freed =
                std::find(reverse_.sources.begin(), reverse_.sources.end(), saved_source);
            require(freed != reverse_.sources.end() &&
                        static_cast<ThemedCombo*>(reverse_.source)
                            ->item_enabled(freed - reverse_.sources.begin()),
                    "Cleared endpoint remained reserved in Return");
            require(*profile(controller_->snapshot(), viewed_) == original,
                    "Picker changes silently saved the profile");
            draft_ = original;
            load_hole(0);
            refresh();
            auto* action = reverse_.playback_check.action;
            wxCommandEvent beep(wxEVT_BUTTON, action->GetId());
            action->GetEventHandler()->ProcessEvent(beep);
            require(controller_->snapshot().beep_destination ==
                        original.holes[0].reverse.destination,
                    "Return beep targeted another direction");
            action->GetEventHandler()->ProcessEvent(beep);
            require(controller_->snapshot().beep_destination.empty(), "Return beep did not cancel");
        }
        forward_.advanced_tuning->SetValue(true);
        tuning_changed(forward_);
        save(false); // Buffer-Apply checks below deliberately opt into manual tuning.
        wxCommandEvent start(wxEVT_BUTTON, start_->GetId());
        start_->GetEventHandler()->ProcessEvent(start);
        controller_->advance(300);
        refresh();
        require(controller_->snapshot().state == audiohole::State::Running,
                "GUI Start did not reach Running");
        require(controller_->snapshot().paths.size() == 2, "Duplex directions missing");
        auto active_profile_context = profile_menu(viewed_);
        require(!profile_remove_->IsEnabled() &&
                    !active_profile_context->FindItem(wxID_DELETE)->IsEnabled(),
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
        require(!forward_.buffer_apply->IsEnabled(),
                "Buffer Apply remained enabled after its saved value matched the editor");
        require(!dirty_, "Buffer-only Apply left an otherwise unchanged profile marked unsaved");
        require(controller_->snapshot().paths[0].applied_buffer_ms == 13,
                "Buffer Apply did not confirm the edited forward buffer of 13 ms");
        require(controller_->snapshot().paths[1].applied_buffer_ms == 6,
                "Forward Buffer Apply changed the Return direction's simulated 6 ms buffer");
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
        show_result({false, "Operational warning retained during DFA validation."}, true);
        const auto warning = technical_copy_;
        dfa_value_->ChangeValue(wxS("11.001"));
        dfa_value_->GetEventHandler()->ProcessEvent(enter);
        require(technical_copy_ == warning && operational_warning_visible_ &&
                    !dfa_feedback_->GetLabel().empty() && dfa_feedback_->IsShown(),
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
                        !dfa_feedback_->IsShown(),
                    "DFA numeric controls visible in Vanilla");
            require(!personality_notice_->IsShown(),
                    "Empty personality notice reserves a Settings row");
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
        notebook_->SetSelection(0);
        Layout();
        check_endpoint_heights();
        check_reset_layout();
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
            auto capture_extra = [&](const wxString& name) {
                Layout();
                Update();
                wxYield();
                wxBitmap bitmap(GetClientSize());
                wxMemoryDC destination(bitmap);
                wxClientDC source(this);
                destination.Blit(0, 0, GetClientSize().x, GetClientSize().y, &source, 0, 0);
                destination.SelectObject(wxNullBitmap);
                bitmap.SaveFile(directory + "/" + name + ".bmp", wxBITMAP_TYPE_BMP);
            };
            notebook_->SetSelection(0);
            for (unsigned section = 0; section < 3; ++section) {
                forward_.tuning->Expand();
                for (unsigned i = 0; i < 3; ++i)
                    forward_.sections[i]->Collapse(i != section);
                layout_direction(forward_);
                const auto position = profile_editor_->CalcUnscrolledPosition(
                    profile_editor_->ScreenToClient(forward_.tuning->GetScreenPosition()));
                int step_x = 0, step_y = 0;
                profile_editor_->GetScrollPixelsPerUnit(&step_x, &step_y);
                profile_editor_->Scroll(
                    -1, std::max(0, (position.y - FromDIP(4)) / std::max(1, step_y)));
                capture_extra(wxString::Format("advanced-%u", section));
            }
            const auto appearance = theme_choice_->GetSelection();
            theme_choice_->SetSelection(1);
            wxCommandEvent theme(wxEVT_CHOICE, theme_choice_->GetId());
            theme_choice_->GetEventHandler()->ProcessEvent(theme);
            reset_layout();
            for (unsigned i : {0u, 1u, 3u}) {
                notebook_->SetSelection(i);
                if (i == 1 && playback_devices_->GetSelection() >= 0) {
                    playback_devices_->SetCurrentItem(playback_devices_->RowToItem(
                        static_cast<unsigned>(playback_devices_->GetSelection())));
                    playback_devices_->GetMainWindow()->SetFocus();
                    playback_devices_->GetMainWindow()->Refresh();
                }
                capture_extra(wxString::Format("light-page-%u", i));
            }
            theme_choice_->SetSelection(appearance);
            theme_choice_->GetEventHandler()->ProcessEvent(theme);
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
