#include "gui/engine_smoke.h"
#include "gui/controls.h"
#include "gui/frame.h"
#include "tests/support/engine_fixture.h"

namespace audiohole::gui {
std::unique_ptr<Controller> make_engine_smoke_controller() {
    auto fixture = std::make_shared<testing_engine::Control>();
    return std::make_unique<ConfigurationController>(std::make_unique<testing_engine::Store>(),
                                                     nullptr, nullptr,
                                                     testing_engine::service(std::move(fixture)));
}
bool Frame::engine_smoke(const wxString& directory) {
    try {
        auto require = [](bool ok, const char* message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        auto until = [&](const auto& predicate) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!predicate()) {
                require(std::chrono::steady_clock::now() < deadline,
                        "Engine GUI smoke timed out waiting for confirmed state");
                controller_->advance(10);
                refresh();
                wxYield();
                wxMilliSleep(5);
            }
        };
        auto click = [&](wxButton* button) {
            require(button && button->IsEnabled(), "Engine GUI action is disabled");
            wxCommandEvent event(wxEVT_BUTTON, button->GetId());
            button->GetEventHandler()->ProcessEvent(event);
        };
        for (const auto* name : {"Preview startup", "Cancel pending activation"}) {
            const auto* control = wxWindow::FindWindowByName(name, this);
            require(control && control->IsEnabled(),
                    "Engine startup scheduling/cancellation controls are disabled");
        }
        until([&] { return controller_->snapshot().capabilities.routing; });
        require(!controller_->snapshot().capabilities.simulated && !tutorial_,
                "Engine smoke selected the tutorial/legacy simulator");
        click(start_);
        require(controller_->snapshot().state == State::Starting,
                "Start bypassed engine admission/progress");
        until([&] { return controller_->snapshot().state == State::Running; });
        require(controller_->snapshot().paths.size() == 2, "Engine GUI lost a duplex direction");
        require(forward_.source_detail->GetLabel().Contains("Opened: 48000 Hz") &&
                    reverse_.destination_detail->GetLabel().Contains("Opened: 48000 Hz"),
                "Endpoint cards did not show actual independent engine formats");
        require(!start_->IsEnabled() && profile_apply_->IsEnabled() &&
                    forward_.buffer_apply->IsEnabled() && reverse_.buffer_apply->IsEnabled(),
                "Live Apply buttons are unavailable or Start permits a duplicate activation");
        require(path_status_->GetValue().Contains("48000 Hz") &&
                    path_status_->GetValue().Contains("Allocated buffer") &&
                    path_status_->GetValue().Contains("Transferred frames"),
                "GUI did not show actual engine format/buffer/frame observations");
        const auto visible_diagnostics = path_status_->GetValue();
        const auto transferred =
            controller_->snapshot().routing_status->paths[0].observation.transferred_frames.value();
        select_page(SettingsPage);
        until([&] {
            return controller_->snapshot()
                       .routing_status->paths[0]
                       .observation.transferred_frames.value() > transferred + 960;
        });
        require(path_status_->GetValue() == visible_diagnostics &&
                    controller_->snapshot().state == State::Running,
                "Hidden diagnostics rebuilt text or paused routing supervision");
        select_page(ProfilesPage);
        refresh();
        require(path_status_->GetValue() != visible_diagnostics &&
                    path_status_->GetValue().Contains("Transferred frames"),
                "Returning to Profiles did not refresh deferred diagnostic counters");
        require(!forward_.capture_check.action->IsEnabled() &&
                    !forward_.playback_check.action->IsEnabled(),
                "Temporary device checks remain enabled during routing");
        // Both duplicated Apply buttons save the entire profile and restart the
        // real Runtime fixture, including edits outside their own buffer field.
        for (auto* action : {forward_.buffer_apply, reverse_.buffer_apply, profile_apply_}) {
            const int requested = action == profile_apply_          ? 19
                                  : action == forward_.buffer_apply ? 13
                                                                    : 17;
            profile_notes_->SetValue(wxString::Format("Apply saves all controls %d", requested));
            forward_.exclusive_preferred->SetValue(true);
            forward_.advanced_tuning->SetValue(true);
            forward_.buffer->SetValue(requested);
            reverse_.advanced_tuning->SetValue(true);
            reverse_.buffer->SetValue(requested + 1);
            refresh();
            click(action);
            require(
                !dirty_ &&
                    controller_->snapshot().profiles[0].notes == utf8(profile_notes_->GetValue()) &&
                    controller_->snapshot().profiles[0].holes[0].forward.exclusive_mode_preferred,
                "Apply did not save the full profile before restarting");
            require(controller_->snapshot().state == State::Stopping,
                    "Apply did not request asynchronous routing cleanup");
            until([&] { return controller_->snapshot().state == State::Running; });
            const auto& paths = controller_->snapshot().routing_status->paths;
            require(paths[0].negotiated->destination.applied_buffer_request_ms == requested &&
                        paths[1].negotiated->destination.applied_buffer_request_ms == requested + 1,
                    "Apply did not restart both directions with the saved requests");
            require(paths[0].negotiated->source.exclusive_mode_preferred &&
                        paths[0].negotiated->source.mode == engine::AccessMode::Exclusive &&
                        paths[0].negotiated->destination.mode == engine::AccessMode::Exclusive,
                    "Saved exclusive preference did not reach the restarted engine request");
        }
        const auto applied = controller_->snapshot().profiles;
        click(stop_);
        require(controller_->snapshot().state == State::Stopping && !controller_->snapshot().armed,
                "Stop did not enter asynchronous cleanup/disarm");
        until([&] {
            return controller_->snapshot().state == State::Stopped &&
                   !controller_->snapshot().routing_pending;
        });
        require(controller_->snapshot().profiles == applied,
                "Engine GUI rewrote saved routing intent");
        require(start_->IsEnabled(), "Confirmed cleanup did not enable the next Start");
        for (int milliseconds : {1, 6, 20, 50}) {
            for (auto* direction : {&forward_, &reverse_}) {
                direction->advanced_tuning->SetValue(true);
                wxCommandEvent tune(wxEVT_CHECKBOX, direction->advanced_tuning->GetId());
                direction->advanced_tuning->GetEventHandler()->ProcessEvent(tune);
                direction->buffer->SetValue(milliseconds + (direction == &reverse_ ? 1 : 0));
                direction->mode->SetSelection(2);
                wxCommandEvent mode(wxEVT_CHOICE, direction->mode->GetId());
                direction->mode->GetEventHandler()->ProcessEvent(mode);
            }
            tuning_timer_.Stop();
            refresh();
            click(profile_save_);
            require(!dirty_, "Advanced buffer request did not save through actual GUI controls");
            click(start_);
            until([&] { return controller_->snapshot().state == State::Running; });
            const auto& paths = controller_->snapshot().routing_status->paths;
            require(paths.size() == 2 && paths[0].negotiated && paths[1].negotiated &&
                        paths[0].negotiated->destination.applied_buffer_request_ms ==
                            milliseconds &&
                        paths[1].negotiated->destination.applied_buffer_request_ms ==
                            milliseconds + 1 &&
                        path_status_->GetValue().Contains("Buffer request supplied"),
                    "Advanced GUI requests failed startup, crossed directions or lost allocation "
                    "feedback");
            click(stop_);
            until([&] {
                return controller_->snapshot().state == State::Stopped &&
                       !controller_->snapshot().routing_pending;
            });
        }
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream output(std::filesystem::path(directory.ToStdWstring()) /
                                 "smoke-result.txt");
            output << "PASS: GUI Start/Stop through real engine Runtime with software-only "
                      "adapters; confirmed duplex progress, telemetry, cleanup and unchanged saved "
                      "intent; all three Apply buttons save the full profile and restart both "
                      "directions; hidden telemetry text pauses while routing continues and "
                      "refreshes on return; advanced tuning Save/Start/Stop with independent "
                      "1/6/20/50-ms "
                      "requests and quantized buffers. No hardware opened.\n";
            require(bool(output), "Could not write engine GUI smoke evidence");
        }
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GUI engine smoke FAIL: %s\n", error.what());
        std::fflush(stderr);
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream(std::filesystem::path(directory.ToStdWstring()) / "smoke-result.txt")
                << "FAIL: " << error.what() << '\n';
        }
        return false;
    }
}
} // namespace audiohole::gui
