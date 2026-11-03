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
        (s.capabilities.routing ? audiohole::name(s.state) : "Test Mode; engine unavailable") +
        " | " + (s.active_profile.empty() ? "No active profile" : s.active_profile);
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
    const auto& s = frame_.controller().snapshot();
    auto* menu = new wxMenu;
    menu->Append(wxID_ANY, text(std::string{} + audiohole::name(s.state)))->Enable(false);
    auto add = [menu](const wxString& title, std::function<void()> action, bool enabled = true) {
        const int id = wxWindow::NewControlId();
        menu->Append(id, title)->Enable(enabled);
        menu->Bind(wxEVT_MENU, [action](wxCommandEvent&) { action(); }, id);
    };
    add("Open AudioHole", [this] { frame_.open(); });
    add("Start Routing", [this] { frame_.tray_start(); }, s.capabilities.routing);
    add(
        "Stop Routing",
        [this] {
            frame_.controller().stop();
            frame_.refresh();
        },
        s.capabilities.routing);
    auto* profiles = new wxMenu;
    for (const auto& p : s.profiles) {
        const int id = wxWindow::NewControlId();
        auto* item = profiles->AppendRadioItem(id, text(p.display_name));
        item->Check(p.id == s.active_profile);
        item->Enable(s.capabilities.routing);
        profiles->Bind(
            wxEVT_MENU, [this, key = p.id](wxCommandEvent&) { frame_.tray_activate(key); }, id);
    }
    menu->AppendSubMenu(profiles, "Activate profile");
    auto* holes = new wxMenu;
    for (const auto& p : s.paths)
        holes
            ->Append(wxID_ANY, text(p.hole_id + (p.reverse ? " reverse: " : " forward: ") +
                                    audiohole::name(p.state)))
            ->Enable(false);
    if (s.paths.empty())
        holes->Append(wxID_ANY, "No active directions")->Enable(false);
    menu->AppendSubMenu(holes, "Direction states (read-only)");
    menu->AppendSeparator();
    add("Exit AudioHole", [this] { frame_.exit(); });
    return menu;
}

} // namespace audiohole::gui
