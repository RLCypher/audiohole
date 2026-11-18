#include "board_frame.hpp"
#include "dialogs.hpp"
#include "native_theme.hpp"

#include <windows.h>

#include <algorithm>

namespace audiohole::plug_board::desktop {
namespace {
class BoardTray final : public wxTaskBarIcon {
  public:
    std::function<void()> open, quit;
    std::function<Appearance()> appearance;
    BoardTray() {
        Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent&) { open(); });
        Bind(wxEVT_MENU, [this](wxCommandEvent& event) {
            if (event.GetId() == wxID_OPEN)
                open();
            else if (event.GetId() == wxID_EXIT)
                quit();
        });
    }
    wxMenu* CreatePopupMenu() override {
        auto* menu = new wxMenu;
        menu->Append(new MenuItem(menu, wxID_OPEN, "Open Plug Board"));
        menu->AppendSeparator();
        menu->Append(new MenuItem(menu, wxID_EXIT, "Exit Plug Board"));
        if (appearance)
            apply_menu_appearance(*menu, appearance());
        return menu;
    }
};
} // namespace

BoardFrame::BoardFrame(ModuleOptions options, bool hidden, ViewOptions view_options)
    : wxFrame(nullptr, wxID_ANY, "AudioHole - Plug Board", wxDefaultPosition, wxSize(1180, 760)),
      data_root(options.data_root), local_root(options.local_root) {
    SetFont(wxFont(wxFontInfo(10).FaceName("Segoe UI")));
    SetMinSize(FromDIP(wxSize(980, 680)));
    SetSize(FromDIP(wxSize(1180, 760)));
    wxIcon icon("AUDIOHOLE_ICON", wxBITMAP_TYPE_ICO_RESOURCE);
    if (icon.IsOk())
        SetIcon(icon);
    colors = appearance_for("Follow System");
    options.icon_pixels = static_cast<unsigned>(FromDIP(32));
    service = std::make_unique<Module>(std::move(options));
    build();
    view_options.completed =
        [this, prior = std::move(view_options.completed)](const CommandResult& result) {
            if (prior)
                prior(result);
            if (!result.ok) {
                set_status(ui_text(result.message));
                show_message(this, ui_text(result.message), "Could not update routing",
                             wxOK | wxICON_ERROR, colors);
            }
        };
    view = new PlugBoardView(surface, *service, std::move(view_options));
    surface->GetSizer()->Add(view, 1, wxEXPAND);
    Layout();
    mailbox->receiver = this;
    mailbox->enabled = true;
    mailbox->event_id = wxWindow::NewControlId();
    Bind(wxEVT_THREAD, &BoardFrame::receive, this, mailbox->event_id);
    const auto weak = std::weak_ptr<ViewMailbox>(mailbox);
    subscription = service->subscribe([weak](auto value) {
        if (const auto sink = weak.lock())
            sink->post(std::move(value));
    });
    Bind(wxEVT_TIMER, [this](wxTimerEvent& event) {
        if (event.GetId() == completion_timer.GetId())
            poll();
        else if (event.GetId() == smoke_timer.GetId() && smoke)
            smoke->tick(*this);
    });
    Bind(wxEVT_CLOSE_WINDOW, &BoardFrame::close, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        if (status)
            set_status(status_message);
        event.Skip();
    });
    Bind(wxEVT_ICONIZE, [this](wxIconizeEvent& event) {
        update_visibility();
        event.Skip();
        CallAfter([this] { update_visibility(); });
    });
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        event.Skip();
        CallAfter([this] { update_visibility(); });
    });
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
        apply_colors();
        event.Skip();
    });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
        event.Skip();
        CallAfter([this] {
            service->set_icon_pixels(static_cast<unsigned>(FromDIP(32)));
            view->set_appearance(colors);
            Layout();
        });
    });
    auto icon_tray = std::make_unique<BoardTray>();
    icon_tray->appearance = [this] { return colors; };
    icon_tray->open = [this] {
        Show();
        Iconize(false);
        Raise();
        update_visibility();
    };
    icon_tray->quit = [this] { exit(); };
    if (icon.IsOk() && icon_tray->SetIcon(icon, "AudioHole Plug Board"))
        tray = std::move(icon_tray);
    snapshot = service->snapshot();
    apply_colors();
    render();
    if (!hidden)
        Show();
    else if (!tray) {
        Show();
        Iconize();
    }
}

BoardFrame::~BoardFrame() {
    completion_timer.Stop();
    smoke_timer.Stop();
    {
        std::lock_guard lock(mailbox->mutex);
        mailbox->receiver = nullptr;
        mailbox->latest.reset();
    }
    service->unsubscribe(subscription);
    DeletePendingEvents();
    if (view)
        view->detach_module();
    tray.reset();
    // wxFrame destroys children after C++ members. Detach first so that the
    // view's later destructor cannot refer to an already-destroyed service.
    service->begin_shutdown();
}

void BoardFrame::build() {
    surface = new wxPanel(this);
    auto* content = new wxBoxSizer(wxVERTICAL);
    auto* header = new wxBoxSizer(wxHORIZONTAL);
    auto* title = new wxStaticText(surface, wxID_ANY, "AudioHole | Plug Board");
    title->SetFont(title->GetFont().Bold());
    header->Add(title, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    appearance_choice = new Choice(surface, wxID_ANY);
    appearance_choice->SetName("Appearance");
    appearance_choice->SetToolTip(
        "Follow Windows, Light, or Dark. Windows high contrast takes precedence.");
    for (const auto* mode : {"Follow System", "Light", "Dark"})
        appearance_choice->Append(mode);
    appearance_choice->SetSelection(0);
    header->Add(appearance_choice, 0, wxRIGHT, FromDIP(8));
    refresh = new Button(surface, "Refresh", "Refresh applications and playback outputs.");
    auto* quit =
        new Button(surface, "Exit", "Exit Plug Board. Saved application routing is retained.");
    header->Add(refresh, 0, wxRIGHT, FromDIP(8));
    header->Add(quit);
    content->Add(header, 0, wxEXPAND | wxALL, FromDIP(10));
    status = new wxStaticText(surface, wxID_ANY, "Loading");
    status->SetName("Module status");
    content->Add(status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));
    surface->SetSizer(content);
    auto* frame_layout = new wxBoxSizer(wxVERTICAL);
    frame_layout->Add(surface, 1, wxEXPAND);
    SetSizer(frame_layout);
    appearance_choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { apply_colors(); });
    quit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { exit(); });
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        repairs.push_back(service->submit(RefreshApplications{}));
        completion_timer.Start(50);
    });
}

void BoardFrame::receive(wxThreadEvent&) {
    {
        std::lock_guard lock(mailbox->mutex);
        snapshot = std::exchange(mailbox->latest, {});
        mailbox->queued = false;
    }
    if (snapshot && IsShown() && !IsIconized())
        render();
}
// Full status messages reflow instead of being truncated or ellipsized. Keep
// the unwrapped source so a resize can give the text more space again.
void BoardFrame::set_status(const wxString& message) {
    status_message = message;
    status->SetLabel(status_message);
    status->Wrap(std::max(FromDIP(100), surface->GetClientSize().x - FromDIP(20)));
    Layout();
}
void BoardFrame::render() {
    if (!snapshot)
        return;
    std::string message = snapshot->simulated ? "Demo | " : "";
    if (snapshot->phase == ModulePhase::Loading)
        message += "Loading configuration";
    else if (snapshot->phase == ModulePhase::ReadOnlyError)
        message += "Configuration error: " + snapshot->storage.detail;
    else if (snapshot->phase == ModulePhase::Stopping)
        message += "Finishing owned work";
    else
        message += std::to_string(snapshot->applications.size()) + " applications";
    if (!snapshot->capability.ok)
        message += " | Routing unavailable: " + snapshot->capability.detail;
    set_status(ui_text(message));
}
void BoardFrame::update_visibility() {
    if (!view)
        return;
    const bool foreground = IsShown() && !IsIconized() && !exiting;
    view->set_host_visible(foreground);
    if (foreground && snapshot)
        render();
}
WXLRESULT BoardFrame::MSWWindowProc(WXUINT message, WXWPARAM first, WXLPARAM second) {
    const auto result = wxFrame::MSWWindowProc(message, first, second);
    // App appearance changes use WM_SETTINGCHANGE, not the legacy color event.
    // Re-read the chosen preference and refresh owned dialogs without polling.
    if (message == WM_SETTINGCHANGE &&
        (first == SPI_SETHIGHCONTRAST ||
         (second && lstrcmpW(reinterpret_cast<LPCWSTR>(second), L"ImmersiveColorSet") == 0)))
        CallAfter([this] {
            if (!exiting && appearance_choice)
                apply_colors();
        });
    return result;
}
void BoardFrame::apply_colors() {
    colors = appearance_for(appearance_choice->GetStringSelection());
    apply_appearance(*this, colors);
    if (view)
        view->set_appearance(colors);
}
void BoardFrame::poll() {
    for (auto it = repairs.begin(); it != repairs.end();) {
        if (it->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }
        const auto result = it->get();
        if (!result.ok)
            set_status(ui_text(result.message));
        it = repairs.erase(it);
    }
    if (exiting && shutdown.valid() &&
        shutdown.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const auto result = shutdown.get();
        if (!result.ok) {
            set_status(ui_text("Shutdown cleanup reported: " + result.detail));
        }
        completion_timer.Stop();
        Destroy();
        return;
    }
    if (repairs.empty() && !exiting)
        completion_timer.Stop();
}
void BoardFrame::exit() {
    if (exiting)
        return;
    exiting = true;
    view->set_host_visible(false);
    surface->Disable();
    set_status("Finishing owned work. Windows application preferences will be retained.");
    shutdown = service->begin_shutdown();
    completion_timer.Start(50);
}
void BoardFrame::close(wxCloseEvent& event) {
    if (exiting) {
        if (event.CanVeto())
            event.Veto();
        return;
    }
    if (!event.CanVeto()) {
        exit();
        return;
    }
    event.Veto();
    if (tray)
        Hide();
    else
        Iconize();
    update_visibility();
}
void BoardFrame::start_smoke(const std::filesystem::path& output,
                             std::function<void(int)> finished) {
    smoke = std::make_unique<BoardSmokeDriver>();
    smoke->output = output;
    smoke->finished = std::move(finished);
    // Install the same providers on the existing view; controls and bindings are
    // unchanged. Only modal answers are injected for deterministic automation.
    view->implementation_->options = smoke->hooks();
    smoke_timer.Start(40);
}
} // namespace audiohole::plug_board::desktop
