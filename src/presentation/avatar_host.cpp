#include "presentation/avatar_host.h"
#include <algorithm>

namespace audiohole::presentation {
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
}
void AvatarHost::select(const Personality* package, std::int64_t now) {
    shutdown(); // Cancels/joins the previous identity; no old frame can reappear.
    if (!package || !package->avatar_available || package->avatar_style.empty())
        return;
    personality_ = package->id;
    style_ = package->avatar_style;
    supported_ = package->avatar_states;
    revision_ = 2;
    session_ = std::make_unique<avatar::Session>(package->avatar_registry.parent_path(), decoder_);
    session_->select(personality_, style_, now);
    session_->player().reduced_motion(reduced_);
    baseline_.revision = revision_;
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
    baseline_ = {desired.expression, desired.priority, revision_};
    if (!changed)
        return;
    baseline_.revision = ++revision_;
    if (session_)
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
    return session_->react(
        {expression, "dialogue:" + std::to_string(occurrence), revision_, now, 2000, priority},
        now);
}
avatar::Presentation AvatarHost::tick(std::int64_t now) {
    return session_ ? session_->tick(now) : avatar::Presentation{};
}
void AvatarHost::reduced_motion(bool enabled) {
    reduced_ = enabled;
    if (session_)
        session_->player().reduced_motion(enabled);
}
std::size_t AvatarHost::decoded_bytes() const {
    return session_ ? session_->decoded_bytes() : 0;
}
} // namespace audiohole::presentation
