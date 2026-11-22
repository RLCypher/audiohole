#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
Tray::Tray(Frame& frame) : frame_(frame) {
    Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent&) { frame_.open(); });
}
// wxTaskBarIcon handles TaskbarCreated when Explorer restarts. Textual state
// accompanies the icon so color is never the sole health indicator.
void Tray::update(const audiohole::Snapshot& s) {
    const std::string tip =
        std::string("AudioHole — ") +
        (s.capabilities.routing ? audiohole::name(s.state) : "Engine unavailable") + " | " +
        (s.active_profile.empty() ? "No active profile" : s.active_profile);
    // The tray host can be on a different monitor; use the window's effective DPI
    // as a practical size hint and let Explorer select its final notification size.
    const int size = GetSystemMetricsForDpi(SM_CXSMICON, static_cast<UINT>(frame_.GetDPI().x));
    if (IsIconInstalled() && tooltip_ == tip && last_ == s.state && icon_size_ == size)
        return;
    if (!SetIcon(icons_.get(size, s.state), text(tip)))
        return; // Retry through the existing refresh path if Explorer is unavailable.
    icon_size_ = size;
    last_ = s.state;
    tooltip_ = tip;
}
wxMenu* Tray::CreatePopupMenu() {
    return frame_.shell_menu().release(); // wxTaskBarIcon takes ownership.
}
std::unique_ptr<wxMenu> Frame::shell_menu() {
    const auto& s = controller_->snapshot();
    auto* menu = new wxMenu;
    menu->Append(wxID_ANY, text(std::string{} + audiohole::name(s.state)))->Enable(false);
    auto add = [menu](const wxString& title, std::function<void()> action, bool enabled = true) {
        const int id = wxWindow::NewControlId();
        menu->Append(id, title)->Enable(enabled);
        menu->Bind(wxEVT_MENU, [action](wxCommandEvent&) { action(); }, id);
    };
    add("Plug Board", [this] { perform_shell_action(ShellAction::PlugBoard); });
    add("Open AudioHole", [this] { perform_shell_action(ShellAction::Open); });
    add(
        "Start Routing", [this] { perform_shell_action(ShellAction::Start); },
        s.capabilities.routing);
    add(
        "Stop Routing", [this] { perform_shell_action(ShellAction::Stop); },
        s.capabilities.routing);
    auto* profiles = new wxMenu;
    for (const auto& p : s.profiles) {
        const int id = wxWindow::NewControlId();
        auto* item = profiles->AppendRadioItem(id, text(p.display_name));
        item->Check(p.id == s.active_profile);
        item->Enable(s.capabilities.routing);
        profiles->Bind(wxEVT_MENU, [this, key = p.id](wxCommandEvent&) { tray_activate(key); }, id);
    }
    menu->AppendSubMenu(profiles, "Activate profile");
    auto* holes = new wxMenu;
    for (const auto& p : s.paths)
        holes
            ->Append(wxID_ANY, text(p.hole_id + (p.reverse ? " return: " : " forward: ") +
                                    audiohole::name(p.state)))
            ->Enable(false);
    if (s.paths.empty())
        holes->Append(wxID_ANY, "No active directions")->Enable(false);
    menu->AppendSubMenu(holes, "Direction states (read-only)");
    menu->AppendSeparator();
    add("Close window", [this] { perform_shell_action(ShellAction::Close); });
    add("Exit AudioHole", [this] { perform_shell_action(ShellAction::Exit); });
    return std::unique_ptr<wxMenu>(menu);
}

void Frame::perform_shell_action(ShellAction action) {
    if (IsBeingDeleted())
        return;
    switch (action) {
    case ShellAction::PlugBoard:
        open();
        select_page(PlugBoardPage, true);
        break;
    case ShellAction::Open:
        open();
        break;
    case ShellAction::Controls: {
        open();
        auto menu = shell_menu();
        PopupMenu(menu.get(), ScreenToClient(wxGetMousePosition()));
        break;
    }
    case ShellAction::Start:
        if (controller_->snapshot().capabilities.routing)
            tray_start();
        break;
    case ShellAction::Stop:
        if (controller_->snapshot().capabilities.routing) {
            controller_->stop();
            refresh();
        }
        break;
    case ShellAction::Close:
        Close();
        break; // Existing close-to-tray handling.
    case ShellAction::Exit:
        request_exit();
        break;
    }
}
void Frame::initialize_shell_controls() {
    if (const auto menu = GetSystemMenu(static_cast<HWND>(GetHandle()), FALSE)) {
        // Shift+right-click and Alt+Space retain the native window commands too.
        ModifyMenuW(menu, SC_CLOSE, MF_BYCOMMAND | MF_STRING, SC_CLOSE, L"Close window\tAlt+F4");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, shell_plug_board_command, L"Plug Board");
        AppendMenuW(menu, MF_STRING, shell_controls_command, L"AudioHole controls…");
        AppendMenuW(menu, MF_STRING, shell_exit_command, L"Exit AudioHole");
    }
    if (!smoke_ && !tutorial_) {
        const auto result = publish_shell_tasks(controller_->snapshot().capabilities.routing);
        if (FAILED(result))
            wxLogDebug("Windows could not publish AudioHole taskbar tasks (HRESULT 0x%08lx).",
                       static_cast<unsigned long>(result));
    }
}

} // namespace audiohole::gui
