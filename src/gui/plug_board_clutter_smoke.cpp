#include "audiohole/plug_board/application_list.hpp"
#include "gui/controls.h"
#include "gui/frame.h"
#include <tuple>
#include <wx/access.h>
#include <wx/modalhook.h>
#include <wx/textdlg.h>
#include <wx/vlbox.h>

namespace audiohole::gui {
void Frame::plug_board_clutter_smoke() {
    namespace pb = audiohole::plug_board;
    const auto require = [](bool condition, const std::string& message) {
        if (!condition)
            throw std::runtime_error("Plug Board app list: " + message);
    };
    require(smoke_ && plug_simulation_ && plug_board_->snapshot()->simulated,
            "fixture requires an isolated simulated smoke host");
    const auto pump = [&](const auto& ready) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        do {
            wxYield();
            refresh_plug_board();
            if (ready())
                return;
            wxMilliSleep(5);
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("Plug Board app-list fixture timed out");
    };
    const auto command = [&](pb::Command value) {
        auto future = plug_board_->submit(std::move(value));
        pump([&] { return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
        const auto result = future.get();
        require(result.ok, result.message);
        pump([&] { return !plug_board_view_->layout_diagnostics().command_pending; });
    };
    const auto click = [&](wxWindow* control) {
        require(control && control->IsEnabled(), "missing or disabled action");
        wxCommandEvent event(wxEVT_BUTTON, control->GetId());
        control->GetEventHandler()->ProcessEvent(event);
    };
    const auto original = plug_board_->snapshot();
    const auto board_id = original->selected_board_id;
    const auto original_settings = pb::find_board(*original->library, board_id)->working.settings;
    std::vector<pb::SessionObservation> original_sessions;
    for (const auto& app : original->applications)
        if (app.observation)
            original_sessions.insert(original_sessions.end(), app.observation->sessions.begin(),
                                     app.observation->sessions.end());
    auto sessions = original_sessions;
    for (unsigned i = 0; i < 3; ++i) {
        pb::SessionObservation row;
        row.endpoint_id = plug_simulated_default_;
        row.instance_id = "clutter-script-" + std::to_string(i);
        row.display_name = "powershell";
        row.state = pb::SessionState::Inactive;
        row.process = pb::ProcessToken{10000 + i, 123400 + i, "C:\\Windows\\powershell.exe"};
        sessions.push_back(row);
    }
    select_page(PlugBoardPage);
    plug_board_view_->set_drawer_open(true);
    command(pb::SetBoardSettings{board_id, true, 1});
    plug_simulation_->set_sessions({sessions, true, {}});
    std::vector<std::string> hosts;
    pump([&] {
        hosts.clear();
        for (const auto& app : plug_board_->snapshot()->applications)
            if (app.group_name == "powershell")
                hosts.push_back(app.id);
        return hosts.size() == 3;
    });
    const auto view_app = [&](const std::string& id) {
        const auto snapshot = plug_board_->snapshot();
        const auto found =
            std::find_if(snapshot->applications.begin(), snapshot->applications.end(),
                         [&](const auto& app) { return app.id == id; });
        require(found != snapshot->applications.end(), "app record disappeared");
        return *found;
    };
    const auto bundle = [&](std::size_t count, bool expand) {
        for (const auto& column : plug_board_view_->layout_diagnostics().sockets) {
            auto* rows = dynamic_cast<wxVListBox*>(
                wxWindow::FindWindowById(column.applications_control_id, plug_board_view_));
            auto* accessible = rows ? rows->GetAccessible() : nullptr;
            int children = 0;
            if (!accessible || accessible->GetChildCount(&children) != wxACC_OK)
                continue;
            for (int child = 1; child <= children; ++child) {
                wxString name;
                if (accessible->GetName(child, &name) != wxACC_OK ||
                    !name.StartsWith(wxString::Format("powershell (%zu instances)", count)))
                    continue;
                if (expand && name.EndsWith("collapsed"))
                    accessible->DoDefaultAction(child);
                return std::pair{rows, static_cast<std::size_t>(child - 1)};
            }
        }
        return std::pair<wxVListBox*, std::size_t>{nullptr, 0};
    };
    // Earlier disclosure checks deliberately leave activity sections collapsed.
    // Expand the real Inactive headers before looking for their app groups.
    for (const auto& column : plug_board_view_->layout_diagnostics().sockets) {
        auto* rows = wxWindow::FindWindowById(column.applications_control_id, plug_board_view_);
        auto* accessible = rows ? rows->GetAccessible() : nullptr;
        int children = 0;
        if (!accessible || accessible->GetChildCount(&children) != wxACC_OK)
            continue;
        for (int child = 1; child <= children; ++child) {
            wxString name;
            if (accessible->GetName(child, &name) == wxACC_OK && name.StartsWith("Inactive (") &&
                name.EndsWith("collapsed")) {
                require(accessible->DoDefaultAction(child) == wxACC_OK,
                        "could not expand Inactive for grouped apps");
                break;
            }
        }
    }
    pump([&] { return bundle(3, false).first != nullptr; });
    auto [group_rows, group_row] = bundle(3, true);
    require(group_rows, "shared hosts lack a real expandable group");
    const auto app_menu = [&](const std::string& id, const char* action) {
        const auto rect = plug_board_view_->application_screen_rect(id, true);
        require(rect.has_value(), "expanded app has no row geometry");
        wxWindow* rows = nullptr;
        for (const auto& column : plug_board_view_->layout_diagnostics().sockets)
            if (std::find(column.rendered_application_ids.begin(),
                          column.rendered_application_ids.end(),
                          id) != column.rendered_application_ids.end())
                rows = wxWindow::FindWindowById(column.applications_control_id, plug_board_view_);
        require(rows, "app row is not rendered");
        plug_smoke_menu_ = action;
        wxContextMenuEvent event(wxEVT_CONTEXT_MENU, rows->GetId(),
                                 rect->GetPosition() + wxPoint(8, 8));
        rows->GetEventHandler()->ProcessEvent(event);
        require(!plug_smoke_menu_, "context action was not constructed");
    };
    class RenameAnswer final : public wxModalDialogHook {
      public:
        bool seen = false;
        int Enter(wxDialog* dialog) override {
            auto* entry = dynamic_cast<wxTextEntryDialog*>(dialog);
            if (!entry || dialog->GetTitle() != "Rename app")
                return wxID_NONE;
            seen = true;
            entry->CallAfter([entry] {
                entry->SetValue("My PowerShell script");
                entry->EndModal(wxID_OK);
            });
            return wxID_NONE;
        }
    } rename;
    rename.Register();
    app_menu(hosts[0], "Rename");
    rename.Unregister();
    pump([&] { return view_app(hosts[0]).display_name == "My PowerShell script"; });
    require(rename.seen && view_app(hosts[0]).group_name == "powershell",
            "rename changed the group identity");
    const auto* board = pb::find_board(*plug_board_->snapshot()->library, board_id);
    require(board->working.sockets.size() > 1, "assignment fixture needs a custom socket");
    const auto socket_id = board->working.sockets[1].id;
    command(pb::MoveApplication{board_id, hosts[0], socket_id});
    // Keep all instances together for group-menu checks after checking rename.
    command(pb::MoveApplication{board_id, hosts[0], "system"});
    auto* exempt_button = wxWindow::FindWindowByLabel("Exempt apps", plug_board_view_);
    auto* clear_button = wxWindow::FindWindowByLabel("Clear inactive apps", plug_board_view_);
    auto* mixer_button = wxWindow::FindWindowByLabel("Windows Mixer", plug_board_view_);
    require(exempt_button && clear_button && mixer_button &&
                exempt_button->GetContainingSizer() == mixer_button->GetContainingSizer() &&
                clear_button->GetPosition().y > mixer_button->GetPosition().y,
            "new buttons do not follow the requested mixer layout");
    require(exempt_button->GetToolTipText() ==
                    "Apps on this list won't show as inactive on Plug Board" &&
                clear_button->GetToolTipText() ==
                    "Tidies your list, but the apps will still remember where they live.",
            "authored tooltip changed");
    class ExemptAnswer final : public wxModalDialogHook, public wxEvtHandler {
      public:
        Frame* frame{};
        bool save = false, seen = false;
        wxColour foreground;
        wxTimer timer{this};
        wxDialog* dialog{};
        std::chrono::steady_clock::time_point deadline;
        ExemptAnswer() {
            Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
                auto* status = wxWindow::FindWindowByName("Exempt app save status", dialog);
                if (status && status->GetLabel() == "Saved.") {
                    timer.Stop();
                    wxCommandEvent close(wxEVT_BUTTON, wxID_CLOSE);
                    dialog->FindWindow(wxID_CLOSE)->GetEventHandler()->ProcessEvent(close);
                } else if (std::chrono::steady_clock::now() >= deadline) {
                    throw std::runtime_error(
                        "Exempt app Apply did not finish: " +
                        (status ? utf8(status->GetLabel()) : "missing status"));
                }
            });
        }
        int Enter(wxDialog* candidate) override {
            if (candidate->GetName() != "Exempt apps dialog")
                return wxID_NONE;
            seen = true;
            dialog = candidate;
            dialog->CallAfter([this] {
                auto* field = dynamic_cast<wxTextCtrl*>(
                    wxWindow::FindWindowByName("Exempt app names", dialog));
                auto* message = wxWindow::FindWindowByName("Exempt app instructions", dialog);
                const wxString exact =
                    "Apps on this list will not be shown on the inactive list.\nIf the app is on "
                    "the list it will still be shown as active if it is producing audio.\nType the "
                    "names of apps you want to exempt, separated by commas.";
                if (!field || !field->IsEditable() || !message || message->GetHelpText() != exact ||
                    field->GetForegroundColour() != foreground ||
                    field->GetClientSize().y < field->GetCharHeight() * 5 ||
                    !(GetWindowLongPtrW(static_cast<HWND>(field->GetHandle()), GWL_STYLE) &
                      WS_VSCROLL))
                    throw std::runtime_error(
                        "Exempt popup lost wording, theme, five-row size or scrolling");
                const auto before = field->GetSize();
                const auto original_size = dialog->GetSize();
                dialog->SetSize(original_size + dialog->FromDIP(wxSize(100, 70)));
                dialog->Layout();
                if (field->GetSize().x <= before.x || field->GetSize().y <= before.y)
                    throw std::runtime_error("Exempt field did not grow with the popup");
                dialog->SetSize(original_size);
                dialog->Layout();
                if (field->GetSize() != before)
                    throw std::runtime_error("Exempt field did not contract with the popup");
                const auto initial = frame->plug_board_->snapshot()->library->inactive_exempt_apps;
                field->ChangeValue("Custom app, powershell");
                auto* common =
                    wxWindow::FindWindowByName("Exempt common Windows processes", dialog);
                wxCommandEvent amend(wxEVT_BUTTON, common->GetId());
                common->GetEventHandler()->ProcessEvent(amend);
                const auto names = pb::parse_exemptions(utf8(field->GetValue()));
                if (names.empty() || names[0] != "Custom app" ||
                    std::count_if(names.begin(), names.end(),
                                  [](const auto& name) {
                                      return pb::normalized_app_name(name) == "powershell";
                                  }) != 1 ||
                    frame->plug_board_->snapshot()->library->inactive_exempt_apps != initial)
                    throw std::runtime_error("Common process suggestions replaced custom names, "
                                             "duplicated or applied themselves");
                field->ChangeValue(save ? "powershell, My PowerShell script" : "Do not save this");
                if (save) {
                    wxCommandEvent apply(wxEVT_BUTTON, wxID_APPLY);
                    dialog->FindWindow(wxID_APPLY)->GetEventHandler()->ProcessEvent(apply);
                    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    timer.Start(25);
                } else {
                    wxCommandEvent close(wxEVT_BUTTON, wxID_CLOSE);
                    dialog->FindWindow(wxID_CLOSE)->GetEventHandler()->ProcessEvent(close);
                }
            });
            return wxID_NONE;
        }
    };
    const auto original_theme = theme_choice_->GetSelection();
    for (int choice : {0, 1}) {
        theme_choice_->SetSelection(choice);
        wxCommandEvent theme(wxEVT_CHOICE, theme_choice_->GetId());
        theme_choice_->GetEventHandler()->ProcessEvent(theme);
        ExemptAnswer answer;
        answer.frame = this;
        answer.save = choice == 0;
        answer.foreground = choice == 0 ? *wxWHITE : *wxBLACK;
        answer.Register();
        click(exempt_button);
        answer.Unregister();
        require(answer.seen, "Exempt apps did not open its actual popup");
        require(plug_board_->snapshot()->library->inactive_exempt_apps ==
                    std::vector<std::string>{"powershell", "My PowerShell script"},
                "Apply or unapplied Close changed the wrong list");
    }
    theme_choice_->SetSelection(original_theme);
    wxCommandEvent restore_theme(wxEVT_CHOICE, theme_choice_->GetId());
    theme_choice_->GetEventHandler()->ProcessEvent(restore_theme);
    require(std::all_of(hosts.begin(), hosts.end(),
                        [&](const auto& id) { return view_app(id).hidden_inactive; }),
            "exemption left an inactive app visible");
    command(pb::SetInactiveExemptions{original->library->inactive_exempt_apps});
    pump([&] { return bundle(3, false).first != nullptr; });
    std::tie(group_rows, group_row) = bundle(3, false);
    group_rows->ScrollToRow(group_row);
    plug_smoke_menu_ = "Clear group";
    wxContextMenuEvent group_action(
        wxEVT_CONTEXT_MENU, group_rows->GetId(),
        group_rows->ClientToScreen(group_rows->GetItemRect(group_row).GetPosition() +
                                   wxPoint(8, 8)));
    group_rows->GetEventHandler()->ProcessEvent(group_action);
    require(!plug_smoke_menu_, "Clear group was not on its context menu");
    pump([&] {
        return std::all_of(hosts.begin(), hosts.end(),
                           [&](const auto& id) { return view_app(id).hidden_inactive; });
    });
    command(pb::MoveApplication{board_id, hosts[0], socket_id});
    for (std::size_t i = original_sessions.size(); i < sessions.size(); ++i)
        sessions[i].state = pb::SessionState::Active;
    plug_simulation_->set_sessions({sessions, true, {}});
    pump([&] {
        return std::all_of(hosts.begin(), hosts.end(), [&](const auto& id) {
            return view_app(id).activity == pb::Activity::Active && !view_app(id).hidden_inactive;
        });
    });
    require(view_app(hosts[0]).socket_id == socket_id, "returning app lost its socket");
    for (std::size_t i = original_sessions.size(); i < sessions.size(); ++i)
        sessions[i].state = pb::SessionState::Inactive;
    plug_simulation_->set_sessions({sessions, true, {}});
    pump([&] {
        return std::all_of(hosts.begin(), hosts.end(), [&](const auto& id) {
            return view_app(id).activity == pb::Activity::Inactive;
        });
    });
    bundle(2, true);
    app_menu(hosts[1], "Clear app");
    pump([&] { return view_app(hosts[1]).hidden_inactive; });
    require(!view_app(hosts[0]).hidden_inactive && !view_app(hosts[2]).hidden_inactive,
            "Clear app cleared another entry");
    click(clear_button);
    pump([&] {
        return std::all_of(hosts.begin(), hosts.end(),
                           [&](const auto& id) { return view_app(id).hidden_inactive; });
    });
    require(
        pb::assigned_socket(pb::find_board(*plug_board_->snapshot()->library, board_id)->working,
                            hosts[0]) == socket_id,
        "clearing removed saved assignment");
    command(pb::SetBoardSettings{board_id, original_settings.show_inactive_sources,
                                 original_settings.recently_active_seconds});
    plug_simulation_->set_sessions({original_sessions, true, {}});
}
} // namespace audiohole::gui
