#include "dialogs.hpp"
#include "view_state.hpp"

#include <wx/access.h>
#include <wx/dcbuffer.h>
#include <wx/statline.h>

#include <algorithm>

namespace audiohole::plug_board::desktop {
namespace {
// The vertical tab behaves as a button for both keyboard and screen readers.
class TabAccessible final : public wxAccessible {
  public:
    explicit TabAccessible(DrawerTab* owner) : wxAccessible(owner), tab(*owner) {}
    wxAccStatus GetName(int child, wxString* value) override {
        if (child)
            return wxACC_FAIL;
        *value = tab.GetName();
        return wxACC_OK;
    }
    wxAccStatus GetHelpText(int child, wxString* value) override {
        if (child)
            return wxACC_FAIL;
        *value = tab.GetToolTipText();
        return wxACC_OK;
    }
    wxAccStatus GetRole(int child, wxAccRole* role) override {
        if (child)
            return wxACC_FAIL;
        *role = wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }
    wxAccStatus GetState(int child, long* state) override {
        if (child)
            return wxACC_FAIL;
        *state = wxACC_STATE_SYSTEM_FOCUSABLE |
                 (tab.expanded ? wxACC_STATE_SYSTEM_EXPANDED : wxACC_STATE_SYSTEM_COLLAPSED);
        if (tab.HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        return wxACC_OK;
    }
    wxAccStatus GetChildCount(int* count) override {
        *count = 0;
        return wxACC_OK;
    }
    wxAccStatus GetLocation(wxRect& rectangle, int child) override {
        if (child)
            return wxACC_FAIL;
        rectangle = tab.GetScreenRect();
        return wxACC_OK;
    }
    wxAccStatus GetDefaultAction(int child, wxString* value) override {
        if (child)
            return wxACC_FAIL;
        *value = "Press";
        return wxACC_OK;
    }
    wxAccStatus DoDefaultAction(int child) override {
        if (child)
            return wxACC_FAIL;
        wxCommandEvent event(wxEVT_BUTTON, tab.GetId());
        event.SetEventObject(&tab);
        tab.GetEventHandler()->ProcessEvent(event);
        return wxACC_OK;
    }

  private:
    DrawerTab& tab;
};
} // namespace

// This panel overlays the workspace. Its controls issue typed commands through
// the same service as an embedded host; no configuration or COM work lives here.
void PlugBoardView::Implementation::build_drawer() {
    drawer = new wxScrolledWindow(&panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxVSCROLL | wxBORDER_SIMPLE);
    drawer->SetName("Routing Board Settings");
    drawer->SetScrollRate(0, panel.FromDIP(12));
    auto* content = new wxBoxSizer(wxVERTICAL);
    const int gap = panel.FromDIP(8);
    auto label = [&](const wxString& text) {
        auto* value = new wxStaticText(drawer, wxID_ANY, text);
        content->Add(value, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);
        return value;
    };
    auto line = [&] { content->Add(new wxStaticLine(drawer), 0, wxEXPAND | wxALL, gap); };
    auto* title = label("Routing Board Settings");
    title->SetFont(title->GetFont().Bold());
    label("Routing Board:");
    boards = choice(
        drawer, "Routing Board",
        "Choose a saved routing board. Manual selection lasts until another profile activates.");
    content->Add(boards, 0, wxEXPAND | wxALL, gap);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    add_board =
        button(drawer, "+", "Create a routing board with its own System socket. Clean slate.",
               "New board");
    delete_board =
        button(drawer, L"\u2212", "Delete this board after confirmation. The last one has tenure.",
               "Delete board");
    save = button(drawer, "Save",
                  "Save a checkpoint of this board. Working changes are already saved.");
    save_as = button(drawer, "Save As", "Duplicate this board with new board and socket IDs.");
    rename_board =
        button(drawer, "Rename", "Rename this routing board. A little rebranding never hurt.",
               "Rename board");
    for (auto* control : {add_board, delete_board, save, save_as, rename_board}) {
        control->SetMinSize(
            panel.FromDIP(wxSize(control == add_board || control == delete_board ? 28
                                 : control == save                               ? 43
                                                                                 : 61,
                                 32)));
        actions->Add(control, 0, wxRIGHT, panel.FromDIP(3));
    }
    content->Add(actions, 0, wxLEFT | wxRIGHT, gap);
    line();
    use_default = new wxCheckBox(drawer, wxID_ANY, "Use as default");
    use_default->SetName("Use as default");
    use_default->SetToolTip("Make this the default board. There can be only one.");
    content->Add(use_default, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);
    associate = new wxCheckBox(drawer, wxID_ANY, "Associate with profile");
    associate->SetName("Associate with profile");
    associate->SetToolTip(
        "Associate this board with a profile for automatic selection when it activates.");
    content->Add(associate, 0, wxLEFT | wxRIGHT, gap);
    profiles = choice(drawer, "Profile",
                      "Choose which profile to associate. This won't activate that profile.");
    content->Add(profiles, 0, wxEXPAND | wxALL, gap);
    line();
    inactive = new wxCheckBox(drawer, wxID_ANY, "Show inactive sources");
    inactive->SetName("Show inactive sources");
    inactive->SetToolTip(
        "Show expandable inactive apps. Hidden apps keep their socket assignments.");
    content->Add(inactive, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);
    label("Recently active window:");
    recent = choice(drawer, "Recently active window",
                    "How long an app stays in Recently Active after it stops making audio.");
    content->Add(recent, 0, wxEXPAND | wxALL, gap);
    line();
    label("Sockets");
    auto* socket_row = new wxBoxSizer(wxHORIZONTAL);
    sockets = new ListBox(drawer, wxID_ANY, panel.FromDIP(wxSize(220, 160)));
    sockets->SetName("Sockets");
    sockets->SetToolTip("Drag to reorder sockets. System stays first. F2 renames; Delete removes.");
    socket_row->Add(sockets, 1, wxEXPAND | wxRIGHT, panel.FromDIP(5));
    auto* socket_actions = new wxBoxSizer(wxVERTICAL);
    add_socket =
        button(drawer, "+", "Create a new socket. Give your apps somewhere to go.", "New socket");
    delete_socket =
        button(drawer, L"\u2212", "Delete this socket. Its apps get sent back to System.",
               "Delete socket");
    for (auto* control : {add_socket, delete_socket}) {
        control->SetMinSize(panel.FromDIP(wxSize(30, 32)));
        socket_actions->Add(control, 0, wxBOTTOM, panel.FromDIP(4));
    }
    socket_row->Add(socket_actions);
    content->Add(socket_row, 0, wxEXPAND | wxALL, gap);
    windows_mixer =
        button(drawer, "Windows Mixer",
               "Open the Windows volume mixer. Sometimes you need the original equipment.");
    auto* mixer_row = new wxBoxSizer(wxHORIZONTAL);
    mixer_row->Add(windows_mixer, 1, wxRIGHT, panel.FromDIP(4));
    exempt_apps =
        button(drawer, "Exempt apps", "Apps on this list won't show as inactive on Plug Board");
    mixer_row->Add(exempt_apps, 1);
    content->Add(mixer_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
    clear_inactive_button =
        button(drawer, "Clear inactive apps",
               "Tidies your list, but the apps will still remember where they live.");
    content->Add(clear_inactive_button, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
    clear_inactive_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { clear_inactive(); });
    exempt_apps->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { exempt_apps_dialog(); });
    content->AddSpacer(gap);
    drawer->SetSizer(content);
    drawer->FitInside();
    windows_mixer->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        constexpr auto uri = "ms-settings:apps-volume";
        const bool opened = options.open_url ? options.open_url(uri) : wxLaunchDefaultBrowser(uri);
        if (!opened)
            show_message(&panel,
                         "Windows Mixer could not be opened. Open Settings > System > "
                         "Sound > Volume mixer.",
                         "Windows Mixer", wxOK | wxICON_ERROR, appearance);
    });
    add_board->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { new_board(); });
    delete_board->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { remove_board(); });
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (board())
            submit(SaveBoard{board()->id});
    });
    save_as->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { copy_board(); });
    rename_board->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { rename_current_board(); });
    add_socket->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { new_socket(); });
    delete_socket->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { remove_socket(); });
    boards->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        const int value = boards->GetSelection();
        if (!rendering && value >= 0 && static_cast<std::size_t>(value) < board_ids.size())
            submit(SelectBoard{board_ids[static_cast<std::size_t>(value)]});
    });
    use_default->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (!rendering && board()) {
            if (use_default->IsChecked())
                submit(SetDefaultBoard{board()->id});
            else
                use_default->SetValue(true);
        }
    });
    auto update_profile = [this] {
        const int selected = profiles->GetSelection();
        if (!rendering && board() && selected >= 0 &&
            static_cast<std::size_t>(selected) < profile_ids.size())
            submit(LinkProfile{profile_ids[static_cast<std::size_t>(selected)], board()->id,
                               associate->IsChecked()});
    };
    associate->Bind(wxEVT_CHECKBOX, [update_profile](wxCommandEvent&) { update_profile(); });
    profiles->Bind(wxEVT_CHOICE, [update_profile](wxCommandEvent&) { update_profile(); });
    auto settings = [this] {
        const int value = recent->GetSelection();
        if (!rendering && board() && value >= 0 &&
            static_cast<std::size_t>(value) < recent_values.size())
            submit(SetBoardSettings{board()->id, inactive->IsChecked(),
                                    recent_values[static_cast<std::size_t>(value)]});
    };
    inactive->Bind(wxEVT_CHECKBOX, [settings](wxCommandEvent&) { settings(); });
    recent->Bind(wxEVT_CHOICE, [settings](wxCommandEvent&) { settings(); });
    sockets->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        const int selected = sockets->GetSelection();
        delete_socket->Enable(!busy() && selected > 0);
        if (selected >= 0 && static_cast<std::size_t>(selected) < socket_ids.size())
            ensure_socket_visible(socket_ids[static_cast<std::size_t>(selected)]);
    });
    sockets->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) {
        const int selected = sockets->GetSelection();
        if (selected >= 0)
            rename_socket(socket_ids[static_cast<std::size_t>(selected)]);
    });
    sockets->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event) {
        wxPoint point = event.GetPosition();
        int selected = sockets->GetSelection();
        if (point != wxDefaultPosition) {
            point = sockets->ScreenToClient(point);
            selected = sockets->HitTest(point);
        } else {
            wxRect rectangle;
            if (selected >= 0 &&
                sockets->GetItemRect(static_cast<std::size_t>(selected), rectangle))
                point = rectangle.GetBottomLeft();
        }
        if (selected < 0 || static_cast<std::size_t>(selected) >= socket_ids.size())
            return;
        sockets->SetSelection(selected);
        delete_socket->Enable(!busy() && selected > 0);
        socket_menu(socket_ids[static_cast<std::size_t>(selected)], *sockets, point);
    });
    sockets->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        const int selected = sockets->GetSelection();
        if (selected < 0) {
            event.Skip();
            return;
        }
        if (event.GetKeyCode() == WXK_ESCAPE && sockets->HasCapture()) {
            finish_socket_drag(false);
        } else if (event.GetKeyCode() == WXK_F2)
            rename_socket(socket_ids[static_cast<std::size_t>(selected)]);
        else if (event.GetKeyCode() == WXK_DELETE)
            remove_socket();
        else if (event.AltDown() && event.GetKeyCode() == WXK_UP && selected > 1)
            reorder_socket(static_cast<std::size_t>(selected),
                           static_cast<std::size_t>(selected - 1));
        else if (event.AltDown() && event.GetKeyCode() == WXK_DOWN && selected > 0 &&
                 static_cast<std::size_t>(selected + 1) < socket_ids.size())
            reorder_socket(static_cast<std::size_t>(selected),
                           static_cast<std::size_t>(selected + 1));
        else
            event.Skip();
    });
    // Consume the pointer sequence ourselves. The native listbox's default
    // selection procedure captures the pointer and can eat the motion/up events
    // needed by a second drag implementation. Keyboard/accessibility selection
    // remains native. A click or cancellation never changes persistent order.
    sockets->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& event) {
        const int hit = sockets->HitTest(event.GetPosition());
        if (busy() || hit < 0 || static_cast<std::size_t>(hit) >= socket_ids.size()) {
            event.Skip();
            return;
        }
        sockets->SetFocus();
        sockets->SetSelection(hit);
        wxCommandEvent selected(wxEVT_LISTBOX, sockets->GetId());
        selected.SetEventObject(sockets);
        selected.SetInt(hit);
        sockets->GetEventHandler()->ProcessEvent(selected);
        if (hit == 0)
            return; // System can be selected but is always first.
        socket_drag_index = hit;
        socket_drag_start = event.GetPosition();
        socket_drag_context = snapshot;
        socket_drag_moved = false;
        ++interaction_depth; // Keep row IDs/order stable until release or cancel.
        sockets->CaptureMouse();
    });
    sockets->Bind(wxEVT_MOTION, [this](wxMouseEvent& event) {
        if (!socket_drag_context || !sockets->HasCapture()) {
            event.Skip();
            return;
        }
        if (!event.LeftIsDown()) {
            finish_socket_drag(false);
            return;
        }
        if (std::abs(event.GetY() - socket_drag_start.y) >
            wxSystemSettings::GetMetric(wxSYS_DRAG_Y, sockets)) {
            socket_drag_moved = true;
            sockets->SetCursor(wxCursor(wxCURSOR_SIZENS));
            // One row per delivered motion permits dragging through a long
            // scrolled list without an idle or background polling timer.
            const auto top = sockets->GetTopItem();
            if (event.GetY() < 0 && top > 0)
                sockets->SetFirstItem(top - 1);
            else if (event.GetY() >= sockets->GetClientSize().y &&
                     top + 1 < static_cast<int>(sockets->GetCount()))
                sockets->SetFirstItem(top + 1);
        }
    });
    sockets->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
        if (socket_drag_context)
            finish_socket_drag(true, event.GetPosition());
        else
            event.Skip();
    });
    sockets->Bind(wxEVT_MOUSE_CAPTURE_LOST,
                  [this](wxMouseCaptureLostEvent&) { finish_socket_drag(false); });
}

void PlugBoardView::Implementation::finish_socket_drag(bool commit, const wxPoint& point) {
    const auto context = std::exchange(socket_drag_context, {});
    if (!context)
        return;
    const int source = std::exchange(socket_drag_index, wxNOT_FOUND);
    const bool moved = std::exchange(socket_drag_moved, false);
    const int destination = point == wxDefaultPosition ? wxNOT_FOUND : sockets->HitTest(point);
    if (sockets->HasCapture())
        sockets->ReleaseMouse();
    sockets->SetCursor(wxNullCursor);
    --interaction_depth;
    // An unrelated discovery publication may arrive during capture. Reuse its
    // revision only when this exact board's working configuration is unchanged.
    const auto latest = module ? module->snapshot() : nullptr;
    const auto* original =
        context->library ? find_board(*context->library, context->selected_board_id) : nullptr;
    const auto* current = latest && latest->library
                              ? find_board(*latest->library, context->selected_board_id)
                              : nullptr;
    if (commit && moved && source > 0 && destination > 0 && source != destination && original &&
        current && latest->selected_board_id == original->id &&
        current->working_revision == original->working_revision &&
        static_cast<std::size_t>(source) < original->working.sockets.size() &&
        static_cast<std::size_t>(destination) < original->working.sockets.size())
        submit(ReorderSocket{original->id,
                             original->working.sockets[static_cast<std::size_t>(source)].id,
                             static_cast<std::size_t>(destination)},
               latest);
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
}

void PlugBoardView::Implementation::render_drawer() {
    const bool previous_rendering = rendering;
    rendering = true;
    const auto* current = board();
    const bool enabled = !busy() && current;
    exempt_apps->Enable(enabled);
    clear_inactive_button->Enable(enabled && snapshot &&
                                  std::any_of(snapshot->applications.begin(),
                                              snapshot->applications.end(), [](const auto& app) {
                                                  return app.activity == Activity::Inactive &&
                                                         !app.hidden_inactive;
                                              }));
    std::string selected_profile;
    if (profiles->GetSelection() >= 0 &&
        static_cast<std::size_t>(profiles->GetSelection()) < profile_ids.size())
        selected_profile = profile_ids[static_cast<std::size_t>(profiles->GetSelection())];
    if (current && rendered_board != current->id) {
        // A board may be linked to a profile beyond the first dropdown item.
        // Reveal that existing link on board changes instead of showing an
        // unchecked, disabled selector that hides the association.
        selected_profile.clear();
        for (const auto& profile : snapshot->profiles) {
            const auto linked = snapshot->library->profile_associations.find(profile.id);
            if (linked != snapshot->library->profile_associations.end() &&
                linked->second == current->id) {
                selected_profile = profile.id;
                break;
            }
        }
    }
    const int old_socket = sockets->GetSelection();
    std::string selected_socket;
    if (old_socket >= 0 && static_cast<std::size_t>(old_socket) < socket_ids.size())
        selected_socket = socket_ids[static_cast<std::size_t>(old_socket)];
    board_ids.clear();
    wxArrayString board_names;
    if (snapshot && snapshot->library)
        for (const auto& item : snapshot->library->boards) {
            board_ids.push_back(item.id);
            board_names.Add(ui_text(item.display_name));
        }
    if (boards->GetStrings() != board_names)
        boards->Set(board_names);
    if (current) {
        const auto item = std::find(board_ids.begin(), board_ids.end(), current->id);
        boards->SetSelection(static_cast<int>(item - board_ids.begin()));
    }
    profile_ids.clear();
    wxArrayString profile_names;
    if (snapshot)
        for (const auto& item : snapshot->profiles) {
            profile_ids.push_back(item.id);
            profile_names.Add(ui_text(item.display_name));
        }
    if (profile_names.empty())
        profile_names.Add("No host profiles available");
    if (profiles->GetStrings() != profile_names)
        profiles->Set(profile_names);
    const auto chosen = std::find(profile_ids.begin(), profile_ids.end(), selected_profile);
    profiles->SetSelection(
        chosen != profile_ids.end() ? static_cast<int>(chosen - profile_ids.begin()) : 0);
    profiles->Enable(enabled && !profile_ids.empty());
    associate->Enable(enabled && !profile_ids.empty());
    const int profile = profiles->GetSelection();
    bool linked = false;
    if (current && profile >= 0 && static_cast<std::size_t>(profile) < profile_ids.size()) {
        const auto association = snapshot->library->profile_associations.find(
            profile_ids[static_cast<std::size_t>(profile)]);
        linked = association != snapshot->library->profile_associations.end() &&
                 association->second == current->id;
    }
    associate->SetValue(linked);
    profiles->Enable(enabled && linked);
    socket_ids.clear();
    wxArrayString socket_names;
    if (current)
        for (const auto& socket : current->working.sockets) {
            socket_ids.push_back(socket.id);
            socket_names.Add(ui_text(socket.display_name));
        }
    if (sockets->GetStrings() != socket_names)
        sockets->Set(socket_names);
    const auto socket = std::find(socket_ids.begin(), socket_ids.end(), selected_socket);
    if (!socket_ids.empty()) {
        int selection =
            socket == socket_ids.end() ? 0 : static_cast<int>(socket - socket_ids.begin());
        // Removing the selected socket should choose its next neighbor (or the
        // previous final custom socket), not silently select protected System.
        // Keep this fallback local to the same board; board switches start at 0.
        if (socket == socket_ids.end() && current && current->id == rendered_board &&
            old_socket > 0 && socket_ids.size() > 1)
            selection = std::min(old_socket, static_cast<int>(socket_ids.size()) - 1);
        sockets->SetSelection(selection);
    }
    sockets->Enable(enabled);
    delete_socket->Enable(enabled && sockets->GetSelection() > 0);
    for (wxWindow* control : std::vector<wxWindow*>{boards, add_board, save, save_as, rename_board,
                                                    add_socket, inactive, recent, use_default})
        control->Enable(enabled);
    delete_board->Enable(enabled && snapshot->library->boards.size() > 1);
    if (current) {
        use_default->SetValue(snapshot->library->default_board_id == current->id);
        inactive->SetValue(current->working.settings.show_inactive_sources);
        auto values = recent_values;
        const auto seconds = current->working.settings.recently_active_seconds;
        if (std::find(values.begin(), values.end(), seconds) == values.end())
            values.push_back(seconds);
        recent_values = std::move(values);
        wxArrayString times;
        for (const auto value : recent_values)
            times.Add(value == 86400    ? wxString("Last 24 hours")
                      : value % 60 == 0 ? wxString::Format("Last %u minutes", value / 60)
                                        : wxString::Format("Last %u seconds", value));
        if (recent->GetStrings() != times)
            recent->Set(times);
        recent->SetSelection(
            static_cast<int>(std::find(recent_values.begin(), recent_values.end(), seconds) -
                             recent_values.begin()));
    }
    rendering = previous_rendering;
}

// Capture the context before showing any modal UI. A profile activation can
// change boards during its nested event loop; the service rejects stale edits.
void PlugBoardView::Implementation::new_board() {
    if (busy())
        return;
    const auto context = snapshot;
    if (const auto value = name("New routing board", "New Board"))
        submit(CreateBoard{*value}, context);
}
void PlugBoardView::Implementation::copy_board() {
    if (busy() || !board())
        return;
    const auto context = snapshot;
    const auto id = board()->id;
    const auto initial = board()->display_name + " Copy";
    if (const auto value = name("Save board as", initial))
        submit(SaveBoardAs{id, *value}, context);
}
void PlugBoardView::Implementation::rename_current_board() {
    if (busy() || !board())
        return;
    const auto context = snapshot;
    const auto id = board()->id;
    const auto initial = board()->display_name;
    if (const auto value = name("Rename routing board", initial))
        submit(RenameBoard{id, *value}, context);
}
void PlugBoardView::Implementation::remove_board() {
    if (busy() || !board() || snapshot->library->boards.size() < 2)
        return;
    const auto context = snapshot;
    const auto id = board()->id;
    const auto title = board()->display_name;
    std::vector<std::string> ids, names;
    for (const auto& candidate : context->library->boards)
        if (candidate.id != id) {
            ids.push_back(candidate.id);
            names.push_back(candidate.display_name);
        }
    std::string replacement = context->library->default_board_id;
    if (replacement == id) {
        const auto choice = select("Choose the replacement default board", names);
        if (!choice || *choice >= ids.size())
            return;
        replacement = ids[*choice];
    }
    std::size_t links = 0;
    for (const auto& [profile, linked] : context->library->profile_associations)
        if (linked == id)
            ++links;
    if (confirm("Delete routing board",
                "Delete '" + title + "'? " + std::to_string(links) +
                    " profile associations will revert to the designated default. Remaining "
                    "boards and application identities are retained."))
        submit(DeleteBoard{id, replacement, true}, context);
}
void PlugBoardView::Implementation::new_socket() {
    const auto* selected = board();
    if (busy() || !selected)
        return;
    // A modal popup can render a newer publication. Retain the original board
    // for validation; submit still checks its revision before committing.
    const auto context = snapshot;
    const auto id = selected->id;
    if (const auto value =
            name("New socket", options.require_distinct_socket_names ? "" : "New Socket")) {
        if (options.require_distinct_socket_names) {
            const bool blank = value->find_first_not_of(" \t\r\n") == std::string::npos;
            const bool duplicate =
                std::any_of(selected->working.sockets.begin(), selected->working.sockets.end(),
                            [&](const auto& socket) { return socket.display_name == *value; });
            if (blank || duplicate) {
                show_message(&panel,
                             blank ? "Enter a name for the new socket."
                                   : "Choose a name different from the existing sockets.",
                             "Socket name", wxOK | wxICON_INFORMATION, appearance);
                return;
            }
        }
        submit(CreateSocket{id, *value}, context);
    }
}
void PlugBoardView::Implementation::remove_socket() {
    const int selected = sockets->GetSelection();
    if (busy() || !board() || selected <= 0 ||
        static_cast<std::size_t>(selected) >= socket_ids.size())
        return;
    const auto socket_id = socket_ids[static_cast<std::size_t>(selected)];
    remove_socket(socket_id);
}
void PlugBoardView::Implementation::remove_socket(const std::string& socket_id) {
    if (busy() || !board() || socket_id == "system" ||
        std::none_of(board()->working.sockets.begin(), board()->working.sockets.end(),
                     [&](const auto& socket) { return socket.id == socket_id; }))
        return;
    // Header and list menus share the same deletion path. Select the affected
    // row before confirmation so a committed removal can retain its neighbor,
    // even if the drawer previously had System or another socket selected.
    const auto row = std::find(socket_ids.begin(), socket_ids.end(), socket_id);
    if (row != socket_ids.end())
        sockets->SetSelection(static_cast<int>(row - socket_ids.begin()));
    const auto context = snapshot;
    const auto board_id = board()->id;
    std::size_t count = 0;
    for (const auto& app : snapshot->applications)
        if (app.socket_id == socket_id)
            ++count;
    if (confirm("Delete socket", "Remove this socket and move its " + std::to_string(count) +
                                     " applications to System? Their Windows preferences will use "
                                     "System's current output."))
        submit(DeleteSocket{board_id, socket_id, true}, context);
}
void PlugBoardView::Implementation::rename_socket(const std::string& socket_id) {
    if (busy() || !board())
        return;
    const auto item = std::find_if(board()->working.sockets.begin(), board()->working.sockets.end(),
                                   [&](const auto& socket) { return socket.id == socket_id; });
    if (item == board()->working.sockets.end())
        return;
    const auto context = snapshot;
    const auto id = board()->id;
    const auto initial = item->display_name;
    if (const auto value = name("Rename socket", initial))
        submit(RenameSocket{id, socket_id, *value}, context);
}
void PlugBoardView::Implementation::reorder_socket(std::size_t source, std::size_t destination) {
    if (busy() || !board() || source == 0 || destination == 0 || source >= socket_ids.size() ||
        destination >= socket_ids.size())
        return;
    submit(ReorderSocket{board()->id, socket_ids[source], destination});
}

DrawerTab::DrawerTab(wxWindow* parent, std::function<void()> action)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxBORDER_SIMPLE | wxWANTS_CHARS),
      toggle(std::move(action)) {
    SetName("Routing Board Settings toggle");
    SetToolTip("Show or hide board settings without changing your scroll position.");
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetAccessible(new TabAccessible(this));
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { toggle(); });
    Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(appearance.surface));
        dc.Clear();
        dc.SetTextForeground(appearance.foreground);
        dc.SetFont(GetFont());
        const wxString label = expanded ? ">> Routing Board Settings" : "<< Routing Board Settings";
        dc.DrawRotatedText(label, FromDIP(4), GetClientSize().y - FromDIP(6), 90);
        if (HasFocus()) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(appearance.accent));
            dc.DrawRectangle(GetClientRect().Deflate(2));
        }
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) {
        SetFocus();
        toggle();
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_SPACE || event.GetKeyCode() == WXK_RETURN)
            toggle();
        else
            event.Skip();
    });
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
        Refresh();
        event.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
        Refresh();
        event.Skip();
    });
}
void DrawerTab::set_appearance(const ViewAppearance& value) {
    appearance = value;
    Refresh();
}

} // namespace audiohole::plug_board::desktop
