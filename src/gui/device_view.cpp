#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
void Frame::build_devices() {
    auto* page = new wxPanel(notebook_);
    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* left = new wxBoxSizer(wxVERTICAL);
    device_search_ = field(page, left, "Search devices");
    auto list = [&](const char* title, wxListBox*& box, std::vector<std::string>& ids) {
        label(page, left, title);
        box = new wxListBox(page, wxID_ANY, wxDefaultPosition, wxSize(300, 120), 0, nullptr,
                            wxLB_SINGLE | wxLB_HSCROLL);
        box->SetMinSize(wxSize(300, 48)); // Both lists shrink independently on short windows.
        box->SetName(text(title));
        box->SetToolTip("Right-click a device for update scans and Hide / Unhide. Hidden devices "
                        "remain saved.");
        left->Add(box, 1, wxEXPAND | wxALL, 4);
        auto* control = box;
        auto* mapped_ids = &ids;
        box->Bind(wxEVT_LISTBOX, [this, control, mapped_ids](wxCommandEvent&) {
            const int index = control->GetSelection();
            if (index >= 0 && !mapped_ids->at(static_cast<std::size_t>(index)).empty())
                select_device(mapped_ids->at(static_cast<std::size_t>(index)));
            else
                populate_devices(); // A divider has no logical identity or action.
        });
        box->Bind(wxEVT_CONTEXT_MENU, [this, control, mapped_ids](wxContextMenuEvent& e) {
            device_context(control, *mapped_ids, e);
        });
    };
    list("Capture devices", capture_devices_, capture_device_ids_);
    list("Playback devices", playback_devices_, playback_device_ids_);
    root->Add(left, 0, wxEXPAND);
    auto* right = new wxBoxSizer(wxVERTICAL);
    root->Add(right, 1, wxEXPAND | wxALL, 6);
    device_name_ = field(page, right, "Display name");
    device_notes_ = field(page, right, "Notes", wxTE_MULTILINE);
    device_notes_->SetMinSize(wxSize(-1, 60));
    device_detail_ = label(page, right, "Select a device.");
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    device_save_ = button(page, actions, "Save device", [this] {
        if (const auto* e = endpoint(controller_->snapshot(), device_id_)) {
            auto edit = *e;
            edit.display_name = utf8(device_name_->GetValue());
            edit.notes = utf8(device_notes_->GetValue());
            const auto result = controller_->update_endpoint(edit);
            if (!result.ok) {
                show_result(result);
                return;
            }
            device_dirty_ = false;
            populate_devices();
            refresh();
            present(presentation::Message::DeviceSaved);
        }
    });
    device_update_ = button(page, actions, "Update device",
                            [this] { request_scan(false, ScanScope::Selected, device_id_); });
    device_deep_ = button(page, actions, "Deep update device",
                          [this] { request_scan(true, ScanScope::Selected, device_id_); });
    device_update_->SetToolTip("Refresh capabilities for this exact saved logical device only.");
    device_deep_->SetToolTip("Refresh this device and request consent for bounded Deep stream "
                             "tests. Uses the Scanning tab's limits.");
    right->Add(actions);
    device_view_ = new wxChoice(page, wxID_ANY);
    for (const auto* view : inspection_view_names)
        device_view_->Append(text(view));
    device_view_->SetSelection(0);
    device_view_->SetName("Device evidence view");
    right->Add(device_view_, 0, wxEXPAND | wxALL, 3);
    device_evidence_ = new wxTextCtrl(page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                      wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    device_evidence_->SetName("Device evidence");
    right->Add(device_evidence_, 1, wxEXPAND | wxALL, 3);
    auto* maintenance = new wxBoxSizer(wxHORIZONTAL);
    for (const auto& item :
         {std::pair{"Reload saved devices", ScannerActionKind::Reload},
          std::pair{"Refresh availability", ScannerActionKind::RefreshAvailability}}) {
        auto* action = button(page, maintenance, item.first, [this, kind = item.second] {
            if (device_dirty_ &&
                wxMessageBox("Discard unsaved device edits?", "Device edits",
                             wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
                return;
            device_dirty_ = false;
            ScannerActionRequest request;
            request.kind = kind;
            scanner_action(request);
        });
        action->SetToolTip(item.second == ScannerActionKind::Reload
                               ? "Read externally edited device JSON and filename display names; "
                                 "no hardware access."
                               : "Refresh known-device Windows availability and bindings without "
                                 "capability tests or creating new records.");
        scanner_actions_.push_back(action);
    }
    right->Add(maintenance);
    auto* toggle = button(page, right, "Toggle availability", [this] {
        if (controller_->snapshot().operating_mode != OperatingMode::Preview)
            return;
        if (const auto* e = endpoint(controller_->snapshot(), device_id_)) {
            if (e->availability == Availability::Ambiguous) {
                show_result({false, "Ambiguous identities require explicit binding."});
                return;
            }
            auto edit = *e;
            edit.availability = e->availability == Availability::Available
                                    ? Availability::Unavailable
                                    : Availability::Available;
            show_result(controller_->update_endpoint(edit));
            populate_devices();
            refresh();
        }
    });
    toggle->Show(controller_->snapshot().operating_mode == OperatingMode::Preview);
    page->SetSizer(root);
    notebook_->AddPage(page, "Devices");
    device_search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { populate_devices(); });
    device_view_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { load_device(); });
    for (auto* editor : {device_name_, device_notes_})
        editor->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { device_dirty_ = true; });
}
bool Frame::select_device(const std::string& id) {
    if (id != device_id_ && device_dirty_ &&
        wxMessageBox("Discard unsaved device edits?", "Device edits",
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) {
        populate_devices();
        return false;
    }
    if (id != device_id_)
        device_dirty_ = false;
    device_id_ = id;
    populate_devices();
    return true;
}
void Frame::populate_devices() {
    capture_devices_->Clear();
    playback_devices_->Clear();
    capture_device_ids_.clear();
    playback_device_ids_.clear();
    const auto search = device_search_->GetValue().Lower();
    for (bool capture : {true, false}) {
        auto* box = capture ? capture_devices_ : playback_devices_;
        auto& ids = capture ? capture_device_ids_ : playback_device_ids_;
        bool divider = false;
        for (bool hidden : {false, true})
            for (const auto& e : controller_->snapshot().endpoints) {
                const auto searchable = text(e.display_name + " " + e.windows_name + " " + e.id);
                if (e.capture != capture || e.hidden != hidden ||
                    (!search.empty() && !searchable.Lower().Contains(search)))
                    continue;
                if (hidden && !divider) {
                    box->Append("----- Hidden devices -----");
                    ids.push_back("");
                    divider = true;
                }
                box->Append(text(e.display_name + " — " + (e.inspection ? "last observed " : "") +
                                 name(e.availability)));
                ids.push_back(e.id);
                if (e.id == device_id_)
                    box->SetSelection(static_cast<int>(ids.size()) - 1);
            }
    }
    if (device_id_.empty()) {
        const auto& ids = capture_device_ids_.empty() ? playback_device_ids_ : capture_device_ids_;
        const auto first =
            std::find_if(ids.begin(), ids.end(), [](const auto& id) { return !id.empty(); });
        if (first != ids.end()) {
            device_id_ = *first;
            populate_devices();
            return;
        }
    }
    load_device();
}
void Frame::device_context(wxListBox* box, const std::vector<std::string>& ids,
                           wxContextMenuEvent& event) {
    int index = box->GetSelection();
    if (event.GetPosition() != wxDefaultPosition)
        index = box->HitTest(box->ScreenToClient(event.GetPosition()));
    if (index < 0 || static_cast<std::size_t>(index) >= ids.size() ||
        ids[static_cast<std::size_t>(index)].empty())
        return;
    const auto id = ids[static_cast<std::size_t>(index)]; // Snapshot before list rebuilding.
    if (!select_device(id))
        return;
    const auto* device = endpoint(controller_->snapshot(), id);
    if (!device)
        return;
    const bool hidden = device->hidden;
    wxMenu menu;
    const int update = wxWindow::NewControlId(), deep = wxWindow::NewControlId(),
              hide = wxWindow::NewControlId();
    menu.Append(update, "Update device");
    menu.Append(deep, "Deep update device");
    menu.AppendSeparator();
    menu.Append(hide, hidden ? "Unhide device" : "Hide device");
    const bool idle = !controller_->snapshot().scanner.busy && !controller_->snapshot().scanning;
    menu.Enable(update, idle && controller_->snapshot().capabilities.scanning);
    menu.Enable(deep, idle && controller_->snapshot().capabilities.scanning);
    menu.Enable(hide, idle);
    menu.Bind(
        wxEVT_MENU, [this, id](wxCommandEvent&) { request_scan(false, ScanScope::Selected, id); },
        update);
    menu.Bind(
        wxEVT_MENU, [this, id](wxCommandEvent&) { request_scan(true, ScanScope::Selected, id); },
        deep);
    menu.Bind(
        wxEVT_MENU,
        [this, id, hidden](wxCommandEvent&) {
            ScannerActionRequest request;
            request.kind = ScannerActionKind::SetHidden;
            request.logical_id = id;
            request.hidden = !hidden;
            scanner_action(request);
        },
        hide);
    box->PopupMenu(&menu);
}
void Frame::load_device() {
    const auto* e = endpoint(controller_->snapshot(), device_id_);
    const bool editable = e && !e->inspection;
    device_name_->SetEditable(editable);
    device_notes_->SetEditable(editable);
    device_save_->Enable(editable && !controller_->snapshot().scanner.busy);
    const bool can_scan = e && controller_->snapshot().capabilities.scanning &&
                          !controller_->snapshot().scanner.busy &&
                          !controller_->snapshot().scanning;
    device_update_->Enable(can_scan);
    device_deep_->Enable(can_scan);
    if (!device_dirty_ || loaded_device_id_ != device_id_) {
        device_name_->ChangeValue(e ? text(e->display_name) : wxString{});
        device_notes_->ChangeValue(e ? text(e->notes) : wxString{});
    }
    loaded_device_id_ = device_id_;
    if (!e) {
        device_detail_->SetLabel("Select a device.");
        device_evidence_->ChangeValue("");
        return;
    }
    device_detail_->SetLabel(e->inspection
                                 ? "Rename the device JSON file to change its display name. Edit "
                                   "notes externally, then Reload."
                                 : "Saved logical identity is independent of the display name.");
    device_detail_->Wrap(600);
    std::string details;
    if (e->inspection)
        details = e->inspection->views.at(static_cast<std::size_t>(device_view_->GetSelection()));
    else {
        details = "Logical ID: " + e->id + "\nWindows name: " + e->windows_name +
                  "\nAvailability: " + name(e->availability) + "\n";
        for (const auto& f : e->formats)
            details += f.label() + "\n";
        details += controller_->snapshot().operating_mode == OperatingMode::Preview
                       ? "Preview fixture; no hardware measurements."
                       : "Legacy saved configuration; unverified, not scanner evidence.";
    }
    if (device_evidence_->GetValue() != text(details))
        device_evidence_->ChangeValue(text(details));
}
void Frame::scanner_action(const ScannerActionRequest& request) {
    const auto result = controller_->scanner_action(request);
    if (!result.ok)
        show_result(result);
    if (!controller_->snapshot().scanner.available) {
        populate_devices();
        capture_draft();
        if (hole_index_ >= 0) {
            const auto& h = draft_.holes.at(static_cast<std::size_t>(hole_index_));
            populate_direction(forward_, h.forward);
            populate_direction(reverse_, h.reverse);
        }
        update_beep_destinations();
    }
    refresh();
}
} // namespace audiohole::gui
