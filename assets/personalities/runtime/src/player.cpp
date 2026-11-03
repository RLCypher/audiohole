#include "audiohole/avatar/player.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace audiohole::avatar {
bool Player::owned(const ClipPtr& clip) const {
    return clip && clip->personality == personality_ && clip->style == style_;
}
void Player::reset(const std::string& personality, const std::string& style, std::int64_t now) {
    *this = Player{};
    personality_ = personality;
    style_ = style;
    last_now_ = now;
    clock_valid_ = true;
}
void Player::advance(std::int64_t now) {
    if (now < 0 || (clock_valid_ && now < last_now_))
        throw std::invalid_argument("Clock must be monotonic milliseconds");
    if (clock_valid_ && !paused_) {
        const auto delta = static_cast<double>(now - last_now_) * speed_;
        elapsed_ += delta;
        baseline_elapsed_ += delta;
    }
    last_now_ = now;
    clock_valid_ = true;
    if (active_ && finite_ && (!hold_expressions_ || inspecting_)) {
        const int duration = active_->duration_ms();
        if (elapsed_ >= (duration ? duration : active_->behavior.static_hold_ms) + active_hold_ms_)
            complete();
    }
}
void Player::complete() {
    active_.reset();
    active_hold_ms_ = 0;
    finite_ = false;
    inspecting_ = false;
    elapsed_ = 0;
}

Decision Player::context(std::string baseline, std::uint64_t revision, int priority,
                         std::int64_t now, bool invalidate_reaction) {
    advance(now);
    if (revision < revision_)
        return {false, "stale_baseline"};
    if (priority < 0 || priority > 100 || baseline.empty())
        return {false, "invalid_baseline"};
    const bool same = baseline == baseline_state_ && priority == baseline_priority_;
    if (revision == revision_ && same)
        return {false, "already_current"};
    if (revision == revision_ && !same)
        return {false, "revision_conflict"};
    revision_ = revision;
    baseline_priority_ = priority;
    baseline_state_ = std::move(baseline);
    if (!same) {
        baseline_.reset();
        baseline_elapsed_ = 0;
    }
    if (active_ && (invalidate_reaction || priority > active_priority_ || inspecting_))
        complete();
    return {true, same ? "context_revision_updated" : "baseline_pending"};
}
void Player::install_baseline(ClipPtr clip, std::uint64_t revision, std::int64_t now) {
    advance(now);
    if (!owned(clip) || revision != revision_)
        return;
    if (!baseline_ || baseline_->requested != clip->requested)
        baseline_elapsed_ = 0;
    baseline_ = std::move(clip);
}
Decision Player::react(ClipPtr clip, const Reaction& event, std::int64_t now) {
    advance(now);
    if (!owned(clip))
        return {false, "personality_or_style_mismatch"};
    if (event.state != clip->requested)
        return {false, "request_state_mismatch"};
    if (event.revision != revision_)
        return {false, "stale_context"};
    if (event.event_id.empty() || event.ttl_ms < 0 || event.occurred_ms < 0 ||
        event.occurred_ms > now || now - event.occurred_ms > event.ttl_ms)
        return {false, "stale_or_invalid_event"};
    if (event.final_hold_ms < 0 || event.final_hold_ms > 5000)
        return {false, "invalid_final_hold"};
    if (events_.contains(event.event_id))
        return {false, "duplicate_event"};
    const int priority = event.priority < 0 ? clip->behavior.priority : event.priority;
    if (priority < 0 || priority > 100)
        return {false, "invalid_priority"};
    if (priority < baseline_priority_)
        return {false, "baseline_severity"};
    if (active_ && finite_) {
        const auto duration = active_->duration_ms();
        const bool held = hold_expressions_ && !inspecting_ &&
                          elapsed_ >= (duration ? duration : active_->behavior.static_hold_ms);
        if (priority < active_priority_ ||
            (!active_->behavior.interruptible && priority <= active_priority_ && !held))
            return {false, "priority_or_interrupt_policy"};
        if (active_->requested == clip->requested && priority == active_priority_ && !held)
            return {false, "already_active"};
    }
    const auto previous = last_started_.find(event.state);
    const bool escalation = active_ && priority > active_priority_;
    if (!escalation && previous != last_started_.end() &&
        now - previous->second < clip->behavior.cooldown_ms)
        return {false, "cooldown"};
    if (clip->frames.empty())
        return {false, "no_valid_owned_asset"};
    events_[event.event_id] = true;
    event_order_.push_back(event.event_id);
    while (event_order_.size() > 128) {
        events_.erase(event_order_.front());
        event_order_.pop_front();
    }
    if (last_started_.size() >= 128 && !last_started_.contains(event.state)) {
        const auto oldest =
            std::min_element(last_started_.begin(), last_started_.end(),
                             [](const auto& a, const auto& b) { return a.second < b.second; });
        last_started_.erase(oldest);
    }
    last_started_[event.state] = now;
    active_ = std::move(clip);
    active_priority_ = priority;
    active_hold_ms_ = event.final_hold_ms;
    finite_ = true;
    inspecting_ = false;
    elapsed_ = 0;
    return {true, "reaction_started"};
}
void Player::inspect(ClipPtr clip, std::int64_t now) {
    advance(now);
    if (!owned(clip))
        return;
    active_ = std::move(clip);
    active_priority_ = active_->behavior.priority;
    active_hold_ms_ = 0; // Viewer inspection retains the authored timing.
    finite_ = active_->behavior.finite;
    inspecting_ = true;
    elapsed_ = 0;
}
Presentation Player::tick(std::int64_t now) {
    advance(now);
    Presentation result;
    result.personality = personality_;
    result.style = style_;
    result.baseline = baseline_state_;
    result.paused = paused_;
    const auto clip = active_ ? active_ : baseline_;
    result.state = active_ ? active_->requested : baseline_state_;
    result.priority = active_ ? active_priority_ : baseline_priority_;
    result.reaction = finite_;
    if (!clip) {
        result.source = "loading";
        result.loading = true;
        return result;
    }
    result.source = clip->source;
    result.diagnostics = clip->diagnostics;
    result.frame_count = static_cast<int>(clip->frames.size());
    if (clip->frames.empty()) {
        result.source = "hide";
        return result;
    }
    double position = active_ ? elapsed_ : baseline_elapsed_;
    result.elapsed_ms = position;
    const auto duration = clip->duration_ms();
    result.holding = active_ && finite_ && !inspecting_ &&
                     position >= (duration ? duration : clip->behavior.static_hold_ms);
    if (reduced_) {
        result.image = clip->still ? clip->still : clip->frames.front();
        result.source = "reduced_motion_static";
        return result;
    }
    if (duration > 0) {
        // Finite reactions also accept looping baseline clips as fallbacks.
        // Their extra hold must keep the final frame, rather than wrap to frame 0.
        const bool loop =
            clip->loop &&
            !(active_ && finite_ && (active_hold_ms_ > 0 || (hold_expressions_ && !inspecting_)));
        position = loop ? std::fmod(position, duration) : std::min(position, double(duration - 1));
        double end = 0;
        for (std::size_t i = 0; i < clip->durations_ms.size(); ++i) {
            end += clip->durations_ms[i];
            if (position < end) {
                result.index = static_cast<int>(i);
                result.exposure_ms = clip->durations_ms[i];
                break;
            }
        }
    }
    result.image = clip->frames.at(result.index);
    return result;
}
void Player::pause(bool value, std::int64_t now) {
    advance(now);
    paused_ = value;
}
void Player::speed(double multiplier, std::int64_t now) {
    advance(now);
    if (!std::isfinite(multiplier) || multiplier < .1 || multiplier > 4)
        throw std::invalid_argument("Speed must be between 0.1 and 4.0");
    speed_ = multiplier;
}
void Player::restart(std::int64_t now) {
    advance(now);
    if (active_)
        elapsed_ = 0;
    else
        baseline_elapsed_ = 0;
}
void Player::step(std::int64_t now) {
    pause(true, now);
    const auto clip = active_ ? active_ : baseline_;
    if (!clip || clip->durations_ms.empty())
        return;
    auto& elapsed = active_ ? elapsed_ : baseline_elapsed_;
    const auto duration = clip->duration_ms();
    const bool loop = clip->loop && !(active_ && finite_ &&
                                      (active_hold_ms_ > 0 || (hold_expressions_ && !inspecting_)));
    const double position = loop ? std::fmod(elapsed, duration) : elapsed;
    int end = 0;
    for (int exposure : clip->durations_ms) {
        end += exposure;
        if (position < end) {
            elapsed += end - position;
            break;
        }
    }
    if (active_ && finite_ && (!hold_expressions_ || inspecting_) &&
        elapsed >= duration + active_hold_ms_)
        complete();
}
} // namespace audiohole::avatar
