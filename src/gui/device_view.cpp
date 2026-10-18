#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
void Frame::build_devices() {
    auto* page = new wxPanel(notebook_);
    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* left = new wxBoxSizer(wxVERTICAL);
    device_search_ = field(page, left, "Search devices");
    devices_ = new wxListBox(page, wxID_ANY, wxDefaultPosition, wxSize(350, 360));
    left->Add(devices_, 1, wxEXPAND | wxALL, 4);
    root->Add(left, 0, wxEXPAND);
    auto* right = new wxBoxSizer(wxVERTICAL);
    root->Add(right, 1, wxEXPAND | wxALL, 6);
    device_name_ = field(page, right, "Display name");
    device_notes_ = field(page, right, "User-editable notes", wxTE_MULTILINE);
    device_detail_ = label(page, right, "Select a device.");
    button(page, right, "Save device", [this] {
        if (const auto* e = audiohole::endpoint(controller_->snapshot(), device_id_)) {
            auto edit = *e;
            edit.display_name = utf8(device_name_->GetValue());
            edit.notes = utf8(device_notes_->GetValue());
            show_result(controller_->update_endpoint(edit));
            populate_devices();
        }
    });
    button(page, right, "Toggle availability", [this] {
        if (const auto* e = audiohole::endpoint(controller_->snapshot(), device_id_)) {
            if (e->availability == audiohole::Availability::Ambiguous) {
                show_result({false, "Ambiguous identities are not automatically resolved. Explicit "
                                    "binding is required."});
                return;
            }
            auto edit = *e;
            edit.availability = e->availability == audiohole::Availability::Available
                                    ? audiohole::Availability::Unavailable
                                    : audiohole::Availability::Available;
            show_result(controller_->update_endpoint(edit));
            populate_devices();
            refresh();
        }
    });
    page->SetSizer(root);
    notebook_->AddPage(page, "Devices");
    devices_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        if (devices_->GetSelection() >= 0) {
            device_id_ = device_ids_.at(static_cast<std::size_t>(devices_->GetSelection()));
            load_device();
        }
    });
    device_search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { populate_devices(); });
}
void Frame::populate_devices() {
    devices_->Clear();
    device_ids_.clear();
    const auto search = device_search_->GetValue().Lower();
    for (const auto& e : controller_->snapshot().endpoints) {
        const auto caption = text(e.display_name + " | " + e.windows_name + " | " + e.id);
        if (!search.empty() && !caption.Lower().Contains(search))
            continue;
        devices_->Append(text(std::string(e.capture ? "[Capture] " : "[Playback] ") +
                              e.display_name + " — " + audiohole::name(e.availability)));
        device_ids_.push_back(e.id);
        if (e.id == device_id_)
            devices_->SetSelection(static_cast<int>(device_ids_.size()) - 1);
    }
    if (devices_->GetSelection() < 0 && !device_ids_.empty()) {
        devices_->SetSelection(0);
        device_id_ = device_ids_.front();
    }
    load_device();
}
void Frame::load_device() {
    const auto* e = audiohole::endpoint(controller_->snapshot(), device_id_);
    if (!e)
        return;
    device_name_->ChangeValue(text(e->display_name));
    device_notes_->ChangeValue(text(e->notes));
    std::string details = "windows_name (read-only): " + e->windows_name +
                          "\nLogical ID: " + e->id +
                          "\nDirection: " + (e->capture ? "Capture" : "Playback") +
                          "\nAvailability: " + audiohole::name(e->availability) +
                          "\nDriver version: unavailable\nSupported formats:\n";
    for (const auto& f : e->formats)
        details += "  " + f.label() + "\n";
    if (e->formats.empty())
        details += "  Unknown / untested. No verified recommendation.\n";
    details += "Clock relationship: unknown.";
    device_detail_->SetLabel(text(details));
    device_detail_->Wrap(650);
}

} // namespace audiohole::gui
