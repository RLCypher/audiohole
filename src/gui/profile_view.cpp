#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/access.h>

namespace audiohole::gui {
namespace {
#if wxUSE_ACCESSIBILITY
// A glyph's native button caption is not a useful screen-reader name.
class ListActionAccessible final : public wxWindowAccessible {
  public:
    explicit ListActionAccessible(wxWindow* window) : wxWindowAccessible(window) {}
    wxAccStatus GetName(int child, wxString* name) override {
        if (child != 0)
            return wxACC_NOT_IMPLEMENTED;
        *name = GetWindow()->GetName();
        return wxACC_OK;
    }
};
#endif
std::string buffer_caption(int milliseconds) {
    return milliseconds ? std::to_string(milliseconds) + " ms" : "Automatic";
}
std::optional<int> buffer_value(const wxSpinCtrl* control) {
    long value = 0;
    if (!control->GetTextValue().ToLong(&value) || value < 0 || value > 100)
        return {};
    return static_cast<int>(value);
}
wxButton* add_control(wxWindow* parent, wxSizer* row, const wxString& name, const wxString& help,
                      std::function<void()> action, const wxString& glyph = wxS("+")) {
    auto* control = new ActionButton(parent, glyph, wxBU_EXACTFIT);
    control->SetMinSize(parent->FromDIP(wxSize(32, 32)));
    control->SetName(name);
#if wxUSE_ACCESSIBILITY
    control->SetAccessible(new ListActionAccessible(control)); // Owned by the wx window.
#endif
    control->SetToolTip(help);
    row->Add(control, 0, wxALL, parent->FromDIP(3));
    control->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) { action(); });
    return control;
}
int context_item(wxListBox* list, const wxContextMenuEvent& event) {
    return event.GetPosition() == wxDefaultPosition
               ? list->GetSelection() // Keyboard context menu (Shift+F10/Menu key).
               : list->HitTest(list->ScreenToClient(event.GetPosition()));
}
} // namespace
void Frame::build_direction(wxWindow* parent, wxSizer* sizer, DirectionWidgets& w,
                            const wxString& title) {
    w.group = new wxStaticBoxSizer(wxVERTICAL, parent, title);
    sizer->Add(w.group, 0, wxEXPAND | wxALL, 4);
    auto* box = w.group->GetStaticBox();
    auto* endpoints = new wxBoxSizer(wxHORIZONTAL);
    auto* capture = new SurfacePanel(box, "capture-card");
    auto* playback = new SurfacePanel(box, "playback-card");
    auto* source_layout = new wxBoxSizer(wxVERTICAL);
    auto* destination_layout = new wxBoxSizer(wxVERTICAL);
    material_heading(capture, source_layout, "Source (Capture)", true);
    w.source = new ThemedCombo(capture, wxID_ANY);
    w.source->SetMinSize(FromDIP(wxSize(120, -1)));
    source_layout->Add(w.source, 0, wxEXPAND | wxALL, FromDIP(8));
    w.source_detail = label(capture, source_layout, "Select a source");
    capture->set_content_sizer(source_layout);
    material_heading(playback, destination_layout, "Destination (Playback)", false);
    w.destination = new ThemedCombo(playback, wxID_ANY);
    w.destination->SetMinSize(FromDIP(wxSize(120, -1)));
    destination_layout->Add(w.destination, 0, wxEXPAND | wxALL, FromDIP(8));
    w.destination_detail = label(playback, destination_layout, "Select a destination");
    playback->set_content_sizer(destination_layout);
    endpoints->Add(capture, 1, wxEXPAND | wxALL, FromDIP(5));
    label(box, endpoints, "→");
    endpoints->Add(playback, 1, wxEXPAND | wxALL, FromDIP(5));
    w.group->Add(endpoints, 0, wxEXPAND);
    w.information = new wxTextCtrl(
        box, wxID_ANY, "Select endpoints to see a format recommendation.", wxDefaultPosition,
        wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP | wxBORDER_NONE);
    w.information->SetName(title + " configuration information");
    // A defined eight-line viewport prevents asynchronous status text from
    // outgrowing a stale static-label allocation. Longer content stays scrollable.
    const int information_height = w.information->GetCharHeight() * 8 + FromDIP(12);
    w.information->SetMinSize(wxSize(0, information_height));
    w.information->SetMaxSize(wxSize(-1, information_height));
    w.information->SetSize(wxSize(w.information->GetSize().x, information_height));
    w.group->Add(w.information, 0, wxEXPAND | wxALL, FromDIP(4));
    auto* tuning = new wxStaticBoxSizer(wxVERTICAL, advanced_panel_, title + " tuning");
    w.tuning_group = tuning;
    advanced_panel_->GetSizer()->Add(tuning, 0, wxEXPAND | wxALL, FromDIP(4));
    box = tuning->GetStaticBox();
    label(box, tuning, "Audio format");
    w.format = new ThemedChoice(box, wxID_ANY);
    tuning->Add(w.format, 0, wxEXPAND | wxALL, 3);
    label(box, tuning, "Buffer (ms; 0 = Automatic)");
    w.buffer = new wxSpinCtrl(box, wxID_ANY, "0", wxDefaultPosition, wxDefaultSize,
                              wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 100, 0);
    auto* buffer_row = new wxBoxSizer(wxHORIZONTAL);
    buffer_row->Add(w.buffer, 0, wxALL, 3);
    w.buffer_apply = button(box, buffer_row, "Apply", [this, &w] { apply_buffer(w); });
    w.buffer_apply->Disable();
    w.buffer_apply->SetToolTip(
        "Save just this direction's buffer preference. A matching active Preview direction also "
        "updates its simulated buffer. Test Mode saves configuration only; no hardware buffer is "
        "applied.");
    tuning->Add(buffer_row, 0, wxEXPAND);
    w.buffer_state = label(box, tuning, "Edited/saved preferences; engine values unavailable.");
    w.buffer_state->SetMinSize(wxSize(1, -1));
    w.buffer->SetToolTip("Use 0 for Automatic, or request a duration in milliseconds. Smaller "
                         "values can reduce latency but "
                         "increase dropout risk. This is an edited preference, not a measured or "
                         "confirmed hardware buffer. Apply saves this preference; status shows any "
                         "simulated confirmation.");
    w.format->SetToolTip("Choose the recommended compatible format or an explicit format. The "
                         "visible evidence explains the recommendation.");
    w.source->SetToolTip("Choose the capture endpoint for this direction. Type to search; select a "
                         "listed device to bind it.");
    w.destination->SetToolTip("Choose the playback endpoint for this direction. The Windows name "
                              "is shown for the selected device.");
    w.mode = choices(box, tuning, "Operating mode", {"Automatic", "Shared", "Exclusive"});
    w.mode->SetToolTip("Automatic uses available evidence. Shared cooperates with other apps; "
                       "Exclusive can reserve a device. Changes wait for explicit Apply.");
    auto* fallback = new ThemedChoice(box, wxID_ANY);
    fallback->Append(wxS("Fallback device / channel mapping — unavailable"));
    fallback->SetSelection(0);
    fallback->Disable();
    tuning->Add(fallback, 0, wxEXPAND | wxALL, FromDIP(4));
    auto changed = [this, &w](bool source) {
        if (loading_ || hole_index_ < 0)
            return;
        capture_draft();
        update_beep_destinations();
        auto& d = (&w == &forward_)
                      ? draft_.holes.at(static_cast<std::size_t>(hole_index_)).forward
                      : draft_.holes.at(static_cast<std::size_t>(hole_index_)).reverse;
        populate_direction(w, d);
        dirty_ = true;
        present(source ? presentation::Message::Source : presentation::Message::Destination);
    };
    w.source->Bind(wxEVT_COMBOBOX, [changed](wxCommandEvent&) { changed(true); });
    w.destination->Bind(wxEVT_COMBOBOX, [changed](wxCommandEvent&) { changed(false); });
    w.format->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_)
            dirty_ = true;
    });
    w.mode->Bind(wxEVT_CHOICE, [this, &w](wxCommandEvent&) {
        if (loading_ || hole_index_ < 0)
            return;
        capture_draft();
        const auto& hole = draft_.holes.at(static_cast<std::size_t>(hole_index_));
        populate_direction(w, &w == &forward_ ? hole.forward : hole.reverse);
        dirty_ = true;
    });
    w.buffer->Bind(wxEVT_SPINCTRL, [this, &w](wxSpinEvent&) { acknowledge_buffer(w); });
    w.buffer->Bind(wxEVT_TEXT_ENTER, [this, &w](wxCommandEvent&) { acknowledge_buffer(w); });
    w.buffer->Bind(wxEVT_KILL_FOCUS, [this, &w](wxFocusEvent& event) {
        acknowledge_buffer(w);
        event.Skip();
    });
    w.buffer->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (!loading_) {
            dirty_ = true;
            refresh();
        }
    });
}
// The checkbox stages a policy change, preserving the last valid hidden values.
// A single delayed reaction follows settled intent; rapid toggles restart it.
void Frame::tuning_changed() {
    if (loading_ || hole_index_ < 0)
        return;
    auto& hole = draft_.holes.at(static_cast<std::size_t>(hole_index_));
    const bool enabled = advanced_tuning_->GetValue();
    advanced_tuning_->SetValue(hole.advanced_tuning);
    capture_draft();
    advanced_tuning_->SetValue(enabled);
    hole.advanced_tuning = enabled;
    auto* focused = wxWindow::FindFocus();
    if (!enabled && focused && advanced_panel_->IsDescendant(focused))
        advanced_tuning_->SetFocus();
    advanced_panel_->Show(enabled);
    populate_direction(forward_, hole.forward);
    populate_direction(reverse_, hole.reverse);
    auto* editor = static_cast<wxScrolledWindow*>(advanced_panel_->GetParent());
    editor->Layout();
    editor->FitInside();
    dirty_ = true;
    tuning_timer_.StartOnce(650);
    refresh();
}
// Spin, Enter, and focus loss can describe the same edit. Compare the settled value
// so polling, loading, and duplicate native notifications never consume commentary.
void Frame::acknowledge_buffer(DirectionWidgets& widgets) {
    if (loading_ || hole_index_ < 0)
        return;
    const auto parsed = buffer_value(widgets.buffer);
    if (!parsed) {
        refresh();
        return;
    }
    const int value = *parsed;
    if (value == widgets.acknowledged_buffer_ms)
        return;
    widgets.acknowledged_buffer_ms = value;
    dirty_ = true;
    present(presentation::Message::BufferChanged);
    refresh();
}
void Frame::apply_buffer(DirectionWidgets& widgets) {
    tuning_timer_.Stop();
    if (loading_ || hole_index_ < 0 || static_cast<std::size_t>(hole_index_) >= draft_.holes.size())
        return;
    const auto parsed = buffer_value(widgets.buffer);
    if (!parsed) {
        show_result(
            {false, "Enter 0 for Automatic or a whole buffer value from 1 to 100 milliseconds."});
        return;
    }
    capture_draft();
    const auto& hole = draft_.holes[static_cast<std::size_t>(hole_index_)];
    if (&widgets == &reverse_ && !hole.duplex)
        return;
    const auto result =
        controller_->update_buffer(viewed_, hole.id, &widgets == &reverse_, *parsed);
    commentary_copy_.clear(); // Replace the old "Save / Apply required" flavor after this result.
    show_result(result);
    if (result.ok) {
        if (const auto* saved = profile(controller_->snapshot(), viewed_))
            dirty_ = draft_ != *saved;
    }
    refresh();
}
std::unique_ptr<wxMenu> Frame::profile_menu(const std::string& id) {
    auto menu = std::make_unique<wxMenu>();
    menu->Append(wxID_EDIT, wxS("Rename…"),
                 "Rename the profile in the editor; Save keeps the change.");
    menu->Append(wxID_DUPLICATE, "Duplicate", "Create a separate copy of the saved profile.");
    menu->AppendSeparator();
    menu->Append(wxID_DELETE, "Delete", "Delete this saved profile after confirmation.");
    const auto& snapshot = controller_->snapshot();
    menu->Enable(wxID_DELETE, !(snapshot.armed && id == snapshot.active_profile) &&
                                  id != snapshot.pending_profile);
    const auto choose = [this, id] {
        if (!profile(controller_->snapshot(), id))
            return false;
        if (viewed_ == id)
            return true;
        if (!discard())
            return false;
        select_profile(id);
        return !viewed_.empty();
    };
    menu->Bind(
        wxEVT_MENU,
        [this, choose](wxCommandEvent&) {
            if (!choose())
                return;
            wxTextEntryDialog dialog(this, "Profile name", "Rename profile",
                                     text(draft_.display_name));
            if (dialog.ShowModal() == wxID_OK)
                rename_profile(utf8(dialog.GetValue()));
        },
        wxID_EDIT);
    menu->Bind(
        wxEVT_MENU,
        [this, choose](wxCommandEvent&) {
            if (!choose() || !discard())
                return;
            const auto* saved = profile(controller_->snapshot(), viewed_);
            if (!saved)
                return;
            const auto copy = controller_->create_profile(saved->display_name + " copy", viewed_);
            if (copy.empty())
                show_result({false, controller_->last_edit_error()});
            else
                select_profile(copy);
        },
        wxID_DUPLICATE);
    menu->Bind(
        wxEVT_MENU,
        [this, choose](wxCommandEvent&) {
            if (!choose())
                return;
            if (wxMessageBox("Delete this saved profile?", "Confirm profile deletion",
                             wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
                return;
            const auto result = controller_->remove_profile(viewed_);
            show_result(result);
            if (result.ok)
                select_profile(controller_->snapshot().profiles.empty()
                                   ? ""
                                   : controller_->snapshot().profiles.front().id);
        },
        wxID_DELETE);
    return menu;
}
std::unique_ptr<wxMenu> Frame::hole_menu(const std::string& id) {
    auto menu = std::make_unique<wxMenu>();
    menu->Append(wxID_EDIT, wxS("Rename…"), "Rename the hole in staged profile changes.");
    menu->Append(wxID_DUPLICATE, "Duplicate",
                 "Copy this hole's endpoints and directional settings.");
    menu->AppendSeparator();
    menu->Append(wxID_DELETE, "Delete", "Remove this hole from staged changes after confirmation.");
    const auto choose = [this, id] {
        capture_draft();
        const auto found = std::find_if(draft_.holes.begin(), draft_.holes.end(),
                                        [&](const auto& hole) { return hole.id == id; });
        if (found == draft_.holes.end())
            return false;
        const int index = static_cast<int>(found - draft_.holes.begin());
        if (index != hole_index_)
            load_hole(index);
        return true;
    };
    menu->Bind(
        wxEVT_MENU,
        [this, choose](wxCommandEvent&) {
            if (!choose())
                return;
            wxTextEntryDialog dialog(
                this, "Hole name", "Rename hole",
                text(draft_.holes[static_cast<std::size_t>(hole_index_)].display_name));
            if (dialog.ShowModal() == wxID_OK)
                rename_hole(utf8(dialog.GetValue()));
        },
        wxID_EDIT);
    menu->Bind(
        wxEVT_MENU,
        [this, choose](wxCommandEvent&) {
            if (choose())
                duplicate_hole();
        },
        wxID_DUPLICATE);
    menu->Bind(
        wxEVT_MENU,
        [this, choose](wxCommandEvent&) {
            if (choose())
                remove_hole();
        },
        wxID_DELETE);
    return menu;
}
void Frame::rename_profile(const std::string& name) {
    auto value = text(name);
    value.Trim(true).Trim(false);
    if (value.empty() || viewed_.empty()) {
        show_result({false, "Enter a profile name."});
        return;
    }
    capture_draft();
    draft_.display_name = utf8(value);
    dirty_ = true;
    const auto found = std::find(profile_ids_.begin(), profile_ids_.end(), viewed_);
    if (found != profile_ids_.end())
        profiles_->SetString(static_cast<unsigned>(found - profile_ids_.begin()), value);
    refresh();
}
void Frame::rename_hole(const std::string& name) {
    auto value = text(name);
    value.Trim(true).Trim(false);
    if (value.empty() || hole_index_ < 0) {
        show_result({false, "Enter a hole name."});
        return;
    }
    capture_draft();
    draft_.holes.at(static_cast<std::size_t>(hole_index_)).display_name = utf8(value);
    dirty_ = true;
    load_holes();
    holes_->SetSelection(hole_index_);
    hole_heading_->SetLabel(value);
    hole_heading_->SetToolTip(value);
    refresh();
}
void Frame::add_hole() {
    if (viewed_.empty())
        return;
    capture_draft();
    draft_.holes.push_back(controller_->make_hole());
    dirty_ = true;
    load_holes();
    load_hole(static_cast<int>(draft_.holes.size()) - 1);
    present(presentation::Message::Created);
    refresh();
}
void Frame::duplicate_hole() {
    if (hole_index_ < 0)
        return;
    capture_draft();
    auto copy = draft_.holes.at(static_cast<std::size_t>(hole_index_));
    copy.id = controller_->make_hole().id;
    copy.display_name += " copy";
    draft_.holes.push_back(std::move(copy));
    dirty_ = true;
    load_holes();
    load_hole(static_cast<int>(draft_.holes.size()) - 1);
    present(presentation::Message::Created);
    refresh();
}
void Frame::remove_hole() {
    if (hole_index_ < 0)
        return;
    if (wxMessageBox("Remove this hole from the editor? Save keeps the change; Apply changes "
                     "active routing.",
                     "Confirm hole removal", wxYES_NO | wxNO_DEFAULT, this) != wxYES)
        return;
    capture_draft();
    draft_.holes.erase(draft_.holes.begin() + hole_index_);
    dirty_ = true;
    load_holes();
    load_hole(draft_.holes.empty()
                  ? -1
                  : std::min(hole_index_, static_cast<int>(draft_.holes.size()) - 1));
    refresh();
}
// Reuse the context-menu command so keyboard and glyph actions share targeting,
// default-No confirmation, staged hole removal and active-profile protection.
void Frame::delete_selection(bool profile) {
    const int selected = profile ? profiles_->GetSelection() : holes_->GetSelection();
    if (selected < 0 || (profile && static_cast<std::size_t>(selected) >= profile_ids_.size()) ||
        (!profile && static_cast<std::size_t>(selected) >= draft_.holes.size()))
        return;
    auto menu = profile ? profile_menu(profile_ids_[static_cast<std::size_t>(selected)])
                        : hole_menu(draft_.holes[static_cast<std::size_t>(selected)].id);
    if (menu->IsEnabled(wxID_DELETE)) {
        wxCommandEvent command(wxEVT_MENU, wxID_DELETE);
        menu->ProcessEvent(command);
    }
}
// Test records share configuration.json; device scan JSON lives in devices/.
// Opening a folder never saves a draft, scans hardware or changes configuration.
void Frame::open_data_folder(bool profiles) {
    if (controller_->snapshot().operating_mode != OperatingMode::Test) {
        show_result({false, "Preview profiles and devices are session-only. Saved data folders are "
                            "available in Test Mode."});
        return;
    }
    auto directory = preferences_->test_configuration().parent_path();
    if (!profiles)
        directory /= "devices";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        show_result({false, "Could not open the data folder: " + error.message()});
        return;
    }
    if (!wxLaunchDefaultApplication(wxString(directory.wstring())))
        show_result({false, "Windows could not open the data folder. Path: " +
                                utf8(wxString(directory.wstring()))});
}
void Frame::build_profiles() {
    auto* page = new wxPanel(notebook_);
    auto* page_layout = new wxBoxSizer(wxVERTICAL);
    // This action bar is outside both editor panes. Scrolling a field or the
    // routing editor can never hide the profile's Save/Apply/Revert actions.
    profile_actions_ = new wxPanel(page);
    profile_actions_->SetName("Profile actions");
    auto* saves = new wxBoxSizer(wxHORIZONTAL);
    profile_save_ = button(profile_actions_, saves, "Save", [this] { save(false); });
    profile_save_->SetFont(profile_save_->GetFont().Bold());
    profile_save_->SetToolTip(
        "Save the current profile's staged changes without activating routing. Ctrl+S.");
    profile_apply_ = button(profile_actions_, saves, "Apply", [this] { save(true); });
    if (controller_->snapshot().operating_mode == OperatingMode::Test)
        profile_apply_->SetToolTip("Live routing is unavailable in Test Mode. Use Save to persist "
                                   "profile preferences without claiming engine application.");
    profile_revert_ = button(profile_actions_, saves, "Revert", [this] {
        if (discard())
            select_profile(viewed_);
    });
    profile_edit_state_ =
        new wxStaticText(profile_actions_, wxID_ANY, "No profile selected", wxDefaultPosition,
                         wxDefaultSize, wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    profile_edit_state_->SetMinSize(wxSize(0, -1));
    saves->Add(profile_edit_state_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
    profile_actions_->SetSizer(saves);
    page_layout->Add(profile_actions_, 0, wxEXPAND | wxALL, FromDIP(3));
    auto* bundles = new wxBoxSizer(wxHORIZONTAL);
    scanner_actions_.push_back(
        button(page, bundles, "Export profiles…", [this] { bundle_dialog(true, false); }));
    scanner_actions_.push_back(
        button(page, bundles, "Import profiles…", [this] { bundle_dialog(true, true); }));
    profiles_folder_ =
        button(page, bundles, "Open Profiles Folder", [this] { open_data_folder(true); });
    profiles_folder_->SetName("Open Profiles Folder");
    profiles_folder_->SetToolTip("Open the Test data folder containing saved profiles in "
                                 "configuration.json. Preview profiles are session-only.");
    page_layout->Add(bundles, 0, wxEXPAND);
    profile_columns_ = new wxSplitterWindow(page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                            wxSP_LIVE_UPDATE | wxSP_3D);
    profile_columns_->SetName("Resize profile column width");
    profile_columns_->SetMinimumPaneSize(FromDIP(180));
    profile_columns_->SetSashGravity(0);
    auto* sidebar = new wxPanel(profile_columns_);
    profile_sidebar_ = sidebar;
    sidebar->SetName("Profile fields");
    auto* sidebar_layout = new wxBoxSizer(wxVERTICAL);
    profile_rows_ = new wxSplitterWindow(sidebar, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                         wxSP_LIVE_UPDATE | wxSP_3D);
    profile_rows_->SetName("Resize profiles height");
    profile_rows_->SetMinimumPaneSize(FromDIP(72));
    profile_rows_->SetSashGravity(0.34);
    profile_lower_rows_ = new wxSplitterWindow(profile_rows_, wxID_ANY, wxDefaultPosition,
                                               wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    profile_lower_rows_->SetName("Resize notes and holes heights");
    profile_lower_rows_->SetMinimumPaneSize(FromDIP(72));
    profile_lower_rows_->SetSashGravity(0.5);
    profile_lower_rows_->SetMinSize(
        wxSize(0, FromDIP(144) + profile_lower_rows_->GetSashSize() + 4));
    auto* profile_panel = new wxPanel(profile_rows_);
    auto* notes_panel = new wxPanel(profile_lower_rows_);
    auto* holes_panel = new wxPanel(profile_lower_rows_);
    auto* profile_layout = new wxBoxSizer(wxVERTICAL);
    auto* notes_layout = new wxBoxSizer(wxVERTICAL);
    auto* holes_layout = new wxBoxSizer(wxVERTICAL);
    auto* profile_heading = new wxBoxSizer(wxHORIZONTAL);
    label(profile_panel, profile_heading, "Profiles");
    profiles_ = new ThemedList(profile_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0,
                               nullptr, wxLB_SINGLE | wxLB_NEEDED_SB | wxLB_OWNERDRAW);
    profiles_->SetName("Profiles");
    profiles_->SetMinSize(wxSize(0, profiles_->GetCharHeight() + FromDIP(8)));
    profiles_->SetToolTip("Open a profile for editing. Right-click or Shift+F10 for Rename, "
                          "Duplicate and Delete. Drag pane dividers to resize.");
    profile_heading->AddStretchSpacer();
    profile_new_ =
        add_control(profile_panel, profile_heading, "New profile", "Create a new profile.", [this] {
            if (!discard())
                return;
            const auto id = controller_->create_profile("New profile");
            if (id.empty())
                show_result({false, controller_->last_edit_error()});
            else
                select_profile(id);
        });
    profile_remove_ = add_control(
        profile_panel, profile_heading, "Delete profile",
        "Delete the selected saved profile after confirmation. Active or pending profiles must be "
        "stopped first.",
        [this] { delete_selection(true); }, wxS("−"));
    profile_layout->Add(profile_heading, 0, wxEXPAND);
    profile_layout->Add(profiles_, 1, wxEXPAND | wxALL, 3);
    label(notes_panel, notes_layout, "Notes");
    profile_notes_ = new wxTextCtrl(notes_panel, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxTE_WORDWRAP);
    profile_notes_->SetName("Profile notes");
    profile_notes_->SetMinSize(wxSize(0, profile_notes_->GetCharHeight() + FromDIP(8)));
    profile_notes_->SetToolTip("Notes for the selected profile. Drag the dividers above/below to "
                               "resize. Save keeps staged edits.");
    notes_layout->Add(profile_notes_, 1, wxEXPAND | wxALL, 3);
    auto* hole_heading = new wxBoxSizer(wxHORIZONTAL);
    label(holes_panel, hole_heading, "Holes");
    hole_heading->AddStretchSpacer();
    hole_new_ = add_control(holes_panel, hole_heading, "Add hole",
                            "Add a hole to this profile's staged changes.", [this] { add_hole(); });
    hole_remove_ = add_control(
        holes_panel, hole_heading, "Delete hole",
        "Remove the selected hole after confirmation. Save keeps the staged removal.",
        [this] { delete_selection(false); }, wxS("−"));
    holes_layout->Add(hole_heading, 0, wxEXPAND);
    holes_ = new ThemedList(holes_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr,
                            wxLB_SINGLE | wxLB_NEEDED_SB | wxLB_OWNERDRAW);
    holes_->SetName("Holes");
    holes_->SetMinSize(wxSize(0, holes_->GetCharHeight() + FromDIP(8)));
    holes_->SetToolTip("Choose a hole. Right-click or Shift+F10 for Rename, Duplicate and Delete.");
    holes_layout->Add(holes_, 1, wxEXPAND | wxALL, 3);
    profile_panel->SetSizer(profile_layout);
    notes_panel->SetSizer(notes_layout);
    holes_panel->SetSizer(holes_layout);
    profile_lower_rows_->SplitHorizontally(notes_panel, holes_panel);
    profile_rows_->SplitHorizontally(profile_panel, profile_lower_rows_);
    sidebar_layout->Add(profile_rows_, 1, wxEXPAND);
    sidebar->SetSizer(sidebar_layout);
    auto* editor = new wxScrolledWindow(profile_columns_);
    editor->SetScrollRate(FromDIP(12), FromDIP(12));
    auto* right = new wxBoxSizer(wxVERTICAL);
    editor->SetSizer(right);
    profile_columns_->SplitVertically(sidebar, editor, FromDIP(300));
    hole_heading_ = label(editor, right, "Select a hole");
    hole_heading_->SetWindowStyleFlag(wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    hole_heading_->SetMinSize(wxSize(0, -1));
    hole_heading_->SetFont(hole_heading_->GetFont().Bold());
    auto* toggles = new wxBoxSizer(wxHORIZONTAL);
    enabled_ = new wxCheckBox(editor, wxID_ANY, "Enabled");
    duplex_ = new wxCheckBox(editor, wxID_ANY, "Duplex (independent directions)");
    toggles->Add(enabled_, 0, wxALL, 4);
    toggles->Add(duplex_, 0, wxALL, 4);
    right->Add(toggles);
    hole_notes_ = field(editor, right, "Hole notes");
    advanced_panel_ = new wxPanel(editor);
    advanced_panel_->SetSizer(new wxBoxSizer(wxVERTICAL));
    routing_engine_ =
        choices(advanced_panel_, advanced_panel_->GetSizer(), "Routing Engine", {"Automatic"});
    routing_engine_->SetToolTip("Requested engine selection for this hole. Only Automatic is "
                                "available; no engine is selected or started by this control.");
    routing_engine_notice_ = label(advanced_panel_, advanced_panel_->GetSizer(), "");
    routing_engine_notice_->SetMinSize(wxSize(0, -1));
    routing_engine_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_) {
            capture_draft();
            routing_engine_notice_->SetLabel("");
            dirty_ = true;
            refresh();
        }
    });
    build_direction(editor, right, forward_, "Forward");
    build_direction(editor, right, reverse_, "Return");
    advanced_tuning_ = new wxCheckBox(editor, wxID_ANY, "Enable Advanced Tuning");
    advanced_tuning_->SetName("Enable Advanced Tuning");
    advanced_tuning_->SetToolTip("Allow retained manual overrides for this hole. Save / Apply is "
                                 "explicit; toggling never starts routing.");
    right->Add(advanced_tuning_, 0, wxALL, FromDIP(4));
    right->Add(advanced_panel_, 0, wxEXPAND);
    advanced_panel_->MoveAfterInTabOrder(advanced_tuning_);
    advanced_panel_->Hide();
    advanced_tuning_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { tuning_changed(); });
    path_status_ = label(editor, right, "No active paths.");
    build_hole_diagnostics(editor, right);
    page_layout->Add(profile_columns_, 1, wxEXPAND | wxALL, 3);
    page->SetSizer(page_layout);
    notebook_->AddPage(page, "Profiles");
    profiles_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        const int i = profiles_->GetSelection();
        if (i >= 0 && discard())
            select_profile(profile_ids_.at(static_cast<std::size_t>(i)));
        else {
            const auto it = std::find(profile_ids_.begin(), profile_ids_.end(), viewed_);
            profiles_->SetSelection(it == profile_ids_.end()
                                        ? wxNOT_FOUND
                                        : static_cast<int>(it - profile_ids_.begin()));
        }
    });
    holes_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        const int i = holes_->GetSelection();
        capture_draft();
        load_hole(i);
    });
    profiles_->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event) {
        const int index = context_item(profiles_, event);
        if (index < 0 || static_cast<std::size_t>(index) >= profile_ids_.size())
            return;
        auto menu = profile_menu(profile_ids_[static_cast<std::size_t>(index)]);
        profiles_->SetSelection(index);
        profiles_->PopupMenu(menu.get());
        const auto selected = std::find(profile_ids_.begin(), profile_ids_.end(), viewed_);
        profiles_->SetSelection(selected == profile_ids_.end()
                                    ? wxNOT_FOUND
                                    : static_cast<int>(selected - profile_ids_.begin()));
    });
    holes_->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event) {
        const int index = context_item(holes_, event);
        if (index < 0 || static_cast<std::size_t>(index) >= draft_.holes.size())
            return;
        auto menu = hole_menu(draft_.holes[static_cast<std::size_t>(index)].id);
        holes_->SetSelection(index);
        holes_->PopupMenu(menu.get());
        holes_->SetSelection(hole_index_);
    });
    auto delete_selected = [this](bool profile, wxKeyEvent& event) {
        if (event.GetKeyCode() != WXK_DELETE || event.HasAnyModifiers()) {
            event.Skip();
            return;
        }
        delete_selection(profile);
    };
    profiles_->Bind(wxEVT_KEY_DOWN, [delete_selected](wxKeyEvent& e) { delete_selected(true, e); });
    holes_->Bind(wxEVT_KEY_DOWN, [delete_selected](wxKeyEvent& e) { delete_selected(false, e); });
    for (auto* f : {profile_notes_, hole_notes_})
        f->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            if (!loading_)
                dirty_ = true;
        });
    enabled_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        dirty_ = true;
        present(enabled_->GetValue() ? presentation::Message::Enabled
                                     : presentation::Message::Disabled);
    });
    duplex_->Bind(wxEVT_CHECKBOX, [this, editor](wxCommandEvent&) {
        dirty_ = true;
        activity_direction_->Enable(duplex_->GetValue());
        if (!duplex_->GetValue())
            activity_direction_->SetSelection(0);
        end_observation();
        reverse_.group->ShowItems(duplex_->GetValue());
        reverse_.tuning_group->ShowItems(duplex_->GetValue());
        editor->FitInside();
        editor->Layout();
        present(duplex_->GetValue() ? presentation::Message::Duplex
                                    : presentation::Message::Simplex);
    });
    monitor_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        controller_->monitor(monitor_->GetValue());
        const auto& paths = controller_->snapshot().paths;
        const auto observed_hole = hole_index_ >= 0
                                       ? draft_.holes.at(static_cast<std::size_t>(hole_index_)).id
                                       : std::string{};
        const bool reverse = activity_direction_->GetSelection() == 1;
        const bool running = std::any_of(paths.begin(), paths.end(), [&](const auto& path) {
            return path.hole_id == observed_hole && path.reverse == reverse &&
                   path.state == audiohole::State::Running;
        });
        present(!monitor_->GetValue() ? presentation::Message::Cancelled
                : running             ? presentation::Message::Signal
                                      : presentation::Message::NoSignal);
        refresh();
    });
}
// Test uses exact worker-projected scanner tuples; Preview uses authored fixtures.
// Rebuilding must not silently erase a manual choice or invent runtime evidence.
void Frame::populate_direction(DirectionWidgets& w, const audiohole::Direction& d) {
    const bool old_loading = loading_;
    loading_ = true;
    w.source->Clear();
    w.destination->Clear();
    w.sources.clear();
    w.destinations.clear();
    w.retained_source = d.source;
    w.retained_destination = d.destination;
    wxArrayString source_search, dest_search;
    const auto offered_devices = configuration_devices(controller_->snapshot());
    for (const auto* offered : offered_devices) {
        const auto& e = *offered;
        const bool duplicate_name =
            std::count_if(offered_devices.begin(), offered_devices.end(), [&](const auto* other) {
                return other->capture == e.capture && other->display_name == e.display_name;
            }) > 1;
        const auto caption = text(e.display_name + (duplicate_name ? " [" + e.id + "]" : "") +
                                  " — " + audiohole::name(e.availability));
        auto* list = e.capture ? w.source : w.destination;
        auto& ids = e.capture ? w.sources : w.destinations;
        list->Append(caption);
        ids.push_back(e.id);
        (e.capture ? source_search : dest_search).Add(caption);
        if (e.id == (e.capture ? d.source : d.destination)) {
            list->SetSelection(static_cast<int>(ids.size()) - 1);
            list->SetToolTip(text(e.windows_name + " | logical ID: " + e.id));
        }
    }
    // A configured hidden/missing binding remains visible as field text, never
    // as an offered list item. Saving unrelated edits retains its exact ID.
    auto retained_caption = [&](const std::string& id, const std::string& saved_name) {
        const auto* device = endpoint(controller_->snapshot(), id);
        const auto display_name = device ? device->display_name : saved_name;
        return text("Saved binding (hidden or missing): " + display_name + " [" + id + "]");
    };
    if (w.source->GetSelection() == wxNOT_FOUND && !d.source.empty())
        w.source->SetValue(retained_caption(d.source, d.source_name));
    if (w.destination->GetSelection() == wxNOT_FOUND && !d.destination.empty())
        w.destination->SetValue(retained_caption(d.destination, d.destination_name));
    w.retained_source_caption = w.source->GetValue();
    w.retained_destination_caption = w.destination->GetValue();
    w.source->AutoComplete(source_search);
    w.destination->AutoComplete(dest_search);
    const auto r = audiohole::recommend(controller_->snapshot(), d);
    w.formats = r.compatible;
    w.format->Clear();
    // Keep the literal branch a wxString too; mixed types are ambiguous on MSVC.
    w.format->Append(r.compatible.empty()
                         ? wxString(wxS("Automatic — unverified compatibility"))
                         : wxS("Automatic — ") + text(r.compatible.front().label()) + "");
    int selection = 0;
    for (const auto& f : r.compatible) {
        w.format->Append(text(f.label()) + "");
        if (!d.settings.automatic && f == d.settings.format)
            selection = static_cast<int>(w.format->GetCount()) - 1;
    }
    if (!d.settings.automatic && !selection) {
        w.formats.push_back(d.settings.format);
        w.format->Append(text(d.settings.format.label()) +
                         " [not supported by current evidence; retained]");
        selection = static_cast<int>(w.format->GetCount()) - 1;
    }
    w.format->SetSelection(selection);
    update_direction_details(w, d);
    w.buffer->SetValue(d.settings.buffer_ms);
    w.acknowledged_buffer_ms = d.settings.buffer_ms;
    w.mode->Clear();
    for (const auto* mode : {"Automatic", "Shared", "Exclusive"})
        w.mode->Append(mode);
    if (w.mode->FindString(text(d.settings.mode)) == wxNOT_FOUND)
        w.mode->Append(text(d.settings.mode)); // Preserve externally authored unknown requests.
    w.mode->SetStringSelection(text(d.settings.mode));
    loading_ = old_loading;
    if (!loading_)
        layout_direction(w);
}
// Read-only display of requested settings and available endpoint records; never
// label these as negotiated runtime measurements or verified hardware evidence.
void Frame::update_direction_details(DirectionWidgets& w, const audiohole::Direction& d) {
    const auto& snapshot = controller_->snapshot();
    std::string saved = "unsaved";
    const bool reverse = &w == &reverse_;
    bool saved_direction = false;
    if (const auto* stored = profile(snapshot, viewed_)) {
        const auto selected =
            hole_index_ >= 0 && static_cast<std::size_t>(hole_index_) < draft_.holes.size()
                ? draft_.holes[static_cast<std::size_t>(hole_index_)].id
                : std::string{};
        const auto hole = std::find_if(stored->holes.begin(), stored->holes.end(),
                                       [&](const auto& record) { return record.id == selected; });
        if (hole != stored->holes.end() && (!reverse || hole->duplex)) {
            saved = buffer_caption((reverse ? hole->reverse : hole->forward).settings.buffer_ms);
            saved_direction = true;
        }
    }
    const auto selected_hole =
        hole_index_ >= 0 && static_cast<std::size_t>(hole_index_) < draft_.holes.size()
            ? draft_.holes[static_cast<std::size_t>(hole_index_)].id
            : std::string{};
    const auto path =
        std::find_if(snapshot.paths.begin(), snapshot.paths.end(), [&](const auto& item) {
            return snapshot.active_profile == viewed_ && item.hole_id == selected_hole &&
                   item.reverse == reverse;
        });
    const auto requested =
        path == snapshot.paths.end() ? std::optional<int>{} : path->requested_buffer_ms;
    const auto confirmed =
        path == snapshot.paths.end() ? std::optional<int>{} : path->applied_buffer_ms;
    const bool saved_changed = saved != buffer_caption(d.settings.buffer_ms);
    const bool active_changed =
        snapshot.capabilities.live_buffer && requested && *requested != d.settings.buffer_ms;
    const auto parsed = buffer_value(w.buffer);
    w.buffer_apply->Enable(advanced_tuning_->GetValue() && saved_direction && parsed &&
                           (saved_changed || active_changed));
    const wxString buffer_help =
        !parsed ? "Enter 0 for Automatic or a whole buffer value from 1 to 100 milliseconds."
        : !saved_direction
            ? "Save this profile and direction before applying an individual buffer preference."
        : !saved_changed && !active_changed
            ? "This buffer already matches the saved preference and any active Preview request."
            : "Save only this direction's buffer preference. A matching active Preview direction "
              "updates its simulated buffer; Test Mode saves configuration only.";
    if (!w.buffer_apply->GetToolTip() || w.buffer_apply->GetToolTip()->GetTip() != buffer_help)
        w.buffer_apply->SetToolTip(buffer_help);
    auto value = [](std::optional<int> buffer) {
        return buffer ? std::to_string(*buffer) + " ms" : "unavailable";
    };
    w.buffer_state->SetLabel(
        text("Edited: " + (parsed ? buffer_caption(*parsed) : "invalid (0 = Automatic; 1–100 ms)") +
             " | Saved: " + saved + "\n" +
             (snapshot.capabilities.simulated
                  ? "Preview requested: " +
                        (path != snapshot.paths.end() && !requested ? std::string("Automatic")
                                                                    : value(requested)) +
                        " | confirmed: " + value(confirmed) + " (simulated)"
                  : "Engine requested/applied: unavailable")));
    auto effective = d;
    const bool advanced = advanced_tuning_->GetValue();
    if (!advanced)
        effective.settings = Settings{};
    const auto recommendation = audiohole::recommend(snapshot, effective);
    const bool compatible = !recommendation.compatible.empty();
    const auto selected = !effective.settings.automatic ? effective.settings.format
                          : compatible                  ? recommendation.compatible.front()
                                                        : Format{};
    auto detail = [&](const std::string& id) {
        const auto* device = endpoint(snapshot, id);
        return device ? std::string(name(device->availability)) + " | " +
                            (compatible ? "Candidate: " + selected.label() : "Format unverified")
                      : std::string("No endpoint selected");
    };
    const auto update_detail = [](wxStaticText* control, wxString& previous,
                                  const wxString& value) {
        if (previous == value)
            return false;
        previous = value;
        control->SetLabel(value);
        control->Wrap(260);
        return true;
    };
    const bool source_changed =
        update_detail(w.source_detail, w.source_detail_copy, text(detail(d.source)));
    const bool destination_changed =
        update_detail(w.destination_detail, w.destination_detail_copy, text(detail(d.destination)));
    std::string summary;
    if (d.source.empty() || d.destination.empty())
        summary = "Automatic configuration — awaiting endpoint selection.";
    else if (!compatible)
        summary = "Automatic configuration unavailable for the selected endpoints. Format "
                  "compatibility is unverified.";
    else
        summary = "Compatible format identified from available endpoint evidence. Runtime "
                  "negotiation remains unverified.";
    if (advanced) {
        summary =
            "Advanced Tuning requested — Format: " +
            (d.settings.automatic ? std::string("Automatic") : d.settings.format.label()) +
            " | Mode: " + d.settings.mode + " | Buffer: " +
            (d.settings.buffer_ms ? std::to_string(d.settings.buffer_ms) + " ms" : "Automatic") +
            ". Save / Apply is explicit; engine values are shown separately.";
    } else if (!d.settings.automatic || d.settings.buffer_ms != 0 ||
               d.settings.mode != "Automatic") {
        summary +=
            " Automatic policy active in this draft — retained manual settings are inactive.";
    }
    if (advanced && !d.settings.automatic &&
        std::find(recommendation.compatible.begin(), recommendation.compatible.end(),
                  d.settings.format) == recommendation.compatible.end())
        summary += " Requested format is unsupported by current evidence; change the override "
                   "before Apply.";
    if (!advanced && hole_index_ >= 0 &&
        draft_.holes.at(static_cast<std::size_t>(hole_index_)).routing_engine != "automatic")
        summary += " Saved Routing Engine choice is retained but inactive; effective engine policy "
                   "is Automatic.";
    const auto information = text(summary + "\n\n" + recommendation.explanation);
    const bool information_changed = w.information->GetValue() != information;
    if (information_changed) {
        w.information->ChangeValue(information);
        w.information->ShowPosition(0);
    }
    if (!loading_ && (source_changed || destination_changed || information_changed))
        layout_direction(w);
}
// Relayout only after actual content/selection changes, never on unchanged
// polling. FitInside updates the editor's virtual extent without resizing Frame.
void Frame::layout_direction(DirectionWidgets& widgets) {
    widgets.group->Layout();
    auto* editor = static_cast<wxScrolledWindow*>(widgets.group->GetStaticBox()->GetParent());
    editor->FitInside();
    editor->Layout(); // Use the updated virtual extent in this same UI action.
}
// Only an explicit list selection becomes a logical binding. Free text used for
// searching is never guessed as a device identity; validation reports missing IDs.
void Frame::read_direction(DirectionWidgets& w, audiohole::Direction& d) {
    auto read_id = [](wxComboBox* box, const std::vector<std::string>& ids,
                      const std::string& retained, const wxString& original) {
        const int i = box->GetSelection();
        return i >= 0 && static_cast<std::size_t>(i) < ids.size() ? ids[static_cast<std::size_t>(i)]
               : box->GetValue() == original                      ? retained
                                                                  : std::string{};
    };
    d.source = read_id(w.source, w.sources, w.retained_source, w.retained_source_caption);
    d.destination = read_id(w.destination, w.destinations, w.retained_destination,
                            w.retained_destination_caption);
    if (!advanced_tuning_->GetValue())
        return;
    const int f = w.format->GetSelection();
    d.settings.automatic = f <= 0;
    if (f > 0 && static_cast<std::size_t>(f) <= w.formats.size())
        d.settings.format = w.formats[static_cast<std::size_t>(f - 1)];
    if (const auto parsed = buffer_value(w.buffer))
        d.settings.buffer_ms = *parsed;
    d.settings.mode = utf8(w.mode->GetStringSelection());
}
void Frame::capture_draft() {
    if (loading_)
        return;
    draft_.notes = utf8(profile_notes_->GetValue());
    if (hole_index_ < 0 || static_cast<std::size_t>(hole_index_) >= draft_.holes.size())
        return;
    auto& h = draft_.holes[static_cast<std::size_t>(hole_index_)];
    h.notes = utf8(hole_notes_->GetValue());
    h.enabled = enabled_->GetValue();
    h.duplex = duplex_->GetValue();
    h.advanced_tuning = advanced_tuning_->GetValue();
    if (h.advanced_tuning && routing_engine_->GetSelection() == 0)
        h.routing_engine = "automatic";
    read_direction(forward_, h.forward);
    read_direction(reverse_, h.reverse);
}
// Viewed profile and active profile are deliberately independent. Navigation
// loads a saved editor copy; activation remains a separate controller command.
void Frame::select_profile(const std::string& id) {
    viewed_ = id;
    const auto* p = audiohole::profile(controller_->snapshot(), id);
    draft_ = p ? *p : audiohole::Profile{};
    loading_ = true;
    profile_notes_->ChangeValue(text(draft_.notes));
    profiles_->Clear();
    profile_ids_.clear();
    startup_target_->Clear();
    for (const auto& saved : controller_->snapshot().profiles) {
        profiles_->Append(text(saved.display_name));
        profile_ids_.push_back(saved.id);
        startup_target_->Append(text(saved.display_name));
        if (saved.id == id)
            profiles_->SetSelection(static_cast<int>(profile_ids_.size()) - 1);
    }
    if (startup_target_->GetCount()) {
        const auto& desired = preferences_->values().startup_profile_id;
        const auto found = std::find(profile_ids_.begin(), profile_ids_.end(), desired);
        startup_target_->SetSelection(
            found == profile_ids_.end() ? 0 : static_cast<int>(found - profile_ids_.begin()));
    }
    loading_ = false;
    dirty_ = false;
    load_holes();
    load_hole(draft_.holes.empty() ? -1 : 0);
    refresh();
}
void Frame::load_holes() {
    holes_->Clear();
    for (const auto& h : draft_.holes) {
        std::string caption = h.enabled ? "[enabled] " : "[disabled] ";
        caption += h.display_name + (h.duplex ? " — duplex" : " — simplex");
        holes_->Append(text(caption));
    }
}
void Frame::load_hole(int index) {
    tuning_timer_.Stop();
    loading_ = true;
    end_observation(); // An observation never follows navigation to a different source implicitly.
    hole_index_ = index;
    const audiohole::Hole h =
        index >= 0 ? draft_.holes.at(static_cast<std::size_t>(index)) : audiohole::Hole{};
    holes_->SetSelection(index);
    hole_heading_->SetLabel(text(index >= 0 ? h.display_name : std::string("Select a hole")));
    hole_heading_->SetToolTip(text(h.display_name));
    hole_notes_->ChangeValue(text(h.notes));
    enabled_->SetValue(h.enabled);
    duplex_->SetValue(h.duplex);
    advanced_tuning_->SetValue(index >= 0 && h.advanced_tuning);
    advanced_tuning_->Enable(index >= 0);
    hole_remove_->Enable(index >= 0);
    advanced_panel_->Show(index >= 0 && h.advanced_tuning);
    routing_engine_->SetSelection(h.routing_engine == "automatic" ? 0 : wxNOT_FOUND);
    routing_engine_->Enable(index >= 0);
    routing_engine_notice_->SetLabel(
        h.routing_engine == "automatic"
            ? wxString{}
            : text("Saved Routing Engine unavailable in this build: " + h.routing_engine +
                   ". Retained until you explicitly select Automatic."));
    routing_engine_notice_->Wrap(560);
    activity_direction_->SetSelection(0);
    activity_direction_->Enable(index >= 0 && h.duplex);
    hole_notes_->SetEditable(index >= 0);
    for (wxWindow* widget : std::vector<wxWindow*>{enabled_, duplex_})
        widget->Enable(index >= 0);
    populate_direction(forward_, h.forward);
    populate_direction(reverse_, h.reverse);
    // Keep containers and captions enabled: disabling the whole static box makes
    // Windows emboss every label and paints inactive edit fields white.
    for (auto* direction : {&forward_, &reverse_}) {
        for (wxWindow* input :
             std::vector<wxWindow*>{direction->source, direction->destination, direction->format,
                                    direction->mode, direction->buffer})
            input->Enable(index >= 0);
        if (index < 0)
            direction->buffer_apply->Disable();
    }
    reverse_.group->ShowItems(h.duplex && index >= 0);
    reverse_.tuning_group->ShowItems(h.duplex && index >= 0);
    update_beep_destinations();
    loading_ = false;
    auto* editor = dynamic_cast<wxScrolledWindow*>(hole_heading_->GetParent());
    editor->FitInside();
    editor->Layout();
}
void Frame::save(bool apply) {
    tuning_timer_.Stop();
    if (hole_index_ >= 0 && advanced_tuning_->GetValue() &&
        (!buffer_value(forward_.buffer) ||
         (duplex_->GetValue() && !buffer_value(reverse_.buffer)))) {
        show_result({false, "Enter 0 for Automatic or whole buffer values from 1 to 100 "
                            "milliseconds before saving."});
        return;
    }
    capture_draft();
    const auto r = controller_->save(draft_, apply);
    show_result(r);
    if (r.ok) {
        if (const auto* saved = profile(controller_->snapshot(), viewed_))
            draft_ = *saved; // Include controller-enriched evidence/name hints in the clean draft.
        dirty_ = false;
        const auto item = std::find(profile_ids_.begin(), profile_ids_.end(), viewed_);
        if (item != profile_ids_.end()) {
            const auto index = static_cast<unsigned>(item - profile_ids_.begin());
            profiles_->SetString(index, text(draft_.display_name));
            startup_target_->SetString(index, text(draft_.display_name));
        }
        load_holes();
        holes_->SetSelection(hole_index_);
    }
    refresh();
}
void Frame::activate(bool ready) {
    const auto& snapshot = controller_->snapshot();
    if (dirty_ && snapshot.pending_profile.empty()) {
        show_result({false, "Save or Apply editor changes before activating. Viewing a profile "
                            "does not change routing."});
        return;
    }
    const auto id = snapshot.pending_profile.empty() ? viewed_ : snapshot.pending_profile;
    show_result(controller_->activate(id, ready));
    refresh();
}

} // namespace audiohole::gui

namespace audiohole::gui {
// Temporary tools are independent from passive monitoring: the beep always
// needs an explicit destination and remains a no-sound simulation in this build.
void Frame::build_hole_diagnostics(wxWindow* parent, wxSizer* layout) {
    signal_tools_ =
        new wxCollapsiblePane(parent, wxID_ANY, "Signal Testing & Monitoring", wxDefaultPosition,
                              wxDefaultSize, wxCP_DEFAULT_STYLE | wxCP_NO_TLW_RESIZE);
    signal_tools_->SetName("Signal Testing & Monitoring");
    layout->Add(signal_tools_, 0, wxEXPAND | wxALL, FromDIP(4));
    auto* pane = signal_tools_->GetPane();
    auto* groups = new wxBoxSizer(wxVERTICAL);
    auto* observation = new wxStaticBoxSizer(wxVERTICAL, pane, "Source Activity");
    auto* box = observation->GetStaticBox();
    groups->Add(observation, 0, wxEXPAND | wxALL, FromDIP(4));
    activity_direction_ = choices(box, observation, "Monitor direction",
                                  {"Forward source", "Return source (duplex only)"});
    activity_direction_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        end_observation();
        refresh();
    });
    monitor_ = new wxCheckBox(box, wxID_ANY, "Monitor source activity");
    monitor_->SetToolTip(
        "Observe source activity temporarily. Leaving this page or hiding the window turns "
        "monitoring off; source activity does not prove destination audibility.");
    observation->Add(monitor_, 0, wxALL, 4);
    activity_ = label(box, observation, "Observation off.");
    button(box, observation, "Clear observation status", [this] {
        controller_->monitor(false);
        refresh();
    })->SetToolTip("Stop temporary source observation and clear its displayed status.");
    auto* beep_group = new wxStaticBoxSizer(wxVERTICAL, pane, "Test Beep");
    groups->Add(beep_group, 0, wxEXPAND | wxALL, FromDIP(4));
    box = beep_group->GetStaticBox();
    label(box, beep_group, "Test Beep destination");
    destination_test_ = new ThemedChoice(box, wxID_ANY);
    beep_group->Add(destination_test_, 0, wxEXPAND | wxALL, 3);
    destination_test_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        present(presentation::Message::BeepReady);
        refresh();
    });
    auto* beep = new wxBoxSizer(wxHORIZONTAL);
    beep_button_ = button(box, beep, "Test Beep", [this] {
        // Read authoritative state on each click, including rapid repeat clicks.
        if (!controller_->snapshot().beep_destination.empty()) {
            controller_->cancel_beep();
            refresh();
            return;
        }
        const int index = destination_test_->GetSelection();
        show_result(
            controller_->beep(index >= 0 && static_cast<std::size_t>(index) < beep_ids_.size()
                                  ? beep_ids_[static_cast<std::size_t>(index)]
                                  : ""));
        refresh();
    });
    // This starts inside a collapsed pane: a minimum alone does not resize the
    // native window until its first layout. Reserve both captions and initialize
    // the actual size too, so disclosure/label changes cannot change its width.
    auto beep_size = beep_button_->GetEffectiveMinSize();
    beep_button_->SetLabel("Cancel Beep");
    beep_size.IncTo(beep_button_->GetEffectiveMinSize());
    beep_button_->SetLabel("Test Beep");
    beep_button_->SetMinSize(beep_size);
    beep_button_->SetMaxSize(wxSize(beep_size.x, -1));
    beep_button_->SetSize(beep_size);
    beep_group->Add(beep);
    beep_status_ = label(box, beep_group, "Test Beep inactive.");
    auto* performance = new wxStaticBoxSizer(wxVERTICAL, pane, "Performance Diagnostics");
    groups->Add(performance, 0, wxEXPAND | wxALL, FromDIP(4));
    label(performance->GetStaticBox(), performance,
          "Latency / buffer occupancy / clock drift / CPU: unavailable.");
    pane->SetSizer(groups);
    signal_tools_->Collapse(true);
    signal_tools_->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [this, parent](wxCollapsiblePaneEvent&) {
        // Disclosure is presentation only; do not stop monitoring or a beep.
        auto* focused = wxWindow::FindFocus();
        if (signal_tools_->IsCollapsed() && focused &&
            signal_tools_->GetPane()->IsDescendant(focused))
            signal_tools_->SetFocus();
        parent->Layout();
        static_cast<wxScrolledWindow*>(parent)->FitInside();
        refresh();
    });
    if (controller_->snapshot().operating_mode != OperatingMode::Preview)
        return;
    auto* simulation = new wxStaticBoxSizer(wxVERTICAL, parent, "Simulate Errors");
    auto* simulation_box = simulation->GetStaticBox();
    layout->Add(simulation, 0, wxEXPAND | wxALL, 4);
    label(simulation_box, simulation,
          "Inject simulated routing failures to test status reporting and recovery behavior.");
    auto* failures = new wxBoxSizer(wxHORIZONTAL);
    button(simulation_box, failures, "Forward failure", [this] {
        if (hole_index_ >= 0)
            controller_->fail_path(draft_.holes.at(static_cast<std::size_t>(hole_index_)).id,
                                   false);
        refresh();
    });
    button(simulation_box, failures, "Return failure", [this] {
        if (hole_index_ >= 0 && draft_.holes.at(static_cast<std::size_t>(hole_index_)).duplex)
            controller_->fail_path(draft_.holes.at(static_cast<std::size_t>(hole_index_)).id, true);
        refresh();
    });
    button(simulation_box, failures, "Recover", [this] {
        controller_->recover();
        refresh();
    });
    simulation->Add(failures);
}
} // namespace audiohole::gui

namespace audiohole::gui {
// Recompute only after an editor selection changes; never choose a beep target
// implicitly or carry an armed test to a newly edited destination.
void Frame::update_beep_destinations() {
    const audiohole::Hole hole = hole_index_ >= 0
                                     ? draft_.holes.at(static_cast<std::size_t>(hole_index_))
                                     : audiohole::Hole{};
    controller_->cancel_beep();
    destination_test_->Clear();
    beep_ids_.clear();
    destination_test_->Append("Select a destination direction explicitly");
    beep_ids_.push_back("");
    destination_test_->SetSelection(0);
    for (const auto* d : {&hole.forward, hole.duplex ? &hole.reverse : nullptr})
        if (d) {
            if (const auto* e = audiohole::endpoint(controller_->snapshot(), d->destination)) {
                if (e->hidden)
                    continue;
                destination_test_->Append(
                    text(std::string(d == &hole.forward ? "Forward → " : "Return → ") +
                         e->display_name + " [" + e->id + "]"));
                beep_ids_.push_back(e->id);
            }
        }
}
} // namespace audiohole::gui
