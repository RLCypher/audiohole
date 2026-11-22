#include "dialogs.hpp"
#include "view_state.hpp"

#include <wx/artprov.h>
#include <wx/choicdlg.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <cmath>
#include <windows.h>

namespace audiohole::plug_board::desktop {

wxString ui_text(const std::string& value) {
    return wxString::FromUTF8(value);
}
namespace {
std::string utf8(const wxString& value) {
    const auto bytes = value.ToUTF8();
    return bytes ? std::string(bytes.data(), bytes.length()) : std::string{};
}
} // namespace

void ViewMailbox::post(std::shared_ptr<const ModuleSnapshot> snapshot) {
    std::lock_guard lock(mutex);
    latest = std::move(snapshot);
    if (receiver && enabled && !queued) {
        queued = true;
        wxQueueEvent(receiver, new wxThreadEvent(wxEVT_THREAD, event_id));
    }
}

PlugBoardView::Implementation::Implementation(PlugBoardView& owner, Module& service,
                                              ViewOptions hooks)
    : panel(owner), module(&service), options(std::move(hooks)) {
    appearance = appearance_for("Follow System");
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        reduced_motion = !animations;
    mailbox->receiver = this;
    mailbox->event_id = wxWindow::NewControlId();
    Bind(wxEVT_THREAD, &Implementation::receive, this, mailbox->event_id);
    Bind(wxEVT_TIMER, [this](wxTimerEvent& event) {
        if (event.GetId() == completion_timer.GetId())
            poll_commands();
        else if (event.GetId() == animation_timer.GetId()) {
            const double step = 0.2;
            drawer_fraction += drawer_fraction < animation_target ? step : -step;
            if (std::abs(drawer_fraction - animation_target) <= step) {
                drawer_fraction = animation_target;
                animation_timer.Stop();
                animation_running = false;
            }
            layout();
        } else if (event.GetId() == drag_timer.GetId() && drag_active) {
            const auto point = viewport->ScreenToClient(wxGetMousePosition());
            const int exposed = viewport->GetClientSize().x -
                                (drawer_open ? panel.FromDIP(drawer_width_dip + tab_width_dip)
                                             : panel.FromDIP(tab_width_dip));
            auto scroll = viewport->GetViewStart();
            if (point.y >= 0 && point.y < viewport->GetClientSize().y) {
                if (point.x < panel.FromDIP(24))
                    scroll.x = std::max(0, scroll.x - 1);
                else if (point.x > exposed - panel.FromDIP(24) && point.x <= exposed)
                    ++scroll.x;
                viewport->Scroll(scroll.x, 0);
            }
        } else if (event.GetId() == age_timer.GetId() && visible) {
            const auto bounds = viewport->GetScreenRect();
            for (auto* column : columns)
                if (bounds.Intersects(column->GetScreenRect()))
                    column->rows->Refresh();
        }
    });
    build();
    panel.Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        layout();
        event.Skip();
    });
    panel.Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (event.ControlDown() && event.GetKeyCode() == 'S' && board() && !busy()) {
            if (event.ShiftDown())
                copy_board();
            else
                submit(SaveBoard{board()->id});
        } else
            event.Skip();
    });
    const auto weak = std::weak_ptr<ViewMailbox>(mailbox);
    subscription = module->subscribe([weak](auto snapshot) {
        if (const auto sink = weak.lock())
            sink->post(std::move(snapshot));
    });
    snapshot = module->snapshot();
    render(snapshot);
}
PlugBoardView::Implementation::~Implementation() {
    detach();
}
void PlugBoardView::Implementation::detach() {
    finish_socket_drag(false);
    completion_timer.Stop();
    animation_timer.Stop();
    drag_timer.Stop();
    age_timer.Stop();
    {
        std::lock_guard lock(mailbox->mutex);
        mailbox->receiver = nullptr;
        mailbox->latest.reset();
        mailbox->enabled = false;
    }
    DeletePendingEvents();
    if (module) {
        module->unsubscribe(subscription);
        module->set_visible(false);
        module = nullptr;
    }
    options.completed = {};
    pending.clear();
}
void PlugBoardView::Implementation::receive(wxThreadEvent&) {
    std::shared_ptr<const ModuleSnapshot> latest;
    {
        std::lock_guard lock(mailbox->mutex);
        latest = std::exchange(mailbox->latest, {});
        mailbox->queued = false;
    }
    if (visible && latest)
        render(std::move(latest));
}

wxButton* PlugBoardView::Implementation::button(wxWindow* parent, const wxString& label,
                                                const wxString& help, const wxString& name) {
    auto* result =
        options.button ? options.button(parent, label, help) : new Button(parent, label, help);
    result->SetName(name.empty() ? label : name);
    result->SetToolTip(help);
    return result;
}
wxChoice* PlugBoardView::Implementation::choice(wxWindow* parent, const wxString& name,
                                                const wxString& help) {
    auto* result = options.choice ? options.choice(parent, name) : new Choice(parent, wxID_ANY);
    result->SetName(name);
    result->SetToolTip(help);
    return result;
}
void PlugBoardView::Implementation::build() {
    viewport = new wxScrolledWindow(&panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxHSCROLL | wxBORDER_NONE);
    viewport->SetName("Socket workspace");
    viewport->SetScrollRate(panel.FromDIP(12), 0);
    viewport->ShowScrollbars(wxSHOW_SB_ALWAYS, wxSHOW_SB_NEVER);
    for (const auto type :
         {wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM, wxEVT_SCROLLWIN_LINEUP,
          wxEVT_SCROLLWIN_LINEDOWN, wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN,
          wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE}) {
        viewport->Bind(type, [this](wxScrollWinEvent& event) {
            event.Skip();
            CallAfter([this] {
                if (module && visible)
                    request_visible_icons();
            });
        });
    }
    empty = new wxStaticText(viewport, wxID_ANY, "Loading Routing Boards...");
    empty->SetName("Plug Board load status");
    build_drawer();
    tab = new DrawerTab(&panel, [this] { set_drawer(!drawer_open); });
    // wx omits sibling clipping for ordinary non-overlapping layouts. This
    // overlay needs it: a column repaint must not erase the drawer or tab.
    for (wxWindow* window : std::vector<wxWindow*>{viewport, drawer, tab}) {
        const auto handle = static_cast<HWND>(window->GetHandle());
        SetWindowLongPtrW(handle, GWL_STYLE,
                          GetWindowLongPtrW(handle, GWL_STYLE) | WS_CLIPSIBLINGS);
    }
    palette();
    layout();
}
const RoutingBoard* PlugBoardView::Implementation::board() const {
    return snapshot && snapshot->library
               ? find_board(*snapshot->library, snapshot->selected_board_id)
               : nullptr;
}
const ApplicationView* PlugBoardView::Implementation::application(const std::string& id) const {
    const auto found = applications.find(id);
    return found == applications.end() ? nullptr : found->second;
}
bool PlugBoardView::Implementation::busy() const {
    return !module || !snapshot || snapshot->phase != ModulePhase::Ready || !pending.empty() ||
           drag_active;
}

void PlugBoardView::Implementation::render(std::shared_ptr<const ModuleSnapshot> value) {
    if (drag_active || interaction_depth) {
        deferred_snapshot = std::move(value);
        return;
    }
    snapshot = std::move(value);
    snapshot_received = std::chrono::steady_clock::now();
    applications.clear();
    placements.clear();
    rendering = true;
    if (snapshot)
        for (const auto& app : snapshot->applications)
            applications.emplace(app.id, &app);
    const auto* current = board();
    if (!current) {
        empty->SetLabel(snapshot && snapshot->phase == ModulePhase::ReadOnlyError
                            ? "Configuration could not be loaded. Your saved files have been "
                              "preserved. Close Plug Board before correcting the library file."
                            : "Loading Routing Boards");
        empty->Show();
    } else {
        empty->Hide();
        std::vector<std::string> order;
        for (const auto& socket : current->working.sockets)
            order.push_back(socket.id);
        std::vector<std::string> old;
        for (const auto* column : columns)
            old.push_back(column->id);
        if (rendered_board != current->id || old != order) {
            // A profile/board change can arrive while a row's event handler is
            // unwinding. Hide now and let wx destroy it after the current event.
            for (auto* column : columns) {
                column->Hide();
                column->Destroy();
            }
            columns.clear();
            for (const auto& socket : current->working.sockets)
                columns.push_back(new SocketColumn(viewport, *this, socket));
        }
        std::map<std::string, std::array<std::vector<std::string>, 3>> groups;
        for (const auto& app : snapshot->applications) {
            const auto placement =
                application_placement(current->working, app, snapshot->inventory);
            placements.emplace(app.id, placement);
            if (!app.hidden_inactive)
                groups[placement.socket_id][static_cast<std::size_t>(app.activity)].push_back(
                    app.id);
        }
        for (auto& [id, sections] : groups)
            for (auto& section : sections)
                std::sort(
                    section.begin(), section.end(), [&](const auto& first, const auto& second) {
                        return application(first)->display_name < application(second)->display_name;
                    });
        for (std::size_t index = 0; index < columns.size(); ++index)
            columns[index]->update(current->working.sockets[index], groups[columns[index]->id]);
        rendered_working_revision = current->working_revision;
    }
    render_drawer();
    rendered_board = current ? current->id : std::string{};
    layout();
    palette();
    rendering = false;
    if (visible)
        CallAfter([this] {
            if (module && visible)
                request_visible_icons();
        });
}

void PlugBoardView::Implementation::layout() {
    const auto size = panel.GetClientSize();
    if (size.x <= 0 || size.y <= 0 || !viewport)
        return;
    viewport->SetSize(0, 0, size.x, size.y);
    const int width = panel.FromDIP(socket_width_dip), gap = panel.FromDIP(socket_gap_dip);
    const int inset = panel.FromDIP(8);
    const int content = inset + static_cast<int>(columns.size()) * (width + gap);
    // Reserve the covered trailing space consistently, so toggling the overlay
    // preserves the exact scroll offset and can reveal the final socket fully.
    viewport->SetVirtualSize(
        std::max(size.x, content + panel.FromDIP(drawer_width_dip + tab_width_dip)),
        viewport->GetClientSize().y);
    const auto height = viewport->GetClientSize().y;
    for (std::size_t index = 0; index < columns.size(); ++index) {
        int x = 0, y = 0;
        viewport->CalcScrolledPosition(inset + static_cast<int>(index) * (width + gap), 0, &x, &y);
        columns[index]->SetSize(x, 0, width, height);
    }
    empty->SetPosition(wxPoint(inset, panel.FromDIP(12)));
    empty->SetSize(std::max(0, size.x - panel.FromDIP(32)), panel.FromDIP(60));
    const int displayed =
        static_cast<int>(std::lround(panel.FromDIP(drawer_width_dip) * drawer_fraction));
    drawer->SetSize(size.x - displayed, 0, panel.FromDIP(drawer_width_dip), height);
    drawer->Show(displayed > 0);
    drawer->Raise();
    const int tab_height = std::min(panel.FromDIP(180), height);
    tab->SetSize(size.x - displayed - panel.FromDIP(tab_width_dip),
                 std::max(0, (height - tab_height) / 2), panel.FromDIP(tab_width_dip), tab_height);
    tab->expanded = drawer_open;
    tab->Raise();
    tab->Refresh();
}

void PlugBoardView::Implementation::palette() {
    if (!viewport)
        return;
    apply_appearance(panel, appearance);
    empty->SetForegroundColour(appearance.muted);
    for (auto* column : columns)
        column->palette();
    if (tab)
        tab->set_appearance(appearance);
}

void PlugBoardView::Implementation::submit(Command command,
                                           std::shared_ptr<const ModuleSnapshot> context) {
    if (busy())
        return;
    if (!context)
        context = snapshot;
    if (!context || !context->library)
        return;
    pending.push_back(
        module->submit(std::move(command), context->library->revision, context->selected_board_id));
    render_drawer();
    for (auto* column : columns)
        column->endpoint->Disable();
    if (visible)
        completion_timer.Start(50);
}
void PlugBoardView::Implementation::poll_commands() {
    std::vector<CommandResult> results;
    for (auto current = pending.begin(); current != pending.end();) {
        if (current->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            ++current;
            continue;
        }
        try {
            results.push_back(current->get());
        } catch (const std::exception& error) {
            results.push_back({false, false, "command_failed", error.what(), {}, {}});
        }
        current = pending.erase(current);
    }
    if (pending.empty())
        completion_timer.Stop();
    if (module && visible)
        render(module->snapshot());
    const auto callback = options.completed;
    for (const auto& result : results)
        if (callback)
            callback(result);
}

bool PlugBoardView::Implementation::confirm(const std::string& title, const std::string& message) {
    ++interaction_depth;
    const bool answer = options.confirm ? options.confirm(title, message)
                                        : show_message(&panel, ui_text(message), ui_text(title),
                                                       wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                                                       appearance) == wxID_YES;
    --interaction_depth;
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
    return answer;
}
std::optional<std::string> PlugBoardView::Implementation::name(const std::string& title,
                                                               const std::string& current) {
    ++interaction_depth;
    std::optional<std::string> answer;
    if (options.name)
        answer = options.name(title, current);
    else {
        wxTextEntryDialog dialog(&panel, ui_text(title), "AudioHole Plug Board", ui_text(current));
        if (show_themed_dialog(dialog, appearance) == wxID_OK)
            answer = utf8(dialog.GetValue());
    }
    --interaction_depth;
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
    return answer;
}
std::optional<std::size_t>
PlugBoardView::Implementation::select(const std::string& title,
                                      const std::vector<std::string>& values) {
    ++interaction_depth;
    std::optional<std::size_t> result;
    if (options.select)
        result = options.select(title, values);
    else {
        result = choose_item(&panel, ui_text(title), values, appearance);
    }
    --interaction_depth;
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
    return result;
}

std::string PlugBoardView::Implementation::output_name(const std::string& id) const {
    if (snapshot)
        for (const auto& endpoint : snapshot->inventory.endpoints)
            if (endpoint.windows_id == id)
                return endpoint.display_name.empty() ? "Unnamed playback output"
                                                     : endpoint.display_name;
    return "Unmapped playback output";
}
wxBitmap PlugBoardView::Implementation::icon(const std::string& id) {
    const auto* app = application(id);
    if (app && app->image && app->image->width && app->image->height &&
        app->image->rgba.size() == app->image->width * app->image->height * 4) {
        const auto old = bitmap_sources.find(id);
        if (old == bitmap_sources.end() || old->second.lock() != app->image) {
            const auto& source = *app->image;
            wxImage decoded(static_cast<int>(source.width), static_cast<int>(source.height));
            decoded.InitAlpha();
            for (std::size_t pixel = 0; pixel < source.width * source.height; ++pixel) {
                std::copy_n(source.rgba.data() + pixel * 4, 3, decoded.GetData() + pixel * 3);
                decoded.GetAlpha()[pixel] = source.rgba[pixel * 4 + 3];
            }
            const int size = panel.FromDIP(32);
            bitmaps[id] = wxBitmap(decoded.Scale(size, size, wxIMAGE_QUALITY_HIGH));
            bitmap_sources[id] = app->image;
        }
        bitmap_usage[id] = ++bitmap_sequence;
        while (bitmaps.size() > 256) {
            const auto oldest = std::min_element(
                bitmap_usage.begin(), bitmap_usage.end(),
                [](const auto& first, const auto& second) { return first.second < second.second; });
            bitmaps.erase(oldest->first);
            bitmap_sources.erase(oldest->first);
            bitmap_usage.erase(oldest);
        }
        return bitmaps.at(id);
    }
    return wxArtProvider::GetBitmap(wxART_EXECUTABLE_FILE, wxART_OTHER,
                                    panel.FromDIP(wxSize(32, 32)));
}

void PlugBoardView::Implementation::request_visible_icons() {
    if (!module || !visible || drag_active)
        return;
    auto exposed = viewport->GetScreenRect();
    exposed.width =
        std::max(0, exposed.width - panel.FromDIP(drawer_open ? drawer_width_dip + tab_width_dip
                                                              : tab_width_dip));
    std::vector<std::string> ids;
    bool recent_visible = false;
    for (auto* column : columns)
        if (exposed.Intersects(column->GetScreenRect())) {
            for (std::size_t row = column->rows->GetVisibleBegin();
                 row < column->rows->GetVisibleEnd() && row < column->rows->entries.size(); ++row) {
                const auto& entry = column->rows->entries[row];
                if (!entry.heading && ids.size() < 256) {
                    ids.push_back(entry.application_id);
                    const auto* app = application(entry.application_id);
                    recent_visible = recent_visible || (app && app->inactive_seconds &&
                                                        *app->inactive_seconds <= 28 * 86400);
                }
            }
        }
    if (!ids.empty())
        module->request_images(std::move(ids));
    if (recent_visible && !age_timer.IsRunning())
        age_timer.Start(1000);
    if (!recent_visible)
        age_timer.Stop();
}

void PlugBoardView::Implementation::set_drawer(bool open) {
    // Tutorial guidance is refreshed while the user is interacting. Repeated
    // requests must neither restart the animation nor move focus off a button
    // between mouse-down and mouse-up (which can swallow its first click).
    if (drawer_open == open)
        return;
    drawer_open = open;
    animation_target = open ? 1 : 0;
    if (reduced_motion || !visible) {
        drawer_fraction = animation_target;
        animation_running = false;
        animation_timer.Stop();
        layout();
    } else {
        animation_running = true;
        animation_timer.Start(15);
    }
    if (open)
        boards->SetFocus();
}
void PlugBoardView::Implementation::ensure_socket_visible(const std::string& id) {
    const auto found = std::find_if(columns.begin(), columns.end(),
                                    [&](const auto* column) { return column->id == id; });
    if (found == columns.end())
        return;
    const int step = panel.FromDIP(12);
    const int exposed =
        viewport->GetClientSize().x -
        panel.FromDIP(drawer_open ? drawer_width_dip + tab_width_dip : tab_width_dip);
    const auto rectangle = (*found)->GetRect();
    auto offset = viewport->GetViewStart();
    if (rectangle.x < 0)
        offset.x = std::max(0, offset.x + rectangle.x / step);
    else if (rectangle.GetRight() >= exposed)
        offset.x += (rectangle.GetRight() - exposed + step) / step;
    viewport->Scroll(offset.x, 0);
}

ViewLayout PlugBoardView::Implementation::inspect() const {
    ViewLayout result;
    result.drawer_open = drawer_open;
    result.drawer_animating = animation_running;
    result.dragging = drag_active;
    result.command_pending = !pending.empty();
    result.viewport_screen_rectangle = viewport->GetScreenRect();
    result.drawer_screen_rectangle = drawer->GetScreenRect();
    result.tab_screen_rectangle = tab->GetScreenRect();
    result.horizontal_scroll_units = viewport->GetViewStart();
    for (const auto* column : columns)
        result.sockets.push_back(column->inspect());
    return result;
}

PlugBoardView::PlugBoardView(wxWindow* parent, Module& module, ViewOptions options)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL) {
    SetName("Plug Board");
    implementation_ = std::make_unique<Implementation>(*this, module, std::move(options));
}
PlugBoardView::~PlugBoardView() {
    implementation_->detach();
    DestroyChildren();
}
void PlugBoardView::set_host_visible(bool value) {
    auto& state = *implementation_;
    state.visible = value;
    {
        std::lock_guard lock(state.mailbox->mutex);
        state.mailbox->enabled = value;
    }
    if (state.module)
        state.module->set_visible(value);
    if (value && state.module) {
        state.poll_commands();
        state.render(state.module->snapshot());
    } else {
        state.completion_timer.Stop();
        state.age_timer.Stop();
        state.drag_timer.Stop();
    }
}
void PlugBoardView::set_appearance(const ViewAppearance& appearance) {
    implementation_->appearance = appearance;
    implementation_->palette();
}
void PlugBoardView::set_drawer_open(bool open) {
    implementation_->set_drawer(open);
}
void PlugBoardView::detach_module() {
    implementation_->detach();
}
bool PlugBoardView::dragging() const {
    return implementation_->drag_active;
}
ViewLayout PlugBoardView::layout_diagnostics() const {
    return implementation_->inspect();
}
void PlugBoardView::ensure_socket_visible(const std::string& id) {
    implementation_->ensure_socket_visible(id);
}
std::optional<wxRect> PlugBoardView::application_screen_rect(const std::string& id, bool reveal) {
    for (auto* column : implementation_->columns) {
        auto* rows = column->rows;
        for (std::size_t index = 0; index < rows->entries.size(); ++index) {
            if (rows->entries[index].heading || rows->entries[index].application_id != id)
                continue;
            if (reveal) {
                implementation_->ensure_socket_visible(column->id);
                rows->ScrollToRow(index);
            }
            auto rectangle = rows->GetItemRect(index);
            rectangle.Intersect(rows->GetClientRect());
            if (rectangle.IsEmpty())
                return {};
            rectangle.SetPosition(rows->ClientToScreen(rectangle.GetPosition()));
            return rectangle;
        }
    }
    return {};
}

} // namespace audiohole::plug_board::desktop
