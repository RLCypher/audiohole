#include "gui/controls.h"
#include "gui/frame.h"
#include "gui/update_dialog.h"
#include <wx/stdpaths.h>

namespace audiohole::gui {
void Frame::update_error(const std::string& detail) {
    if (update_errors_.size() == 32)
        update_errors_.erase(update_errors_.begin());
    update_errors_.push_back("[update-failed] " + detail);
    ++avatar_log_revision_;
    refresh_log();
}
void Frame::start_auto_updates() {
    if (smoke_ || tutorial_ || updates_ || !preferences_->values().check_updates_automatically)
        return;
    const auto root = preferences_->data_root(OperatingMode::Test).parent_path();
    update_store_ = std::make_unique<updates::Store>(root / "updates.json");
    const auto loaded = update_store_->load();
    if (!loaded.ok) {
        update_error(loaded.detail);
        return;
    }
    updates_ = std::make_unique<windows::UpdateService>(root / "update-downloads");
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
void Frame::poll_updates() {
    if (!updates_ || closing_)
        return;
    auto result = updates_->take_result();
    if (!result)
        return;
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
        if (!saved.ok)
            update_error(saved.detail);
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
void Frame::offer_update() {
    if (update_offer_pending_ || closing_ || !update_store_ ||
        !preferences_->values().check_updates_automatically ||
        !updates::should_notify(update_store_->values(), AUDIOHOLE_VERSION))
        return;
    update_offer_pending_ = true;
    CallAfter([this] {
        update_offer_pending_ = false;
        if (closing_ || !preferences_->values().check_updates_automatically)
            return;
        const auto release = *update_store_->values().cached;
        const auto executable =
            std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
        const bool installed = registered_installation(executable.parent_path());
        UpdateDialog dialog(this, release, AUDIOHOLE_VERSION, installed);
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
} // namespace audiohole::gui
