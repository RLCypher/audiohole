#include "gui/controls.h"
#include "gui/frame.h"
namespace audiohole::gui {
void Frame::build_device_check(wxWindow* parent, wxSizer* layout, DiagnosticWidgets& widgets,
                               bool capture, const wxString& name) {
    widgets.capture = capture;
    widgets.panel = new wxPanel(parent);
    widgets.panel->SetName(name + " diagnostic controls");
    auto* content = new wxBoxSizer(wxHORIZONTAL);
    widgets.panel->SetSizer(content);
    widgets.action = button(widgets.panel, content, capture ? "Check level" : "Test beep",
                            [this, &widgets] { start_device_check(widgets); });
    auto size = widgets.action->GetEffectiveMinSize();
    for (const auto* caption : {"Check level", "Cancel check", "Test beep", "Cancel beep"}) {
        widgets.action->SetLabel(caption);
        size.IncTo(widgets.action->GetEffectiveMinSize());
    }
    widgets.action->SetLabel(capture ? "Check level" : "Test beep");
    widgets.action->SetMinSize(size);
    widgets.action->SetMaxSize(wxSize(size.x, -1));
    widgets.action->SetSize(size);
    widgets.action->SetToolTip(capture ? "Temporarily check this capture endpoint's peak input "
                                         "level. Cancel releases the stream."
                                       : "Play a quiet, one-second test beep on this playback "
                                         "endpoint. Cancel requests stream cleanup.");
    widgets.meter = new wxGauge(widgets.panel, wxID_ANY, 100);
    widgets.meter->SetName(name + " input level");
    widgets.meter->SetMinSize(FromDIP(wxSize(1, 12)));
    content->Add(widgets.meter, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(4));
    content->AddStretchSpacer();
    widgets.panel->Bind(wxEVT_SIZE, [panel = widgets.panel,
                                     meter = widgets.meter](wxSizeEvent& event) {
        meter->SetMinSize(wxSize(std::max(20, panel->GetClientSize().x / 2), panel->FromDIP(12)));
        panel->Layout();
        event.Skip();
    });
    widgets.meter->Show(capture);
    layout->Add(widgets.panel, 0, wxEXPAND | wxALL, FromDIP(3));
}
void Frame::start_device_check(DiagnosticWidgets& widgets) {
    capture_draft();
    refresh_device_checks();
    const auto& state = controller_->snapshot();
    const bool active = widgets.capture ? state.monitoring && state.monitor_source == widgets.target
                                        : !state.beep_destination.empty() &&
                                              state.beep_destination == widgets.target;
    if (active) {
        end_observation();
        present(presentation::Message::Cancelled);
        refresh();
        return;
    }
    if (state.monitoring || !state.beep_destination.empty()) {
        show_result({false, "Cancel the active device check before starting another."});
        return;
    }
    const auto result = widgets.capture ? controller_->check_level(widgets.target)
                                        : controller_->beep(widgets.target);
    if (result.ok)
        active_check_ = &widgets;
    if (result.ok) {
        present(widgets.capture ? presentation::Message::LevelCheckStarted
                                : presentation::Message::BeepStarted);
        if (state.capabilities.simulated) {
            technical_copy_ = "Simulated device check; no real audio is opened.";
            layout_header();
        }
    } else
        show_result(result);
    refresh();
}
void Frame::refresh_device_checks() {
    const auto& state = controller_->snapshot();
    const Hole* hole =
        hole_index_ >= 0 && static_cast<std::size_t>(hole_index_) < draft_.holes.size()
            ? &draft_.holes[hole_index_]
            : nullptr;
    forward_.capture_check.target = hole ? hole->forward.source : "";
    forward_.playback_check.target = hole ? hole->forward.destination : "";
    reverse_.capture_check.target = hole && hole->duplex ? hole->reverse.source : "";
    reverse_.playback_check.target = hole && hole->duplex ? hole->reverse.destination : "";
    device_check_.target = device_id_;
    if (active_check_ && ((state.monitoring && active_check_->capture &&
                           state.monitor_source != active_check_->target) ||
                          (!state.beep_destination.empty() && !active_check_->capture &&
                           state.beep_destination != active_check_->target)))
        end_observation();
    const auto* selected = endpoint(state, device_id_);
    if (selected)
        device_check_.capture = selected->capture;
    device_check_.panel->Show(bool(selected));
    for (auto* widgets : {&forward_.capture_check, &forward_.playback_check,
                          &reverse_.capture_check, &reverse_.playback_check, &device_check_}) {
        const auto* device = endpoint(state, widgets->target);
        const bool active =
            widgets->capture
                ? state.monitoring && state.monitor_source == widgets->target
                : !state.beep_destination.empty() && state.beep_destination == widgets->target;
        const bool busy = state.monitoring || !state.beep_destination.empty();
        const bool supported =
            widgets->capture ? state.capabilities.monitoring : state.capabilities.beep;
        widgets->action->SetLabel(active ? (widgets->capture ? "Cancel check" : "Cancel beep")
                                         : (widgets->capture ? "Check level" : "Test beep"));
        widgets->action->Enable(active ||
                                (supported && device && device->capture == widgets->capture &&
                                 device->availability == Availability::Available &&
                                 !state.scanner.busy && !busy));
        widgets->meter->Show(widgets->capture);
        const int peak = active && widgets->capture && state.input_peak
                             ? static_cast<int>(*state.input_peak * 100)
                             : 0;
        if (widgets->meter->GetValue() != peak)
            widgets->meter->SetValue(peak);
        if (active && !state.diagnostic_detail.empty())
            widgets->action->SetToolTip(text(state.diagnostic_detail));
        widgets->panel->Layout();
    }
    if (active_check_ && !state.monitoring && state.beep_destination.empty()) {
        const bool playback = !active_check_->capture;
        active_check_ = nullptr;
        if (state.diagnostic_failed) {
            show_result({false, state.diagnostic_detail});
            technical_copy_ = state.diagnostic_detail;
            layout_header();
        } else if (playback && !state.capabilities.simulated)
            present(presentation::Message::BeepComplete);
    }
}
} // namespace audiohole::gui
