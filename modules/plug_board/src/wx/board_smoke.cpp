#include "audiohole/plug_board/socket_styles.hpp"
#include "board_frame.hpp"
#include "popup_smoke.hpp"
#include "socket_style.hpp"

#include <windows.h>
#include <wx/access.h>
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#include <wx/listctrl.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace audiohole::plug_board::desktop {
namespace {
thread_local BoardSmokeDriver* pointer_target = nullptr;
LRESULT CALLBACK pointer_barrier(int code, WPARAM message, LPARAM details) {
    // SendInput returning only proves queuing. A thread-local hook acknowledges
    // actual mouse-up dispatch before the test inspects commands/menus. It is
    // installed only in explicit smoke mode and never monitors another thread.
    if (code >= 0 && pointer_target && (message == WM_LBUTTONUP || message == WM_RBUTTONUP))
        pointer_target->pointer_delivered = true;
    return CallNextHookEx(nullptr, code, message, details);
}
void require(bool condition, const std::string& message, std::vector<std::string>& assertions) {
    if (!condition)
        throw std::runtime_error(message);
    assertions.push_back(message);
}
void click(wxWindow* control) {
    if (!control || !control->IsEnabled())
        throw std::runtime_error("Requested smoke control is unavailable");
    wxCommandEvent event(wxEVT_BUTTON, control->GetId());
    event.SetEventObject(control);
    control->GetEventHandler()->ProcessEvent(event);
}
void choose(wxChoice* control, int selected) {
    control->SetSelection(selected);
    wxCommandEvent event(wxEVT_CHOICE, control->GetId());
    event.SetEventObject(control);
    event.SetInt(selected);
    control->GetEventHandler()->ProcessEvent(event);
}
void check(wxCheckBox* control, bool selected) {
    control->SetValue(selected);
    wxCommandEvent event(wxEVT_CHECKBOX, control->GetId());
    event.SetEventObject(control);
    event.SetInt(selected);
    control->GetEventHandler()->ProcessEvent(event);
}
void capture(wxWindow& frame, const std::filesystem::path& path) {
    frame.Raise();
    frame.Update();
    const auto rectangle = frame.GetScreenRect();
    wxBitmap bitmap(rectangle.width, rectangle.height);
    wxScreenDC source;
    wxMemoryDC destination(bitmap);
    destination.Blit(0, 0, rectangle.width, rectangle.height, &source, rectangle.x, rectangle.y);
    destination.SelectObject(wxNullBitmap);
    if (!bitmap.SaveFile(wxString(path.wstring()), wxBITMAP_TYPE_BMP))
        throw std::runtime_error("Could not save current GUI screenshot");
}
wxPoint center(const wxRect& rectangle) {
    return wxPoint(rectangle.x + rectangle.width / 2, rectangle.y + rectangle.height / 2);
}
wxPoint socket_point(wxListBox& list, std::size_t row) {
    wxRect rectangle;
    if (!list.GetItemRect(row, rectangle) || !list.GetClientRect().Contains(center(rectangle)))
        throw std::runtime_error("Socket drag fixture row is not visible");
    return list.ClientToScreen(center(rectangle));
}
// Inspect pixels on the actual native nonclient scrollbar, not just palette
// properties. The sampled edge is outside the inset thumb and arrow glyphs.
void check_scrollbar(wxWindow& owner, LONG object, const Appearance& colors,
                     std::vector<std::string>& assertions) {
    const auto handle = static_cast<HWND>(owner.GetHandle());
    RedrawWindow(handle, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
    SCROLLBARINFO bar{};
    bar.cbSize = sizeof(bar);
    require(GetScrollBarInfo(handle, object, &bar) && !(bar.rgstate[0] & STATE_SYSTEM_INVISIBLE),
            "Theme check reached a visible native scrollbar", assertions);
    RECT bounds{};
    GetWindowRect(handle, &bounds);
    const int x = object == OBJID_VSCROLL ? bar.rcScrollBar.left + 1
                                          : (bar.rcScrollBar.left + bar.rcScrollBar.right) / 2;
    const int y = object == OBJID_HSCROLL ? bar.rcScrollBar.top + 1
                                          : (bar.rcScrollBar.top + bar.rcScrollBar.bottom) / 2;
    const auto dc = GetWindowDC(handle);
    const auto actual = GetPixel(dc, x - bounds.left, y - bounds.top);
    ReleaseDC(handle, dc);
    require(actual == RGB(colors.surface.Red(), colors.surface.Green(), colors.surface.Blue()),
            "Native scrollbar pixels match the chosen app palette", assertions);
}
void check_popup(wxChoice& choice, const Appearance& colors, const std::filesystem::path& file,
                 std::vector<std::string>& assertions) {
    const auto original = choice.GetStrings();
    const int selected = choice.GetSelection();
    for (unsigned index = 0; index < 45; ++index)
        choice.Append(wxString::Format("Theme popup row %u", index));
    const auto handle = static_cast<HWND>(choice.GetHandle());
    SendMessageW(handle, CB_SHOWDROPDOWN, TRUE, 0);
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    GetComboBoxInfo(handle, &info);
    require(info.hwndList && IsWindowVisible(info.hwndList),
            "Theme check opened the actual dropdown popup", assertions);
    RedrawWindow(info.hwndList, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
    RECT client{};
    GetClientRect(info.hwndList, &client);
    const auto dc = GetDC(info.hwndList);
    const auto actual = GetPixel(dc, client.right - 3, client.bottom - 3);
    ReleaseDC(info.hwndList, dc);
    require(actual == RGB(colors.input.Red(), colors.input.Green(), colors.input.Blue()),
            "Dropdown popup pixels follow the app palette", assertions);
    SendMessageW(info.hwndList, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
    require(choice.GetSelection() == selected,
            "Popup wheel cannot silently change a routing selection", assertions);
    wxScreenDC screen;
    const auto size = screen.GetSize();
    wxBitmap bitmap(size.x, size.y);
    wxMemoryDC memory(bitmap);
    memory.Blit(0, 0, size.x, size.y, &screen, 0, 0);
    memory.SelectObject(wxNullBitmap);
    bitmap.SaveFile(wxString(file.wstring()), wxBITMAP_TYPE_BMP);
    SendMessageW(handle, CB_SHOWDROPDOWN, FALSE, 0);
    choice.Set(original);
    choice.SetSelection(selected);
}
} // namespace
ViewOptions BoardSmokeDriver::hooks() {
    ViewOptions result;
    result.confirm = [this](const auto&, const auto&) {
        assertions.push_back("A destructive action presented confirmation");
        return true;
    };
    result.name = [this](const auto&, const auto&) {
        ++name_count;
        return std::optional(next_name);
    };
    result.select = [](const auto&, const auto& choices) -> std::optional<std::size_t> {
        return choices.empty() ? std::nullopt : std::optional<std::size_t>(0);
    };
    result.menu = [this](const wxMenu& menu, const auto& entries) {
        ++menu_count;
        require(!entries.empty(), "Context menu contains actions", assertions);
        const auto* first = menu.FindItemByPosition(0);
        require(first != nullptr, "Presenter receives the actual constructed menu", assertions);
        if (entries.front() == "Rename") {
            const auto* remove = menu.FindItemByPosition(1);
            require(menu.GetMenuItemCount() == 4 && first->GetItemLabelText() == "Rename" &&
                        remove && remove->GetItemLabelText() == "Delete",
                    "Socket context menu has Rename and Delete actions", assertions);
            last_socket_delete_enabled = remove->IsEnabled();
            const auto* colors = menu.FindItemByPosition(2)->GetSubMenu();
            const auto* icons = menu.FindItemByPosition(3)->GetSubMenu();
            require(colors && colors->GetMenuItemCount() == 17 && icons &&
                        icons->GetMenuItemCount() == 19,
                    "Socket menu offers 16 colors and 18 Material icons with automatic/none resets",
                    assertions);
        } else {
            const auto* move = first->GetSubMenu();
            require(first->GetItemLabelText() == "Move to" && move &&
                        move->GetMenuItemCount() == entries.size() && menu.GetMenuItemCount() == 1,
                    "Audio source menu contains a Move to submenu for every socket", assertions);
            for (std::size_t index = 0; index < move->GetMenuItemCount(); ++index)
                require(move->FindItemByPosition(index)->GetItemLabelText() ==
                            ui_text(entries[index].substr(8)),
                        "Move to submenu uses the available socket names", assertions);
        }
        return menu_selection;
    };
    result.open_url = [this](const std::string& uri) {
        opened_uri = uri;
        return true;
    };
    result.completed = [this](const auto& result) {
        if (!result.ok)
            error = result.code + ": " + result.message;
    };
    return result;
}

BoardSmokeDriver::~BoardSmokeDriver() {
    if (pointer_input.joinable()) {
        pointer_input.request_stop();
        pointer_input.join();
    }
    if (pointer_hook)
        UnhookWindowsHookEx(static_cast<HHOOK>(pointer_hook));
    if (pointer_target == this)
        pointer_target = nullptr;
}
void BoardSmokeDriver::pointer_gesture(wxPoint source, std::optional<wxPoint> destination,
                                       bool cancel) {
    if (pointer_input.joinable())
        pointer_input.join();
    if (!pointer_hook) {
        pointer_hook = SetWindowsHookExW(WH_MOUSE, pointer_barrier, nullptr, GetCurrentThreadId());
        require(pointer_hook != nullptr,
                "Pointer checks acknowledge real mouse-up dispatch on the GUI thread", assertions);
    }
    pointer_target = this;
    pointer_delivered = false;
    pointer_finished = false;
    pointer_failed = false;
    pointer_input = std::jthread([this, source, destination, cancel](std::stop_token stop) {
        const auto pause = [] { std::this_thread::sleep_for(std::chrono::milliseconds(25)); };
        auto button = [this](DWORD flags) {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = flags;
            if (SendInput(1, &input, sizeof(input)) != 1)
                pointer_failed = true;
        };
        if (!SetCursorPos(source.x, source.y))
            pointer_failed = true;
        pause();
        button(destination ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN);
        pause();
        if (destination)
            for (int step = 1; step <= 20 && !stop.stop_requested(); ++step) {
                const auto point = source + wxPoint((destination->x - source.x) * step / 20,
                                                    (destination->y - source.y) * step / 20);
                if (!SetCursorPos(point.x, point.y))
                    pointer_failed = true;
                pause();
            }
        if (cancel) {
            INPUT keys[2]{};
            for (auto& key : keys) {
                key.type = INPUT_KEYBOARD;
                key.ki.wVk = VK_ESCAPE;
            }
            keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
            if (SendInput(2, keys, sizeof(INPUT)) != 2)
                pointer_failed = true;
            pause();
        }
        button(destination ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP);
        pointer_finished = true;
    });
}

// Exercise the current row painter's measured strings and hover binding at
// actual column width. Temporary snapshots are explicit simulated presentation
// fixtures; the authoritative service/library and audio preferences stay intact.
void BoardSmokeDriver::verify_statuses(BoardFrame& frame) {
    auto& view = *frame.view->implementation_;
    const auto saved = view.snapshot;
    const auto saved_scroll = view.viewport->GetViewStart();
    const auto source =
        std::find_if(saved->applications.begin(), saved->applications.end(), [](const auto& app) {
            return app.activity == Activity::Active && app.observation &&
                   app.observation->active_endpoint_ids.size() == 1;
        });
    require(source != saved->applications.end(), "Status checks reached a live sample row",
            assertions);
    SocketColumn* column = nullptr;
    for (auto* candidate : view.columns)
        for (const auto& row : candidate->rows->entries)
            if (!row.heading && row.application_id == source->id)
                column = candidate;
    require(column != nullptr, "Status checks reached the actual application list", assertions);
    auto* rows = column->rows;
    const auto saved_font = rows->GetFont();
    const auto saved_mode = frame.appearance_choice->GetSelection();
    const auto width =
        rows->visual_layout(wxRect(0, 0, rows->GetClientSize().x, rows->FromDIP(48))).detail.width;
    for (const auto mode : {0, 1, 2}) {
        choose(frame.appearance_choice, mode);
        for (const auto points : {10.0, 14.0}) {
            auto font = saved_font;
            font.SetFractionalPointSize(points);
            rows->SetFont(font);
            rows->refresh_metrics();
            for (const auto seconds :
                 {std::int64_t(0), std::int64_t(60), std::int64_t(2 * 3600 + 41 * 60),
                  std::int64_t(86399), std::int64_t(86400), std::int64_t(6 * 86400),
                  std::int64_t(7 * 86400), std::int64_t(28 * 86400), std::int64_t(29 * 86400)}) {
                auto app = *source;
                app.activity = Activity::Inactive;
                app.observation_fresh = false;
                app.inactive_seconds = seconds;
                view.snapshot_received = std::chrono::steady_clock::now();
                const auto visual = rows->status_visual(app, width);
                require(visual.text == ui_text(rows->detail_text(app)) &&
                            visual.extent.x <= width && !visual.text.Contains("...") &&
                            !visual.text.Contains(L"\u2026") && !visual.text.Contains("\n"),
                        "Complete Last seen status fits one line in " + std::to_string(mode) +
                            " theme at " + std::to_string(static_cast<int>(points)) +
                            " pt: " + visual.text.ToStdString(),
                        assertions);
            }
        }
    }
    rows->SetFont(saved_font);
    rows->refresh_metrics();
    auto story = std::make_shared<ModuleSnapshot>(*saved);
    const std::string device =
        "Game output with a deliberately long complete playback device name (USB audio interface)";
    for (auto& output : story->inventory.endpoints)
        if (output.windows_id == source->observation->active_endpoint_ids.front())
            output.display_name = device;
    view.render(story);
    const auto visual = rows->status_visual(*view.application(source->id), width);
    require(visual.extent.x <= width && visual.text != ui_text(rows->detail_text(*source)) &&
                !visual.text.Contains("\n"),
            "Long playback device names remain a single ellipsized status line", assertions);
    const auto find_row = [&] {
        return static_cast<std::size_t>(
            std::find_if(rows->entries.begin(), rows->entries.end(),
                         [&](const auto& row) { return row.application_id == source->id; }) -
            rows->entries.begin());
    };
    const auto bounds = rows->GetItemRect(find_row());
    const auto geometry = rows->visual_layout(bounds);
    wxMouseEvent hover(wxEVT_MOTION);
    hover.SetPosition(center(geometry.detail));
    rows->GetEventHandler()->ProcessEvent(hover);
    require(rows->GetToolTipText() == ui_text(device),
            "Hovering the playback status exposes the complete actual device name", assertions);
    hover.SetPosition(center(geometry.name));
    rows->GetEventHandler()->ProcessEvent(hover);
    require(rows->GetToolTipText() ==
                "Drag an app to another socket. Relocation without the moving boxes.",
            "Hovering the app name retains the concise drag hint", assertions);
    choose(frame.appearance_choice, 2);
    view.ensure_socket_visible(column->id);
    capture(frame, output / "playback-status-dark.bmp");

    auto library = std::make_shared<Library>(*saved->library);
    for (auto& board : library->boards)
        if (board.id == saved->selected_board_id)
            board.working.settings.show_inactive_sources = true;
    story->library = library;
    for (auto& app : story->applications) {
        app.activity = Activity::Inactive;
        app.observation_fresh = false;
        app.inactive_seconds = 2 * 3600 + 41 * 60;
    }
    std::vector<bool> expanded;
    for (auto* candidate : view.columns) {
        expanded.push_back(candidate->rows->expanded[2]);
        candidate->rows->expanded[2] = true;
        candidate->rows->entries.clear();
    }
    view.render(story);
    view.ensure_socket_visible("system");
    std::size_t displayed = 0;
    for (auto* candidate : view.columns)
        for (const auto& entry : candidate->rows->entries)
            if (!entry.heading) {
                const auto* app = view.application(entry.application_id);
                require(app && candidate->rows->detail_text(*app) == "Last seen: 2h 41m ago",
                        "An expanded inactive application row renders the compact Last seen label",
                        assertions);
                ++displayed;
            }
    require(displayed == story->applications.size(),
            "Last seen screenshot contains every inactive application row", assertions);
    capture(frame, output / "last-seen-dark.bmp");
    choose(frame.appearance_choice, 1);
    capture(frame, output / "last-seen-light.bmp");
    for (std::size_t index = 0; index < view.columns.size(); ++index) {
        view.columns[index]->rows->expanded[2] = expanded[index];
        view.columns[index]->rows->entries.clear();
    }
    view.render(saved);
    choose(frame.appearance_choice, saved_mode);
    view.viewport->Scroll(saved_scroll.x, saved_scroll.y);
}

// One bounded tick per GUI event-loop turn. All mutations go through actual
// controls or their installed drop target, then wait for the asynchronous model.
// A simulated backend makes this safe on a developer PC; it is not native proof.
void BoardSmokeDriver::tick(BoardFrame& frame) {
    try {
        if (!error.empty())
            throw std::runtime_error(error);
        if (std::chrono::steady_clock::now() - started > std::chrono::seconds(100))
            throw std::runtime_error("Board smoke timed out at stage " + std::to_string(stage));
        const auto current = frame.service->snapshot();
        if (current->phase != ModulePhase::Ready || !current->library || !frame.view)
            return;
        auto& view = *frame.view->implementation_;
        if (pointer_failed)
            throw std::runtime_error("Native pointer injection failed");
        if (!pointer_finished || !pointer_delivered || view.drag_active || view.socket_drag_context)
            return;
        if (!view.snapshot ||
            (current->visible && view.snapshot->publication < current->publication) ||
            !view.pending.empty() || view.animation_running)
            return;
        const auto* board = view.board();
        if (!board)
            return;
        ++tick_count;
        switch (stage) {
        case 0: {
            if (current->applications.size() < 3 || !current->visible)
                return;
            std::filesystem::create_directories(output);
            board_id = board->id;
            const auto source = std::find_if(
                current->applications.begin(), current->applications.end(),
                [](const auto& app) { return app.display_name == "Discord (sample)"; });
            require(source != current->applications.end(),
                    "Known sample app is available for pointer tests", assertions);
            application_id = source->id;
            require(current->simulated, "Board smoke explicitly uses simulation", assertions);
            require(board->working.sockets.size() == 1 && view.columns.size() == 1,
                    "First run contains only System", assertions);
            wxMouseEvent heading_hover(wxEVT_MOTION);
            heading_hover.SetPosition(center(view.columns.front()->rows->GetItemRect(0)));
            view.columns.front()->rows->GetEventHandler()->ProcessEvent(heading_hover);
            require(view.columns.front()->rows->GetToolTipText().empty(),
                    "Activity headers have no application tooltip", assertions);
            const auto system = view.columns.front()->inspect();
            require(system.section_counts[0] == 2 && system.section_counts[2] == 1 &&
                        system.rendered_application_ids.size() == 2,
                    "One row per application; inactive application is counted and hidden",
                    assertions);
            require(!view.delete_board->IsEnabled() && !view.delete_socket->IsEnabled(),
                    "Last board and System deletion controls are protected", assertions);
            auto* rows = view.columns.front()->rows;
            const auto bounds = rows->GetItemRect(1);
            const wxPoint point(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
            require(rows->row_at(point) == 1, "Pointer hit test identifies an application row",
                    assertions);
            require(!rows->row_at(wxPoint(-1, point.y)),
                    "Pointer hit test rejects points outside the list", assertions);
            require(!rows->row_at(
                        wxPoint(5, rows->GetItemRect(rows->entries.size() - 1).GetBottom() + 10)),
                    "Pointer hit test rejects empty space below rows", assertions);
            const auto geometry = rows->visual_layout(bounds);
            require(std::abs((geometry.icon.y * 2 + geometry.icon.height) -
                             (geometry.name.y + geometry.detail.GetBottom() + 1)) <= 2,
                    "The icon and the two-line text block share a vertical center", assertions);
            require(geometry.name.GetBottom() < geometry.detail.y &&
                        geometry.detail.GetBottom() < bounds.GetBottom(),
                    "Both text lines fit inside the compact application row", assertions);
            require(rows->detail_text(current->applications[0]).starts_with("Playing:"),
                    "Active rows show the playing endpoint as secondary text", assertions);
            next_name = "Game";
            click(view.add_socket);
            ++stage;
            break;
        }
        case 1:
            if (board->working.sockets.size() != 2)
                return;
            socket_id = board->working.sockets.back().id;
            require(socket_id != "system", "New socket has a stable independent ID", assertions);
            require(view.columns[1]->endpoint->GetCount() >= 3,
                    "Playback selectors contain the supplied inventory", assertions);
            choose(view.columns[1]->endpoint, 1);
            SendMessageW(static_cast<HWND>(view.columns[1]->endpoint->GetHandle()), WM_MOUSEWHEEL,
                         MAKEWPARAM(0, WHEEL_DELTA), 0);
            require(view.columns[1]->endpoint->GetSelection() == 1,
                    "Native mouse wheel cannot change a socket playback output", assertions);
            ++stage;
            break;
        case 2: {
            if (!board->working.sockets.back().endpoint.logical_device_id)
                return;
            menu_selection = 1;
            auto* rows = view.columns.front()->rows;
            const auto found =
                std::find_if(rows->entries.begin(), rows->entries.end(),
                             [&](const auto& row) { return row.application_id == application_id; });
            if (found == rows->entries.end())
                throw std::runtime_error("Application row missing");
            const auto index = static_cast<std::size_t>(found - rows->entries.begin());
            const auto rectangle = rows->GetItemRect(index);
            require(view.columns[1]->inspect().section_counts[0] == 1,
                    "An unassigned active app appears in the socket matching its actual output",
                    assertions);
            rows->SetFocus();
            pointer_gesture(rows->ClientToScreen(wxPoint(rectangle.x + 45, rectangle.y + 15)), {});
            ++stage;
            break;
        }
        case 3: {
            if (assigned_socket(board->working, application_id) != socket_id)
                return;
            const auto* app = view.application(application_id);
            if (!app || app->policy.state == PolicyState::Applying)
                return;
            require(app->policy.state == PolicyState::Accepted,
                    "Move menu commits assignment and receives policy readback", assertions);
            require(app->policy.roles.size() >= 3, "Policy receipt exposes all three audio roles",
                    assertions);
            require(true, "Physical right-click on an app opens its constructed Move to menu",
                    assertions);
            auto* rows = view.columns[1]->rows;
            const auto found =
                std::find_if(rows->entries.begin(), rows->entries.end(),
                             [&](const auto& row) { return row.application_id == application_id; });
            require(found != rows->entries.end(),
                    "Assigned row is available as a real mouse drag source", assertions);
            const auto rectangle =
                rows->GetItemRect(static_cast<std::size_t>(found - rows->entries.begin()));
            const auto target = view.columns.front()->heading->GetScreenRect();
            pointer_gesture(rows->ClientToScreen(wxPoint(rectangle.x + 45, rectangle.y + 15)),
                            wxPoint(target.x + target.width / 2, target.y + target.height / 2));
            ++stage;
            break;
        }
        case 4:
            if (assigned_socket(board->working, application_id) != "system")
                return;
            require(true, "Physical mouse down/move/up drops the app back into System", assertions);
            {
                auto* target =
                    dynamic_cast<wxTextDropTarget*>(view.columns.front()->heading->GetDropTarget());
                require(target && !target->OnDropText(2, 2, "untrusted text"),
                        "Drop target rejects malformed external text", assertions);
            }
            next_name = "Comms";
            click(view.add_socket);
            ++stage;
            break;
        case 5:
            if (board->working.sockets.size() < 6) {
                next_name = "Socket " + std::to_string(board->working.sockets.size());
                click(view.add_socket);
                return;
            }
            frame.SetSize(frame.FromDIP(wxSize(980, 680)));
            frame.Layout();
            ++stage;
            break;
        case 6: {
            const auto layout = frame.view->layout_diagnostics();
            require(frame.GetSize().x == frame.FromDIP(980),
                    "Existing 980 DIP minimum window width is preserved", assertions);
            require(layout.sockets.front().screen_rectangle.width == frame.FromDIP(186),
                    "Socket columns retain fixed 186 DIP width", assertions);
            require(layout.drawer_screen_rectangle.width == frame.FromDIP(300),
                    "Drawer width is 300 DIP", assertions);
            require(layout.tab_screen_rectangle.GetRight() + 1 ==
                        layout.drawer_screen_rectangle.GetLeft(),
                    "Settings tab remains attached to drawer edge", assertions);
            {
                const auto rectangle = view.boards->GetScreenRect();
                auto* hit = wxFindWindowAtPoint(rectangle.GetPosition() +
                                                wxPoint(rectangle.width / 2, rectangle.height / 2));
                while (hit && hit != view.drawer)
                    hit = hit->GetParent();
                require(hit == view.drawer,
                        "Drawer controls receive input above the socket workspace", assertions);
            }
            const int exposed = layout.tab_screen_rectangle.x - layout.viewport_screen_rectangle.x -
                                frame.FromDIP(8);
            require(exposed >= frame.FromDIP(600) && exposed < frame.FromDIP(660),
                    "Minimum window exposes approximately 3.25 socket columns", assertions);
            view.viewport->Scroll(5, 0);
            original_scroll = view.viewport->GetViewStart();
            original_size = frame.GetSize();
            frame.view->set_drawer_open(false);
            ++stage;
            break;
        }
        case 7:
            require(frame.GetSize() == original_size &&
                        view.viewport->GetViewStart() == original_scroll,
                    "Collapsing drawer preserves window and horizontal scroll", assertions);
            frame.view->set_drawer_open(true);
            ++stage;
            break;
        case 8: {
            require(frame.GetSize() == original_size &&
                        view.viewport->GetViewStart() == original_scroll,
                    "Expanding drawer preserves window and horizontal scroll", assertions);
            view.sockets->SetSelection(2);
            wxKeyEvent event(wxEVT_KEY_DOWN);
            event.m_keyCode = WXK_UP;
            event.SetAltDown(true);
            view.sockets->GetEventHandler()->ProcessEvent(event);
            ++stage;
            break;
        }
        case 9:
            require(board->working.sockets.front().id == "system" &&
                        board->working.sockets[2].id == socket_id,
                    "Keyboard socket reordering preserves System first", assertions);
            frame.view->set_drawer_open(false);
            {
                auto* rows = view.columns.front()->rows;
                const auto heading =
                    std::find_if(rows->entries.begin(), rows->entries.end(),
                                 [](const auto& row) { return row.heading && row.section == 2; });
                require(heading != rows->entries.end(),
                        "Hidden inactive group has an actionable heading", assertions);
                rows->activate(static_cast<std::size_t>(heading - rows->entries.begin()));
            }
            ++stage;
            break;
        case 10:
            if (!board->working.settings.show_inactive_sources)
                return;
            {
                auto* rows = view.columns.front()->rows;
                const auto heading =
                    std::find_if(rows->entries.begin(), rows->entries.end(),
                                 [](const auto& row) { return row.heading && row.section == 2; });
                require(heading != rows->entries.end(),
                        "Inactive group remains available with a count", assertions);
                require(!view.drawer_open && rows->expanded[2],
                        "Inactive disclosure enables and expands rows without opening settings",
                        assertions);
                const auto heading_index =
                    static_cast<std::size_t>(heading - rows->entries.begin());
                rows->activate(heading_index);
                require(rows->GetAccessible()->DoDefaultAction(static_cast<int>(heading_index) +
                                                               1) == wxACC_OK,
                        "Accessible group action expands inactive sources", assertions);
                std::set<std::string> rendered;
                std::size_t count = 0;
                for (const auto* column : view.columns)
                    for (const auto& id : column->inspect().rendered_application_ids) {
                        rendered.insert(id);
                        ++count;
                    }
                require(rendered.size() == 3 && count == 3,
                        "Inactive expansion and observed-output placement render each app once",
                        assertions);
            }
            frame.view->set_drawer_open(true);
            choose(view.recent, 2);
            ++stage;
            break;
        case 11:
            if (board->working.settings.recently_active_seconds != 900)
                return;
            click(view.save);
            ++stage;
            break;
        case 12:
            if (board->saved_revision != board->working_revision)
                return;
            require(true, "Save promotes the durable working checkpoint", assertions);
            next_name = "Smoke Copy";
            click(view.save_as);
            ++stage;
            break;
        case 13:
            if (current->library->boards.size() != 2 || board->id == board_id)
                return;
            require(board->working.sockets[2].id != socket_id, "Save As remaps custom socket IDs",
                    assertions);
            require(current->library->applications.size() == 3,
                    "Save As retains shared application identities", assertions);
            next_name = "Renamed Copy";
            click(view.rename_board);
            ++stage;
            break;
        case 14:
            if (board->display_name != "Renamed Copy")
                return;
            check(view.use_default, true);
            ++stage;
            break;
        case 15:
            if (current->library->default_board_id != board->id)
                return;
            check(view.associate, true);
            ++stage;
            break;
        case 16:
            if (!current->library->profile_associations.contains("demo-profile"))
                return;
            require(true, "Default and profile association controls persist through model commands",
                    assertions);
            view.sockets->SetSelection(1);
            {
                wxCommandEvent selected(wxEVT_LISTBOX, view.sockets->GetId());
                selected.SetEventObject(view.sockets);
                selected.SetInt(1);
                view.sockets->GetEventHandler()->ProcessEvent(selected);
            }
            click(view.delete_socket);
            ++stage;
            break;
        case 17:
            if (board->working.sockets.size() != 5)
                return;
            click(view.delete_board);
            ++stage;
            break;
        case 18:
            if (current->library->boards.size() != 1)
                return;
            require(current->library->profile_associations.at("demo-profile") == board_id &&
                        current->library->default_board_id == board_id,
                    "Deleting a default board remaps profile links and selects replacement",
                    assertions);
            frame.SetSize(frame.FromDIP(wxSize(1180, 760)));
            frame.Layout();
            choose(frame.appearance_choice, 2);
            check_scrollbar(*view.viewport, OBJID_HSCROLL, frame.colors, assertions);
            {
                // The shorter settings panel can fit without a scrollbar. Make
                // it scrollable deliberately before checking native pixels.
                const auto size = view.drawer->GetSize();
                view.drawer->SetSize(size.x, frame.FromDIP(420));
                check_scrollbar(*view.drawer, OBJID_VSCROLL, frame.colors, assertions);
                view.drawer->SetSize(size);
            }
            {
                auto* rows = view.columns.front()->rows;
                const auto size = rows->GetSize();
                rows->SetSize(size.x, frame.FromDIP(70));
                check_scrollbar(*rows, OBJID_VSCROLL, frame.colors, assertions);
                rows->SetSize(size);
            }
            capture(frame, output / "board-dark.bmp");
            ++view.interaction_depth;
            check_popup(*view.columns.front()->endpoint, frame.colors, output / "dropdown-dark.bmp",
                        assertions);
            --view.interaction_depth;
            choose(frame.appearance_choice, 1);
            check_scrollbar(*view.viewport, OBJID_HSCROLL, frame.colors, assertions);
            ++view.interaction_depth;
            check_popup(*view.columns.front()->endpoint, frame.colors,
                        output / "dropdown-light.bmp", assertions);
            --view.interaction_depth;
            capture(frame, output / "board-light.bmp");
            require(frame.colors.dark == false, "Current appearance control changes palette",
                    assertions);
            ++stage;
            break;
        case 19: {
            std::size_t controls = 0;
            const auto inspect = [&](const auto& self, wxWindow* root) -> void {
                if (dynamic_cast<wxButton*>(root) || dynamic_cast<wxChoice*>(root) ||
                    dynamic_cast<wxCheckBox*>(root) || dynamic_cast<wxListBox*>(root)) {
                    require(!root->GetName().empty() && !root->GetToolTipText().empty(),
                            "Interactive control has an accessible name and help: " +
                                root->GetName().ToStdString(),
                            assertions);
                    ++controls;
                }
                for (auto* child : root->GetChildren())
                    self(self, child);
            };
            inspect(inspect, frame.surface);
            require(controls == 18 + 2 * view.columns.size(),
                    "Accessibility inspection reached all current board and mixer controls",
                    assertions);
            require(!wxWindow::FindWindowByLabel("Diagnostics", &frame) &&
                        !wxWindow::FindWindowByLabel("Probe Tools", &frame),
                    "The user-facing frame has no Diagnostics or Probe Tools navigation",
                    assertions);
            require(view.windows_mixer && view.windows_mixer->GetParent() == view.drawer,
                    "Windows Mixer is available in Routing Board Settings", assertions);
            const auto writes = current->platform_metrics.policy_writes;
            click(view.windows_mixer);
            require(
                opened_uri == "ms-settings:apps-volume" &&
                    frame.service->snapshot()->platform_metrics.policy_writes == writes,
                "Windows Mixer opens the exact Settings URI without writing routing preferences",
                assertions);
            verify_statuses(frame);
            // Diagnostics/session/export screens were intentionally removed.
            // Core log/export contracts remain covered by portable model tests.
            stage = 22;
            break;
        }
        case 22:
            if (!current->visible)
                return;
            frame.Hide();
            frame.update_visibility();
            ++stage;
            break;
        case 23:
            if (current->visible || current->platform_metrics.discovery_active)
                return;
            require(true, "Hiding the actual window stops session discovery", assertions);
            frame.Show();
            frame.update_visibility();
            ++stage;
            break;
        case 24:
            if (!current->visible)
                return;
            require(true, "Reopening the window restores foreground discovery", assertions);
            view.drawer->Scroll(0, view.drawer->GetScrollRange(wxVERTICAL));
            view.sockets->SetFirstItem(0);
            pointer_gesture(socket_point(*view.sockets, 2), socket_point(*view.sockets, 3));
            ++stage;
            break;
        case 25: {
            require(board->working.sockets[3].id == socket_id,
                    "Real pointer drag moves a socket downward in the settings list", assertions);
            for (std::size_t index = 0; index < view.columns.size(); ++index)
                require(view.columns[index]->id == board->working.sockets[index].id,
                        "Main socket columns follow the dragged order", assertions);
            std::ifstream file(frame.data_root / "library.json");
            const auto persisted =
                parse_library(std::string(std::istreambuf_iterator<char>(file), {}));
            require(find_board(persisted, board_id)->working.sockets[3].id == socket_id,
                    "Dragged socket order is durable without pressing Save", assertions);
            pointer_gesture(socket_point(*view.sockets, 3), socket_point(*view.sockets, 1));
            ++stage;
            break;
        }
        case 26:
            require(board->working.sockets[1].id == socket_id,
                    "Real pointer drag moves a socket upward in the settings list", assertions);
            socket_drag_revision = board->working_revision;
            pointer_gesture(socket_point(*view.sockets, 1), socket_point(*view.sockets, 0));
            ++stage;
            break;
        case 27:
            require(board->working_revision == socket_drag_revision &&
                        board->working.sockets.front().id == "system",
                    "Dropping on System leaves socket order unchanged", assertions);
            pointer_gesture(socket_point(*view.sockets, 0), socket_point(*view.sockets, 2));
            ++stage;
            break;
        case 28:
            require(board->working_revision == socket_drag_revision &&
                        board->working.sockets.front().id == "system",
                    "Dragging System cannot move it from the first column", assertions);
            pointer_gesture(socket_point(*view.sockets, 1), socket_point(*view.sockets, 3), true);
            ++stage;
            break;
        case 29:
            require(board->working_revision == socket_drag_revision && !view.sockets->HasCapture(),
                    "Escape cancels socket dragging and releases capture without an edit",
                    assertions);
            pointer_gesture(socket_point(*view.sockets, 1), socket_point(*view.sockets, 1));
            ++stage;
            break;
        case 30:
            require(board->working_revision == socket_drag_revision,
                    "Selecting a socket without dragging does not rewrite configuration",
                    assertions);
            capture(frame, output / "sockets-reordered.bmp");
            menu_selection.reset();
            menu_count_before = menu_count;
            name_count_before = name_count;
            view.ensure_socket_visible(socket_id);
            {
                const auto rectangle = view.columns[1]->heading->GetScreenRect();
                pointer_gesture(center(rectangle), {});
            }
            ++stage;
            break;
        case 31:
            if (menu_count == menu_count_before)
                return;
            require(menu_count == menu_count_before + 1 && name_count == name_count_before &&
                        last_socket_delete_enabled,
                    "Right-clicking a header opens its menu without an automatic rename popup",
                    assertions);
            menu_selection = 0;
            next_name = "Renamed from header";
            menu_count_before = menu_count;
            pointer_gesture(center(view.columns[1]->heading->GetScreenRect()), {});
            ++stage;
            break;
        case 32:
            if (menu_count == menu_count_before)
                return;
            require(board->working.sockets[1].display_name == next_name &&
                        name_count == name_count_before + 1,
                    "Header menu Rename opens the named socket's rename dialog", assertions);
            menu_selection.reset();
            menu_count_before = menu_count;
            name_count_before = name_count;
            pointer_gesture(socket_point(*view.sockets, 1), {});
            ++stage;
            break;
        case 33:
            if (menu_count == menu_count_before)
                return;
            require(menu_count == menu_count_before + 1 && name_count == name_count_before,
                    "Right-clicking the socket list opens a menu without an automatic rename popup",
                    assertions);
            menu_selection = 0;
            next_name = "Renamed from list";
            menu_count_before = menu_count;
            pointer_gesture(socket_point(*view.sockets, 1), {});
            ++stage;
            break;
        case 34:
            if (menu_count == menu_count_before)
                return;
            require(board->working.sockets[1].display_name == next_name,
                    "Socket list menu Rename targets the clicked stable socket ID", assertions);
            menu_selection = 1;
            socket_drag_revision = board->working_revision;
            menu_count_before = menu_count;
            pointer_gesture(socket_point(*view.sockets, 0), {});
            ++stage;
            break;
        case 35:
            if (menu_count == menu_count_before)
                return;
            require(
                board->working_revision == socket_drag_revision && !last_socket_delete_enabled,
                "System's context menu disables Delete and ignores an injected delete selection",
                assertions);
            {
                const auto revision = current->library->revision;
                auto injected = std::exchange(view.options.confirm, {});
                frame.smoke_timer.Stop();
                verify_confirmation_cancel(
                    frame, frame.colors,
                    [&] { view.socket_menu(socket_id, *view.columns[1]->heading, wxPoint(5, 5)); },
                    assertions);
                view.options.confirm = std::move(injected);
                frame.smoke_timer.Start(60);
                require(frame.service->snapshot()->library->revision == revision,
                        "Choosing No in the real delete popup preserves every socket", assertions);
            }
            sockets_before_delete = board->working.sockets.size();
            pointer_gesture(center(view.columns[1]->heading->GetScreenRect()), {});
            ++stage;
            break;
        case 36:
            require(board->working.sockets.size() + 1 == sockets_before_delete &&
                        view.columns.size() == board->working.sockets.size(),
                    "Delete socket removes the selected custom column", assertions);
            if (board->working.sockets.size() > 1) {
                require(view.sockets->GetSelection() > 0 && view.delete_socket->IsEnabled(),
                        "Delete remains enabled for the neighboring custom socket", assertions);
                sockets_before_delete = board->working.sockets.size();
                click(view.delete_socket);
                break;
            }
            require(board->working.sockets.front().id == "system" &&
                        view.sockets->GetSelection() == 0 && !view.delete_socket->IsEnabled(),
                    "Only the final System socket is protected from deletion", assertions);
            ++stage;
            break;
        case 37:
            view.viewport->Scroll(0, 0);
            frame.smoke_timer.Stop();
            for (const auto preference : {2, 1, 0}) {
                choose(frame.appearance_choice, preference);
                verify_popup_appearance(frame, frame.colors,
                                        output / (preference == 2   ? "popups-dark"
                                                  : preference == 1 ? "popups-light"
                                                                    : "popups-system"),
                                        assertions);
            }
            frame.smoke_timer.Start(60);
            style_policy_writes = frame.service->snapshot()->platform_metrics.policy_writes;
            style_index = 0;
            menu_selection = 3;
            view.socket_menu("system", *view.columns.front()->heading, wxPoint(5, 5));
            ++stage;
            break;
        case 38: {
            const auto& color = socket_colors[style_index];
            require(board->working.sockets[0].appearance.color == color.id,
                    "Recolor menu persists the selected named socket color", assertions);
            auto* heading = view.columns.front()->heading;
            require(contrast_ratio(heading->GetBackgroundColour(),
                                   heading->GetForegroundColour()) >= 4.5,
                    "Socket name text remains readable for the selected color", assertions);
            if (++style_index < socket_colors.size()) {
                menu_selection = 3 + style_index;
                view.socket_menu("system", *heading, wxPoint(5, 5));
                break;
            }
            style_index = 0;
            menu_selection = 20;
            view.socket_menu("system", *heading, wxPoint(5, 5));
            ++stage;
            break;
        }
        case 39: {
            const auto& icon = socket_icons[style_index];
            require(board->working.sockets[0].appearance.icon == icon.id &&
                        view.columns.front()->heading->GetBitmap().IsOk(),
                    "Icon menu persists and renders the selected bundled Material symbol",
                    assertions);
            if (++style_index < socket_icons.size()) {
                menu_selection = 20 + style_index;
                view.socket_menu("system", *view.columns.front()->heading, wxPoint(5, 5));
                break;
            }
            capture(frame, output / "colored-socket-icon.bmp");
            std::ifstream file(frame.data_root / "library.json");
            const auto stored =
                parse_library(std::string(std::istreambuf_iterator<char>(file), {}));
            file.close(); // Windows atomic replacement needs this read handle released.
            const auto& style = find_board(stored, board_id)->working.sockets[0].appearance;
            require(style.color == "slate" && style.icon == socket_icons.back().id,
                    "Socket color and icon are durable without pressing Save", assertions);
            require(frame.service->snapshot()->platform_metrics.policy_writes ==
                        style_policy_writes,
                    "Socket appearance edits never write audio preferences", assertions);
            menu_selection = 2;
            view.socket_menu("system", *view.columns.front()->heading, wxPoint(5, 5));
            ++stage;
            break;
        }
        case 40:
            require(board->working.sockets[0].appearance.color == "default",
                    "Recolor can return to the app theme", assertions);
            menu_selection = 19;
            view.socket_menu("system", *view.columns.front()->heading, wxPoint(5, 5));
            ++stage;
            break;
        case 41:
            require(board->working.sockets[0].appearance.icon == "none",
                    "Socket icon can be cleared without affecting its color or identity",
                    assertions);
            finish(frame);
            break;
        default:
            break;
        }
    } catch (const std::exception& failure) {
        error = failure.what();
        finish(frame);
    }
}
void BoardSmokeDriver::finish(BoardFrame& frame) {
    frame.smoke_timer.Stop();
    try {
        std::filesystem::create_directories(output);
        Document report{{"context", "simulated board UI; not native Windows audio validation"},
                        {"passed", error.empty()},
                        {"stage", stage},
                        {"assertions", assertions},
                        {"error", error}};
        std::ofstream file(output / "board-assertions.json", std::ios::binary);
        file << report.dump(2) << '\n';
        if (!error.empty())
            capture(frame, output / "failure.bmp");
    } catch (const std::exception&) {
        if (error.empty())
            error = "Could not write smoke evidence";
    }
    if (finished)
        finished(error.empty() ? 0 : 1);
    frame.exit();
}
} // namespace audiohole::plug_board::desktop
