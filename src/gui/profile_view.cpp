#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
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
    label(capture, source_layout, "Source (Capture)")->SetFont(GetFont().Bold());
    w.source = new wxComboBox(capture, wxID_ANY);
    w.source->SetMinSize(FromDIP(wxSize(120, -1)));
    source_layout->Add(w.source, 0, wxEXPAND | wxALL, FromDIP(8));
    w.source_detail = label(capture, source_layout, "Select a source");
    capture->set_content_sizer(source_layout);
    label(playback, destination_layout, "Destination (Playback)")->SetFont(GetFont().Bold());
    w.destination = new wxComboBox(playback, wxID_ANY);
    w.destination->SetMinSize(FromDIP(wxSize(120, -1)));
    destination_layout->Add(w.destination, 0, wxEXPAND | wxALL, FromDIP(8));
    w.destination_detail = label(playback, destination_layout, "Select a destination");
    playback->set_content_sizer(destination_layout);
    endpoints->Add(capture, 1, wxEXPAND | wxALL, FromDIP(5));
    label(box, endpoints, "→");
    endpoints->Add(playback, 1, wxEXPAND | wxALL, FromDIP(5));
    w.group->Add(endpoints, 0, wxEXPAND);
    w.summary = label(box, w.group, "Configuration");
    label(box, w.group, "Audio format");
    w.format = new wxChoice(box, wxID_ANY);
    w.group->Add(w.format, 0, wxEXPAND | wxALL, 3);
    w.evidence = label(box, w.group, "Select endpoints to see a format recommendation.");
    w.evidence->Wrap(560);
    label(box, w.group, "Buffer (ms)");
    w.buffer = new wxSpinCtrl(box, wxID_ANY, "6", wxDefaultPosition, wxDefaultSize,
                              wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 1, 100, 6);
    auto* buffer_row = new wxBoxSizer(wxHORIZONTAL);
    buffer_row->Add(w.buffer, 0, wxALL, 3);
    w.buffer_apply = button(box, buffer_row, "Apply", [] {});
    w.buffer_apply->Disable();
    w.buffer_apply->SetToolTip(
        "Live buffer application is unavailable in this preview backend. This button stays "
        "disabled because there is no engine acknowledgment of an applied buffer. Save stores "
        "the edited preference in this session; profile Apply activates a simulated plan.");
    w.group->Add(buffer_row, 0, wxEXPAND);
    w.buffer->SetToolTip("Buffer duration in milliseconds. Smaller values can reduce latency but "
                         "increase dropout risk. This is an edited preference, not a measured or "
                         "confirmed engine buffer; live application is unavailable here.");
    w.format->SetToolTip("Choose the recommended compatible format or an explicit format. The "
                         "evidence below explains the recommendation.");
    w.source->SetToolTip("Choose the capture endpoint for this direction. Type to search; select a "
                         "listed device to bind it.");
    w.destination->SetToolTip("Choose the playback endpoint for this direction. The Windows name "
                              "is shown for the selected device.");
    auto* advanced = new wxCollapsiblePane(box, wxID_ANY, "Advanced directional settings");
    w.group->Add(advanced, 0, wxEXPAND | wxALL, 3);
    auto* pane = advanced->GetPane();
    auto* inner = new wxBoxSizer(wxVERTICAL);
    w.mode = choices(pane, inner, "Operating mode", {"Automatic", "Shared", "Exclusive"});
    w.mode->SetToolTip("Automatic chooses an operating preference. Shared cooperates with other "
                       "applications; Exclusive can reserve the endpoint. Changes wait for Apply.");
    auto* fallback = new wxChoice(pane, wxID_ANY);
    fallback->Append(wxS("Fallback device / channel mapping — unavailable"));
    fallback->SetSelection(0);
    fallback->Disable();
    inner->Add(fallback, 0, wxEXPAND | wxALL, 3);
    pane->SetSizer(inner);
    advanced->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [parent](wxCollapsiblePaneEvent&) {
        parent->Layout();
        if (auto* scroll = dynamic_cast<wxScrolledWindow*>(parent))
            scroll->FitInside();
    });
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
    w.mode->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_)
            dirty_ = true;
    });
    w.buffer->Bind(wxEVT_SPINCTRL, [this, &w](wxSpinEvent&) { acknowledge_buffer(w); });
    w.buffer->Bind(wxEVT_TEXT_ENTER, [this, &w](wxCommandEvent&) { acknowledge_buffer(w); });
    w.buffer->Bind(wxEVT_KILL_FOCUS, [this, &w](wxFocusEvent& event) {
        acknowledge_buffer(w);
        event.Skip();
    });
    w.buffer->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (!loading_)
            dirty_ = true;
    });
}
// Spin, Enter, and focus loss can describe the same edit. Compare the settled value
// so polling, loading, and duplicate native notifications never consume commentary.
void Frame::acknowledge_buffer(DirectionWidgets& widgets) {
    if (loading_ || hole_index_ < 0)
        return;
    const int value = widgets.buffer->GetValue();
    if (value == widgets.acknowledged_buffer_ms)
        return;
    widgets.acknowledged_buffer_ms = value;
    dirty_ = true;
    present(presentation::Message::BufferChanged);
}
void Frame::build_profiles() {
    auto* page = new wxPanel(notebook_);
    auto* horizontal = new wxBoxSizer(wxHORIZONTAL);
    auto* sidebar = new wxScrolledWindow(page);
    sidebar->SetScrollRate(FromDIP(12), FromDIP(12));
    sidebar->SetMinSize(FromDIP(wxSize(290, -1)));
    auto* left = new wxBoxSizer(wxVERTICAL);
    sidebar->SetSizer(left);
    horizontal->Add(sidebar, 0, wxEXPAND | wxALL, 4);
    label(sidebar, left, "Profile");
    profiles_ = new wxListBox(sidebar, wxID_ANY, wxDefaultPosition, wxSize(260, 135));
    profiles_->SetToolTip("Open a profile for editing. Selection alone does not change active "
                          "routing; use Start Routing or Apply.");
    left->Add(profiles_, 0, wxEXPAND | wxALL, 3);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    button(sidebar, row, "New", [this] {
        if (!discard())
            return;
        select_profile(controller_->create_profile("New profile"));
    });
    button(sidebar, row, "Duplicate", [this] {
        if (!discard())
            return;
        select_profile(controller_->create_profile(draft_.display_name + " copy", viewed_));
    });
    button(sidebar, row, "Delete", [this] {
        if (wxMessageBox("Delete this saved session profile?", "Confirm profile deletion",
                         wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
            return;
        const auto r = controller_->remove_profile(viewed_);
        show_result(r);
        if (r.ok) {
            viewed_.clear();
            select_profile(controller_->snapshot().profiles.empty()
                               ? ""
                               : controller_->snapshot().profiles.front().id);
        }
    });
    left->Add(row);
    profile_name_ = field(sidebar, left, "Profile display name");
    profile_notes_ = field(sidebar, left, "Profile notes", wxTE_MULTILINE);
    profile_notes_->SetMinSize(wxSize(250, 65));
    label(sidebar, left, "Holes");
    holes_ = new wxListBox(sidebar, wxID_ANY, wxDefaultPosition, wxSize(260, 145));
    left->Add(holes_, 1, wxEXPAND | wxALL, 3);
    button(sidebar, left, "+ Add Hole", [this] {
        capture_draft();
        draft_.holes.push_back(controller_->make_hole());
        dirty_ = true;
        load_holes();
        load_hole(static_cast<int>(draft_.holes.size()) - 1);
        present(presentation::Message::Created);
    });
    button(sidebar, left, "Remove selected hole", [this] {
        if (hole_index_ < 0)
            return;
        if (wxMessageBox(
                "Remove this hole from the editor? Apply is required to change active routing.",
                "Confirm hole removal", wxYES_NO | wxNO_DEFAULT, this) != wxYES)
            return;
        capture_draft();
        draft_.holes.erase(draft_.holes.begin() + hole_index_);
        dirty_ = true;
        load_holes();
        load_hole(draft_.holes.empty() ? -1 : 0);
    });
    auto* saves = new wxBoxSizer(wxHORIZONTAL);
    button(sidebar, saves, "Save", [this] { save(false); });
    button(sidebar, saves, "Apply", [this] { save(true); });
    button(sidebar, saves, "Revert", [this] {
        if (discard())
            select_profile(viewed_);
    });
    left->Add(saves);
    auto* editor = new wxScrolledWindow(page);
    editor->SetScrollRate(FromDIP(12), FromDIP(12));
    auto* right = new wxBoxSizer(wxVERTICAL);
    editor->SetSizer(right);
    horizontal->Add(editor, 1, wxEXPAND | wxALL, 3);
    hole_name_ = field(editor, right, "Hole display name");
    auto* toggles = new wxBoxSizer(wxHORIZONTAL);
    enabled_ = new wxCheckBox(editor, wxID_ANY, "Enabled");
    duplex_ = new wxCheckBox(editor, wxID_ANY, "Duplex (independent directions)");
    toggles->Add(enabled_, 0, wxALL, 4);
    toggles->Add(duplex_, 0, wxALL, 4);
    right->Add(toggles);
    hole_notes_ = field(editor, right, "Hole notes");
    build_direction(editor, right, forward_, "Forward");
    build_direction(editor, right, reverse_, "Reverse");
    path_status_ = label(editor, right, "No active paths.");
    build_hole_diagnostics(editor, right);
    page->SetSizer(horizontal);
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
    for (auto* f : {profile_name_, profile_notes_, hole_name_, hole_notes_})
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
        editor->FitInside();
        editor->Layout();
        present(duplex_->GetValue() ? presentation::Message::Duplex
                                    : presentation::Message::Simplex);
    });
    monitor_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        controller_->monitor(monitor_->GetValue());
        const auto& paths = controller_->snapshot().paths;
        const bool running = std::any_of(paths.begin(), paths.end(), [](const auto& path) {
            return path.state == audiohole::State::Running;
        });
        present(!monitor_->GetValue() ? presentation::Message::Cancelled
                : running             ? presentation::Message::Signal
                                      : presentation::Message::NoSignal);
        refresh();
    });
}
// Capability lists are authored fixtures, not results from IsFormatSupported or
// runtime stream initialization. Rebuilding must not silently erase a manual choice.
void Frame::populate_direction(DirectionWidgets& w, const audiohole::Direction& d) {
    const bool old_loading = loading_;
    loading_ = true;
    w.source->Clear();
    w.destination->Clear();
    w.sources.clear();
    w.destinations.clear();
    wxArrayString source_search, dest_search;
    for (const auto& e : controller_->snapshot().endpoints) {
        const auto caption =
            text(e.display_name + " [" + e.id + "] — " + audiohole::name(e.availability));
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
    w.source->AutoComplete(source_search);
    w.destination->AutoComplete(dest_search);
    const auto r = audiohole::recommend(controller_->snapshot(), d);
    w.formats = r.compatible;
    w.format->Clear();
    // Keep the literal branch a wxString too; mixed types are ambiguous on MSVC.
    w.format->Append(r.compatible.empty()
                         ? wxString(wxS("Automatic — unverified compatibility"))
                         : wxS("Recommended — ") + text(r.compatible.front().label()) + "");
    int selection = 0;
    for (const auto& f : r.compatible) {
        w.format->Append(text(f.label()) + "");
        if (!d.settings.automatic && f == d.settings.format)
            selection = static_cast<int>(w.format->GetCount()) - 1;
    }
    if (!d.settings.automatic && !selection) {
        w.formats.push_back(d.settings.format);
        w.format->Append(text(d.settings.format.label()) + " [UNSUPPORTED override retained]");
        selection = static_cast<int>(w.format->GetCount()) - 1;
    }
    w.format->SetSelection(selection);
    update_direction_details(w, d);
    w.evidence->SetLabel(text(r.explanation));
    w.evidence->Wrap(570);
    w.buffer->SetValue(d.settings.buffer_ms);
    w.acknowledged_buffer_ms = d.settings.buffer_ms;
    w.mode->SetStringSelection(text(d.settings.mode));
    loading_ = old_loading;
}
// Read-only display of requested settings and available endpoint records; never
// label these as negotiated runtime measurements or verified hardware evidence.
void Frame::update_direction_details(DirectionWidgets& w, const audiohole::Direction& d) {
    const auto r = audiohole::recommend(controller_->snapshot(), d);
    const auto selected = !d.settings.automatic   ? d.settings.format
                          : !r.compatible.empty() ? r.compatible.front()
                                                  : d.settings.format;
    const bool known = !d.settings.automatic || !r.compatible.empty();
    auto detail = [&](const std::string& id) {
        const auto* device = endpoint(controller_->snapshot(), id);
        return device ? std::string(name(device->availability)) + " | " +
                            (known ? "Selected: " + selected.label()
                                   : std::string("Format unavailable"))
                      : std::string("No endpoint selected");
    };
    w.source_detail->SetLabel(text(detail(d.source)));
    w.source_detail->Wrap(260);
    w.destination_detail->SetLabel(text(detail(d.destination)));
    w.destination_detail->Wrap(260);
    w.summary->SetLabel(text(
        std::string(d.settings.automatic ? "Automatic configuration: " : "Manual configuration: ") +
        selected.label() + " | Mode: " + d.settings.mode + " | Buffer: " +
        std::to_string(d.settings.buffer_ms) + " ms | Sync: unavailable; processing unavailable"));
    if (!known)
        w.summary->SetLabel("Automatic configuration: no compatible format established. Review "
                            "endpoint selection.");
    w.summary->Wrap(600);
}
// Only an explicit list selection becomes a logical binding. Free text used for
// searching is never guessed as a device identity; validation reports missing IDs.
void Frame::read_direction(DirectionWidgets& w, audiohole::Direction& d) {
    auto read_id = [](wxComboBox* box, const std::vector<std::string>& ids) {
        const int i = box->GetSelection();
        return i >= 0 && static_cast<std::size_t>(i) < ids.size() ? ids[static_cast<std::size_t>(i)]
                                                                  : std::string{};
    };
    d.source = read_id(w.source, w.sources);
    d.destination = read_id(w.destination, w.destinations);
    const int f = w.format->GetSelection();
    d.settings.automatic = f <= 0;
    if (f > 0 && static_cast<std::size_t>(f) <= w.formats.size())
        d.settings.format = w.formats[static_cast<std::size_t>(f - 1)];
    d.settings.buffer_ms = w.buffer->GetValue();
    d.settings.mode = utf8(w.mode->GetStringSelection());
}
void Frame::capture_draft() {
    if (loading_)
        return;
    draft_.display_name = utf8(profile_name_->GetValue());
    draft_.notes = utf8(profile_notes_->GetValue());
    if (hole_index_ < 0 || static_cast<std::size_t>(hole_index_) >= draft_.holes.size())
        return;
    auto& h = draft_.holes[static_cast<std::size_t>(hole_index_)];
    h.display_name = utf8(hole_name_->GetValue());
    h.notes = utf8(hole_notes_->GetValue());
    h.enabled = enabled_->GetValue();
    h.duplex = duplex_->GetValue();
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
    profile_name_->ChangeValue(text(draft_.display_name));
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
    if (startup_target_->GetCount())
        startup_target_->SetSelection(0);
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
        holes_->Append(text(caption + "\n" + h.forward.source + " → " + h.forward.destination));
    }
}
void Frame::load_hole(int index) {
    loading_ = true;
    end_observation(); // An observation never follows navigation to a different source implicitly.
    hole_index_ = index;
    const audiohole::Hole h =
        index >= 0 ? draft_.holes.at(static_cast<std::size_t>(index)) : audiohole::Hole{};
    holes_->SetSelection(index);
    hole_name_->ChangeValue(text(h.display_name));
    hole_notes_->ChangeValue(text(h.notes));
    enabled_->SetValue(h.enabled);
    duplex_->SetValue(h.duplex);
    activity_direction_->SetSelection(0);
    activity_direction_->Enable(index >= 0 && h.duplex);
    for (wxWindow* widget : std::vector<wxWindow*>{hole_name_, hole_notes_, enabled_, duplex_})
        widget->Enable(index >= 0);
    populate_direction(forward_, h.forward);
    populate_direction(reverse_, h.reverse);
    forward_.group->GetStaticBox()->Enable(index >= 0);
    reverse_.group->GetStaticBox()->Enable(index >= 0);
    reverse_.group->ShowItems(h.duplex && index >= 0);
    update_beep_destinations();
    loading_ = false;
    auto* editor = dynamic_cast<wxScrolledWindow*>(hole_name_->GetParent());
    editor->Layout();
    editor->FitInside();
}
void Frame::save(bool apply) {
    capture_draft();
    const auto r = controller_->save(draft_, apply);
    show_result(r);
    if (r.ok) {
        dirty_ = false;
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
    auto* observation = new wxStaticBoxSizer(wxVERTICAL, parent, wxS("Setup tools"));
    auto* box = observation->GetStaticBox();
    layout->Add(observation, 0, wxEXPAND | wxALL, 4);
    activity_direction_ = choices(box, observation, "Monitor direction",
                                  {"Forward source", "Reverse source (duplex only)"});
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
    label(box, observation, "Test Beep destination");
    destination_test_ = new wxChoice(box, wxID_ANY);
    observation->Add(destination_test_, 0, wxEXPAND | wxALL, 3);
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
    beep_button_->SetLabel("Cancel Beep");
    beep_button_->SetMinSize(beep_button_->GetBestSize());
    beep_button_->SetLabel("Test Beep");
    observation->Add(beep);
    beep_status_ = label(box, observation, "Test Beep inactive.");
    label(box, observation, "Latency / buffer occupancy / clock drift / CPU: unavailable.");
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
    button(simulation_box, failures, "Reverse failure", [this] {
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
                destination_test_->Append(
                    text(std::string(d == &hole.forward ? "Forward → " : "Reverse → ") +
                         e->display_name + " [" + e->id + "]"));
                beep_ids_.push_back(e->id);
            }
        }
}
} // namespace audiohole::gui
