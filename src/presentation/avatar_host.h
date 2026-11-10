#pragma once
#include "audiohole/avatar/session.hpp"
#include "model/application_controller.h"
#include "presentation/messages.h"

namespace audiohole::presentation {
// Control/GUI-thread adapter to PR6's shared engine. Copies only presentation
// context; owns no routing state and issues no Controller or Messages commands.
// The engine owns one cancellable loader; tick performs no file reads/decoding.
class AvatarHost {
  public:
    explicit AvatarHost(avatar::Decoder decoder);
    ~AvatarHost();
    void select(const Personality* package, std::int64_t now);
    void observe(const Snapshot&, bool warning_visible, std::int64_t now);
    // Host input/reactions reset the two-minute cosmetic inactivity timer.
    // Waking never changes routing state or selects new dialogue.
    void activity(std::int64_t now);
    avatar::Decision react(Message, const Copy&, std::uint64_t occurrence, std::int64_t now);
    avatar::Presentation tick(std::int64_t now);
    void reduced_motion(bool enabled);
    void first_frame_only(bool enabled);
    void hold_expressions(bool enabled);
    void reaction_playback(int count, bool until_event);
    void shutdown();
    bool enabled() const {
        return static_cast<bool>(session_);
    }
    std::size_t decoded_bytes() const;
    const Baseline& baseline() const {
        return baseline_;
    }

  private:
    avatar::Decoder decoder_;
    std::unique_ptr<avatar::Session> session_;
    std::string personality_, style_;
    std::vector<std::string> supported_;
    Baseline baseline_, awake_baseline_;
    std::int64_t last_activity_ms_ = 0;
    bool sleeping_ = false, hold_expressions_ = false;
    int reaction_loops_ = 1;
    bool loop_until_event_ = false, first_frame_only_ = false;
    State state_ = State::Stopped;
    bool scanning_ = false, warning_ = false, reduced_ = false;
    std::string active_profile_;
    std::uint64_t revision_ = 1, severe_sequence_ = 0;
};
} // namespace audiohole::presentation
