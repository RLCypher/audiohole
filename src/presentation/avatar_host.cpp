#include "presentation/avatar_host.h"
#include <algorithm>

namespace audiohole::presentation {
namespace {
constexpr int reaction_final_hold_ms = 2000;
constexpr std::int64_t sleep_after_ms = 120000;
} // namespace
AvatarHost::AvatarHost(avatar::Decoder decoder) : decoder_(std::move(decoder)) {}
AvatarHost::~AvatarHost() {
    shutdown();
}
void AvatarHost::shutdown() {
    if (session_) {
        session_->shutdown();
        session_.reset();
    }
    personality_.clear();
    style_.clear();
    supported_.clear();
    sleeping_ = false;
}
void AvatarHost::select(const Personality* package, std::int64_t now) {
    shutdown(); // Cancels/joins the previous identity; no old frame can reappear.
    if (!package || !package->avatar_available || package->avatar_style.empty())
        return;
    personality_ = package->id;
    style_ = package->avatar_style;
    supported_ = package->avatar_states;
    last_activity_ms_ = now;
    revision_ = 2;
    session_ = std::make_unique<avatar::Session>(package->avatar_registry.parent_path(), decoder_);
    session_->select(personality_, style_, now);
    session_->player().reduced_motion(reduced_);
    session_->player().first_frame_only(first_frame_only_);
    session_->player().hold_expressions(hold_expressions_);
    session_->player().reaction_playback(reaction_loops_, loop_until_event_);
    baseline_ = {awake_baseline_.expression, awake_baseline_.priority, revision_};
    session_->baseline(baseline_.expression, revision_, baseline_.priority, now, true);
}
void AvatarHost::observe(const Snapshot& snapshot, bool warning_visible, std::int64_t now) {
    auto desired = presentation_baseline(snapshot);
    if (warning_visible && desired.priority < 70)
        desired = {"concerned", 70, snapshot.revision};
    auto latest_severe = severe_sequence_;
    for (const auto& event : snapshot.events)
        if (event.severity == Severity::Error || event.severity == Severity::Critical)
            latest_severe = std::max(latest_severe, event.sequence);
    const bool changed = latest_severe != severe_sequence_ || snapshot.state != state_ ||
                         snapshot.scanning != scanning_ || warning_visible != warning_ ||
                         snapshot.active_profile != active_profile_;
    severe_sequence_ = latest_severe;
    state_ = snapshot.state;
    scanning_ = snapshot.scanning;
    warning_ = warning_visible;
    active_profile_ = snapshot.active_profile;
    awake_baseline_ = desired;
    if (!changed)
        return;
    last_activity_ms_ = now;
    sleeping_ = false;
    baseline_ = {desired.expression, desired.priority, ++revision_};
    if (session_)
        session_->baseline(baseline_.expression, revision_, baseline_.priority, now, true);
}
void AvatarHost::activity(std::int64_t now) {
    last_activity_ms_ = now;
    if (!session_ || !sleeping_)
        return;
    sleeping_ = false;
    baseline_ = {awake_baseline_.expression, awake_baseline_.priority, ++revision_};
    session_->baseline(baseline_.expression, revision_, baseline_.priority, now, true);
}
avatar::Decision AvatarHost::react(Message event, const Copy& copy, std::uint64_t occurrence,
                                   std::int64_t now) {
    if (!session_ || copy.personality_id != personality_ || copy.expression.empty())
        return {false, "no_owned_response"};
    if ((event == Message::Started || event == Message::Reconnected) && state_ != State::Running)
        return {false, "stale_success"};
    if (event == Message::ScanCompleted && scanning_)
        return {false, "stale_scan_completion"};
    activity(now);
    const int priority = event == Message::Invalid || event == Message::ScanFailed ? 100
                         : event == Message::Disconnected || event == Message::Recovering ||
                                 event == Message::Questionable
                             ? 80
                             : 10;
    if (baseline_.priority > priority)
        return {false, "operational_priority"};
    auto owns = [this](const std::string& state) {
        return std::find(supported_.begin(), supported_.end(), state) != supported_.end();
    };
    const auto expression = owns(copy.expression)            ? copy.expression
                            : owns(copy.fallback_expression) ? copy.fallback_expression
                                                             : std::string("idle");
    avatar::Reaction reaction{expression,
                              "dialogue:" + std::to_string(occurrence),
                              revision_,
                              now,
                              2000,
                              priority,
                              reaction_final_hold_ms};
    if (expression == "talking")
        reaction.total_plays = 4 + static_cast<int>(occurrence % 3);
    return session_->react(reaction, now);
}
void AvatarHost::tutorial_expression(const std::string& expression, std::uint64_t occurrence,
                                     std::int64_t now) {
    if (!session_)
        return;
    activity(now);
    const auto owned =
        std::find(supported_.begin(), supported_.end(), expression) != supported_.end();
    session_->player().reaction_playback(1, true);
    avatar::Reaction reaction{owned ? expression : "idle",
                              "tutorial:" + std::to_string(occurrence),
                              revision_,
                              now,
                              2000,
                              100,
                              0};
    if (owned && expression == "talking")
        reaction.total_plays = 4 + static_cast<int>(occurrence % 3);
    session_->react(reaction, now);
}
avatar::Presentation AvatarHost::tick(std::int64_t now) {
    if (!session_)
        return {};
    auto frame = session_->tick(now);
    // Only a quiet, healthy baseline may sleep. Telemetry refreshes do not
    // count as activity; active/pending reactions must finish before sleeping.
    if (!sleeping_ && awake_baseline_.expression == "idle" && awake_baseline_.priority == 0 &&
        (!frame.reaction || frame.holding || frame.looping) && !frame.loading &&
        now - last_activity_ms_ >= sleep_after_ms &&
        std::find(supported_.begin(), supported_.end(), "sleeping") != supported_.end()) {
        sleeping_ = true;
        baseline_ = {"sleeping", 0, ++revision_};
        session_->baseline(baseline_.expression, revision_, baseline_.priority, now, true);
        return session_->tick(now);
    }
    return frame;
}
void AvatarHost::first_frame_only(bool enabled) {
    first_frame_only_ = enabled;
    if (session_)
        session_->player().first_frame_only(enabled);
}
void AvatarHost::reduced_motion(bool enabled) {
    reduced_ = enabled;
    if (session_)
        session_->player().reduced_motion(enabled);
}
void AvatarHost::reaction_playback(int count, bool until_event) {
    if (count < 1 || count > 20)
        throw std::invalid_argument("Invalid animation loop count");
    reaction_loops_ = count;
    loop_until_event_ = until_event;
    if (session_)
        session_->player().reaction_playback(count, until_event);
}
void AvatarHost::hold_expressions(bool enabled) {
    hold_expressions_ = enabled;
    if (session_)
        session_->player().hold_expressions(enabled);
}
std::size_t AvatarHost::decoded_bytes() const {
    return session_ ? session_->decoded_bytes() : 0;
}
} // namespace audiohole::presentation
