#pragma once

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/colour.h>
#include <wx/menu.h>
#include <wx/panel.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace audiohole::plug_board {
class Module;
struct CommandResult;
struct ModuleSnapshot;
namespace desktop {

struct ViewAppearance {
    bool dark = false;
    bool high_contrast = false;
    wxColour surface, input, foreground, muted, accent;
};
struct ViewOptions {
    // Optional host UI factories reuse its existing button/choice implementations.
    // The choice factory must preserve wheel protection. All UI callbacks execute
    // on the GUI thread; they must not perform native audio-policy operations.
    std::function<wxButton*(wxWindow*, const wxString&, const wxString&)> button;
    std::function<wxChoice*(wxWindow*, const wxString&)> choice;
    std::function<bool(const std::string& title, const std::string& message)> confirm;
    std::function<std::optional<std::string>(const std::string& title, const std::string& current)>
        name;
    std::function<std::optional<std::size_t>(const std::string& title,
                                             const std::vector<std::string>&)>
        select;
    // Optional menu presenter; smoke uses it to select actual constructed menu
    // actions without a blocking native popup. The default is a native menu.
    std::function<std::optional<std::size_t>(const wxMenu&, const std::vector<std::string>&)> menu;
    std::function<void(const CommandResult&)> completed;
    // Optional host URL launcher. The default opens Windows Settings through
    // the Shell; tests intercept the exact mixer URI without opening Settings.
    std::function<bool(const std::string&)> open_url;
    // Guided creation confirms a nonblank name distinct from existing sockets
    // in the same themed popup. Ordinary hosts retain the library's defaults.
    bool require_distinct_socket_names = false;
};

struct SocketLayout {
    std::string id;
    wxRect screen_rectangle;
    int endpoint_control_id = wxID_NONE;
    int applications_control_id = wxID_NONE;
    std::array<std::size_t, 3> section_counts{};
    std::vector<std::string> rendered_application_ids;
    std::size_t first_visible_row = 0;
};
struct ViewLayout {
    bool drawer_open = false;
    bool drawer_animating = false;
    bool dragging = false;
    bool command_pending = false;
    wxRect viewport_screen_rectangle;
    wxRect drawer_screen_rectangle;
    wxRect tab_screen_rectangle;
    wxPoint horizontal_scroll_units;
    std::vector<SocketLayout> sockets;
};

// Embeddable tab content: no engine/header/tray ownership. The host owns Module
// for application lifetime and explicitly supplies actual tab/window visibility.
// Destroy/detach the view before destroying Module. Native work and persistence
// stay on the service; a coalesced mailbox posts copied snapshots to the GUI.
class PlugBoardView final : public wxPanel {
  public:
    PlugBoardView(wxWindow*, Module&, ViewOptions = {});
    ~PlugBoardView() override;
    void set_host_visible(bool);
    void set_appearance(const ViewAppearance&);
    void set_drawer_open(bool);
    void detach_module();
    bool dragging() const;
    // GUI-thread geometry/identity inspection supports diagnostics and smoke
    // assertions against actual controls rather than an invented layout model.
    ViewLayout layout_diagnostics() const;
    // GUI-thread tutorial/accessibility geometry, from the same virtual rows
    // used for painting/hit testing. No layout or assignment changes.
    std::optional<wxRect> application_screen_rect(const std::string&, bool ensure_visible = false);
    void ensure_socket_visible(const std::string&);

  private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
    friend class SocketColumn;
    friend class ApplicationRows;
    friend class ApplicationDropTarget;
    friend class DrawerTab;
    friend class BoardFrame;
    friend struct BoardSmokeDriver;
};

} // namespace desktop
} // namespace audiohole::plug_board
