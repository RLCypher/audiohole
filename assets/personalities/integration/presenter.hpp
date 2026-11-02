#pragma once
#include "audiohole/avatar/wx_panel.hpp"
#include "proposed_adapter.hpp"
#include <iostream>

namespace audiohole::avatar::integration {
// Proposed host binding, not installed into the AudioHole GUI by this task.
// It observes only copied authoritative state fields and existing authored
// Message values. It never calls Controller or Messages methods, changes a
// personality ID, generates dialogue, or activates DFA behavior.
class Presenter {
  public:
    using Diagnostics = std::function<void(const std::vector<std::string>&)>;
    Presenter(std::filesystem::path asset_root, Panel& panel, Diagnostics diagnostics = {})
        : panel_(panel), session_(std::move(asset_root), native_decoder()),
          diagnostics_(std::move(diagnostics)) {
        initialize_image_decoder();
    }
    void select(const std::string& personality, const std::string& style) {
        personality_ = personality;
        revision_ = 1;
        const auto now = monotonic_ms();
        session_.select(personality, style, now);
        const auto desired = baseline(state_, scanning_);
        session_.baseline(desired.state, ++revision_, desired.priority, now, true);
        panel_.present(session_.tick(now));
    }
    void observe(const audiohole::Snapshot& snapshot) {
        if (snapshot.state == state_ && snapshot.scanning == scanning_)
            return;
        state_ = snapshot.state;
        scanning_ = snapshot.scanning;
        if (personality_.empty())
            return;
        const auto desired = baseline(state_, scanning_);
        // A new authoritative condition invalidates the previous reaction even
        // if its expressive animation had higher cosmetic priority.
        session_.baseline(desired.state, ++revision_, desired.priority, monotonic_ms(), true);
    }
    void on_message(presentation::Message message, std::uint64_t occurrence) {
        if (const auto requested =
                reaction_for_snapshot(message, personality_, state_, scanning_)) {
            const auto now = monotonic_ms();
            session_.react({requested->state, "message:" + std::to_string(occurrence), revision_,
                            now, 2000, requested->priority},
                           now);
        }
    }
    void poll() {
        auto frame = session_.tick(monotonic_ms());
        if (frame.diagnostics != last_diagnostics_) {
            last_diagnostics_ = frame.diagnostics;
            if (diagnostics_)
                diagnostics_(last_diagnostics_);
            else
                for (const auto& line : last_diagnostics_)
                    std::clog << "Avatar: " << line << '\n';
        }
        panel_.present(std::move(frame));
    }
    void reduced_motion(bool enabled) {
        session_.player().reduced_motion(enabled);
    }
    void shutdown() {
        session_.shutdown();
        panel_.present({});
    }

  private:
    Panel& panel_;
    Session session_;
    Diagnostics diagnostics_;
    std::vector<std::string> last_diagnostics_;
    std::string personality_;
    std::uint64_t revision_ = 1;
    audiohole::State state_ = audiohole::State::Stopped;
    bool scanning_ = false;
};
} // namespace audiohole::avatar::integration
