#include "gui/controls.h"
#include "gui/frame.h"
#include "model/persistence.h"
#include <sstream>
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
// UI channel numbers are one-based; JSON and engine frame indexes are zero-based.
bool read_preferences(const DirectionWidgets& w, Preferences& p) {
    auto number = [](wxTextCtrl* control, std::optional<int>& out, long maximum) {
        auto value = control->GetValue();
        value.Trim(true).Trim(false);
        if (value.empty() || value.CmpNoCase("Automatic") == 0) {
            out.reset();
            return true;
        }
        long n = 0;
        if (!value.ToLong(&n) || n < 1 || n > maximum)
            return false;
        out = static_cast<int>(n);
        return true;
    };
    if (!number(w.rate, p.common.sample_rate_hz, 768000) ||
        !number(w.channels, p.common.channel_count, 256) ||
        !number(w.source_channels, p.source.channel_count, 256) ||
        !number(w.destination_channels, p.destination.channel_count, 256))
        return false;
    const auto representation = utf8(w.representation->GetStringSelection());
    p.common.sample_format =
        representation == "Automatic" ? std::nullopt : std::optional<std::string>(representation);
    p.channel_map.clear();
    auto raw_map = w.channel_map->GetValue();
    raw_map.Trim(true).Trim(false);
    auto map = utf8(raw_map);
    std::istringstream input(map);
    std::string token;
    while (std::getline(input, token, ',')) {
        std::istringstream pair(token);
        int source = 0, destination = 0;
        char colon = 0;
        if (!(pair >> source >> colon >> destination) || colon != ':' || source < 1 ||
            destination < 1)
            return false;
        pair >> std::ws;
        if (!pair.eof())
            return false;
        p.channel_map.push_back({source - 1, destination - 1});
    }
    if (!map.empty() && map.back() == ',')
        return false;
    return validate_preferences(p).ok;
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
// Defer until native edit/selection handlers finish. A queued Enter may not
// save a different record if the user navigates before it is consumed.
void Frame::queue_field_save(bool device) {
    if (loading_ || enter_save_pending_)
        return;
    enter_save_pending_ = true;
    const auto record = device ? device_id_ : viewed_;
    const int hole = hole_index_;
    CallAfter([this, device, record, hole] {
        enter_save_pending_ = false;
        if (IsBeingDeleted() || record.empty())
            return;
        if (device) {
            if (device_id_ != record || !device_save_->IsEnabled())
                return;
            wxCommandEvent click(wxEVT_BUTTON, device_save_->GetId());
            device_save_->GetEventHandler()->ProcessEvent(click);
        } else if (viewed_ == record && hole_index_ == hole && !bundle_generation_ &&
                   profile_edits_pending())
            save(false);
    });
}
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
    build_device_check(capture, source_layout, w.capture_check, true, title + " capture");
    capture->set_content_sizer(source_layout);
    material_heading(playback, destination_layout, "Destination (Playback)", false);
    w.destination = new ThemedCombo(playback, wxID_ANY);
    w.destination->SetMinSize(FromDIP(wxSize(120, -1)));
    destination_layout->Add(w.destination, 0, wxEXPAND | wxALL, FromDIP(8));
    w.destination_detail = label(playback, destination_layout, "Select a destination");
    build_device_check(playback, destination_layout, w.playback_check, false, title + " playback");
    playback->set_content_sizer(destination_layout);
    endpoints->Add(capture, 1, wxEXPAND | wxALL, FromDIP(5));
    auto* chevron = label(box, endpoints, wxString::FromUTF8("\xEE\x97\x8C"));
    chevron->SetFont(wxFont(wxFontInfo(26).FaceName("Material Icons")));
    chevron->SetName("Direction to playback");
    endpoints->Add(playback, 1, wxEXPAND | wxALL, FromDIP(5));
    w.group->Add(endpoints, 0, wxEXPAND);
    w.exclusive_preferred = new wxCheckBox(box, wxID_ANY, "Exclusive mode preferred");
    w.exclusive_preferred->SetToolTip("Prefer exclusive access in Automatic mode for this "
                                      "direction. A forced mode takes precedence.");
    w.group->Add(w.exclusive_preferred, 0, wxALL, FromDIP(4));
    w.exclusive_preferred->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (!loading_) {
            capture_draft();
            dirty_ = true;
            refresh();
        }
    });
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
    w.tuning = new wxCollapsiblePane(box, wxID_ANY, "Advanced tuning", wxDefaultPosition,
                                     wxDefaultSize, wxCP_DEFAULT_STYLE | wxCP_NO_TLW_RESIZE);
    w.tuning->SetName(title + " advanced tuning");
    w.group->Add(w.tuning, 0, wxEXPAND | wxALL, FromDIP(4));
    auto* pane = w.tuning->GetPane();
    auto* pane_layout = new wxBoxSizer(wxVERTICAL);
    w.advanced_tuning = new wxCheckBox(pane, wxID_ANY, "Use advanced tuning");
    w.advanced_tuning->SetName(title + " use advanced tuning");
    w.advanced_tuning->SetToolTip(
        "Use saved tuning for this direction. Opening the panel only changes visibility.");
    pane_layout->Add(w.advanced_tuning, 0, wxALL, FromDIP(4));
    w.advanced_tuning->Bind(wxEVT_CHECKBOX, [this, &w](wxCommandEvent&) { tuning_changed(w); });
    auto* tuning = new wxStaticBoxSizer(wxVERTICAL, pane, title + " tuning");
    w.tuning_group = tuning;
    pane_layout->Add(tuning, 0, wxEXPAND);
    pane->SetSizer(pane_layout);
    w.tuning->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [this, &w](wxCollapsiblePaneEvent&) {
        layout_direction(w); // Visibility only; no draft capture, policy toggle or Fit().
    });
    box = tuning->GetStaticBox();
    label(box, tuning, "Complete format preset (optional)");
    w.format = new ThemedChoice(box, wxID_ANY);
    tuning->Add(w.format, 0, wxEXPAND | wxALL, 3);
    auto* preferences = new wxFlexGridSizer(2, FromDIP(6), FromDIP(8));
    preferences->AddGrowableCol(1, 1);
    label(box, preferences, "Sample rate (Hz; blank = Automatic)");
    w.rate =
        new wxTextCtrl(box, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    preferences->Add(w.rate, 1, wxEXPAND);
    label(box, preferences, "Stream channels, both endpoints (blank = Automatic)");
    w.channels =
        new wxTextCtrl(box, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    preferences->Add(w.channels, 1, wxEXPAND);
    label(box, preferences, "Source channels (blank = inherit)");
    w.source_channels =
        new wxTextCtrl(box, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    preferences->Add(w.source_channels, 1, wxEXPAND);
    label(box, preferences, "Destination channels (blank = inherit)");
    w.destination_channels =
        new wxTextCtrl(box, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    preferences->Add(w.destination_channels, 1, wxEXPAND);
    w.channels->SetToolTip("Requests the endpoint stream channel count, not a channel selection. "
                           "Use source/destination overrides for different counts; use the map to "
                           "choose specific channels.");
    label(box, preferences, "Sample format");
    w.representation = new ThemedChoice(box);
    for (const auto* value :
         {"Automatic", "float32", "pcm_s16", "pcm_s24_packed", "pcm_s24_in_32", "pcm_s32"})
        w.representation->Append(value);
    preferences->Add(w.representation, 1, wxEXPAND);
    label(box, preferences, "Channel map (source:destination)");
    w.channel_map =
        new wxTextCtrl(box, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    preferences->Add(w.channel_map, 1, wxEXPAND);
    w.channel_map->SetToolTip(
        "One-based channel numbers, for example 1:3, 2:4. Blank maps matching channel indexes; "
        "extra outputs are silent. No hardware bus is inferred.");
    tuning->Add(preferences, 0, wxEXPAND | wxALL, FromDIP(4));
    button(box, tuning, "Reset to Automatic", [this, &w] {
        if (hole_index_ < 0)
            return;
        capture_draft();
        auto& hole = draft_.holes.at(static_cast<std::size_t>(hole_index_));
        (&w == &forward_ ? hole.forward : hole.reverse).settings = Settings{};
        populate_direction(w, &w == &forward_ ? hole.forward : hole.reverse);
        dirty_ = true;
        refresh();
    });
    label(box, tuning, "Buffer (ms; 0 = Automatic)");
    w.buffer = new wxSpinCtrl(box, wxID_ANY, "0", wxDefaultPosition, wxDefaultSize,
                              wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 100, 0);
    auto* buffer_row = new wxBoxSizer(wxHORIZONTAL);
    buffer_row->Add(w.buffer, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(4));
    w.buffer_apply = button(box, buffer_row, "Apply", [this, &w] { apply_buffer(w); });
    const int buffer_height = std::max(w.buffer->GetBestSize().y, FromDIP(32));
    w.buffer->SetMinSize(wxSize(FromDIP(96), buffer_height));
    w.buffer->SetMaxSize(wxSize(-1, buffer_height));
    w.buffer_apply->SetMinSize(wxSize(-1, buffer_height));
    w.buffer_apply->SetMaxSize(wxSize(-1, buffer_height));
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
    w.mode = choices(box, tuning, "Operating mode",
                     {"Automatic", "Force exclusive mode", "Force shared mode"});
    w.clock = choices(box, tuning, "Preferred clock device", {"Automatic", "Capture", "Playback"});
    w.clock->SetToolTip("Save the preferred timing reference for this direction.");
    w.clock->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_) {
            capture_draft();
            dirty_ = true;
            refresh();
        }
    });
    w.mode->SetToolTip("Save an access-mode choice for this direction. Use advanced tuning must be "
                       "enabled for it to apply. Forced modes take precedence over Exclusive mode "
                       "preferred; Automatic initially uses shared access.");
    for (auto* field :
         {w.rate, w.channels, w.source_channels, w.destination_channels, w.channel_map})
        field->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            if (!loading_)
                dirty_ = true;
        });
    w.representation->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_)
            dirty_ = true;
    });
    auto changed = [this, &w](bool source) {
        if (loading_ || hole_index_ < 0)
            return;
        auto* control = static_cast<ThemedCombo*>(source ? w.source : w.destination);
        const auto& ids = source ? w.sources : w.destinations;
        const int index = control->GetSelection();
        if (index >= 0 && !control->item_enabled(static_cast<unsigned>(index))) {
            const auto& hole = draft_.holes[hole_index_];
            populate_direction(w, &w == &forward_ ? hole.forward : hole.reverse);
            return;
        }
        capture_draft();
        auto& hole = draft_.holes[hole_index_];
        auto& direction = &w == &forward_ ? hole.forward : hole.reverse;
        if (index >= 0 && static_cast<std::size_t>(index) < ids.size() && ids[index].empty()) {
            (source ? direction.source : direction.destination).clear();
            (source ? direction.source_name : direction.destination_name).clear();
        }
        end_observation();
        populate_direction(forward_, hole.forward);
        populate_direction(reverse_, hole.reverse);
        dirty_ = true;
        present(source ? presentation::Message::Source : presentation::Message::Destination);
        refresh();
    };
    w.source->Bind(wxEVT_COMBOBOX, [changed](wxCommandEvent&) { changed(true); });
    w.destination->Bind(wxEVT_COMBOBOX, [changed](wxCommandEvent&) { changed(false); });
    w.format->Bind(wxEVT_CHOICE, [this, &w](wxCommandEvent&) {
        if (loading_)
            return;
        const auto selected = w.format->GetSelection();
        if (selected > 0 && static_cast<std::size_t>(selected) <= w.formats.size()) {
            const auto& f = w.formats[selected - 1];
            w.rate->ChangeValue(std::to_string(f.rate));
            w.channels->ChangeValue(std::to_string(f.channels));
            auto representation =
                f.representation == "PCM16" ? std::string("pcm_s16") : f.representation;
            w.representation->SetStringSelection(text(representation));
        } else {
            w.rate->ChangeValue("");
            w.channels->ChangeValue("");
            w.representation->SetSelection(0);
        }
        dirty_ = true;
    });
    w.mode->Bind(wxEVT_CHOICE, [this, &w](wxCommandEvent&) {
        if (loading_ || hole_index_ < 0)
            return;
        capture_draft();
        const auto& hole = draft_.holes.at(static_cast<std::size_t>(hole_index_));
        populate_direction(w, &w == &forward_ ? hole.forward : hole.reverse);
        dirty_ = true;
        refresh();
    });
    w.buffer->Bind(wxEVT_SPINCTRL, [this, &w](wxSpinEvent&) { acknowledge_buffer(w); });
    w.buffer->Bind(wxEVT_TEXT_ENTER, [this, &w](wxCommandEvent&) {
        acknowledge_buffer(w);
        queue_field_save(false);
    });
    for (auto* input :
         std::vector<wxWindow*>{w.rate, w.channels, w.source_channels, w.destination_channels,
                                w.channel_map, w.source, w.destination})
        input->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { queue_field_save(false); });
    for (auto* choice : {w.format, w.mode, w.representation, w.clock})
        choice->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& event) {
            if ((event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) &&
                !event.HasAnyModifiers())
                queue_field_save(false);
            event.Skip();
        });
    for (auto* checkbox : {w.advanced_tuning, w.exclusive_preferred})
        checkbox->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& event) {
            if ((event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) &&
                !event.HasAnyModifiers())
                queue_field_save(false);
            event.Skip();
        });
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
void Frame::tuning_changed(DirectionWidgets& widgets) {
    if (loading_ || hole_index_ < 0)
        return;
    auto& hole = draft_.holes.at(static_cast<std::size_t>(hole_index_));
    auto& direction = &widgets == &forward_ ? hole.forward : hole.reverse;
    const bool enabled = widgets.advanced_tuning->GetValue();
    widgets.advanced_tuning->SetValue(direction.advanced_tuning);
    capture_draft();
    widgets.advanced_tuning->SetValue(enabled);
    direction.advanced_tuning = enabled;
    if (!enabled)
        widgets.advanced_tuning->SetFocus();
    populate_direction(widgets, direction);
    layout_direction(widgets);
    dirty_ = true;
    tuning_reaction_reverse_ = &widgets == &reverse_;
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
                 "Rename and save the profile without changing active routing.");
    menu->Append(wxID_DUPLICATE, "Duplicate", "Create a separate copy of the saved profile.");
    const auto reveal = wxWindow::NewControlId();
    menu->Append(reveal, "Show in Explorer",
                 "Open the folder and select this profile's saved JSON.");
    menu->Enable(reveal, controller_->snapshot().operating_mode == OperatingMode::Test);
    menu->Bind(
        wxEVT_MENU,
        [this, id](wxCommandEvent&) {
            std::filesystem::path file;
            const auto found = profile_record_path(preferences_->test_configuration(), id, file);
            show_result(found.ok ? show_in_explorer(file) : found);
        },
        reveal);
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
            theme_dialog(&dialog);
            if (dialog.ShowModal() == wxID_OK) {
                rename_profile(utf8(dialog.GetValue()));
                if (draft_.display_name == utf8(dialog.GetValue()))
                    save(false);
            }
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
            if (themed_message_box("Delete this saved profile?", "Confirm profile deletion",
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
    menu->Append(wxID_EDIT, wxS("Rename…"),
                 "Rename and save this hole without changing active routing.");
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
            theme_dialog(&dialog);
            if (dialog.ShowModal() == wxID_OK) {
                rename_hole(utf8(dialog.GetValue()));
                if (hole_index_ >= 0 &&
                    draft_.holes.at(static_cast<std::size_t>(hole_index_)).display_name ==
                        utf8(dialog.GetValue()))
                    save(false);
            }
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
    auto created = controller_->make_hole();
    std::vector<std::string> names;
    for (const auto& hole : draft_.holes)
        names.push_back(hole.display_name);
    created.display_name = unique_name("New Hole", names);
    draft_.holes.push_back(std::move(created));
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
    if (themed_message_box("Remove this hole from the editor? Save keeps the change; Apply changes "
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
// Named profile files live in profiles/; device scan JSON lives in devices/.
// Opening a folder never saves a draft, scans hardware or changes configuration.
void Frame::open_data_folder(bool profiles) {
    if (controller_->snapshot().operating_mode != OperatingMode::Test) {
        show_result({false, "Preview profiles and devices are session-only. Saved data folders are "
                            "available in Test Mode."});
        return;
    }
    auto directory =
        preferences_->test_configuration().parent_path() / (profiles ? "profiles" : "devices");
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
    profiles_folder_->SetToolTip("Open the profiles folder containing one named JSON per profile. "
                                 "Preview profiles are session-only.");
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
    profile_lower_rows_->SetSashGravity(1.0);
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
        add_control(profile_panel, profile_heading, "New Profile", "Create a new profile.", [this] {
            if (!discard())
                return;
            const auto id = controller_->create_profile("New Profile");
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
    label(notes_panel, notes_layout, "Profile Notes");
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
    profile_lower_rows_->SplitHorizontally(holes_panel, notes_panel);
    profile_rows_->SplitHorizontally(profile_panel, profile_lower_rows_);
    sidebar_layout->Add(profile_rows_, 1, wxEXPAND);
    sidebar->SetSizer(sidebar_layout);
    auto* right_container = new wxPanel(profile_columns_);
    auto* right_column = new wxBoxSizer(wxVERTICAL);
    right_container->SetSizer(right_column);
    auto* editor = new wxScrolledWindow(right_container);
    profile_editor_ = editor;
    right_column->Add(editor, 1, wxEXPAND);
    editor->SetScrollRate(FromDIP(12), FromDIP(12));
    auto* right = new wxBoxSizer(wxVERTICAL);
    editor->SetSizer(right);
    profile_columns_->SplitVertically(sidebar, right_container, FromDIP(300));
    hole_heading_ = label(editor, right, "Select a hole");
    hole_heading_->SetWindowStyleFlag(wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    hole_heading_->SetMinSize(wxSize(0, -1));
    hole_heading_->SetFont(hole_heading_->GetFont().Bold());
    auto* toggles = new wxBoxSizer(wxHORIZONTAL);
    enabled_ = new wxCheckBox(editor, wxID_ANY, "Enabled");
    duplex_ = new wxCheckBox(editor, wxID_ANY, "Enable return channel");
    toggles->Add(enabled_, 0, wxALL, 4);
    toggles->Add(duplex_, 0, wxALL, 4);
    right->Add(toggles);
    hole_notes_ = field(editor, right, "Hole notes", wxTE_MULTILINE | wxTE_WORDWRAP);
    hole_notes_->SetMinSize(wxSize(0, hole_notes_->GetCharHeight() * 3 + FromDIP(8)));
    routing_engine_ = choices(editor, right, "Routing Engine",
                              {"Automatic", "WASAPI", "Miniaudio", "Experimental"});
    routing_engine_->SetToolTip("Save the routing backend request for this hole. Selecting an "
                                "engine does not start routing.");
    routing_engine_notice_ = label(editor, right, "");
    routing_engine_notice_->SetMinSize(wxSize(0, -1));
    routing_engine_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_) {
            capture_draft();
            dirty_ = true;
            refresh();
        }
    });
    build_direction(editor, right, forward_, "Forward");
    build_direction(editor, right, reverse_, "Return");
    for (auto* input : std::vector<wxWindow*>{enabled_, duplex_, routing_engine_})
        input->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& event) {
            if ((event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) &&
                !event.HasAnyModifiers())
                queue_field_save(false);
            event.Skip();
        });
    build_hole_diagnostics(editor, right);
    performance_panel_ = new wxPanel(right_container);
    auto* performance =
        new wxStaticBoxSizer(wxVERTICAL, performance_panel_, "Performance Diagnostics");
    performance_panel_->SetSizer(performance);
    label(performance->GetStaticBox(), performance,
          "Latency / buffer occupancy / clock drift / CPU: unavailable.");
    path_status_ = label(performance->GetStaticBox(), performance, "No active directions.");
    path_status_->SetMinSize(wxSize(0, path_status_->GetCharHeight() * 3));
    path_status_->SetMaxSize(wxSize(-1, path_status_->GetCharHeight() * 3));
    right_column->Add(performance_panel_, 0, wxEXPAND | wxALL, FromDIP(4));
    page_layout->Add(profile_columns_, 1, wxEXPAND | wxALL, 3);
    page->SetSizer(page_layout);
    notebook_->AddPage(page, "Profiles");
    profiles_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        const int i = profiles_->GetSelection();
        if (i >= 0 && discard()) {
            const auto id = profile_ids_.at(static_cast<std::size_t>(i));
            const auto& state = controller_->snapshot();
            const bool switch_now = preferences_->values().activate_selected_profile &&
                                    state.capabilities.routing && state.armed &&
                                    state.active_profile != id;
            select_profile(id);
            if (switch_now)
                activate(false);
        } else {
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
        capture_draft();
        load_holes();
        holes_->SetSelection(hole_index_);
        dirty_ = true;
        present(enabled_->GetValue() ? presentation::Message::Enabled
                                     : presentation::Message::Disabled);
    });
    duplex_->Bind(wxEVT_CHECKBOX, [this, editor](wxCommandEvent&) {
        if (loading_ || hole_index_ < 0)
            return;
        capture_draft();
        const auto& hole = draft_.holes[hole_index_];
        populate_direction(forward_, hole.forward);
        populate_direction(reverse_, hole.reverse);
        load_holes();
        holes_->SetSelection(hole_index_);
        dirty_ = true;
        end_observation();
        reverse_.group->ShowItems(duplex_->GetValue());
        reverse_.tuning_group->ShowItems(duplex_->GetValue());
        editor->FitInside();
        editor->Layout();
        present(duplex_->GetValue() ? presentation::Message::Duplex
                                    : presentation::Message::Simplex);
    });
}
// Test uses exact worker-projected scanner tuples; Preview uses authored fixtures.
// Rebuilding must not silently erase a manual choice or invent runtime evidence.
void Frame::populate_direction(DirectionWidgets& w, const audiohole::Direction& d) {
    const bool old_loading = loading_;
    loading_ = true;
    w.advanced_tuning->SetValue(d.advanced_tuning);
    w.exclusive_preferred->SetValue(d.exclusive_mode_preferred);
    w.source->Clear();
    w.destination->Clear();
    w.sources.clear();
    w.destinations.clear();
    w.retained_source = d.source;
    w.retained_destination = d.destination;
    wxArrayString source_search, dest_search;
    static_cast<ThemedCombo*>(w.source)->disabled_items.clear();
    static_cast<ThemedCombo*>(w.destination)->disabled_items.clear();
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
        if (hole_index_ >= 0 && static_cast<std::size_t>(hole_index_) < draft_.holes.size() &&
            endpoint_used_elsewhere(draft_.holes[hole_index_], &w == &reverse_, e.capture, e.id))
            static_cast<ThemedCombo*>(list)->disabled_items.push_back(
                static_cast<unsigned>(ids.size() - 1));
        if (static_cast<ThemedCombo*>(list)->item_enabled(static_cast<unsigned>(ids.size() - 1)))
            (e.capture ? source_search : dest_search).Add(caption);
        if (e.id == (e.capture ? d.source : d.destination)) {
            list->SetSelection(static_cast<int>(ids.size()) - 1);
            list->SetToolTip(text(e.windows_name + " | logical ID: " + e.id));
        }
    }
    w.source->Append("Clear endpoint");
    w.sources.push_back("");
    w.destination->Append("Clear endpoint");
    w.destinations.push_back("");
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
        if (d.settings.preferences.common.sample_rate_hz == f.rate &&
            d.settings.preferences.common.channel_count == f.channels &&
            d.settings.preferences.common.sample_format == f.representation)
            selection = static_cast<int>(w.format->GetCount()) - 1;
    }
    w.format->SetSelection(selection);
    update_direction_details(w, d);
    const auto& preference = d.settings.preferences;
    w.rate->ChangeValue(
        preference.common.sample_rate_hz ? std::to_string(*preference.common.sample_rate_hz) : "");
    w.channels->ChangeValue(
        preference.common.channel_count ? std::to_string(*preference.common.channel_count) : "");
    w.source_channels->ChangeValue(
        preference.source.channel_count ? std::to_string(*preference.source.channel_count) : "");
    w.destination_channels->ChangeValue(preference.destination.channel_count
                                            ? std::to_string(*preference.destination.channel_count)
                                            : "");
    w.representation->SetStringSelection(
        text(preference.common.sample_format.value_or("Automatic")));
    std::string mapping;
    for (const auto& route : preference.channel_map) {
        if (!mapping.empty())
            mapping += ", ";
        mapping += std::to_string(route.source + 1) + ":" + std::to_string(route.destination + 1);
    }
    w.channel_map->ChangeValue(text(mapping));
    w.buffer->SetValue(d.settings.preferences.common.buffer_ms.value_or(0));
    w.acknowledged_buffer_ms = d.settings.buffer_ms;
    const auto mode = d.settings.preferences.common.mode.value_or("Automatic");
    w.mode->SetSelection(mode == "Exclusive" ? 1 : mode == "Shared" ? 2 : 0);
    const auto clock = d.settings.preferences.preferred_clock_device;
    w.clock->SetSelection(clock == "capture" ? 1 : clock == "playback" ? 2 : 0);
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
    w.buffer_apply->Enable(w.advanced_tuning->GetValue() && saved_direction && parsed &&
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
    const bool advanced = w.advanced_tuning->GetValue();
    if (!advanced)
        effective.settings = Settings{};
    const auto recommendation = audiohole::recommend(snapshot, effective);
    const bool compatible = !recommendation.compatible.empty();
    const auto selected =
        compatible ? recommendation.compatible.front()
                   : derive_pcm_format(effective.settings.preferences.common).value_or(Format{});
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
        summary = "Automatic configuration awaits runtime negotiation. Source and destination may "
                  "use different formats.";
    else
        summary = "Compatible format identified from available endpoint evidence. Runtime "
                  "negotiation remains unverified.";
    if (advanced) {
        summary = "Advanced overrides enabled. Explicit values are requirements; unspecified "
                  "values inherit endpoint → common → Automatic policy. Save / Apply is explicit.";
    } else if (d.settings.preferences != audiohole::Preferences{}) {
        summary +=
            " Automatic policy active in this draft — retained manual settings are inactive.";
    }
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
    d.advanced_tuning = w.advanced_tuning->GetValue();
    d.exclusive_mode_preferred = w.exclusive_preferred->GetValue();
    // The checkbox gates resolution, not editing. Retain explicit choices made
    // while tuning is off; opening a panel or picking a mode must not discard them.
    auto preference = d.settings.preferences;
    if (read_preferences(w, preference))
        d.settings.preferences = std::move(preference);
    if (const auto parsed = buffer_value(w.buffer))
        d.settings.set_buffer_preference(*parsed);
    const auto mode = w.mode->GetSelection();
    d.settings.preferences.common.mode =
        mode == 0 ? std::nullopt : std::optional<std::string>(mode == 1 ? "Exclusive" : "Shared");
    d.settings.preferences.preferred_clock_device = w.clock->GetSelection() == 1   ? "capture"
                                                    : w.clock->GetSelection() == 2 ? "playback"
                                                                                   : "automatic";
    d.settings.refresh_projection();
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
    constexpr const char* engines[]{"automatic", "wasapi", "miniaudio", "experimental"};
    const int engine = routing_engine_->GetSelection();
    if (engine >= 0 && engine < 4)
        h.routing_engine = engines[engine];
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
    auto* list = static_cast<ThemedList*>(holes_);
    list->status_colours.clear();
    for (const auto& h : draft_.holes) {
        const bool failed = std::any_of(
            controller_->snapshot().paths.begin(), controller_->snapshot().paths.end(),
            [&](const auto& path) { return path.hole_id == h.id && path.state == State::Failed; });
        list->status_colours.push_back(failed      ? wxColour(230, 70, 70)
                                       : h.enabled ? wxColour(40, 170, 105)
                                                   : wxColour(145, 154, 168));
        std::string caption;
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
    for (auto* widgets : {&forward_, &reverse_}) {
        const auto& direction = widgets == &forward_ ? h.forward : h.reverse;
        widgets->advanced_tuning->SetValue(direction.advanced_tuning);
        widgets->exclusive_preferred->SetValue(direction.exclusive_mode_preferred);
        widgets->advanced_tuning->Enable(index >= 0);
        widgets->exclusive_preferred->Enable(index >= 0);
    }
    hole_remove_->Enable(index >= 0);
    constexpr const char* engines[]{"automatic", "wasapi", "miniaudio", "experimental"};
    int engine = wxNOT_FOUND;
    for (int i = 0; i < 4; ++i)
        if (h.routing_engine == engines[i])
            engine = i;
    routing_engine_->SetSelection(engine);
    routing_engine_->Enable(index >= 0);
    routing_engine_notice_->SetLabel(
        engine == wxNOT_FOUND ? text("Saved Routing Engine unavailable: " + h.routing_engine +
                                     ". Choose a supported engine explicitly.")
                              : wxString{});
    routing_engine_notice_->Wrap(560);
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
                                    direction->mode, direction->buffer, direction->clock})
            input->Enable(index >= 0);
        for (wxWindow* input :
             std::vector<wxWindow*>{direction->rate, direction->channels, direction->representation,
                                    direction->source_channels, direction->destination_channels,
                                    direction->channel_map})
            input->Enable(index >= 0);
        if (index < 0)
            direction->buffer_apply->Disable();
    }
    reverse_.group->ShowItems(h.duplex && index >= 0);
    reverse_.tuning_group->ShowItems(h.duplex && index >= 0);
    end_observation();
    loading_ = false;
    auto* editor = dynamic_cast<wxScrolledWindow*>(hole_heading_->GetParent());
    editor->FitInside();
    editor->Layout();
}
bool Frame::profile_edits_pending() {
    if (viewed_.empty())
        return false;
    capture_draft();
    const auto* stored = profile(controller_->snapshot(), viewed_);
    if (!stored)
        return false;
    if (draft_ != *stored)
        return true;
    if (hole_index_ >= 0)
        for (auto* widgets : {&forward_, &reverse_}) {
            if (widgets == &reverse_ && !duplex_->GetValue())
                continue;
            auto preference = (widgets == &forward_ ? draft_.holes[hole_index_].forward
                                                    : draft_.holes[hole_index_].reverse)
                                  .settings.preferences;
            if (!buffer_value(widgets->buffer) || !read_preferences(*widgets, preference))
                return true;
        }
    return false;
}
void Frame::save(bool apply) {
    tuning_timer_.Stop();
    // Typing in an endpoint search is not an assignment. Do not let Enter in
    // another field erase a retained binding, including a hidden/missing one.
    const auto assigned = [](wxComboBox* control, const wxString& retained) {
        return control->GetValue().empty() || control->GetSelection() != wxNOT_FOUND ||
               control->GetValue() == retained;
    };
    const auto direction_assigned = [&](const DirectionWidgets& direction) {
        return assigned(direction.source, direction.retained_source_caption) &&
               assigned(direction.destination, direction.retained_destination_caption);
    };
    if (hole_index_ >= 0 &&
        (!direction_assigned(forward_) || (duplex_->GetValue() && !direction_assigned(reverse_)))) {
        show_result({false, "Select a listed capture or playback device to finish the search "
                            "before saving."});
        return;
    }
    if (hole_index_ >= 0) {
        for (auto* widgets : {&forward_, &reverse_}) {
            if (widgets == &reverse_ && !duplex_->GetValue())
                continue;
            auto preference = (widgets == &forward_ ? draft_.holes[hole_index_].forward
                                                    : draft_.holes[hole_index_].reverse)
                                  .settings.preferences;
            if (!buffer_value(widgets->buffer) || !read_preferences(*widgets, preference)) {
                show_result({false, "Check this direction's buffer, sample rate, channel count and "
                                    "channel map before saving."});
                return;
            }
        }
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
    activate_saved_profile(id, ready);
}
void Frame::activate_saved_profile(const std::string& id, bool ready) {
    const auto& snapshot = controller_->snapshot();
    const auto* selected = profile(snapshot, id);
    controller_->wait_for_devices(preferences_->values().automatically_wait_for_devices);
    if (selected && snapshot.armed && snapshot.active_profile != id) {
        const auto valid = validate(snapshot, *selected);
        if (!valid.ok) {
            show_result(valid);
            return;
        }
        bool missing = false;
        for (const auto& hole : selected->holes)
            if (hole.enabled)
                for (const auto* direction : {&hole.forward, hole.duplex ? &hole.reverse : nullptr})
                    if (direction)
                        for (const auto& endpoint_id :
                             {direction->source, direction->destination}) {
                            const auto* device = endpoint(snapshot, endpoint_id);
                            missing |= !device || device->availability != Availability::Available;
                        }
        if (missing && preferences_->values().automatically_wait_for_devices) {
            if (themed_message_box("Replace the current routing profile and wait for the selected "
                                   "profile's missing devices?",
                                   "Switch routing profile?",
                                   wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
                return;
            ready = true;
        }
    }
    show_result(controller_->activate(id, ready));
    refresh();
}

} // namespace audiohole::gui

namespace audiohole::gui {
// Preview-only failure injection stays separate from endpoint diagnostics.
void Frame::build_hole_diagnostics(wxWindow* parent, wxSizer* layout) {
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
