#include "audiohole/plug_board/socket_styles.hpp"
#include "native_theme.hpp"
#include "socket_style.hpp"
#include "view_state.hpp"

#include <wx/access.h>
#include <wx/dcbuffer.h>
#include <wx/menu.h>

#include <algorithm>
#include <cmath>

namespace audiohole::plug_board::desktop {
namespace {
constexpr const char* section_names[]{"Active", "Recently Active", "Inactive"};

// Virtual rows expose the same names, locations and actions as their painted
// counterparts. No per-application HWND or bitmap is retained off screen.
class RowsAccessible final : public wxAccessible {
  public:
    explicit RowsAccessible(ApplicationRows* value) : wxAccessible(value), rows(*value) {}
    wxAccStatus GetChildCount(int* count) override {
        *count = static_cast<int>(rows.entries.size());
        return wxACC_OK;
    }
    wxAccStatus GetChild(int child, wxAccessible** result) override {
        if (child < 0 || child > static_cast<int>(rows.entries.size()))
            return wxACC_FAIL;
        *result = child ? nullptr : this;
        return wxACC_OK;
    }
    wxAccStatus GetName(int child, wxString* value) override {
        if (!valid(child))
            return wxACC_FAIL;
        *value = child ? rows.accessible_name(static_cast<std::size_t>(child - 1)) : rows.GetName();
        return wxACC_OK;
    }
    wxAccStatus GetHelpText(int child, wxString* value) override {
        if (!valid(child))
            return wxACC_FAIL;
        *value = child ? rows.accessible_help(static_cast<std::size_t>(child - 1))
                       : rows.GetToolTipText();
        return wxACC_OK;
    }
    wxAccStatus GetRole(int child, wxAccRole* role) override {
        if (!valid(child))
            return wxACC_FAIL;
        *role = child ? wxROLE_SYSTEM_LISTITEM : wxROLE_SYSTEM_LIST;
        return wxACC_OK;
    }
    wxAccStatus GetState(int child, long* state) override {
        if (!valid(child))
            return wxACC_FAIL;
        *state = wxACC_STATE_SYSTEM_FOCUSABLE | wxACC_STATE_SYSTEM_SELECTABLE;
        if (child && rows.GetSelection() == child - 1)
            *state |= wxACC_STATE_SYSTEM_SELECTED;
        if (rows.HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (child && rows.GetItemRect(static_cast<std::size_t>(child - 1)).IsEmpty())
            *state |= wxACC_STATE_SYSTEM_OFFSCREEN;
        return wxACC_OK;
    }
    wxAccStatus GetLocation(wxRect& rectangle, int child) override {
        if (!valid(child))
            return wxACC_FAIL;
        rectangle =
            child ? rows.GetItemRect(static_cast<std::size_t>(child - 1)) : rows.GetClientRect();
        rectangle.SetPosition(rows.ClientToScreen(rectangle.GetPosition()));
        return wxACC_OK;
    }
    wxAccStatus DoDefaultAction(int child) override {
        if (child <= 0 || !valid(child))
            return wxACC_NOT_SUPPORTED;
        rows.activate(static_cast<std::size_t>(child - 1));
        return wxACC_OK;
    }
    wxAccStatus GetDefaultAction(int child, wxString* action) override {
        if (!valid(child))
            return wxACC_FAIL;
        *action = child ? "Activate" : "";
        return wxACC_OK;
    }

  private:
    ApplicationRows& rows;
    bool valid(int child) const {
        return child >= 0 && child <= static_cast<int>(rows.entries.size());
    }
};

wxString clipped(wxDC& dc, const wxString& value, int width) {
    return wxControl::Ellipsize(value, dc, wxELLIPSIZE_END, std::max(0, width));
}
const char* drag_result_name(wxDragResult result) {
    switch (result) {
    case wxDragError:
        return "error";
    case wxDragNone:
        return "not_accepted";
    case wxDragCopy:
        return "copy";
    case wxDragMove:
        return "move";
    case wxDragLink:
        return "link";
    case wxDragCancel:
        return "cancelled";
    }
    return "unknown";
}

} // namespace

// The same target is attached to the heading, dropdown, list and empty column.
// Payloads contain stable IDs and a revision, never a PID or Windows device ID.
class ApplicationDropTarget final : public wxTextDropTarget {
  public:
    ApplicationDropTarget(PlugBoardView::Implementation& value, wxWindow& owner, std::string socket)
        : view(value), window(owner), socket_id(std::move(socket)) {}
    wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult) override {
        if (view.busy() && !view.drag_active)
            return wxDragNone;
        view.drag_over(socket_id, window.ClientToScreen(wxPoint(x, y)));
        return wxDragMove;
    }
    void OnLeave() override {
        view.highlighted_socket.clear();
        window.Refresh();
    }
    bool OnDropText(wxCoord, wxCoord, const wxString& data) override {
        return view.drop(socket_id, data);
    }

  private:
    PlugBoardView::Implementation& view;
    wxWindow& window;
    std::string socket_id;
};

SocketColumn::SocketColumn(wxWindow* parent, PlugBoardView::Implementation& state,
                           const Socket& socket)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE), id(socket.id),
      view(state) {
    SetName(ui_text(socket.display_name + " socket"));
    heading = view.button(this, ui_text(socket.display_name),
                          "Right-click to rename, recolor, change icon, or delete.",
                          ui_text("Socket heading " + id));
    heading->SetMinSize(wxSize(-1, FromDIP(32)));
    endpoint = view.choice(
        this, ui_text("Playback output " + id),
        "Choose this socket's playback device. Changes are saved before Windows routing updates.");
    rows = new ApplicationRows(this, view, id);
    auto* content = new wxBoxSizer(wxVERTICAL);
    content->Add(heading, 0, wxEXPAND | wxALL, FromDIP(5));
    content->Add(endpoint, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(5));
    content->Add(rows, 1, wxEXPAND);
    SetSizer(content);
    heading->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event) {
        const auto point = event.GetPosition() == wxDefaultPosition
                               ? heading->GetClientRect().GetBottomLeft()
                               : heading->ScreenToClient(event.GetPosition());
        view.socket_menu(id, *heading, point);
    });
    heading->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { rows->SetFocus(); });
    heading->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_F2)
            view.rename_socket(id);
        else
            event.Skip();
    });
    endpoint->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (view.rendering || view.busy() || !view.board())
            return;
        const int selected = endpoint->GetSelection();
        if (selected >= 0 && static_cast<std::size_t>(selected) < endpoint_ids.size())
            view.submit(SetSocketEndpoint{
                view.board()->id, id, {endpoint_ids[static_cast<std::size_t>(selected)]}});
    });
    for (wxWindow* child : std::vector<wxWindow*>{this, heading, endpoint, rows})
        child->SetDropTarget(new ApplicationDropTarget(view, *child, id));
}

void SocketColumn::update(const Socket& socket,
                          const std::array<std::vector<std::string>, 3>& groups) {
    style = socket.appearance;
    heading->SetLabelText(ui_text(socket.display_name));
    heading->SetName(ui_text("Socket heading " + id));
    // Rebuild from values, not display labels: duplicate friendly names are legal.
    std::vector<std::optional<std::string>> ids{std::nullopt};
    wxArrayString labels;
    labels.Add("Windows Default");
    bool selected_found = !socket.endpoint.logical_device_id;
    if (view.snapshot)
        for (const auto& output : view.snapshot->inventory.endpoints) {
            if (!output.playback)
                continue;
            ids.push_back(output.logical_id);
            labels.Add(ui_text(output.display_name + (output.ambiguous   ? " (ambiguous)"
                                                      : output.available ? ""
                                                                         : " (unavailable)")));
            selected_found =
                selected_found || socket.endpoint.logical_device_id == output.logical_id;
        }
    if (!selected_found) {
        ids.push_back(socket.endpoint.logical_device_id);
        labels.Add("Saved output (unavailable)");
    }
    if (ids != endpoint_ids || labels != endpoint->GetStrings()) {
        endpoint_ids = std::move(ids);
        endpoint->Set(labels);
    }
    const auto selected =
        std::find(endpoint_ids.begin(), endpoint_ids.end(), socket.endpoint.logical_device_id);
    endpoint->SetSelection(
        selected == endpoint_ids.end() ? 0 : static_cast<int>(selected - endpoint_ids.begin()));
    endpoint->Enable(!view.busy());
    rows->update(groups, view.board()->working.settings.show_inactive_sources);
}
void SocketColumn::palette() {
    SetBackgroundColour(view.appearance.surface);
    rows->SetBackgroundColour(view.appearance.input);
    rows->SetForegroundColour(view.appearance.foreground);
    rows->refresh_metrics();
    const auto colors = socket_heading_colors(style, view.appearance);
    heading->SetBackgroundColour(colors.background);
    heading->SetForegroundColour(colors.foreground);
    const auto signature = style.icon +
                           colors.foreground.GetAsString(wxC2S_HTML_SYNTAX).ToStdString() +
                           std::to_string(FromDIP(20));
    if (signature != rendered_style) {
        heading->SetBitmap(socket_icon(style.icon, colors.foreground, FromDIP(wxSize(20, 20))));
        rendered_style = signature;
    }
    rows->Refresh();
}
SocketLayout SocketColumn::inspect() const {
    SocketLayout result;
    result.id = id;
    result.screen_rectangle = GetScreenRect();
    result.endpoint_control_id = endpoint->GetId();
    result.applications_control_id = rows->GetId();
    result.section_counts = rows->counts;
    result.first_visible_row = rows->GetVisibleBegin();
    for (const auto& row : rows->entries)
        if (!row.heading)
            result.rendered_application_ids.push_back(row.application_id);
    return result;
}

ApplicationRows::ApplicationRows(wxWindow* parent, PlugBoardView::Implementation& state,
                                 std::string socket)
    : wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), view(state),
      socket_id(std::move(socket)) {
    SetName(ui_text("Applications in " + socket_id));
    SetAccessible(new RowsAccessible(this));
    refresh_metrics();
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& event) {
        press = event.GetPosition();
        pressed_application.clear();
        const auto row = row_at(press);
        if (row) {
            SetSelection(static_cast<int>(*row));
            if (entries[*row].heading)
                activate(*row);
            else {
                pressed_application = entries[*row].application_id;
                if (!HasCapture())
                    CaptureMouse();
            }
        }
        SetFocus();
        event.Skip();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent& event) {
        if (event.Dragging() && event.LeftIsDown() && !pressed_application.empty() &&
            !view.busy() &&
            (std::abs(event.GetX() - press.x) > wxSystemSettings::GetMetric(wxSYS_DRAG_X, this) ||
             std::abs(event.GetY() - press.y) > wxSystemSettings::GetMetric(wxSYS_DRAG_Y, this))) {
            UnsetToolTip();
            const auto id = std::exchange(pressed_application, {});
            view.begin_drag(id, *this);
        }
        if (!event.Dragging())
            update_hover_hint(event.GetPosition());
        event.Skip();
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) {
        UnsetToolTip();
        event.Skip();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
        pressed_application.clear();
        if (HasCapture())
            ReleaseMouse();
        event.Skip();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST,
         [this](wxMouseCaptureLostEvent&) { pressed_application.clear(); });
    Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent& event) {
        const auto row = row_at(event.GetPosition());
        if (row && !entries[*row].heading)
            activate(*row);
    });
    Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event) {
        int row = GetSelection();
        auto point = event.GetPosition();
        if (point != wxDefaultPosition) {
            point = ScreenToClient(point);
            const auto hit = row_at(point);
            row = hit ? static_cast<int>(*hit) : wxNOT_FOUND;
        } else if (row >= 0)
            point = GetItemRect(static_cast<std::size_t>(row)).GetBottomLeft();
        if (row >= 0 && static_cast<std::size_t>(row) < entries.size() &&
            !entries[static_cast<std::size_t>(row)].heading) {
            SetSelection(row);
            view.app_menu(entries[static_cast<std::size_t>(row)].application_id, *this, point);
        }
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        const int row = GetSelection();
        if (event.GetKeyCode() == WXK_ESCAPE && HasCapture()) {
            pressed_application.clear();
            ReleaseMouse();
        } else if ((event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_SPACE) &&
                   row >= 0)
            activate(static_cast<std::size_t>(row));
        else
            event.Skip();
    });
    for (const auto type :
         {wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM, wxEVT_SCROLLWIN_LINEUP,
          wxEVT_SCROLLWIN_LINEDOWN, wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN,
          wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE}) {
        Bind(type, [this](wxScrollWinEvent& event) {
            event.Skip();
            auto* state = &view;
            view.CallAfter([state] {
                if (state->module && state->visible)
                    state->request_visible_icons();
            });
        });
    }
}

ApplicationRows::~ApplicationRows() {
    if (HasCapture())
        ReleaseMouse();
}

std::optional<std::size_t> ApplicationRows::row_at(const wxPoint& point) const {
    if (!GetClientRect().Contains(point))
        return {};
    const int row = VirtualHitTest(point.y);
    if (row < 0 || static_cast<std::size_t>(row) >= entries.size())
        return {};
    return static_cast<std::size_t>(row);
}

void ApplicationRows::update(const std::array<std::vector<std::string>, 3>& incoming,
                             bool inactive) {
    if (incoming == groups && inactive == show_inactive && !entries.empty()) {
        update_hover_hint(ScreenToClient(wxGetMousePosition()));
        RefreshAll();
        return;
    }
    // Retain the selected application's identity across activity transitions.
    std::string selected;
    if (GetSelection() >= 0 && static_cast<std::size_t>(GetSelection()) < entries.size())
        selected = entries[static_cast<std::size_t>(GetSelection())].application_id;
    const auto top = GetVisibleBegin();
    groups = incoming;
    show_inactive = inactive;
    entries.clear();
    for (int section = 0; section < 3; ++section) {
        counts[static_cast<std::size_t>(section)] =
            groups[static_cast<std::size_t>(section)].size();
        entries.push_back({section, true, {}});
        if (expanded[static_cast<std::size_t>(section)] && (section != 2 || show_inactive))
            for (const auto& id : groups[static_cast<std::size_t>(section)])
                entries.push_back({section, false, id});
    }
    SetItemCount(entries.size());
    for (std::size_t row = 0; row < entries.size(); ++row)
        if (!selected.empty() && entries[row].application_id == selected)
            SetSelection(static_cast<int>(row));
    if (top < entries.size())
        ScrollToRow(top);
    update_hover_hint(ScreenToClient(wxGetMousePosition()));
    Refresh();
}
void ApplicationRows::update_hover_hint(const wxPoint& point) {
    // A publication can move an app beneath a stationary pointer. Re-evaluate
    // without restarting unchanged tips or leaving one on a heading/empty row.
    const auto hint = IsShownOnScreen() ? hover_text(point) : wxString{};
    if (hint.empty())
        UnsetToolTip();
    else if (GetToolTipText() != hint)
        SetToolTip(hint);
}
void ApplicationRows::activate(std::size_t row) {
    if (row >= entries.size())
        return;
    const auto entry = entries[row];
    if (!entry.heading) {
        view.app_menu(entry.application_id, *this, GetItemRect(row).GetBottomLeft());
        return;
    }
    if (entry.section == 2 && !show_inactive) {
        if (view.busy() || !view.board())
            return;
        // Expanding this group is an explicit request to show inactive sources.
        // Commit the board preference without opening or moving the drawer;
        // the next snapshot renders this socket's expanded rows.
        expanded[2] = true;
        view.submit(SetBoardSettings{view.board()->id, true,
                                     view.board()->working.settings.recently_active_seconds});
        return;
    }
    expanded[static_cast<std::size_t>(entry.section)] =
        !expanded[static_cast<std::size_t>(entry.section)];
    const auto copy = groups;
    entries.clear();
    update(copy, show_inactive);
    view.request_visible_icons();
}
wxString ApplicationRows::accessible_name(std::size_t row) const {
    if (row >= entries.size())
        return {};
    const auto& entry = entries[row];
    if (entry.heading)
        return wxString::Format("%s (%zu), %s", ui_text(section_names[entry.section]),
                                counts[static_cast<std::size_t>(entry.section)],
                                expanded[static_cast<std::size_t>(entry.section)] &&
                                        (entry.section != 2 || show_inactive)
                                    ? "expanded"
                                    : "collapsed");
    const auto* app = view.application(entry.application_id);
    return app ? ui_text(app->display_name + ", " + detail_text(*app))
               : wxString("Unavailable application");
}
wxString ApplicationRows::accessible_help(std::size_t row) const {
    return row < entries.size() && entries[row].heading
               ? wxString("Activate to expand or collapse this activity group. Expanding "
                          "inactive sources also enables them for this board.")
               : wxString("Drag an app to another socket. Relocation without the moving boxes.");
}
wxCoord ApplicationRows::OnMeasureItem(std::size_t row) const {
    return row < entries.size() && entries[row].heading ? FromDIP(30) : row_height;
}
// Measure fonts and DIP geometry together. The icon and the complete text
// block share a vertical center; both fields retain one physical line.
void ApplicationRows::refresh_metrics() {
    wxClientDC dc(this);
    dc.SetFont(GetFont());
    const int title_height = dc.GetCharHeight();
    auto font = GetFont();
    font.SetFractionalPointSize(std::max(7.0, font.GetFractionalPointSize() - 1));
    dc.SetFont(font);
    const int secondary_height = dc.GetCharHeight();
    const int pixels = FromDIP(32);
    const int height = std::max(
        {FromDIP(48), pixels + FromDIP(16), title_height + secondary_height + FromDIP(18)});
    if (name_height == title_height && detail_height == secondary_height && icon_size == pixels &&
        detail_font == font)
        return;
    name_height = title_height;
    detail_height = secondary_height;
    icon_size = pixels;
    detail_font = font;
    row_height = height;
    const auto top = GetVisibleBegin();
    SetItemCount(entries.size());
    if (top < entries.size())
        ScrollToRow(top);
    RefreshAll();
}
ApplicationRows::VisualLayout ApplicationRows::visual_layout(const wxRect& row,
                                                             int secondary_height) const {
    const int gap = FromDIP(2), inset = FromDIP(7);
    const auto status_height = secondary_height > 0 ? secondary_height : detail_height;
    const int text_top = row.y + (row.height - name_height - status_height - gap) / 2;
    const int left = row.x + inset + icon_size + FromDIP(7);
    const int width = std::max(0, row.GetRight() - left - inset);
    return {{row.x + inset, row.y + (row.height - icon_size) / 2, icon_size, icon_size},
            {left, text_top, width, name_height},
            {left, text_top + name_height + gap, width, status_height}};
}
ApplicationRows::StatusVisual ApplicationRows::status_visual(const ApplicationView& app,
                                                             int width) const {
    wxClientDC dc(const_cast<ApplicationRows*>(this));
    auto font = detail_font;
    const auto full = ui_text(detail_text(app));
    dc.SetFont(font);
    // Device labels may be arbitrarily long. Keep playback status on one line;
    // the status-specific tooltip exposes the full actual output name.
    if (app.activity == Activity::Active) {
        const auto text = clipped(dc, full, width);
        return {text, font, dc.GetTextExtent(text)};
    }
    // The age vocabulary is bounded. Usually it fits at the normal secondary
    // font; a slightly smaller readable font handles larger system font metrics
    // without changing two-line row geometry or adding an ellipsis.
    while (dc.GetTextExtent(full).x > width && font.GetFractionalPointSize() > 7.0) {
        font.SetFractionalPointSize(std::max(7.0, font.GetFractionalPointSize() - 0.25));
        dc.SetFont(font);
    }
    return {full, font, dc.GetTextExtent(full)};
}
wxString ApplicationRows::hover_text(const wxPoint& point) const {
    constexpr auto drag_hint =
        "Drag an app to another socket. Relocation without the moving boxes.";
    const auto row = row_at(point);
    if (!row || entries[*row].heading)
        return {};
    const auto* app = view.application(entries[*row].application_id);
    if (!app || app->activity != Activity::Active || !app->observation_fresh || !app->observation ||
        !visual_layout(GetItemRect(*row)).detail.Contains(point))
        return drag_hint;
    wxString outputs;
    for (const auto& id : app->observation->active_endpoint_ids) {
        if (!outputs.empty())
            outputs += "\n";
        outputs += ui_text(view.output_name(id));
    }
    return outputs.empty() ? wxString(drag_hint) : outputs;
}
std::string ApplicationRows::detail_text(const ApplicationView& app) const {
    if (app.activity == Activity::Active && app.observation_fresh && app.observation) {
        if (app.observation->active_endpoint_ids.size() == 1)
            return "Playing: " + view.output_name(app.observation->active_endpoint_ids.front());
        if (app.observation->active_endpoint_ids.size() > 1)
            return "Playing: " + std::to_string(app.observation->active_endpoint_ids.size()) +
                   " outputs";
    }
    if (app.inactive_seconds) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::steady_clock::now() - view.snapshot_received)
                                 .count();
        return last_seen_text(*app.inactive_seconds, elapsed);
    }
    return "Last seen: unknown";
}
void ApplicationRows::OnDrawBackground(wxDC& dc, const wxRect& rectangle, std::size_t row) const {
    const bool heading_row = row < entries.size() && entries[row].heading;
    // Selection remains the keyboard/context-menu target, not a route state.
    // Activity headings and app rows retain their ordinary surface when selected.
    const auto color = heading_row ? view.appearance.surface : view.appearance.input;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(color));
    dc.DrawRectangle(rectangle);
}
void ApplicationRows::OnDrawItem(wxDC& dc, const wxRect& rectangle, std::size_t row) const {
    if (row >= entries.size())
        return;
    const auto& entry = entries[row];
    const int inset = FromDIP(7);
    const auto colors = item_colors(view.appearance, false);
    dc.SetTextForeground(colors.foreground);
    dc.SetFont(GetFont());
    if (entry.heading) {
        const bool open = expanded[static_cast<std::size_t>(entry.section)] &&
                          (entry.section != 2 || show_inactive);
        const auto title = wxString::Format("%s %s (%zu)", open ? L"\u25be" : L"\u25b8",
                                            ui_text(section_names[entry.section]),
                                            counts[static_cast<std::size_t>(entry.section)]);
        dc.DrawText(clipped(dc, title, rectangle.width - inset * 2), rectangle.x + inset,
                    rectangle.y + FromDIP(6));
        return;
    }
    const auto* app = view.application(entry.application_id);
    if (!app)
        return;
    const auto status = status_visual(*app, visual_layout(rectangle).detail.width);
    const auto geometry = visual_layout(rectangle, status.extent.y);
    dc.DrawBitmap(view.icon(app->id), geometry.icon.x, geometry.icon.y, true);
    dc.DrawText(clipped(dc, ui_text(app->display_name), geometry.name.width),
                geometry.name.GetPosition());
    dc.SetFont(status.font);
    dc.SetTextForeground(view.appearance.muted);
    dc.DrawText(status.text, geometry.detail.GetPosition());
}

wxString PlugBoardView::Implementation::drag_payload(const std::string& id) const {
    if (!board() || !application(id))
        return {};
    return ui_text(Document{
        {"format", "audiohole-application-drag"},
        {"version", 1},
        {"board", board()->id},
        {"working_revision", board()->working_revision},
        {"library_revision", snapshot->library->revision},
        {"application",
         id}}.dump());
}
bool PlugBoardView::Implementation::drop(const std::string& destination, const wxString& payload) {
    const auto reject = [&](const std::string& reason) {
        if (module)
            module->record_view_event("view_drop_rejected", reason,
                                      {{"destination_socket_id", destination}}, Severity::Warning);
        // Report internal failures after OLE unwinds; a callback may switch the
        // host tab and must not destroy the source during its native drag loop.
        if (drag_context && options.completed)
            CallAfter([this, reason] {
                if (options.completed)
                    options.completed({false, false, "view_drop_rejected", reason, {}, {}});
            });
        return false;
    };
    try {
        if (!module || !board() || !pending.empty())
            return reject(
                "The board is not ready for a drop. Wait for the current change and retry.");
        const auto text = payload.ToUTF8();
        if (!text || text.length() > 4096)
            return reject("This is not a valid application drag.");
        const auto data = parse_document(std::string(text.data(), text.length()), 4096);
        const auto current = module->snapshot();
        const auto* current_board =
            current->library ? find_board(*current->library, current->selected_board_id) : nullptr;
        if (data.at("format") != "audiohole-application-drag" || data.at("version") != 1)
            return reject("This is not a Plug Board application drag.");
        if (!current_board || data.at("board") != current->selected_board_id ||
            current->selected_board_id != board()->id ||
            data.at("working_revision") != current_board->working_revision)
            return reject("The board changed during the drag. Review it and try again.");
        const auto id = data.at("application").get<std::string>();
        const auto* record = find_application(*current->library, id);
        if (!record || !application(id) ||
            std::none_of(current_board->working.sockets.begin(),
                         current_board->working.sockets.end(),
                         [&](const auto& socket) { return socket.id == destination; }))
            return reject("The dragged application or destination socket is no longer available.");
        if (data.at("library_revision") != current->library->revision) {
            // Discovery of an unrelated app may advance the catalog during a
            // gesture. Accept that change only if this app's verified identity
            // and this board's working configuration are still the same.
            const auto* original = drag_context && drag_context->library
                                       ? find_application(*drag_context->library, id)
                                       : nullptr;
            if (!original || original->identity != record->identity ||
                original->verified_aliases != record->verified_aliases)
                return reject("Application identity changed during the drag. Start a fresh drag.");
        }
        // Keep drag_active true until DoDragDrop returns. A new publication may
        // otherwise rebuild/destroy the very list whose OLE source is still live.
        pending.push_back(module->submit(MoveApplication{current_board->id, id, destination},
                                         current->library->revision, current_board->id));
        if (visible)
            completion_timer.Start(50);
        module->record_view_event("view_drop_submitted",
                                  "Application move submitted for durable assignment.",
                                  {{"application_id", id}, {"destination_socket_id", destination}});
        return true;
    } catch (const std::exception&) {
        return reject("The drop data could not be read. Drag the application row again.");
    }
}
void PlugBoardView::Implementation::begin_drag(const std::string& id, wxWindow& source) {
    if (busy())
        return;
    module->record_view_event("view_drag_started", "Application mouse gesture started drag/drop.",
                              {{"application_id", id}, {"board_id", board()->id}});
    drag_context = snapshot;
    if (source.HasCapture())
        source.ReleaseMouse();
    wxTextDataObject data(drag_payload(id));
    wxDropSource drag(&source);
    drag.SetData(data);
    drag_active = true;
    drag_source_board = board()->id;
    drag_timer.Start(50);
    const auto result = drag.DoDragDrop(wxDrag_DefaultMove);
    module->record_view_event("view_drag_finished", "Native drag/drop gesture finished.",
                              {{"result", drag_result_name(result)}, {"application_id", id}},
                              result == wxDragError ? Severity::Error : Severity::Information);
    end_drag();
}
void PlugBoardView::Implementation::drag_over(const std::string& id, const wxPoint&) {
    highlighted_socket = id;
}
void PlugBoardView::Implementation::end_drag() {
    drag_active = false;
    drag_context.reset();
    drag_timer.Stop();
    highlighted_socket.clear();
    if (deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
}
void PlugBoardView::Implementation::app_menu(const std::string& id, wxWindow& owner,
                                             wxPoint point) {
    if (busy() || !application(id) || !board())
        return;
    const auto context = snapshot;
    const auto board_id = board()->id;
    std::vector<std::string> labels;
    std::vector<std::string> targets;
    wxMenu menu;
    auto* move = new wxMenu;
    const int first = wxWindow::NewControlId(static_cast<int>(board()->working.sockets.size()));
    for (const auto& socket : board()->working.sockets) {
        const int command = first + static_cast<int>(labels.size());
        move->Append(
            new MenuItem(move, command, wxControl::EscapeMnemonics(ui_text(socket.display_name))));
        labels.push_back("Move to " + socket.display_name);
        targets.push_back(socket.id);
    }
    menu.Append(new MenuItem(&menu, wxID_ANY, "Move to", move));
    apply_menu_appearance(menu, appearance);
    ++interaction_depth;
    std::optional<std::size_t> selection;
    if (options.menu)
        selection = options.menu(menu, labels);
    else {
        const int chosen = owner.GetPopupMenuSelectionFromUser(menu, point);
        if (chosen >= first)
            selection = static_cast<std::size_t>(chosen - first);
    }
    --interaction_depth;
    if (selection && *selection < labels.size()) {
        submit(MoveApplication{board_id, id, targets[*selection]}, context);
    }
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
}
void PlugBoardView::Implementation::socket_menu(const std::string& id, wxWindow& owner,
                                                wxPoint point) {
    if (busy() || !board())
        return;
    const auto found =
        std::find_if(board()->working.sockets.begin(), board()->working.sockets.end(),
                     [&](const auto& socket) { return socket.id == id; });
    if (found == board()->working.sockets.end())
        return;
    const auto context = snapshot;
    const auto board_id = board()->id;
    const auto style = found->appearance;
    // All leaves have one stable flattened action index for native and injected
    // presenters. Rename/Delete retain 0/1; presentation never invokes policy.
    const auto action_count = 2 + (socket_colors.size() + 1) + (socket_icons.size() + 1);
    const int first = wxWindow::NewControlId(static_cast<int>(action_count));
    wxMenu menu;
    std::vector<std::string> labels{"Rename", "Delete"};
    menu.Append(new MenuItem(&menu, first, "Rename"));
    menu.Append(new MenuItem(&menu, first + 1, "Delete"))->Enable(id != "system");
    auto* recolor = new wxMenu;
    auto* automatic =
        recolor->Append(new MenuItem(recolor, first + 2, "Use theme color", nullptr, wxITEM_RADIO));
    if (style.color == "default")
        automatic->Check();
    labels.push_back("Recolor: Use theme color");
    for (const auto& color : socket_colors) {
        auto* item =
            recolor->Append(new MenuItem(recolor, first + static_cast<int>(labels.size()),
                                         ui_text(std::string(color.name)), nullptr, wxITEM_RADIO));
        if (style.color == color.id)
            item->Check();
        labels.push_back("Recolor: " + std::string(color.name));
    }
    menu.Append(new MenuItem(&menu, wxID_ANY, "Recolor", recolor));
    const auto icon_begin = labels.size();
    auto* icons = new wxMenu;
    auto* none = icons->Append(new MenuItem(icons, first + static_cast<int>(labels.size()), "None",
                                            nullptr, wxITEM_RADIO));
    if (style.icon == "none")
        none->Check();
    labels.push_back("Icon: None");
    for (const auto& icon : socket_icons) {
        auto* item = new MenuItem(icons, first + static_cast<int>(labels.size()),
                                  ui_text(std::string(icon.name)), nullptr, wxITEM_RADIO);
        item->SetBitmap(socket_icon(std::string(icon.id), appearance.foreground,
                                    panel.FromDIP(wxSize(18, 18))));
        icons->Append(item);
        if (style.icon == icon.id)
            item->Check();
        labels.push_back("Icon: " + std::string(icon.name));
    }
    menu.Append(new MenuItem(&menu, wxID_ANY, "Icon", icons));
    apply_menu_appearance(menu, appearance);
    ++interaction_depth;
    std::optional<std::size_t> selected;
    if (options.menu)
        selected = options.menu(menu, labels);
    else {
        const int command = owner.GetPopupMenuSelectionFromUser(menu, point);
        if (command >= first && command < first + static_cast<int>(labels.size()))
            selected = static_cast<std::size_t>(command - first);
    }
    if (selected == 0)
        rename_socket(id);
    else if (selected == 1 && id != "system")
        remove_socket(id);
    else if (selected && *selected >= 2 && *selected < icon_begin) {
        const auto color =
            *selected == 2 ? "default" : std::string(socket_colors[*selected - 3].id);
        submit(SetSocketAppearance{board_id, id, color, {}}, context);
    } else if (selected && *selected >= icon_begin && *selected < labels.size()) {
        const auto icon = *selected == icon_begin
                              ? "none"
                              : std::string(socket_icons[*selected - icon_begin - 1].id);
        submit(SetSocketAppearance{board_id, id, {}, icon}, context);
    }
    --interaction_depth;
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
}

} // namespace audiohole::plug_board::desktop
