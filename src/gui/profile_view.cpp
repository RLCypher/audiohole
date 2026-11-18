#include "gui/controls.h"
#include "gui/frame.h"
#include "model/latency_preferences.h"
#include "model/persistence.h"
#include "model/routing_service.h"
#include <cmath>
#include <commctrl.h>
#include <sstream>
#include <wx/access.h>
#include <wx/wrapsizer.h>

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
    if (control->GetTextValue().empty())
        return 0;
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
    std::array<std::string, 8> fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        auto raw = i == 3 ? w.buffer->GetTextValue() : w.timings[i]->GetValue();
        raw.Trim(true).Trim(false);
        fields[i] = i == 3 && raw == "0" ? std::string{} : utf8(raw);
    }
    TimingValues values;
    return parse_timing_values(fields, values).ok && set_timing_values(p, values).ok;
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
    wxWindow* box = w.group->GetStaticBox();
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
            tutorial_event("exclusive");
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
    auto* tuning_header = new wxBoxSizer(wxHORIZONTAL);
    tuning_header->Add(w.advanced_tuning, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(4));
    tuning_header->AddStretchSpacer();
    pane_layout->Add(tuning_header, 0, wxEXPAND);
    w.advanced_tuning->Bind(wxEVT_CHECKBOX, [this, &w](wxCommandEvent&) { tuning_changed(w); });
    auto* tuning = new wxStaticBoxSizer(wxVERTICAL, pane, title + " tuning");
    w.tuning_group = tuning;
    pane_layout->Add(tuning, 0, wxEXPAND);
    pane->SetSizer(pane_layout);
    w.tuning->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED,
                   [this, &w](wxCollapsiblePaneEvent&) { layout_direction(w); });
    auto changed_settings = [this, &w](unsigned section) {
        if (hole_index_ < 0)
            return;
        capture_draft();
        auto& d =
            &w == &forward_ ? draft_.holes[hole_index_].forward : draft_.holes[hole_index_].reverse;
        auto& p = d.settings.preferences;
        if (section == 3) {
            p = {};
            w.manual_format = false;
        }
        if (section == 0) {
            w.manual_format = false;
            for (auto* e : {&p.common, &p.source, &p.destination}) {
                e->sample_rate_hz.reset();
                e->channel_count.reset();
                e->sample_format.reset();
            }
            p.channel_map.clear();
        }
        if (section == 1)
            set_timing_values(p, {});
        if (section == 2) {
            p.common.mode.reset();
            p.source.mode.reset();
            p.destination.mode.reset();
            p.preferred_clock_device = "automatic";
        }
        d.settings.refresh_projection();
        constexpr DirectionFields fields[]{DirectionFields::Format, DirectionFields::Timing,
                                           DirectionFields::Routing, DirectionFields::All};
        populate_direction(w, d, fields[section]);
        dirty_ = true;
        refresh();
    };
    auto* reset_all = button(pane, tuning_header, "Reset all advanced tuning settings",
                             [changed_settings] { changed_settings(3); });
    reset_all->SetToolTip("Reset this direction's format, timing, access-mode and clock "
                          "preferences. Endpoint assignments stay selected.");
    constexpr const char* sections[]{"Audio Format", "Latency and Buffering", "Routing and Clock"};
    std::array<wxBoxSizer*, 3> contents{};
    for (unsigned i = 0; i < 3; ++i) {
        auto* disclosure =
            new wxCollapsiblePane(tuning->GetStaticBox(), wxID_ANY, sections[i], wxDefaultPosition,
                                  wxDefaultSize, wxCP_DEFAULT_STYLE | wxCP_NO_TLW_RESIZE);
        disclosure->SetName(title + " " + sections[i]);
        w.sections[i] = disclosure;
        tuning->Add(disclosure, 0, wxEXPAND | wxALL, FromDIP(3));
        contents[i] = new wxBoxSizer(wxVERTICAL);
        disclosure->GetPane()->SetSizer(contents[i]);
        w.section_summaries[i] =
            new wxStaticText(disclosure, wxID_ANY, "Auto", wxDefaultPosition, wxDefaultSize,
                             wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
        w.section_summaries[i]->SetMinSize(wxSize(1, -1));
        disclosure->Bind(wxEVT_SIZE, [this, &w, i, disclosure](wxSizeEvent& e) {
            auto* summary = w.section_summaries[i];
            const int width = std::min(FromDIP(200), disclosure->GetClientSize().x / 3);
            summary->SetSize(disclosure->GetClientSize().x - width - FromDIP(8), FromDIP(2), width,
                             summary->GetCharHeight() + FromDIP(4));
            e.Skip();
        });
        disclosure->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED,
                         [this, &w](wxCollapsiblePaneEvent&) { layout_direction(w); });
    }
    box = w.sections[0]->GetPane();
    auto* format_layout = contents[0];
    label(box, format_layout, "Complete format preset (optional)");
    w.format = new ThemedChoice(box, wxID_ANY);
    format_layout->Add(w.format, 0, wxEXPAND | wxALL, 3);
    auto* format_grid = new wxGridSizer(0, 4, FromDIP(6), FromDIP(8));
    const auto scalar = [this, &title, &w](wxWindow* parent, wxSizer* grid, const char* caption,
                                           bool inherit) {
        auto* column = new wxBoxSizer(wxVERTICAL);
        label(parent, column, text(caption));
        auto* field = new wxTextCtrl(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                     wxTE_PROCESS_ENTER);
        field->SetMinSize(wxSize(1, -1));
        match_input_height(*field);
        field->SetName(title + " " + text(caption));
        field->SetHint(inherit ? "Inherit" : "Auto");
        field->Bind(wxEVT_TEXT, [this, &w, field](wxCommandEvent&) {
            if (!loading_) {
                dirty_ = true;
                if (field == w.rate || field == w.channels || field == w.source_channels ||
                    field == w.destination_channels || field == w.channel_map) {
                    w.manual_format = true;
                    w.format->SetSelection(1);
                }
            }
        });
        column->Add(field, 0, wxEXPAND | wxALL, 3);
        grid->Add(column, 1, wxEXPAND);
        return field;
    };
    w.rate = scalar(box, format_grid, "Sample rate (Hz)", false);
    w.channels = scalar(box, format_grid, "Stream channels", false);
    w.source_channels = scalar(box, format_grid, "Source channels", true);
    w.destination_channels = scalar(box, format_grid, "Destination channels", true);
    format_layout->Add(format_grid, 0, wxEXPAND);
    auto* map_grid = new wxGridSizer(1, 2, FromDIP(8), FromDIP(8));
    auto* sample = new wxBoxSizer(wxVERTICAL);
    w.representation =
        choices(box, sample, "Sample format",
                {"Automatic", "float32", "pcm_s16", "pcm_s24_packed", "pcm_s24_in_32", "pcm_s32"});
    w.representation->SetMinSize(wxSize(1, -1));
    map_grid->Add(sample, 1, wxEXPAND);
    w.channel_map = scalar(box, map_grid, "Channel map (source:destination)", false);
    w.channel_map->SetToolTip("One-based channel indexes, for example 1:3, 2:4. Auto maps matching "
                              "indexes; extra outputs are silent.");
    format_layout->Add(map_grid, 0, wxEXPAND);
    button(box, format_layout, "Reset Format", [changed_settings] {
        changed_settings(0);
    })->SetToolTip("Restore Automatic format and channel mapping in this direction.");
    box = w.sections[1]->GetPane();
    auto* timing_layout = contents[1];
    auto* timing_grid = new wxGridSizer(0, 4, FromDIP(6), FromDIP(8));
    constexpr const char* timing_labels[]{
        "Routing audio queue",   "Playback audio queue",      "Default processing interval",
        "Default device buffer", "Input processing interval", "Output processing interval",
        "Input device buffer",   "Output device buffer"};
    for (unsigned i = 0; i < 8; ++i) {
        if (i != 3) {
            w.timings[i] = scalar(box, timing_grid, timing_labels[i], i >= 4);
            w.timings[i]->Bind(wxEVT_TEXT_ENTER,
                               [this](wxCommandEvent&) { queue_field_save(false); });
            w.timings[i]->SetToolTip(
                i >= 6   ? "Whole milliseconds, 1–100. Clear to inherit the default device buffer."
                : i >= 4 ? "Positive milliseconds; fractions allowed. Clear to inherit the default "
                           "processing interval."
                         : "Positive milliseconds; fractions allowed. Clear for Automatic.");
            continue;
        }
        auto* column = new wxBoxSizer(wxVERTICAL);
        label(box, column, timing_labels[i]);
        auto* buffer_row = new wxBoxSizer(wxHORIZONTAL);
        w.buffer = new wxSpinCtrl(box, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 100, 0);
        const int height = input_height(*w.buffer);
        w.buffer->SetMinSize(wxSize(1, height));
        match_input_height(*w.buffer);
        w.buffer->SetName(title + " Default device buffer");
        SendMessageW(static_cast<HWND>(w.buffer->GetHandle()), EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Auto"));
        buffer_row->Add(w.buffer, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
        w.buffer_apply = button(box, buffer_row, "Apply", [this, &w] { apply_buffer(w); });
        w.buffer_apply->SetMinSize(wxSize(-1, height));
        w.buffer_apply->SetMaxSize(wxSize(-1, height));
        w.buffer_apply->SetToolTip(button_help("Apply"));
        w.buffer_apply->Disable();
        column->Add(buffer_row, 0, wxEXPAND);
        timing_grid->Add(column, 1, wxEXPAND);
    }
    timing_layout->Add(timing_grid, 0, wxEXPAND);
    auto* buffer_actions = new wxWrapSizer(wxHORIZONTAL);
    auto* defaults = button(box, buffer_actions, "Load safe defaults", [this, &w] {
        auto menu = std::make_unique<wxMenu>();
        constexpr const char* names[]{"Responsive", "Balanced", "Stable", "Conservative"};
        for (unsigned i = 0; i < 4; ++i) {
            const auto id = wxWindow::NewControlId();
            menu->Append(id, names[i]);
            if (i == 1)
                SetMenuDefaultItem(reinterpret_cast<HMENU>(menu->GetHMenu()), static_cast<UINT>(id),
                                   FALSE);
            menu->Bind(
                wxEVT_MENU,
                [this, &w, i](wxCommandEvent&) {
                    if (hole_index_ < 0)
                        return;
                    capture_draft();
                    auto& d = &w == &forward_ ? draft_.holes[hole_index_].forward
                                              : draft_.holes[hole_index_].reverse;
                    const auto& s = controller_->snapshot();
                    auto values = timing_preset(i, endpoint(s, d.source),
                                                endpoint(s, d.destination), d.settings.preferences);
                    // A current native receipt can validate a lead preset only for
                    // the same endpoint, format, capacity request and period.
                    if (s.routing_status && s.routing_intent && s.routing_intent->id == draft_.id) {
                        const auto saved = std::find_if(
                            s.routing_intent->holes.begin(), s.routing_intent->holes.end(),
                            [&](const auto& h) { return h.id == draft_.holes[hole_index_].id; });
                        if (saved != s.routing_intent->holes.end()) {
                            const auto& original =
                                &w == &forward_ ? saved->forward : saved->reverse;
                            const auto effective = inherit_preferences(
                                d.settings.preferences.common, d.settings.preferences.destination);
                            for (const auto& path : s.routing_status->paths) {
                                if (path.hole_id != saved->id ||
                                    (path.direction == engine::DirectionId::Reverse) !=
                                        (&w == &reverse_) ||
                                    !path.negotiated || original.destination != d.destination)
                                    continue;
                                const auto& n = path.negotiated->destination;
                                if (n.mode != engine::AccessMode::Shared ||
                                    !n.allocated_buffer_frames || !n.timing_sample_rate_hz ||
                                    !n.period_frames || !values[5] ||
                                    n.applied_buffer_request_ms != effective.buffer_ms ||
                                    (effective.sample_rate_hz &&
                                     *effective.sample_rate_hz != n.client_format.sample_rate_hz) ||
                                    (effective.channel_count &&
                                     *effective.channel_count != n.client_format.channel_count) ||
                                    (effective.sample_format &&
                                     *effective.sample_format !=
                                         engine::name(n.client_format.sample_format)) ||
                                    std::abs(*values[5] - 1000.0 * *n.period_frames /
                                                              *n.timing_sample_rate_hz) > 0.001)
                                    continue;
                                values = timing_preset(
                                    i, endpoint(s, d.source), endpoint(s, d.destination),
                                    d.settings.preferences,
                                    1000.0 * *n.allocated_buffer_frames / *n.timing_sample_rate_hz);
                            }
                        }
                    }
                    const auto result = set_timing_values(d.settings.preferences, values);
                    if (!result.ok) {
                        show_result(result);
                        return;
                    }
                    d.settings.refresh_projection();
                    populate_direction(w, d, DirectionFields::Timing);
                    dirty_ = true;
                    refresh();
                },
                id);
        }
        w.sections[1]->PopupMenu(menu.get());
    });
    defaults->SetToolTip(
        "Starting requests for this direction, validated on Start. Unknown or incompatible device "
        "periods remain Auto/Inherit; presets are not measured stability guarantees.");
    defaults->SetToolTip("Uses matching scanner evidence for processing intervals and queue "
                         "sizing. Unknown periods stay Auto. Playback lead stays Auto until "
                         "capacity is confirmed; no device allocation is guessed.");
    auto* copy = button(box, buffer_actions, "Copy buffer values", [this, &w] {
        Preferences p;
        if (!read_preferences(w, p)) {
            show_result({false, "Correct the timing values before copying."});
            return;
        }
        if (!wxTheClipboard->Open()) {
            show_result({false, "Could not open the clipboard."});
            return;
        }
        const bool ok = wxTheClipboard->SetData(
            new wxTextDataObject(text(encode_timing_clipboard(timing_values(p)))));
        wxTheClipboard->Close();
        if (!ok)
            show_result({false, "Could not copy buffer values."});
    });
    copy->SetToolTip("Copy only these eight timing/buffer values, including Auto/Inherit choices.");
    auto* paste = button(box, buffer_actions, "Paste buffer values", [this, &w] {
        wxTextDataObject data;
        if (!wxTheClipboard->Open()) {
            show_result({false, "Could not open the clipboard."});
            return;
        }
        const bool got = wxTheClipboard->GetData(data);
        wxTheClipboard->Close();
        TimingValues values{};
        const auto result = got ? decode_timing_clipboard(utf8(data.GetText()), values)
                                : Result{false, "No buffer settings on the clipboard."};
        if (!result.ok) {
            show_result(result);
            return;
        }
        if (hole_index_ < 0)
            return;
        capture_draft();
        auto& d =
            &w == &forward_ ? draft_.holes[hole_index_].forward : draft_.holes[hole_index_].reverse;
        const auto valid = set_timing_values(d.settings.preferences, values);
        if (!valid.ok) {
            show_result(valid);
            return;
        }
        d.settings.refresh_projection();
        populate_direction(w, d, DirectionFields::Timing);
        dirty_ = true;
        refresh();
    });
    paste->SetToolTip("Paste only the eight timing/buffer values into this direction. Other "
                      "settings stay selected.");
    button(box, buffer_actions, "Reset Buffers", [changed_settings] {
        changed_settings(1);
    })->SetToolTip("Restore Automatic/inherited timing values in this direction.");
    timing_layout->Add(buffer_actions, 0, wxEXPAND);
    label(box, timing_layout,
          "All values in milliseconds (ms). Auto chooses automatically; Inherit uses the default. "
          "Allocated capacity may be rounded upward; queue and lead targets are validated "
          "independently.")
        ->Wrap(FromDIP(520));
    w.buffer_state = label(box, timing_layout, "");
    w.buffer_state->SetMinSize(wxSize(1, -1));
    w.buffer->SetToolTip(
        "Requested endpoint capacity in whole milliseconds, 1–100; clear or enter 0 for Automatic. "
        "Drivers may round capacity upward. This does not set queue occupancy or playback lead.");
    box = w.sections[2]->GetPane();
    auto* routing = contents[2];
    auto* routing_grid = new wxGridSizer(1, 2, FromDIP(8), FromDIP(8));
    auto* mode_column = new wxBoxSizer(wxVERTICAL);
    auto* clock_column = new wxBoxSizer(wxVERTICAL);
    w.mode = choices(box, mode_column, "Operating mode",
                     {"Automatic", "Force exclusive mode", "Force shared mode"});
    w.clock =
        choices(box, clock_column, "Placebo clock device", {"Automatic", "Capture", "Playback"});
    w.clock->SetMinSize(
        wxSize(1, std::max(w.mode->GetBestSize().y, w.clock->GetCharHeight() + FromDIP(12))));
    w.mode->SetMinSize(wxSize(1, -1));
    w.clock->SetToolTip(
        "Saved for future compatibility. Currently has no effect on routing or latency.");
    routing_grid->Add(mode_column, 1, wxEXPAND);
    routing_grid->Add(clock_column, 1, wxEXPAND);
    routing->Add(routing_grid, 0, wxEXPAND);
    button(box, routing, "Reset Routing and Clock", [changed_settings] {
        changed_settings(2);
    })->SetToolTip("Restore Automatic routing mode and clock metadata in this direction.");
    w.clock->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (!loading_) {
            capture_draft();
            dirty_ = true;
            refresh();
        }
    });
    for (const auto& [panel, grid] :
         {std::pair{w.sections[0], format_grid}, std::pair{w.sections[1], timing_grid}})
        panel->Bind(wxEVT_SIZE, [this, &w, panel, grid](wxSizeEvent& e) {
            const int columns = panel->GetClientSize().x < FromDIP(740) ? 2 : 4;
            if (grid->GetCols() != columns) {
                grid->SetCols(columns);
                panel->GetPane()->Layout();
            }
            e.Skip();
        });
    w.format->SetToolTip(
        "Choose Automatic, retain typed format values with Manual, or select a "
        "complete format candidate. Timing and explicit channel maps stay selected.");
    w.source->SetToolTip("Choose a listed capture endpoint for this direction. Use arrow keys or "
                         "type a name to select a matching list item.");
    w.destination->SetToolTip("Choose the playback endpoint for this direction. The Windows name "
                              "is shown for the selected device.");
    w.mode->SetToolTip(
        "Save an access-mode choice for this direction. Use advanced tuning must be "
        "enabled for it to apply. Forced modes take precedence over Exclusive mode "
        "preferred. Automatic prefers exclusive access when checked, otherwise shared access.");
    w.representation->Bind(wxEVT_CHOICE, [this, &w](wxCommandEvent&) {
        if (!loading_) {
            dirty_ = true;
            w.manual_format = true;
            w.format->SetSelection(1);
        }
    });
    auto changed = [this, &w](bool source) {
        if (loading_ || hole_index_ < 0)
            return;
        auto* control = static_cast<ThemedCombo*>(source ? w.source : w.destination);
        const auto& ids = source ? w.sources : w.destinations;
        const int index = control->GetSelection();
        if (index >= 0 && !control->item_enabled(static_cast<unsigned>(index))) {
            const auto& hole = draft_.holes[hole_index_];
            populate_direction(w, &w == &forward_ ? hole.forward : hole.reverse,
                               DirectionFields::Preserve);
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
        populate_direction(forward_, hole.forward, DirectionFields::Preserve);
        populate_direction(reverse_, hole.reverse, DirectionFields::Preserve);
        dirty_ = true;
        tutorial_event("endpoints");
        present(source ? presentation::Message::Source : presentation::Message::Destination);
        refresh();
    };
    w.source->Bind(wxEVT_COMBOBOX, [changed](wxCommandEvent&) { changed(true); });
    w.destination->Bind(wxEVT_COMBOBOX, [changed](wxCommandEvent&) { changed(false); });
    w.format->Bind(wxEVT_CHOICE, [this, &w](wxCommandEvent&) {
        if (loading_)
            return;
        const auto selected = w.format->GetSelection();
        w.manual_format = selected == 1;
        if (selected == 1) {
            dirty_ = true;
            return;
        }
        if (selected > 1 && static_cast<std::size_t>(selected - 2) < w.formats.size()) {
            const auto& candidate = w.formats[selected - 2];
            const auto& f = candidate.source;
            w.rate->ChangeValue(std::to_string(f.rate));
            w.channels->ChangeValue(f.channels == candidate.destination.channels
                                        ? std::to_string(f.channels)
                                        : std::string{});
            w.source_channels->ChangeValue(f.channels == candidate.destination.channels
                                               ? std::string{}
                                               : std::to_string(f.channels));
            w.destination_channels->ChangeValue(
                f.channels == candidate.destination.channels
                    ? std::string{}
                    : std::to_string(candidate.destination.channels));
            auto representation =
                f.representation == "PCM16" ? std::string("pcm_s16") : f.representation;
            w.representation->SetStringSelection(text(representation));
        } else {
            w.rate->ChangeValue("");
            w.channels->ChangeValue("");
            w.source_channels->ChangeValue("");
            w.destination_channels->ChangeValue("");
            w.representation->SetSelection(0);
        }
        dirty_ = true;
    });
    w.mode->Bind(wxEVT_CHOICE, [this, &w](wxCommandEvent&) {
        if (loading_ || hole_index_ < 0)
            return;
        capture_draft();
        const auto& hole = draft_.holes.at(static_cast<std::size_t>(hole_index_));
        populate_direction(w, &w == &forward_ ? hole.forward : hole.reverse,
                           DirectionFields::Preserve);
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
    populate_direction(widgets, direction, DirectionFields::Preserve);
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
    // All Profile-tab Apply buttons save and apply the entire editor draft.
    if (!loading_ && widgets.buffer_apply->IsEnabled())
        save(true);
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
                auto chosen = dialog.GetValue();
                chosen.Trim(true).Trim(false);
                rename_profile(utf8(chosen));
                if (!chosen.empty() && draft_.display_name == utf8(chosen))
                    save(false);
                tutorial_event("profile_renamed");
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
            if (result.ok) {
                if (plug_board_)
                    plug_board_->profile_deleted(viewed_);
                select_profile(controller_->snapshot().profiles.empty()
                                   ? ""
                                   : controller_->snapshot().profiles.front().id);
            }
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
        if (index != hole_index_) {
            if (!validate_profile_inputs())
                return false;
            load_hole(index);
        }
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
                auto chosen = dialog.GetValue();
                chosen.Trim(true).Trim(false);
                rename_hole(utf8(chosen));
                if (hole_index_ >= 0 &&
                    draft_.holes.at(static_cast<std::size_t>(hole_index_)).display_name ==
                        utf8(chosen) &&
                    !chosen.empty())
                    save(false);
                tutorial_event("hole_renamed");
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
    if (viewed_.empty() || !validate_profile_inputs())
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
    tutorial_event("hole_created");
    refresh();
}
void Frame::duplicate_hole() {
    if (hole_index_ < 0 || !validate_profile_inputs())
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
    tutorial_event("hole_deleted");
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
    if (controller_->snapshot().operating_mode == OperatingMode::Test &&
        !controller_->snapshot().capabilities.real_engine)
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
            else {
                select_profile(id);
                tutorial_event("profile_created");
            }
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
    for (auto* rows : {profile_rows_, profile_lower_rows_})
        rows->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
            constrain_profile_rows();
            event.Skip();
        });
    profile_rows_->Bind(wxEVT_SPLITTER_SASH_POS_CHANGING, [this](wxSplitterEvent& event) {
        const int low = profile_rows_->GetMinimumPaneSize();
        const int high = profile_rows_->GetClientSize().y - profile_rows_->GetSashSize() -
                         profile_lower_rows_->GetMinSize().y;
        if (high >= low)
            event.SetSashPosition(std::clamp(event.GetSashPosition(), low, high));
    });
    sidebar_layout->Add(profile_rows_, 1, wxEXPAND);
    sidebar->SetSizer(sidebar_layout);
    auto* right_container = new wxSplitterWindow(profile_columns_, wxID_ANY, wxDefaultPosition,
                                                 wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    performance_split_ = right_container;
    right_container->SetName("Resize performance diagnostics height");
    right_container->SetMinimumPaneSize(FromDIP(48));
    right_container->SetSashGravity(1.0);
    auto* editor = new wxScrolledWindow(right_container);
    profile_editor_ = editor;
    editor->Bind(wxEVT_CHILD_FOCUS, [this](wxChildFocusEvent& event) {
        if (!tutorial_scroll_duration_)
            event.Skip();
    });

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
            if (hole_index_ >= 0) {
                const auto& hole = draft_.holes[hole_index_];
                populate_direction(forward_, hole.forward, DirectionFields::Preserve);
                populate_direction(reverse_, hole.reverse, DirectionFields::Preserve);
            }
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
          "Counters and allocated buffers are observations; acoustic latency and CPU are "
          "unmeasured.");
    path_status_ = new wxTextCtrl(performance->GetStaticBox(), wxID_ANY, "No active directions.",
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    path_status_->SetName("Routing performance observations");
    path_status_->SetMinSize(wxSize(0, path_status_->GetCharHeight() * 2 + FromDIP(12)));
    performance->Add(path_status_, 1, wxEXPAND | wxALL, FromDIP(4));
    right_container->SplitHorizontally(editor, performance_panel_, -FromDIP(140));
    page_layout->Add(profile_columns_, 1, wxEXPAND | wxALL, 3);
    page->SetSizer(page_layout);
    notebook_->AddPage(page, "Profiles");
    enable_field_wheel_chaining(*page);
    profiles_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        const int i = profiles_->GetSelection();
        if (i >= 0 && discard()) {
            const auto id = profile_ids_.at(static_cast<std::size_t>(i));
            const auto& state = controller_->snapshot();
            const bool switch_now = preferences_->values().activate_selected_profile &&
                                    !state.capabilities.real_engine && state.capabilities.routing &&
                                    state.armed && state.active_profile != id;
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
        if (!validate_profile_inputs()) {
            holes_->SetSelection(hole_index_);
            return;
        }
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
        if (!duplex_->GetValue()) {
            // Do not hide an invalid Return edit and later save its old value.
            duplex_->SetValue(true);
            if (!validate_profile_inputs())
                return;
            duplex_->SetValue(false);
        }
        capture_draft();
        const auto& hole = draft_.holes[hole_index_];
        populate_direction(forward_, hole.forward, DirectionFields::Preserve);
        populate_direction(reverse_, hole.reverse, DirectionFields::Preserve);
        load_holes();
        holes_->SetSelection(hole_index_);
        dirty_ = true;
        end_observation();
        reverse_.group->ShowItems(duplex_->GetValue());
        reverse_.tuning_group->ShowItems(duplex_->GetValue());
        editor->FitInside();
        editor->Layout();
        tutorial_event("return");
        present(duplex_->GetValue() ? presentation::Message::Duplex
                                    : presentation::Message::Simplex);
    });
}
// Test uses exact worker-projected scanner tuples; Preview uses authored fixtures.
// Rebuilding must not silently erase a manual choice or invent runtime evidence.
void Frame::populate_direction(DirectionWidgets& w, const audiohole::Direction& d,
                               DirectionFields load_settings) {
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
    static_cast<ThemedCombo*>(w.source)->retained_caption.clear();
    static_cast<ThemedCombo*>(w.destination)->retained_caption.clear();
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
        static_cast<ThemedCombo*>(w.source)->retained_caption =
            retained_caption(d.source, d.source_name);
    if (w.destination->GetSelection() == wxNOT_FOUND && !d.destination.empty())
        static_cast<ThemedCombo*>(w.destination)->retained_caption =
            retained_caption(d.destination, d.destination_name);
    w.retained_source_caption = w.source->GetValue();
    w.retained_destination_caption = w.destination->GetValue();
    w.source->Refresh();
    w.destination->Refresh();
    const auto r =
        assess_formats(controller_->snapshot(), d,
                       hole_index_ >= 0 ? draft_.holes[hole_index_].routing_engine : "automatic");
    w.formats = r.candidates;
    w.format->Clear();
    // Keep the literal branch a wxString too; mixed types are ambiguous on MSVC.
    w.format->Append(r.candidates.empty()
                         ? wxString(wxS("Automatic — unverified compatibility"))
                         : wxS("Automatic — ") + text(r.candidates.front().label()));
    w.format->Append("Manual");
    const auto& p = d.settings.preferences;
    const bool format_overrides =
        p.common.sample_rate_hz || p.common.channel_count || p.common.sample_format ||
        p.source.sample_rate_hz || p.source.channel_count || p.source.sample_format ||
        p.destination.sample_rate_hz || p.destination.channel_count || p.destination.sample_format;
    int selection = format_overrides || w.manual_format ? 1 : 0;
    for (const auto& candidate : r.candidates) {
        const auto& f = candidate.source;
        w.format->Append(text(candidate.label()));
        if (!w.manual_format && p.common.sample_rate_hz == f.rate &&
            p.common.channel_count == f.channels && candidate.destination.channels == f.channels &&
            p.common.sample_format == f.representation)
            selection = static_cast<int>(w.format->GetCount()) - 1;
    }
    w.format->SetSelection(selection);
    update_direction_details(w, d);
    // Inventory, endpoint, engine and mode refreshes update choices/details,
    // retaining raw edits (including incomplete ones) until explicit Save/load.
    if (load_settings == DirectionFields::All || load_settings == DirectionFields::Format) {
        const auto& preference = d.settings.preferences;
        w.rate->ChangeValue(preference.common.sample_rate_hz
                                ? std::to_string(*preference.common.sample_rate_hz)
                                : "");
        w.channels->ChangeValue(preference.common.channel_count
                                    ? std::to_string(*preference.common.channel_count)
                                    : "");
        w.source_channels->ChangeValue(preference.source.channel_count
                                           ? std::to_string(*preference.source.channel_count)
                                           : "");
        w.destination_channels->ChangeValue(
            preference.destination.channel_count
                ? std::to_string(*preference.destination.channel_count)
                : "");
        w.representation->SetStringSelection(
            text(preference.common.sample_format.value_or("Automatic")));
        std::string mapping;
        for (const auto& route : preference.channel_map) {
            if (!mapping.empty())
                mapping += ", ";
            mapping +=
                std::to_string(route.source + 1) + ":" + std::to_string(route.destination + 1);
        }
        w.channel_map->ChangeValue(text(mapping));
    }
    const auto& preference = d.settings.preferences;
    if (load_settings == DirectionFields::All || load_settings == DirectionFields::Timing) {
        w.buffer->SetValue(d.settings.preferences.common.buffer_ms
                               ? wxString::Format("%d", *d.settings.preferences.common.buffer_ms)
                               : wxString{});
        const auto values = timing_values(preference);
        for (unsigned i = 0; i < 8; ++i)
            if (i != 3)
                w.timings[i]->ChangeValue(values[i] ? wxString::Format("%.12g", *values[i])
                                                    : wxString{});
        w.acknowledged_buffer_ms = d.settings.buffer_ms;
    }
    if (load_settings == DirectionFields::All || load_settings == DirectionFields::Routing) {
        const auto mode = d.settings.preferences.common.mode.value_or("Automatic");
        w.mode->SetSelection(mode == "Exclusive" ? 1 : mode == "Shared" ? 2 : 0);
        const auto clock = d.settings.preferences.preferred_clock_device;
        w.clock->SetSelection(clock == "capture" ? 1 : clock == "playback" ? 2 : 0);
    }
    loading_ = old_loading;
    if (!loading_)
        layout_direction(w);
}
// Read-only display of requested settings and available endpoint records; never
// label these as negotiated runtime measurements or verified hardware evidence.
void Frame::update_direction_details(DirectionWidgets& w, const audiohole::Direction& d) {
    const auto values = timing_values(d.settings.preferences);
    const auto count =
        std::count_if(values.begin(), values.end(), [](const auto& n) { return n.has_value(); });
    const auto format = derive_pcm_format(d.settings.preferences.common);
    const std::array<wxString, 3> summaries{
        format ? text(format->label()) : wxString("Auto / Inherit"),
        text(std::to_string(count) + (count == 1 ? " override" : " overrides")),
        text(d.settings.preferences.common.mode.value_or("Automatic"))};
    for (unsigned i = 0; i < 3; ++i)
        if (w.section_summaries[i] && w.section_summaries[i]->GetLabel() != summaries[i]) {
            w.section_summaries[i]->SetLabel(summaries[i]);
            w.section_summaries[i]->SetToolTip(summaries[i]);
        }
    const auto& snapshot = controller_->snapshot();
    std::string saved = "unsaved";
    const bool reverse = &w == &reverse_;
    if (const auto* stored = profile(snapshot, viewed_)) {
        const auto selected =
            hole_index_ >= 0 && static_cast<std::size_t>(hole_index_) < draft_.holes.size()
                ? draft_.holes[static_cast<std::size_t>(hole_index_)].id
                : std::string{};
        const auto hole = std::find_if(stored->holes.begin(), stored->holes.end(),
                                       [&](const auto& record) { return record.id == selected; });
        if (hole != stored->holes.end() && (!reverse || hole->duplex)) {
            saved = buffer_caption((reverse ? hole->reverse : hole->forward).settings.buffer_ms);
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
    const auto parsed = buffer_value(w.buffer);
    w.buffer_apply->Enable(profile_apply_->IsEnabled());
    const wxString buffer_help = button_help("Apply");
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
                  : (snapshot.capabilities.real_engine
                         ? "Playback applied buffer request: " + value(confirmed) +
                               "; Save stages edits, Apply restarts routing"
                         : "Engine requested/applied: unavailable"))));
    auto effective = d;
    const bool advanced = w.advanced_tuning->GetValue();
    if (!advanced)
        effective.settings = Settings{};
    const auto recommendation =
        assess_formats(snapshot, effective,
                       hole_index_ >= 0 ? draft_.holes[hole_index_].routing_engine : "automatic");
    const bool compatible = !recommendation.candidates.empty();
    auto detail = [&](const std::string& id, bool capture) {
        const auto* device = endpoint(snapshot, id);
        const auto candidate = compatible ? &recommendation.candidates.front() : nullptr;
        const auto description = candidate ? "Candidate: " + candidate->endpoint_label(capture)
                                           : recommendation.explanation;
        return device ? std::string(name(device->availability)) + " | " + description
                      : std::string("No endpoint selected");
    };
    const engine::PathSnapshot* observed = nullptr;
    if (snapshot.routing_status && snapshot.routing_intent && snapshot.active_profile == viewed_) {
        for (const auto& hole : snapshot.routing_intent->holes) {
            const auto& original = reverse ? hole.reverse : hole.forward;
            if (hole.id != selected_hole || original.source != d.source ||
                original.destination != d.destination)
                continue;
            for (const auto& actual : snapshot.routing_status->paths)
                if (actual.hole_id == selected_hole &&
                    (actual.direction == engine::DirectionId::Reverse) == reverse &&
                    actual.negotiated)
                    observed = &actual;
        }
    }
    auto observed_detail = [&](const engine::StreamConfiguration& stream) {
        const auto& format = stream.client_format;
        return std::string(observed->state == engine::State::Stopped ? "Last opened: "
                                                                     : "Opened: ") +
               std::to_string(format.sample_rate_hz) + " Hz / " +
               std::to_string(format.channel_count) + " ch / " +
               engine::name(format.sample_format) + " | " + engine::name(stream.mode) + " | " +
               engine::name(observed->state);
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
    const bool source_changed = update_detail(
        w.source_detail, w.source_detail_copy,
        text(observed ? observed_detail(observed->negotiated->source) : detail(d.source, true)));
    const bool destination_changed =
        update_detail(w.destination_detail, w.destination_detail_copy,
                      text(observed ? observed_detail(observed->negotiated->destination)
                                    : detail(d.destination, false)));
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
    if (observed)
        summary = "Opened endpoint formats are shown above. Save stages edits; Apply saves and "
                  "restarts routing. " +
                  summary;
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
    widgets.tuning->GetPane()->Layout();
    widgets.tuning->InvalidateBestSize();
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
    forward_.manual_format = reverse_.manual_format = false;
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
bool Frame::validate_profile_inputs() {
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
        return false;
    }
    if (hole_index_ >= 0) {
        for (auto* widgets : {&forward_, &reverse_}) {
            if (widgets == &reverse_ && !duplex_->GetValue())
                continue;
            auto preference = (widgets == &forward_ ? draft_.holes[hole_index_].forward
                                                    : draft_.holes[hole_index_].reverse)
                                  .settings.preferences;
            if (!buffer_value(widgets->buffer) || !read_preferences(*widgets, preference)) {
                show_result({false, std::string(widgets == &forward_ ? "Forward" : "Return") +
                                        ": check the rate, channel counts, channel map and "
                                        "latency/buffer values before saving. Clear a field "
                                        "for Auto/Inherit. Your edits have been retained."});
                return false;
            }
        }
    }
    return true;
}
void Frame::save(bool apply) {
    tuning_timer_.Stop();
    if (!validate_profile_inputs())
        return;
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
        tutorial_event("saved");
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
    if (tutorial_ && !tutorial_->complete() &&
        (tutorial_->beat() != "N026" || !tutorial_->ready_to_route(draft_, hole_index_) ||
         id != draft_.id)) {
        tutorial_->remind(tutorial_->beat() == "N028" ? "B008" : "B007");
        render_tutorial();
        return;
    }
    const auto& snapshot = controller_->snapshot();
    const auto* selected = profile(snapshot, id);
    if (snapshot.capabilities.real_engine &&
        (snapshot.armed || snapshot.routing_pending || snapshot.state == State::Stopping)) {
        show_result({false, "Stop routing and wait for cleanup before starting another profile."});
        return;
    }
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
    const auto activation = controller_->activate(id, ready);
    show_result(activation);
    if (activation.ok)
        tutorial_event("routing_started");
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
// An outer splitter must reserve room for BOTH nested panes, not just its
// generic minimum for one child. Clamp immediately on resize and user drags.
// Restore only view geometry; never reload records or discard pending edits.
void Frame::reset_layout() {
    for (auto* direction : {&forward_, &reverse_}) {
        for (auto* section : direction->sections)
            section->Collapse();
        direction->tuning->Collapse();
        layout_direction(*direction);
    }
    if (auto* deep =
            dynamic_cast<wxCollapsiblePane*>(wxWindow::FindWindowByName("Deep scan limits", this)))
        deep->Collapse();
    for (auto* page : notebook_->GetChildren())
        if (auto* scroll = dynamic_cast<wxScrolledWindow*>(page)) {
            scroll->Layout();
            scroll->FitInside();
            scroll->Scroll(0, 0);
        }
    profile_editor_->Scroll(0, 0);
    Layout();
    // Settle parents before positioning nested dividers. Resizing a parent after
    // its child applies sash gravity again and changes the requested default.
    profile_columns_->SetSashPosition(FromDIP(300));
    profile_columns_->UpdateSize();
    profile_columns_->GetWindow1()->Layout();
    constrain_profile_rows();
    profile_rows_->SetSashPosition(profile_rows_->GetClientSize().y / 3);
    profile_rows_->UpdateSize();
    auto size_bottom_field = [](wxSplitterWindow* split, wxTextCtrl* field, int lines) {
        split->UpdateSize();
        split->GetWindow2()->Layout();
        const int frame = field->GetSize().y - field->GetClientSize().y;
        const int client = std::max(lines * field->GetCharHeight() + field->FromDIP(8),
                                    field->GetMinSize().y - frame);
        const int surrounding = split->GetWindow2()->GetSize().y - field->GetClientSize().y;
        split->SetSashPosition(split->GetClientSize().y - client - surrounding -
                               split->GetSashSize() - split->GetBorderSize());
        split->UpdateSize();
    };
    size_bottom_field(profile_lower_rows_, profile_notes_, 4);
    device_columns_->SetSashPosition(device_bundle_actions_->GetMinSize().x - FromDIP(4));
    device_columns_->UpdateSize();
    device_columns_->GetWindow2()->Layout();
    size_bottom_field(device_rows_, device_notes_, 3);
    size_bottom_field(performance_split_, path_status_, 2);
    scan_output_split_->SetSashPosition(scan_state_->GetCharHeight() * 3 + FromDIP(16));
    scan_output_split_->UpdateSize();
    for (auto* split : {profile_columns_, profile_rows_, profile_lower_rows_, performance_split_,
                        device_columns_, device_rows_, scan_output_split_}) {
        split->Refresh();
    }
}
void Frame::constrain_profile_rows() {
    if (!profile_rows_ || !profile_lower_rows_ || !profile_rows_->IsSplit() ||
        !profile_lower_rows_->IsSplit())
        return;
    auto minimum = [](wxWindow* window, int fallback) {
        return window->GetSizer() ? std::max(fallback, window->GetSizer()->GetMinSize().y)
                                  : fallback;
    };
    const int top = minimum(profile_rows_->GetWindow1(), profile_rows_->GetMinimumPaneSize());
    const int holes =
        minimum(profile_lower_rows_->GetWindow1(), profile_lower_rows_->GetMinimumPaneSize());
    const int notes =
        minimum(profile_lower_rows_->GetWindow2(), profile_lower_rows_->GetMinimumPaneSize());
    const int inner_border = profile_lower_rows_->GetBorderSize();
    const int outer_border = profile_rows_->GetBorderSize();
    const int lower = holes + notes + profile_lower_rows_->GetSashSize() + 2 * inner_border;
    profile_lower_rows_->SetMinSize(wxSize(0, lower));
    profile_rows_->SetMinSize(
        wxSize(0, top + lower + profile_rows_->GetSashSize() + 2 * outer_border));
    const int max_top =
        profile_rows_->GetClientSize().y - profile_rows_->GetSashSize() - lower - outer_border;
    if (max_top >= top + outer_border) {
        const int position =
            std::clamp(profile_rows_->GetSashPosition(), top + outer_border, max_top);
        if (position != profile_rows_->GetSashPosition())
            profile_rows_->SetSashPosition(position);
    }
    profile_rows_->UpdateSize();
    const int max_holes = profile_lower_rows_->GetClientSize().y -
                          profile_lower_rows_->GetSashSize() - notes - inner_border;
    if (max_holes >= holes + inner_border) {
        const int position =
            std::clamp(profile_lower_rows_->GetSashPosition(), holes + inner_border, max_holes);
        if (position != profile_lower_rows_->GetSashPosition())
            profile_lower_rows_->SetSashPosition(position);
    }
    profile_lower_rows_->UpdateSize();
    profile_rows_->GetWindow1()->Layout();
    profile_lower_rows_->GetWindow1()->Layout();
    profile_lower_rows_->GetWindow2()->Layout();
}
} // namespace audiohole::gui
