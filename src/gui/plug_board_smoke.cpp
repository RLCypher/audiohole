#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/modalhook.h>
#include <wx/textdlg.h>

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
        bool asked = false;
        int Enter(wxDialog* dialog) override {
            if (auto* edit = dynamic_cast<wxTextEntryDialog*>(dialog)) {
                asked = true;
                edit->SetValue(answer);
                return wxID_OK;
            }
            return wxID_NONE;
        }
    };
    const auto named_action = [&](const std::string& answer, const auto& action) {
        NameAnswer hook;
        hook.answer = text(answer);
        hook.Register();
        action();
        hook.Unregister();
        require(hook.asked, "action bypassed the real name dialog");
        pump(settled, "name action completion");
    };
    const auto create_socket = [&](const std::string& name) {
        plug_board_view_->set_drawer_open(true);
        pump([&] { return !plug_board_view_->layout_diagnostics().drawer_animating; },
             "settings drawer");
        const auto before = plug_board_->snapshot();
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
    const auto music = create_socket("New Music Socket");
    beat("PB005");
    rename_socket(music, "My playlist");
    beat("PB006");
    const auto music_endpoint =
        lesson ? "tutorial-music" : initial->inventory.endpoints.front().logical_id;
    endpoint_choice(music, music_endpoint);
    beat("PB007");
    const auto game = create_socket("New Game Socket");
    beat("PB008");
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
    } else {
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
        }
        theme_choice_->SetSelection(original);
        apply_appearance();
        require(controller_->snapshot().revision == host_revision,
                "Plug Board UI changed the profile editor or engine");
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
