#pragma once
#include "audiohole/avatar/assets.hpp"
#include <deque>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace audiohole::avatar {
struct Decision {
    bool accepted;
    std::string reason;
};
struct Reaction {
    std::string state, event_id;
    std::uint64_t revision = 0;
    std::int64_t occurred_ms = 0, ttl_ms = 2000;
    int priority = -1; // -1 uses the manifest. Explicit severity wins otherwise.
    // Extra display time after one authored pass (or the static hold), 0..5000 ms.
    // Independent of event freshness/TTL; operational interruption still applies.
    int final_hold_ms = 0;
    // A nonzero event-local play count (1..20) overrides global repeats/hold.
    // Used for finite speech gestures without altering saved playback policy.
    int total_plays = 0;
};
struct Presentation {
    Image image;
    std::string personality, style, state, source, baseline;
    int index = 0, frame_count = 0, exposure_ms = 0;
    double elapsed_ms = 0;
    int priority = 0;
    bool reaction = false, loading = false, paused = false, holding = false, looping = false;
    std::vector<std::string> diagnostics;
};
// Host-thread only, clock-injected playback. No filesystem, GUI, decoding, audio,
// dialogue, operational-state inference, or reaction queue exists here.
class Player {
  public:
    void reset(const std::string& personality, const std::string& style, std::int64_t now);
    Decision context(std::string baseline, std::uint64_t revision, int priority, std::int64_t now,
                     bool invalidate_reaction = false);
    void install_baseline(ClipPtr clip, std::uint64_t revision, std::int64_t now);
    Decision react(ClipPtr clip, const Reaction& event, std::int64_t now);
    // Explicit viewer inspection bypasses event dedup/cooldown, not asset checks.
    void inspect(ClipPtr clip, std::int64_t now);
    Presentation tick(std::int64_t now);
    void pause(bool value, std::int64_t now);
    void speed(double multiplier, std::int64_t now);
    void first_frame_only(bool value) {
        first_frame_only_ = value;
    }
    void reduced_motion(bool value) {
        reduced_ = value;
    }
    // Keep completed reaction poses until replaced/cancelled. Turning this off
    // lets the next tick complete any reaction past its ordinary display deadline.
    void hold_expressions(bool value) {
        hold_expressions_ = value;
    }
    // Total authored passes, 1..20; continuous reactions yield to context changes
    // and subsequent events. Baseline looping/reduced-motion stay independent.
    void reaction_playback(int count, bool until_event) {
        if (count < 1 || count > 20)
            throw std::invalid_argument("Invalid animation loop count");
        reaction_loops_ = count;
        loop_until_event_ = until_event;
    }
    void restart(std::int64_t now);
    void step(std::int64_t now);
    std::uint64_t revision() const {
        return revision_;
    }
    const std::string& personality() const {
        return personality_;
    }
    const std::string& style() const {
        return style_;
    }
    bool baseline_ready() const {
        return static_cast<bool>(baseline_);
    }

  private:
    std::string personality_, style_, baseline_state_ = "idle";
    ClipPtr baseline_, active_;
    std::uint64_t revision_ = 0;
    int baseline_priority_ = 0, active_priority_ = 0;
    int active_hold_ms_ = 0, active_total_plays_ = 0;
    int plays() const {
        return active_total_plays_ ? active_total_plays_ : reaction_loops_;
    }
    bool continuous() const {
        return !active_total_plays_ && loop_until_event_;
    }
    bool held() const {
        return !active_total_plays_ && hold_expressions_;
    }
    int reaction_loops_ = 1;
    bool loop_until_event_ = false, first_frame_only_ = false;
    std::int64_t last_now_ = 0;
    bool clock_valid_ = false, finite_ = false, inspecting_ = false, paused_ = false,
         reduced_ = false, hold_expressions_ = false;
    double elapsed_ = 0, baseline_elapsed_ = 0, speed_ = 1;
    std::deque<std::string> event_order_;
    std::unordered_map<std::string, bool> events_;
    std::unordered_map<std::string, std::int64_t> last_started_;
    void advance(std::int64_t now);
    void complete();
    bool owned(const ClipPtr& clip) const;
};
} // namespace audiohole::avatar
