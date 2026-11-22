#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/access.h>
#include <wx/modalhook.h>
#include <wx/textdlg.h>
#include <wx/vlbox.h>

namespace audiohole::gui {
// Supplemental Windows integration evidence. Only smoke hosts call this; they
// construct an in-memory simulated module before any native policy can exist.
void Frame::plug_board_smoke(bool lesson) {
    namespace pb = audiohole::plug_board;
    const auto require = [](bool condition, const std::string& message) {
        if (!condition)
            throw std::runtime_error("Plug Board: " + message);
    };
    require(smoke_ && plug_simulation_, "smoke must use the isolated backend");
    const auto original_page = notebook_->GetSelection();
    const auto pump = [&](const auto& predicate, const std::string& description) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        do {
            wxYield();
            refresh_plug_board();
            if (predicate())
                return;
            wxMilliSleep(5);
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("Plug Board timed out: " + description);
    };
    const auto settled = [&] { return !plug_board_view_->layout_diagnostics().command_pending; };
    const auto beat = [&](const char* expected) {
        if (lesson)
            require(tutorial_->beat() == expected,
                    "expected " + std::string(expected) + ", reached " + tutorial_->beat());
    };
    const auto click = [&](wxWindow* control) {
        require(control && control->IsEnabled(), "missing or disabled action control");
        wxCommandEvent event(wxEVT_BUTTON, control->GetId());
        event.SetEventObject(control);
        control->GetEventHandler()->ProcessEvent(event);
    };
    const auto choose_reply = [&] {
        require(!tutorial_choices_.empty(), "missing authored tutorial choices");
        click(tutorial_choices_[0]);
    };
    if (lesson) {
        select_page(PlugBoardPage);
        beat("PB002");
        choose_reply();
        beat("PB003");
        choose_reply();
        beat("PB004");
    } else {
        auto menu = shell_menu();
        const int shortcut = menu->FindItem("Plug Board");
        require(shortcut != wxNOT_FOUND, "tray menu lacks Plug Board");
        wxCommandEvent action(wxEVT_MENU, shortcut);
        menu->ProcessEvent(action);
        require(notebook_->GetSelection() == PlugBoardPage, "tray shortcut opened another tab");
        require(parse_shell_action(L"plug-board") == ShellAction::PlugBoard,
                "taskbar relay lacks Plug Board");
        require(GetMenuState(GetSystemMenu(static_cast<HWND>(GetHandle()), FALSE),
                             shell_plug_board_command, MF_BYCOMMAND) != static_cast<UINT>(-1),
                "taskbar system menu lacks Plug Board");
        prepare_plug_tutorial_apps();
        refresh_plug_board();
    }
    if (!lesson && controller_->snapshot().endpoints.empty()) {
        pump(
            [&] {
                const auto state = plug_board_->snapshot();
                return state->phase == pb::ModulePhase::Ready && state->visible && settled();
            },
            "empty Test Mode board");
        const auto state = plug_board_->snapshot();
        require(state->inventory.endpoints.empty() && state->applications.empty() &&
                    state->platform_metrics.policy_writes == 0,
                "empty Test Mode exposed fake or real device/application fixtures");
        auto* exempt = wxWindow::FindWindowByLabel("Exempt apps", plug_board_view_);
        auto* clear = wxWindow::FindWindowByLabel("Clear inactive apps", plug_board_view_);
        require(exempt && exempt->IsEnabled() && clear && !clear->IsEnabled() &&
                    state->library->inactive_exempt_apps.empty(),
                "empty Test Mode app-list controls or opt-in defaults are incorrect");
        select_page(original_page);
        pump([&] { return !plug_board_->snapshot()->visible; }, "hidden empty Test Mode board");
        return;
    }
    const auto host_revision = controller_->snapshot().revision;
    pump(
        [&] {
            const auto state = plug_board_->snapshot();
            return state->phase == pb::ModulePhase::Ready && state->visible &&
                   state->applications.size() == 6 && settled() &&
                   !plug_board_view_->layout_diagnostics().sockets.empty();
        },
        "embedded board and six fictional applications");
    const auto initial = plug_board_->snapshot();
    require(initial->simulated, "smoke discovered real applications");
    require(initial->platform_metrics.policy_writes == 0,
            "discovery changed an application policy");
    const auto* board = pb::find_board(*initial->library, initial->selected_board_id);
    require(board && board->working.sockets.front().id == "system", "System socket missing");
    require(std::all_of(initial->applications.begin(), initial->applications.end(),
                        [](const auto& app) { return app.socket_id == "system"; }),
            "practice applications did not start in System");
    if (lesson) {
        require(initial->inventory.endpoints.size() == 7,
                "tutorial playback inventory is incomplete");
        require(initial->inventory.default_windows_endpoints[0] == "tutorial-boomslab",
                "tutorial System output is not BoomSlab");
    }
    class NameAnswer final : public wxModalDialogHook {
      public:
        wxString answer;
        int response = wxID_OK;
        bool asked = false;
        bool rejected = false;
        int Enter(wxDialog* dialog) override {
            if (auto* edit = dynamic_cast<wxTextEntryDialog*>(dialog)) {
                asked = true;
                edit->SetValue(answer);
                return response;
            }
            if (dialog->GetTitle() == "Socket name") {
                rejected = true;
                return wxID_OK;
            }
            return wxID_NONE;
        }
    };
    const auto named_action = [&](const std::string& answer, const auto& action,
                                  int response = wxID_OK) {
        NameAnswer hook;
        hook.answer = text(answer);
        hook.response = response;
        hook.Register();
        action();
        hook.Unregister();
        require(hook.asked, "action bypassed the real name dialog");
        pump(settled, "name action completion");
        return hook.rejected;
    };
    const auto create_socket = [&](const std::string& name) {
        plug_board_view_->set_drawer_open(true);
        pump([&] { return !plug_board_view_->layout_diagnostics().drawer_animating; },
             "settings drawer");
        auto* create_socket = wxWindow::FindWindowByName("New socket", plug_board_view_);
        require(create_socket != nullptr, "socket creation control missing");
        create_socket->SetFocus();
        for (int refresh = 0; refresh < 4; ++refresh) {
            plug_board_view_->set_drawer_open(true);
            if (lesson)
                render_tutorial();
        }
        require(wxWindow::FindFocus() == create_socket &&
                    !plug_board_view_->layout_diagnostics().drawer_animating,
                "repeated tutorial guidance stole Add socket focus or restarted its animation");

        const auto before = plug_board_->snapshot();
        if (lesson) {
            const auto current_beat = tutorial_->beat();
            named_action(
                "Unconfirmed name",
                [&] { click(wxWindow::FindWindowByName("New socket", plug_board_view_)); },
                wxID_CANCEL);
            const auto cancelled = plug_board_->snapshot();
            const auto* original = pb::find_board(*before->library, before->selected_board_id);
            const auto* current = pb::find_board(*cancelled->library, cancelled->selected_board_id);
            require(
                original && current &&
                    current->working.sockets.size() == original->working.sockets.size() &&
                    tutorial_->beat() == current_beat,
                "Cancelling the mandatory naming popup created a socket or advanced the lesson");
            for (const auto& invalid :
                 {std::string{}, original->working.sockets.back().display_name}) {
                require(
                    named_action(
                        invalid,
                        [&] { click(wxWindow::FindWindowByName("New socket", plug_board_view_)); }),
                    "The tutorial naming popup accepted a blank or duplicate socket name");
                const auto unchanged = plug_board_->snapshot();
                const auto* current =
                    pb::find_board(*unchanged->library, unchanged->selected_board_id);
                require(current &&
                            current->working.sockets.size() == original->working.sockets.size() &&
                            tutorial_->beat() == current_beat,
                        "Invalid popup input created a socket or advanced the lesson");
            }
        }
        named_action(name,
                     [&] { click(wxWindow::FindWindowByName("New socket", plug_board_view_)); });
        const auto state = plug_board_->snapshot();
        const auto* current = pb::find_board(*state->library, state->selected_board_id);
        const auto* previous = pb::find_board(*before->library, before->selected_board_id);
        require(current && previous &&
                    current->working.sockets.size() == previous->working.sockets.size() + 1,
                "New socket did not commit exactly one socket");
        if (!lesson)
            require(last_message_ == presentation::Message::PlugBoardSocketCreated,
                    "socket creation did not select its personality response pool");
        for (const auto& item : current->working.sockets)
            if (std::none_of(previous->working.sockets.begin(), previous->working.sockets.end(),
                             [&](const auto& old) { return old.id == item.id; })) {
                return item.id;
            }
        throw std::runtime_error("New socket has no independent identity");
    };
    const auto rename_socket = [&](const std::string& id, const std::string& name) {
        auto* heading = wxWindow::FindWindowByName(text("Socket heading " + id), plug_board_view_);
        require(heading, "socket has no real header");
        named_action(name, [&] {
            plug_smoke_menu_ = "Rename";
            wxContextMenuEvent event(wxEVT_CONTEXT_MENU, heading->GetId());
            heading->GetEventHandler()->ProcessEvent(event);
        });
        require(!plug_smoke_menu_, "socket header did not open its actual context menu");
        if (!lesson)
            require(last_message_ == presentation::Message::PlugBoardSocketRenamed,
                    "socket rename did not select its personality response pool");
    };
    const auto endpoint_choice = [&](const std::string& id, const std::string& endpoint_id) {
        auto* control = dynamic_cast<wxChoice*>(
            wxWindow::FindWindowByName(text("Playback output " + id), plug_board_view_));
        require(control, "socket has no endpoint choice");
        const auto state = plug_board_->snapshot();
        const auto found =
            std::find_if(state->inventory.endpoints.begin(), state->inventory.endpoints.end(),
                         [&](const auto& value) { return value.logical_id == endpoint_id; });
        require(found != state->inventory.endpoints.end(), "host endpoint was not projected");
        const auto caption = found->display_name + (found->ambiguous   ? " (ambiguous)"
                                                    : found->available ? ""
                                                                       : " (unavailable)");
        const int selection = control->FindString(text(caption));
        require(selection != wxNOT_FOUND,
                "host endpoint " + endpoint_id + " was not offered in " + id + ": " + caption);
        control->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE, control->GetId());
        control->GetEventHandler()->ProcessEvent(event);
        pump(settled, "endpoint preference commit");
        const auto after = plug_board_->snapshot();
        const auto* selected = pb::find_board(*after->library, after->selected_board_id);
        require(selected &&
                    std::any_of(selected->working.sockets.begin(), selected->working.sockets.end(),
                                [&](const auto& item) {
                                    return item.id == id &&
                                           item.endpoint.logical_device_id == endpoint_id;
                                }),
                "endpoint selection was not committed by logical identity");
        if (!lesson)
            require(last_message_ == presentation::Message::PlugBoardSocketEndpointChanged,
                    "endpoint change did not select its personality response pool");
    };
    const auto music = create_socket(lesson ? "My playlist" : "New Music Socket");
    if (!lesson)
        rename_socket(music, "My playlist");
    beat("PB006");
    const auto music_endpoint =
        lesson ? "tutorial-music" : initial->inventory.endpoints.front().logical_id;
    endpoint_choice(music, music_endpoint);
    beat("PB007");
    const auto game = create_socket(lesson ? "Explosions" : "New Game Socket");
    if (!lesson)
        rename_socket(game, "Explosions");
    beat("PB009");
    const auto game_endpoint =
        lesson ? "tutorial-game" : initial->inventory.endpoints.back().logical_id;
    endpoint_choice(game, game_endpoint);
    beat("PB010");
    plug_board_view_->set_drawer_open(false);
    pump([&] { return !plug_board_view_->layout_diagnostics().drawer_animating; },
         "closed settings drawer");
    const auto move_app = [&](const std::string& app_name, const std::string& destination,
                              const std::string& destination_name) {
        const auto state = plug_board_->snapshot();
        const auto found =
            std::find_if(state->applications.begin(), state->applications.end(),
                         [&](const auto& app) { return app.display_name == app_name; });
        require(found != state->applications.end(), "fictional app is missing");
        const auto id = found->id;
        const auto rectangle = plug_board_view_->application_screen_rect(id, true);
        require(rectangle.has_value(), "fictional app has no actual visible row");
        wxWindow* rows = nullptr;
        for (const auto& column : plug_board_view_->layout_diagnostics().sockets)
            if (std::find(column.rendered_application_ids.begin(),
                          column.rendered_application_ids.end(),
                          id) != column.rendered_application_ids.end())
                rows = wxWindow::FindWindowById(column.applications_control_id, plug_board_view_);
        require(rows, "app row has no constructed control");
        plug_smoke_menu_ = "Move to " + destination_name;
        wxContextMenuEvent event(wxEVT_CONTEXT_MENU, rows->GetId(),
                                 rectangle->GetPosition() + wxPoint(8, 8));
        rows->GetEventHandler()->ProcessEvent(event);
        require(!plug_smoke_menu_, "app row did not open its actual context menu");
        pump(
            [&] {
                const auto after = plug_board_->snapshot();
                const auto* current = pb::find_board(*after->library, after->selected_board_id);
                if (!settled() || !current ||
                    pb::assigned_socket(current->working, id) != destination)
                    return false;
                if (!lesson)
                    return true;
                const auto observed =
                    std::find_if(after->applications.begin(), after->applications.end(),
                                 [&](const auto& app) { return app.id == id; });
                const auto& endpoint = destination == game ? game_endpoint : music_endpoint;
                return observed != after->applications.end() && observed->observation &&
                       observed->observation->active_endpoint_ids ==
                           std::vector<std::string>{endpoint};
            },
            "application assignment commit");
        if (!lesson)
            require(last_message_ == presentation::Message::PlugBoardAppMoved,
                    "app move did not select its personality response pool");
    };
    move_app("Felony Jaywalking LXVII", game, "Explosions");
    beat("PB011");
    move_app("Spooty", music, "My playlist");
    beat("PB012");
    pump(
        [&] {
            const auto current = plug_board_->snapshot();
            return std::all_of(
                current->applications.begin(), current->applications.end(),
                [](const auto& app) { return app.image && app.image->application_specific; });
        },
        "all six owner-supplied tutorial icons");
    if (lesson) {
        choose_reply();
        beat("PB013");
        endpoint_choice(music, "tutorial-chat");
        beat("PB013"); // Optional exploration may change the setup.
        select_page(SettingsPage);
        beat("PB006");
        require(!tutorial_->complete() &&
                    tutorial_->view().targets == std::vector<std::string>{"plug_board_tab"},
                "Settings accepted an invalid practice output or lost the recovery tab");
        select_page(PlugBoardPage);
        endpoint_choice(music, music_endpoint);
        beat("PB012");
        choose_reply();
        beat("PB013");
    } else {
        // Exercise the embedded group controls with the drawer closed. A hidden
        // board preference must be enabled by the group itself, never by opening
        // the drawer or changing an application's routing assignment.
        const auto sockets = plug_board_view_->layout_diagnostics().sockets;
        for (const auto& column : sockets) {
            auto* setting = dynamic_cast<wxCheckBox*>(
                wxWindow::FindWindowByName("Show inactive sources", plug_board_view_));
            require(setting != nullptr, "inactive-source setting missing");
            setting->SetValue(false);
            wxCommandEvent disable(wxEVT_CHECKBOX, setting->GetId());
            disable.SetEventObject(setting);
            setting->GetEventHandler()->ProcessEvent(disable);
            pump(
                [&] {
                    const auto state = plug_board_->snapshot();
                    const auto* current = pb::find_board(*state->library, state->selected_board_id);
                    return settled() && current && !current->working.settings.show_inactive_sources;
                },
                "inactive sources hidden before expanding a socket");
            const auto before = plug_board_->snapshot();
            const auto* previous = pb::find_board(*before->library, before->selected_board_id);
            auto* rows = wxWindow::FindWindowById(column.applications_control_id, plug_board_view_);
            auto* accessible = rows ? rows->GetAccessible() : nullptr;
            require(accessible != nullptr, "application rows lack accessibility actions");
            int children = 0;
            require(accessible->GetChildCount(&children) == wxACC_OK, "cannot inspect groups");
            int inactive = 0;
            for (int child = 1; child <= children; ++child) {
                wxString name;
                if (accessible->GetName(child, &name) == wxACC_OK && name.StartsWith("Inactive ("))
                    inactive = child;
            }
            require(inactive != 0, "inactive heading is missing from a socket");
            auto* list = dynamic_cast<wxVListBox*>(rows);
            require(list != nullptr, "socket applications are not a virtual list");
            list->ScrollToRow(static_cast<std::size_t>(inactive - 1));
            wxRect rectangle;
            require(accessible->GetLocation(rectangle, inactive) == wxACC_OK,
                    "inactive heading has no click geometry");
            wxMouseEvent expand(wxEVT_LEFT_DOWN);
            expand.SetPosition(rows->ScreenToClient(
                rectangle.GetPosition() + wxPoint(rectangle.width / 2, rectangle.height / 2)));
            rows->GetEventHandler()->ProcessEvent(expand);
            pump(
                [&] {
                    const auto state = plug_board_->snapshot();
                    const auto* current = pb::find_board(*state->library, state->selected_board_id);
                    return settled() && current && current->working.settings.show_inactive_sources;
                },
                "inactive group enables and expands directly");
            const auto after = plug_board_->snapshot();
            const auto* current = pb::find_board(*after->library, after->selected_board_id);
            const auto layout = plug_board_view_->layout_diagnostics();
            require(!layout.drawer_open && !layout.drawer_animating,
                    "inactive disclosure opened the closed settings drawer");
            require(previous && current &&
                        previous->working.application_assignments ==
                            current->working.application_assignments &&
                        previous->working.settings.recently_active_seconds ==
                            current->working.settings.recently_active_seconds &&
                        before->platform_metrics.policy_writes ==
                            after->platform_metrics.policy_writes,
                    "inactive disclosure changed assignments, policy, or the activity interval");
            wxString name;
            accessible->GetName(inactive, &name);
            require(name.EndsWith("expanded"), "inactive heading did not expand after clicking");
            wxKeyEvent collapse(wxEVT_KEY_DOWN);
            collapse.m_keyCode = WXK_SPACE;
            rows->GetEventHandler()->ProcessEvent(collapse);
            accessible->GetName(inactive, &name);
            require(name.EndsWith("collapsed"), "keyboard action did not collapse inactive group");
            require(accessible->DoDefaultAction(inactive) == wxACC_OK,
                    "accessible action could not expand inactive group");
            accessible->GetName(inactive, &name);
            require(name.EndsWith("expanded") &&
                        !plug_board_view_->layout_diagnostics().drawer_open,
                    "accessible expansion changed drawer state or failed to expand");
        }
        const auto occurrence = occurrence_;
        refresh_plug_board();
        refresh_plug_board();
        require(occurrence_ == occurrence, "unchanged publications repeated personality chatter");
        const auto original = theme_choice_->GetSelection();
        for (int appearance : {0, 1}) {
            theme_choice_->SetSelection(appearance);
            apply_appearance();
            require(plug_board_view_->GetBackgroundColour() == GetBackgroundColour(),
                    "embedded surface ignores host theme");
            const auto sockets = plug_board_view_->layout_diagnostics().sockets;
            // Observed-output placement can empty System when a custom socket
            // matches Windows Default. Test actual rendered active app rows.
            const auto populated =
                std::find_if(sockets.begin(), sockets.end(), [](const auto& socket) {
                    return socket.section_counts[0] > 0 && !socket.rendered_application_ids.empty();
                });
            require(populated != sockets.end(), "theme checks need a rendered active app socket");
            plug_board_view_->ensure_socket_visible(populated->id);
            auto* rows = dynamic_cast<wxVListBox*>(
                wxWindow::FindWindowById(populated->applications_control_id, plug_board_view_));
            auto* accessible = rows ? rows->GetAccessible() : nullptr;
            require(accessible != nullptr, "row presentation checks lack accessible geometry");
            int children = 0;
            require(accessible->GetChildCount(&children) == wxACC_OK && children >= 4,
                    "row presentation checks need headings and an app");
            const auto hover = [&](const wxPoint& point) {
                wxMouseEvent event(wxEVT_MOTION);
                event.SetPosition(point);
                rows->GetEventHandler()->ProcessEvent(event);
            };
            const auto background = [&](std::size_t row) {
                rows->ScrollToRow(row);
                RedrawWindow(static_cast<HWND>(rows->GetHandle()), nullptr, nullptr,
                             RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
                const auto rect = rows->GetItemRect(row);
                const wxPoint point(rect.GetRight() - FromDIP(2), rect.y + rect.height / 2);
                wxColour color;
                wxClientDC dc(rows);
                require(rows->GetClientRect().Contains(point) && dc.GetPixel(point, &color),
                        "cannot sample the visible native row background");
                return color;
            };
            bool checked_app = false;
            unsigned checked_headings = 0;
            const int selection = rows->GetSelection();
            for (int child = 1; child <= children; ++child) {
                wxString name;
                accessible->GetName(child, &name);
                const bool heading = name.StartsWith("Active (") ||
                                     name.StartsWith("Recently Active (") ||
                                     name.StartsWith("Inactive (");
                if (!heading && checked_app)
                    continue;
                const auto row = static_cast<std::size_t>(child - 1);
                rows->SetSelection(wxNOT_FOUND);
                const auto normal = background(row);
                rows->SetSelection(child - 1);
                require(background(row) == normal,
                        "selecting a Plug Board heading/app changed its background fill");
                long state = 0;
                require(accessible->GetState(child, &state) == wxACC_OK &&
                            (state & wxACC_STATE_SYSTEM_SELECTED),
                        "removing the fill lost the keyboard/accessibility selection target");
                const auto rect = rows->GetItemRect(row);
                hover(wxPoint(rect.x + FromDIP(3), rect.y + rect.height / 2));
                if (heading) {
                    ++checked_headings;
                    require(rows->GetToolTip() == nullptr,
                            "activity dropdown acquired an app tooltip");
                } else {
                    checked_app = true;
                    require(
                        rows->GetToolTipText() ==
                            "Drag an app to another socket. Relocation without the moving boxes.",
                        "the app row lost its revised drag tooltip");
                    const auto playing = name.Find(", Playing: ");
                    require(playing != wxNOT_FOUND, "active app fixture lacks playback status");
                    hover(wxPoint(rect.GetRight() - FromDIP(12), rect.GetBottom() - FromDIP(12)));
                    require(rows->GetToolTipText() ==
                                name.Mid(static_cast<std::size_t>(playing) + 11),
                            "playback hover did not retain the full actual output name");
                }
            }
            require(checked_app && checked_headings == 3,
                    "not all activity headers/app were exercised");
            rows->SetSelection(wxNOT_FOUND);
            require(accessible->DoDefaultAction(1) == wxACC_OK,
                    "could not collapse Active to expose empty list space");
            const auto last = rows->GetItemRect(rows->GetItemCount() - 1);
            const wxPoint empty(last.x + FromDIP(3), last.GetBottom() + FromDIP(5));
            require(rows->GetClientRect().Contains(empty), "empty-space fixture is not visible");
            hover(empty);
            require(rows->GetToolTip() == nullptr, "empty list space retained an app tooltip");
            wxMouseEvent leave(wxEVT_LEAVE_WINDOW);
            rows->GetEventHandler()->ProcessEvent(leave);
            require(rows->GetToolTip() == nullptr,
                    "leaving app rows reinstated a container tooltip");
            require(accessible->DoDefaultAction(1) == wxACC_OK,
                    "could not restore Active after the empty-space check");
            rows->SetSelection(selection);
        }
        theme_choice_->SetSelection(original);
        apply_appearance();
        require(controller_->snapshot().revision == host_revision,
                "Plug Board UI changed the profile editor or engine");
        plug_board_clutter_smoke();
        require(controller_->snapshot().revision == host_revision,
                "App-list controls changed the profile editor or engine");
        select_page(original_page);
        pump(
            [&] {
                return !plug_board_->snapshot()->visible &&
                       !plug_board_->snapshot()->platform_metrics.discovery_active;
            },
            "discovery suspended outside Plug Board");
    }
}
} // namespace audiohole::gui
