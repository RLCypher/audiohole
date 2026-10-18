#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
Tray::Tray(Frame& frame) : frame_(frame) {
    Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent&) { frame_.open(); });
}
// wxTaskBarIcon handles TaskbarCreated when Explorer restarts. Textual state
// accompanies the icon so color is never the sole health indicator.
void Tray::update(const audiohole::Snapshot& s) {
    const std::string tip = std::string("AudioHole [SIMULATED] — ") + audiohole::name(s.state) +
                            " | " +
                            (s.active_profile.empty() ? "No active profile" : s.active_profile);
    if (IsIconInstalled() && tooltip_ == tip && last_ == s.state)
        return;
    wxBitmap bitmap(32, 32);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    const wxColour colour = s.state == audiohole::State::Running  ? wxColour(28, 132, 75)
                            : s.state == audiohole::State::Failed ? wxColour(180, 45, 45)
                                                                  : wxColour(115, 95, 40);
    dc.SetBrush(wxBrush(colour));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawCircle(16, 16, 15);
    dc.SetTextForeground(*wxWHITE);
    dc.SetFont(wxFont(wxFontInfo(16).Bold()));
    dc.DrawText(text(std::string(1, audiohole::name(s.state)[0])), 8, 3);
    dc.SelectObject(wxNullBitmap);
    wxIcon icon;
    icon.CopyFromBitmap(bitmap);
    SetIcon(icon, text(tip));
    last_ = s.state;
    tooltip_ = tip;
}
wxMenu* Tray::CreatePopupMenu() {
    const auto& s = frame_.controller().snapshot();
    auto* menu = new wxMenu;
    menu->Append(wxID_ANY, text(std::string("SIMULATED — ") + audiohole::name(s.state)))
        ->Enable(false);
    auto add = [menu](const wxString& title, std::function<void()> action) {
        const int id = wxWindow::NewControlId();
        menu->Append(id, title);
        menu->Bind(wxEVT_MENU, [action](wxCommandEvent&) { action(); }, id);
    };
    add("Open AudioHole", [this] { frame_.open(); });
    add("Start Routing", [this] { frame_.tray_start(); });
    add("Stop Routing", [this] {
        frame_.controller().stop();
        frame_.refresh();
    });
    auto* profiles = new wxMenu;
    for (const auto& p : s.profiles) {
        const int id = wxWindow::NewControlId();
        auto* item = profiles->AppendRadioItem(id, text(p.display_name));
        item->Check(p.id == s.active_profile);
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
