#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/datetime.h>
#include <wx/wrapsizer.h>

namespace audiohole::gui {
namespace {
wxString local_times(const std::string& value) {
    wxString result = text(value);
    // Render local time only in human-facing views; raw JSON remains UTC.
    for (std::size_t i = 0; i + 20 <= result.length(); ++i) {
        if (result[i + 4] != '-' || result[i + 7] != '-' || result[i + 10] != 'T' ||
            result[i + 19] != 'Z')
            continue;
        wxDateTime date;
        if (!date.ParseISOCombined(result.Mid(i, 19)))
            continue;
        date.MakeFromTimezone(wxDateTime::UTC);
        const auto replacement = date.Format("%Y-%m-%d %H:%M:%S %Z (UTC%z)");
        result = result.Left(i) + replacement + result.Mid(i + 20);
        i += replacement.length() - 1;
    }
    return result;
}
wxString device_status(const Endpoint& e) {
    if (e.inspection && e.inspection->unusable)
        return "Unusable";
    if (e.availability == Availability::Ambiguous || (e.inspection && e.inspection->warning))
        return "Error / attention";
    if (e.availability == Availability::Available)
        return "Available";
    if (e.availability == Availability::Unavailable)
        return "Unavailable";
    return "Unknown / not tested";
}
} // namespace
void Frame::build_devices() {
    auto* page = new wxPanel(notebook_);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* bundles = new wxBoxSizer(wxHORIZONTAL);
    scanner_actions_.push_back(
        button(page, bundles, "Export devices…", [this] { bundle_dialog(false, false); }));
    scanner_actions_.push_back(
        button(page, bundles, "Import devices…", [this] { bundle_dialog(false, true); }));
    devices_folder_ =
        button(page, bundles, "Open Devices Folder", [this] { open_data_folder(false); });
    devices_folder_->SetName("Open Devices Folder");
    devices_folder_->SetToolTip("Open the Test scanner devices folder, including capture/playback "
                                "JSON records. Preview devices are session-only.");
    layout->Add(bundles, 0, wxEXPAND);
    device_columns_ = new wxSplitterWindow(page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxSP_LIVE_UPDATE | wxSP_3D);
    device_columns_->SetName("Resize device column width");
    device_columns_->SetMinimumPaneSize(FromDIP(160));
    device_columns_->SetSashGravity(0);
    auto* browser = new wxPanel(device_columns_);
    auto* left = new wxBoxSizer(wxVERTICAL);
    device_search_ = field(browser, left, "Search devices");
    auto list = [&](const char* title, DeviceList*& box, std::vector<std::string>& ids) {
        const bool capture = std::string(title) == "Capture devices";
        auto* card = new SurfacePanel(browser, capture ? "capture-card" : "playback-card");
        auto* content = new wxBoxSizer(wxVERTICAL);
        material_heading(card, content, text(title), capture);
        box = new DeviceList(card);
        box->SetName(text(title));
        box->SetToolTip(
            "Status dots: green available; gray unavailable; yellow attention or unknown; red "
            "explicitly unusable. Right-click for scans and Hide / Unhide.");
        content->Add(box, 1, wxEXPAND | wxALL, 3);
        card->set_content_sizer(content);
        left->Add(card, 1, wxEXPAND | wxALL, 3);
        auto* control = box;
        auto* mapped_ids = &ids;
        box->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this, control, mapped_ids](wxDataViewEvent&) {
            if (populating_devices_)
                return;
            const int index = control->GetSelection();
            if (index >= 0 && static_cast<std::size_t>(index) < mapped_ids->size() &&
                !mapped_ids->at(static_cast<std::size_t>(index)).empty()) {
                const auto& id = mapped_ids->at(static_cast<std::size_t>(index));
                if (id != device_id_)
                    select_device(id);
            } else
                populate_devices();
        });
        box->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                  [this, control, mapped_ids](wxDataViewEvent& event) {
                      const auto point = event.GetPosition() == wxDefaultPosition
                                             ? wxDefaultPosition
                                             : control->ClientToScreen(event.GetPosition());
                      wxContextMenuEvent menu(wxEVT_CONTEXT_MENU, control->GetId(), point);
                      device_context(control, *mapped_ids, menu);
                  });
        // Keyboard context requests arrive as the normal window event rather
        // than the data view's mouse-only row context event on Windows.
        box->Bind(wxEVT_CONTEXT_MENU, [this, control, mapped_ids](wxContextMenuEvent& event) {
            device_context(control, *mapped_ids, event);
        });
    };
    list("Capture devices", capture_devices_, capture_device_ids_);
    list("Playback devices", playback_devices_, playback_device_ids_);
    browser->SetSizer(left);
    auto* details = new wxPanel(device_columns_);
    auto* right = new wxBoxSizer(wxVERTICAL);
    label(details, right, "Display name");
    auto* name_row = new wxBoxSizer(wxHORIZONTAL);
    device_name_ = new wxTextCtrl(details, wxID_ANY);
    device_name_->SetName("Display name");
    name_row->Add(device_name_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    device_save_ = button(details, name_row, "Save device", [this] {
        if (const auto* e = endpoint(controller_->snapshot(), device_id_)) {
            auto edit = *e;
            edit.display_name = utf8(device_name_->GetValue());
            edit.notes = utf8(device_notes_->GetValue());
            const bool scanner_owned = bool(e->inspection);
            if (scanner_owned)
                edit.inspection = device_edit_snapshot_;
            const auto result = controller_->update_endpoint(edit);
            if (!result.ok) {
                show_result(result);
                return;
            }
            if (scanner_owned)
                device_save_generation_ = controller_->snapshot().scanner.generation;
            else {
                device_dirty_ = false;
                populate_devices();
                present(presentation::Message::DeviceSaved);
            }
            refresh();
        }
    });
    device_save_->SetToolTip("Save the display name and notes. The JSON filename follows the name; "
                             "logical identity and existing profile references stay unchanged.");
    right->Add(name_row, 0, wxEXPAND);
    device_detail_ =
        label(details, right, "Select a device. Right-click its list entry to scan it.");
    device_detail_->SetWindowStyleFlag(wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    device_detail_->SetMinSize(wxSize(0, -1));
    device_rows_ = new wxSplitterWindow(details, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxSP_LIVE_UPDATE | wxSP_3D);
    device_rows_->SetName("Resize device notes and overview heights");
    device_rows_->SetMinimumPaneSize(FromDIP(72));
    device_rows_->SetSashGravity(1.0);
    auto* notes_panel = new wxPanel(device_rows_);
    auto* notes_layout = new wxBoxSizer(wxVERTICAL);
    device_notes_ =
        field(notes_panel, notes_layout, "Device Notes", wxTE_MULTILINE | wxTE_WORDWRAP);
    notes_layout->GetItem(device_notes_)->SetProportion(1);
    device_notes_->SetMinSize(wxSize(0, 24));
    notes_panel->SetSizer(notes_layout);
    auto* evidence_panel = new wxPanel(device_rows_);
    auto* evidence_layout = new wxBoxSizer(wxVERTICAL);
    device_view_ = new ThemedChoice(evidence_panel, wxID_ANY);
    for (const auto* view : inspection_view_names)
        device_view_->Append(text(view));
    device_view_->SetSelection(0);
    device_view_->SetName("Device evidence view");
    evidence_layout->Add(device_view_, 0, wxEXPAND | wxALL, 3);
    device_evidence_ =
        new wxTextCtrl(evidence_panel, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                       wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    device_evidence_->SetName("Device evidence");
    device_evidence_->SetMinSize(wxSize(0, 24));
    evidence_layout->Add(device_evidence_, 1, wxEXPAND | wxALL, 3);
    evidence_panel->SetSizer(evidence_layout);
    device_rows_->SplitHorizontally(evidence_panel, notes_panel);
    right->Add(device_rows_, 1, wxEXPAND);
    auto* maintenance = new wxWrapSizer(wxHORIZONTAL);
    for (const auto& item :
         {std::pair{"Reload saved devices", ScannerActionKind::Reload},
          std::pair{"Refresh availability", ScannerActionKind::RefreshAvailability}}) {
        auto* action = button(details, maintenance, item.first, [this, kind = item.second] {
            if (device_dirty_ &&
                themed_message_box("Discard unsaved device edits?", "Device edits",
                                   wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
                return;
            device_dirty_ = false;
            ScannerActionRequest request;
            request.kind = kind;
            scanner_action(request);
        });
        action->SetToolTip(item.second == ScannerActionKind::Reload
                               ? "Reload external device JSON edits and filename changes without "
                                 "querying hardware."
                               : "Refresh known device availability without capability tests or "
                                 "creating new records.");
        scanner_actions_.push_back(action);
    }
    auto* clear = button(details, maintenance, "Clear device list…", [this] {
        if (themed_message_box(
                "Delete all Test Mode device scan records, including device names, notes, hidden "
                "preferences, probe histories, local bindings and saved scan sessions?\n\n"
                "Saved profiles remain, but their devices may need to be selected again after "
                "rescanning. Exported bundles outside these data folders remain.\n\n"
                "This cannot be undone. Continue?",
                "Clear device scan information", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                this) != wxYES)
            return;
        ScannerActionRequest request;
        request.kind = ScannerActionKind::ClearInventory;
        request.confirmed = true;
        device_dirty_ = false;
        scanner_action(request);
    });
    clear->SetToolTip("Delete scanner records and scan history in the isolated Test data roots, "
                      "after confirmation. Available only while the scanner is idle.");
    scanner_actions_.push_back(clear);
    right->Add(maintenance, 0, wxEXPAND);
    auto* toggle = button(details, right, "Toggle availability", [this] {
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
    details->SetSizer(right);
    device_columns_->SplitVertically(browser, details, FromDIP(300));
    layout->Add(device_columns_, 1, wxEXPAND | wxALL, 3);
    page->SetSizer(layout);
    notebook_->AddPage(page, "Devices");
    device_search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { populate_devices(); });
    device_view_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { load_device(); });
    for (auto* editor : {device_name_, device_notes_})
        editor->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            device_dirty_ = true;
            refresh();
        });
}
bool Frame::select_device(const std::string& id) {
    if (id != device_id_ && device_dirty_ &&
        themed_message_box("Discard unsaved device edits?", "Device edits",
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
    if (populating_devices_)
        return;
    populating_devices_ = true;
    capture_devices_->Freeze();
    playback_devices_->Freeze();
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
                box->Append(text(e.display_name), device_status(e));
                ids.push_back(e.id);
            }
    }
    if (!endpoint(controller_->snapshot(), device_id_) && !device_dirty_)
        device_id_.clear();
    if (device_id_.empty()) {
        const auto& ids = capture_device_ids_.empty() ? playback_device_ids_ : capture_device_ids_;
        const auto first =
            std::find_if(ids.begin(), ids.end(), [](const auto& id) { return !id.empty(); });
        if (first != ids.end())
            device_id_ = *first;
    }
    for (bool capture : {true, false}) {
        auto* box = capture ? capture_devices_ : playback_devices_;
        const auto& ids = capture ? capture_device_ids_ : playback_device_ids_;
        const auto selected = std::find(ids.begin(), ids.end(), device_id_);
        box->SetSelection(selected == ids.end() ? wxNOT_FOUND
                                                : static_cast<int>(selected - ids.begin()));
    }
    capture_devices_->Thaw();
    playback_devices_->Thaw();
    populating_devices_ = false;
    load_device();
}
void Frame::device_context(DeviceList* box, const std::vector<std::string>& ids,
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
            if (device_dirty_) {
                show_result({false, "Save device edits before changing visibility."});
                return;
            }
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
    const bool editable = e && !controller_->snapshot().scanner.busy;
    device_name_->SetEditable(editable);
    device_notes_->SetEditable(editable);
    device_save_->Enable(editable && device_dirty_);
    if (!device_dirty_ || loaded_device_id_ != device_id_) {
        device_edit_snapshot_ = e ? e->inspection : nullptr;
        device_name_->ChangeValue(e ? text(e->display_name) : wxString{});
        device_notes_->ChangeValue(e ? text(e->notes) : wxString{});
    }
    loaded_device_id_ = device_id_;
    if (!e) {
        device_detail_->SetLabel("Select a device.");
        device_evidence_->ChangeValue("");
        return;
    }
    device_detail_->SetLabel(
        e->inspection ? "Save changes the filename and notes while preserving device identity."
                      : "Saved logical identity is independent of the display name.");
    device_detail_->SetToolTip(device_detail_->GetLabel());
    std::string details;
    if (e->inspection)
        details = e->inspection->views.at(static_cast<std::size_t>(device_view_->GetSelection()));
    else {
        const bool preview = controller_->snapshot().operating_mode == OperatingMode::Preview;
        details =
            "Display name: " + e->display_name +
            "\nDevice type: " + (e->capture ? "Capture / Recording" : "Playback") +
            "\nAvailability: " + name(e->availability) + "\nSource: " +
            (preview ? "Preview demonstration data" : "Legacy saved configuration, unverified") +
            "\n\nRecorded formats:\n";
        for (const auto& f : e->formats)
            details += "  " + f.label() + "\n";
        if (e->formats.empty())
            details += "  Unknown / not tested.\n";
        details += "\nWindows name: " + e->windows_name + "\nLogical ID: " + e->id +
                   "\nHidden: " + (e->hidden ? "yes" : "no") + "\n\n" +
                   (preview ? "Preview fixture; no hardware measurements."
                            : "Legacy saved observations are unverified; scan hardware to collect "
                              "current evidence.");
    }
    const auto rendered =
        e->inspection && (device_view_->GetSelection() == 0 || device_view_->GetSelection() == 2)
            ? local_times(details)
            : text(details);
    if (device_evidence_->GetValue() != rendered)
        device_evidence_->ChangeValue(rendered);
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
