#pragma once

#include "audiohole/plug_board/module.hpp"
#include "audiohole/plug_board/presentation.hpp"
#include "audiohole/plug_board/view.hpp"
#include "controls.hpp"

#include <wx/dnd.h>
#include <wx/listbox.h>
#include <wx/scrolwin.h>
#include <wx/vlbox.h>

#include <future>
#include <map>
#include <mutex>
#include <set>

namespace audiohole::plug_board::desktop {

class SocketColumn;
class ApplicationRows;
class DrawerTab;

// The mutex covers the handler lifetime and one pending snapshot slot. detach
// nulls the receiver before deleting queued events, so an in-flight service
// callback can finish safely after unsubscribe without touching a dead widget.
struct ViewMailbox {
    std::mutex mutex;
    wxEvtHandler* receiver = nullptr;
    int event_id = 0;
    bool enabled = false;
    bool queued = false;
    std::shared_ptr<const ModuleSnapshot> latest;
    void post(std::shared_ptr<const ModuleSnapshot>);
};

struct PlugBoardView::Implementation final : public wxEvtHandler {
    static constexpr int socket_width_dip = 186;
    static constexpr int socket_gap_dip = 6;
    static constexpr int drawer_width_dip = 300;
    static constexpr int tab_width_dip = 22;
    PlugBoardView& panel;
    Module* module;
    ViewOptions options;
    ViewAppearance appearance;
    std::shared_ptr<ViewMailbox> mailbox = std::make_shared<ViewMailbox>();
    std::uint64_t subscription = 0;
    std::shared_ptr<const ModuleSnapshot> snapshot;
    std::shared_ptr<const ModuleSnapshot> deferred_snapshot;
    std::map<std::string, const ApplicationView*> applications;
    std::map<std::string, ApplicationPlacement> placements;
    std::vector<SocketColumn*> columns;
    wxScrolledWindow* viewport{};
    wxScrolledWindow* drawer{};
    DrawerTab* tab{};
    wxStaticText* empty{};
    wxChoice *boards{}, *profiles{}, *recent{};
    wxCheckBox *use_default{}, *associate{}, *inactive{};
    wxListBox* sockets{};
    wxButton *add_board{}, *delete_board{}, *save{}, *save_as{}, *rename_board{}, *add_socket{},
        *delete_socket{}, *windows_mixer{}, *clear_inactive_button{}, *exempt_apps{};
    std::vector<std::string> board_ids, profile_ids, socket_ids;
    std::vector<unsigned> recent_values{60, 300, 900, 1800, 3600, 86400};
    std::vector<std::future<CommandResult>> pending;
    std::map<std::string, wxBitmap> bitmaps;
    std::map<std::string, std::weak_ptr<const ApplicationImage>> bitmap_sources;
    std::map<std::string, std::uint64_t> bitmap_usage;
    std::uint64_t bitmap_sequence = 0;
    wxTimer completion_timer{this}, animation_timer{this}, drag_timer{this}, age_timer{this};
    bool visible = false, rendering = false, drawer_open = true, drag_active = false;
    bool animation_running = false;
    unsigned interaction_depth = 0;
    double drawer_fraction = 1;
    double animation_target = 1;
    bool reduced_motion = false;
    std::string rendered_board;
    std::uint64_t rendered_working_revision = 0;
    std::string highlighted_socket;
    std::string drag_source_board;
    std::shared_ptr<const ModuleSnapshot> drag_context;
    wxPoint socket_drag_start;
    int socket_drag_index = wxNOT_FOUND;
    bool socket_drag_moved = false;
    std::shared_ptr<const ModuleSnapshot> socket_drag_context;
    std::chrono::steady_clock::time_point snapshot_received = std::chrono::steady_clock::now();

    Implementation(PlugBoardView&, Module&, ViewOptions);
    ~Implementation() override;
    void detach();
    void build();
    void build_drawer();
    void receive(wxThreadEvent&);
    void render(std::shared_ptr<const ModuleSnapshot>);
    void render_drawer();
    void layout();
    void palette();
    void poll_commands();
    void submit(Command, std::shared_ptr<const ModuleSnapshot> context = {});
    bool busy() const;
    wxButton* button(wxWindow*, const wxString&, const wxString&, const wxString& name = {});
    wxChoice* choice(wxWindow*, const wxString& name, const wxString& help);
    bool confirm(const std::string&, const std::string&);
    std::optional<std::string> name(const std::string&, const std::string&);
    std::optional<std::size_t> select(const std::string&, const std::vector<std::string>&);
    const RoutingBoard* board() const;
    const ApplicationView* application(const std::string&) const;
    std::string output_name(const std::string&) const;
    wxBitmap icon(const std::string&);
    void request_visible_icons();
    void set_drawer(bool);
    void ensure_socket_visible(const std::string&);
    void new_board();
    void remove_board();
    void copy_board();
    void rename_current_board();
    void new_socket();
    void remove_socket();
    void remove_socket(const std::string&);
    void rename_socket(const std::string&);
    void reorder_socket(std::size_t source, std::size_t destination);
    void finish_socket_drag(bool commit, const wxPoint& destination = wxDefaultPosition);
    void exempt_apps_dialog();
    void clear_inactive(const std::vector<std::string>& candidates = {});
    void rename_app(const std::string&);
    void group_menu(const std::vector<std::string>&, wxWindow&, wxPoint);
    void app_menu(const std::string&, wxWindow&, wxPoint);
    void socket_menu(const std::string&, wxWindow&, wxPoint);
    wxString drag_payload(const std::string&) const;
    bool drop(const std::string& destination, const wxString& payload);
    void begin_drag(const std::string&, wxWindow&);
    void drag_over(const std::string&, const wxPoint& screen);
    void end_drag();
    ViewLayout inspect() const;
};

class SocketColumn final : public wxPanel {
  public:
    SocketColumn(wxWindow*, PlugBoardView::Implementation&, const Socket&);
    void update(const Socket&, const std::array<std::vector<std::string>, 3>&);
    void palette();
    SocketLayout inspect() const;
    std::string id;
    wxButton* heading{};
    wxChoice* endpoint{};
    ApplicationRows* rows{};

  private:
    PlugBoardView::Implementation& view;
    std::vector<std::optional<std::string>> endpoint_ids;
    std::uint64_t inventory_revision = ~std::uint64_t(0);
    SocketAppearance style;
    std::string rendered_style;
};

class ApplicationRows final : public wxVListBox {
  public:
    struct Row {
        int section = 0;
        bool heading = false;
        std::string application_id;
        std::string group_key{};
        std::string group_name{};
        bool member = false;
    };
    ApplicationRows(wxWindow*, PlugBoardView::Implementation&, std::string socket_id);
    ~ApplicationRows() override;
    void update(const std::array<std::vector<std::string>, 3>&, bool show_inactive);
    void activate(std::size_t);
    bool is_expanded(std::size_t) const;
    wxString accessible_name(std::size_t) const;
    wxString accessible_help(std::size_t) const;
    // VirtualHitTest returns a row; wxWindow::HitTest returns a window region.
    // Keep the bounds check here for every pointer-based row interaction.
    std::optional<std::size_t> row_at(const wxPoint&) const;
    void refresh_metrics();
    struct VisualLayout {
        wxRect icon, name, detail;
    };
    VisualLayout visual_layout(const wxRect&, int secondary_height = 0) const;
    std::string detail_text(const ApplicationView&) const;
    struct StatusVisual {
        wxString text;
        wxFont font;
        wxSize extent;
    };
    StatusVisual status_visual(const ApplicationView&, int width) const;
    wxString hover_text(const wxPoint&) const;
    std::vector<Row> entries;
    std::array<std::size_t, 3> counts{};
    std::array<bool, 3> expanded{true, true, false};
    std::set<std::string> expanded_bundles;
    std::map<std::string, std::vector<std::string>> bundled;
    void context_menu(std::size_t, wxPoint);
    void rebuild();

  protected:
    void OnDrawItem(wxDC&, const wxRect&, std::size_t) const override;
    void OnDrawBackground(wxDC&, const wxRect&, std::size_t) const override;
    wxCoord OnMeasureItem(std::size_t) const override;

  private:
    void update_hover_hint(const wxPoint&);
    PlugBoardView::Implementation& view;
    std::string socket_id;
    std::array<std::vector<std::string>, 3> groups;
    bool show_inactive = false;
    std::vector<std::string> grouping_names;
    wxPoint press;
    std::string pressed_application;
    wxFont detail_font;
    int name_height = 0, detail_height = 0, row_height = 0, icon_size = 0;
};

class DrawerTab final : public wxControl {
  public:
    DrawerTab(wxWindow*, std::function<void()>);
    void set_appearance(const ViewAppearance&);
    bool expanded = true;

  private:
    ViewAppearance appearance;
    std::function<void()> toggle;
};

wxString ui_text(const std::string&);

} // namespace audiohole::plug_board::desktop
