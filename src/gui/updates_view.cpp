#include "gui/controls.h"
#include "gui/frame.h"
#include "gui/update_dialog.h"
#include <atomic>
#include <thread>
#include <wx/generic/msgdlgg.h>
#include <wx/modalhook.h>
#include <wx/stdpaths.h>

namespace audiohole::gui {
void Frame::update_error(const std::string& detail) {
    if (update_errors_.size() == 32)
        update_errors_.erase(update_errors_.begin());
    update_errors_.push_back("[update-failed] " + detail);
    ++avatar_log_revision_;
    refresh_log();
}
bool Frame::prepare_updates() {
    if (updates_ && update_store_)
        return true;
    // Smoke hosts install a fixture transport explicitly. Never create a real
    // network transport or persistent update state in a smoke or tutorial host.
    if (smoke_ || tutorial_)
        return false;
    const auto root = preferences_->data_root(OperatingMode::Test).parent_path();
    update_store_ = std::make_unique<updates::Store>(root / "updates.json");
    const auto loaded = update_store_->load();
    if (!loaded.ok) {
        update_error(loaded.detail);
        return false;
    }
    updates_ = std::make_unique<windows::UpdateService>(root / "update-downloads");
    return true;
}
void Frame::start_auto_updates() {
    if (smoke_ || tutorial_ || updates_ || !preferences_->values().check_updates_automatically ||
        !prepare_updates())
        return;
    auto state = update_store_->values();
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    if (updates::check_due(state, true, now)) {
        state.last_checked_unix = now;
        const auto saved = update_store_->save(state);
        if (!saved.ok) {
            update_error(saved.detail);
            return;
        }
        updates_->check();
    } else {
        offer_update();
    }
}
void Frame::check_updates_now(wxWindow* parent, wxWindow* button) {
    if (tutorial_ || closing_ || manual_update_pending_ || update_offer_pending_ ||
        (updates_ && updates_->busy()))
        return;
    if (!prepare_updates()) {
        themed_message_box("AudioHole could not load its update state. See Scanning / Diagnostics "
                           "for details.",
                           "AudioHole update check failed", wxOK | wxICON_ERROR, parent);
        return;
    }
    auto state = update_store_->values();
    state.last_checked_unix = std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
    const auto saved = update_store_->save(state);
    if (!saved.ok) {
        update_error(saved.detail);
        themed_message_box(text(saved.detail), "AudioHole update check failed", wxOK | wxICON_ERROR,
                           parent);
        return;
    }
    // A manual request deliberately bypasses the weekly interval and skipped
    // version, but shares transport, validation and installer handoff policy.
    if (updates_->check()) {
        manual_update_pending_ = true;
        manual_update_parent_ = parent;
        manual_update_button_ = button;
        button->SetLabel("Checking for updates…");
        button->Disable();
    }
}
void Frame::poll_updates() {
    if (!updates_ || closing_)
        return;
    auto result = updates_->take_result();
    if (!result)
        return;
    const bool manual = manual_update_pending_;
    wxWindow* parent = manual_update_parent_ ? manual_update_parent_.get() : this;
    manual_update_pending_ = false;
    manual_update_parent_ = nullptr;
    if (manual_update_button_) {
        manual_update_button_->SetLabel("Check for updates");
        manual_update_button_->Enable();
        manual_update_button_ = nullptr;
    }
    const bool downloading = update_progress_ != nullptr;
    if (update_progress_) {
        update_progress_->Destroy();
        update_progress_ = nullptr;
    }
    if (downloading && update_download_cancelled_) {
        result->installer.reset();
        result->cancelled = true;
    }
    if (!result->error.empty() && !result->cancelled)
        update_error(result->error);
    if (result->release) {
        auto state = update_store_->values();
        state.cached = std::move(result->release);
        const auto saved = update_store_->save(state);
        if (!saved.ok) {
            update_error(saved.detail);
            if (manual) {
                themed_message_box(text(saved.detail), "AudioHole update check failed",
                                   wxOK | wxICON_ERROR, parent);
                return;
            }
        }
    }
    if (result->installer) {
        auto installer = std::move(result->installer);
        CallAfter([this, installer] {
            if (closing_ || !discard())
                return;
            if (device_dirty_ &&
                themed_message_box(
                    "Discard unsaved device edits before updating? Saved files will be retained.",
                    "Unsaved device changes", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                    this) != wxYES)
                return;
            if (install_update_)
                install_update_(installer);
        });
    } else if (manual) {
        if (result->cancelled)
            return;
        if (!result->error.empty()) {
            themed_message_box("AudioHole could not check for updates. Please try again.\n\n" +
                                   text(result->error),
                               "AudioHole update check failed", wxOK | wxICON_ERROR, parent);
        } else {
            auto state = update_store_->values();
            state.skipped_version.clear();
            if (updates::should_notify(state, AUDIOHOLE_VERSION))
                offer_update(true, parent);
            else
                show_no_update_dialog(parent);
        }
    } else if (!downloading) {
        // A cached reminder is useful even when this week's request failed.
        offer_update();
    } else if (!result->cancelled && !result->error.empty()) {
        themed_message_box("AudioHole could not download a verified installer. "
                           "You can try again next start or download it from GitHub.\n\n" +
                               text(result->error),
                           "AudioHole update", wxOK | wxICON_ERROR, this);
    }
}
void Frame::offer_update(bool manual, wxWindow* parent) {
    auto state = update_store_ ? update_store_->values() : updates::State{};
    if (manual)
        state.skipped_version.clear();
    if (update_offer_pending_ || closing_ || !update_store_ ||
        (!manual && !preferences_->values().check_updates_automatically) ||
        !updates::should_notify(state, AUDIOHOLE_VERSION))
        return;
    update_offer_pending_ = true;
    CallAfter([this, manual, owner = wxWeakRef<wxWindow>(parent)] {
        update_offer_pending_ = false;
        if (closing_ || (!manual && !preferences_->values().check_updates_automatically))
            return;
        const auto release = *update_store_->values().cached;
        const auto executable =
            std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
        const bool installed = registered_installation(executable.parent_path());
        UpdateDialog dialog(owner ? owner.get() : this, release, AUDIOHOLE_VERSION, installed);
        dialog.ShowModal();
        switch (dialog.choice()) {
        case UpdateChoice::Skip: {
            auto state = update_store_->values();
            state.skipped_version = release.version;
            const auto saved = update_store_->save(state);
            if (!saved.ok)
                update_error(saved.detail);
            break;
        }
        case UpdateChoice::OpenGithub:
            if (!wxLaunchDefaultBrowser(text(release.page_url())))
                update_error("Could not open GitHub in the browser.");
            break;
        case UpdateChoice::Install:
            if (installed && install_update_ && updates_->download(release)) {
                // Release About's modal loop before the modeless download and
                // eventual asynchronous application shutdown/installer handoff.
                if (manual && owner) {
                    auto* about = dynamic_cast<wxDialog*>(owner.get());
                    if (about && about->IsModal())
                        about->EndModal(wxID_CLOSE);
                }
                update_download_cancelled_ = false;
                update_progress_ = new wxDialog(this, wxID_ANY, "AudioHole update");
                auto* layout = new wxBoxSizer(wxVERTICAL);
                layout->Add(new wxStaticText(update_progress_, wxID_ANY,
                                             "Downloading and verifying the update…"),
                            0, wxALL, FromDIP(16));
                auto* cancel = new ActionButton(update_progress_, "Cancel", 0, wxID_CANCEL);
                cancel->UnsetToolTip();
                layout->Add(cancel, 0, wxALL | wxALIGN_RIGHT, FromDIP(12));
                const auto cancel_download = [this, cancel] {
                    update_download_cancelled_ = true;
                    updates_->cancel();
                    cancel->Disable();
                };
                cancel->Bind(wxEVT_BUTTON,
                             [cancel_download](wxCommandEvent&) { cancel_download(); });
                update_progress_->Bind(wxEVT_CLOSE_WINDOW, [cancel_download](wxCloseEvent& e) {
                    cancel_download();
                    if (e.CanVeto())
                        e.Veto();
                    else
                        e.Skip();
                });
                update_progress_->SetSizerAndFit(layout);
                theme_dialog(update_progress_);
                update_progress_->CentreOnParent();
                update_progress_->Show();
            }
            break;
        case UpdateChoice::Remind:
            break; // The cached version remains eligible on the very next start.
        }
    });
}
void Frame::smoke_manual_update_check(wxDialog* about) {
    // Exercise the actual About button and worker without network, disk writes,
    // browser actions or installer execution. The gate proves dispatch returns
    // while the transport is still busy, rather than blocking the GUI thread.
    class Fixture final : public windows::UpdateTransport {
      public:
        std::atomic<bool> ready{false};
        std::atomic<unsigned> requests{0};
        std::string version, error;
        void fetch(const std::string& url, std::uint64_t, std::stop_token stop,
                   const Sink& sink) override {
            if (url != updates::latest_release_url)
                throw std::runtime_error("Manual check requested an unexpected URL.");
            ++requests;
            while (!ready && !stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!error.empty())
                throw std::runtime_error(error);
            const auto body = "{\"tag_name\":\"v" + version +
                              "\",\"draft\":false,\"prerelease\":false,\"body\":\"Fixture "
                              "notes.\",\"assets\":[]}";
            sink(std::as_bytes(std::span(body)));
        }
        void cancel() override {
            ready = true;
        }
    };
    class Answer final : public wxModalDialogHook {
      public:
        std::string expected;
        bool seen = false, clicked = false;
        wxWindow* owner = nullptr;
        int Enter(wxDialog* dialog) override {
            if (seen || (dialog->GetTitle() != "AudioHole update" &&
                         dialog->GetTitle() != "AudioHole update check failed"))
                return wxID_NONE;
            seen = true;
            if (dialog->GetParent() != owner)
                throw std::runtime_error("Manual update result is not owned by About/Update.");
            if (expected == "newer") {
                if (!dynamic_cast<UpdateDialog*>(dialog))
                    throw std::runtime_error(
                        "Manual check did not offer the newer skipped version.");
            } else {
                auto* message = dynamic_cast<wxGenericMessageDialog*>(dialog);
                if (!message || (expected == "error"
                                     ? !message->GetMessage().Contains("Fixture network failure")
                                     : message->GetMessage() != "Sorry boss, nothing new yet." ||
                                           message->GetOKLabel() != "Ok"))
                    throw std::runtime_error("Manual update result lost its message or Ok label.");
            }
            dialog->CallAfter([this, dialog] {
                const int id = expected == "newer" ? wxID_CANCEL : wxID_OK;
                auto* button = dialog->FindWindow(id);
                if (!button || (expected != "error" && expected != "newer" &&
                                (button->GetLabel() != "Ok" ||
                                 button->GetForegroundColour() != owner->GetForegroundColour())))
                    throw std::runtime_error(
                        "Manual update result lost its themed closing button.");
                clicked = true;
                wxCommandEvent click(wxEVT_BUTTON, id);
                button->GetEventHandler()->ProcessEvent(click);
            });
            return wxID_NONE;
        }
    };
    auto* check = wxWindow::FindWindowByName("Check for updates", about);
    if (!smoke_ || !check || updates_ || preferences_->values().check_updates_automatically)
        throw std::runtime_error("Manual update fixture requires an idle opt-out smoke host.");
    const auto errors_before = update_errors_;
    for (const auto* outcome : {"current", "older", "newer", "error"}) {
        auto fixture = std::make_unique<Fixture>();
        auto* transport = fixture.get();
        fixture->version = std::string(outcome) == "newer"   ? "99.0.0"
                           : std::string(outcome) == "older" ? "0.0.0"
                                                             : AUDIOHOLE_VERSION;
        if (std::string(outcome) == "error")
            fixture->error = "Fixture network failure";
        update_store_ = std::make_unique<updates::Store>(std::filesystem::path{}, true);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        if (!update_store_->load().ok || !update_store_->save({now, "99.0.0", {}}).ok)
            throw std::runtime_error("Cannot initialize manual update fixture state.");
        // A nonexistent path is sufficient: checks never create a download directory.
        const auto cache = std::filesystem::temp_directory_path() /
                           ("AudioHole-update-smoke-" + std::to_string(GetCurrentProcessId()) +
                            "-" + std::to_string(GetTickCount64()));
        updates_ = std::make_unique<windows::UpdateService>(cache, std::move(fixture));
        Answer answer;
        answer.expected = outcome;
        answer.owner = about;
        answer.Register();
        wxCommandEvent click(wxEVT_BUTTON, check->GetId());
        check->GetEventHandler()->ProcessEvent(click);
        if (!manual_update_pending_ || check->IsEnabled() || !updates_->busy())
            throw std::runtime_error(
                "Check for updates did not start an immediate asynchronous check.");
        transport->ready = true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while ((!answer.clicked || manual_update_pending_ || update_offer_pending_) &&
               std::chrono::steady_clock::now() < deadline) {
            poll_updates();
            wxYield();
            wxMilliSleep(1);
        }
        answer.Unregister();
        if (!answer.seen || !answer.clicked || !check->IsEnabled() ||
            check->GetLabel() != "Check for updates" || transport->requests != 1 ||
            preferences_->values().check_updates_automatically || std::filesystem::exists(cache))
            throw std::runtime_error(
                "Manual update check lost its result, button, opt-out or isolation.");
        updates_.reset();
        update_store_.reset();
    }
    update_errors_ = errors_before;
    ++avatar_log_revision_;
    refresh_log();
}
} // namespace audiohole::gui
