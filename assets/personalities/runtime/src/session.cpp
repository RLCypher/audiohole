#include "audiohole/avatar/session.hpp"
#include <algorithm>

namespace audiohole::avatar {
Session::Session(std::filesystem::path root, Decoder decoder, LoadOptions options)
    : repository_(std::move(root), std::move(decoder)), options_(std::move(options)) {
    worker_ = std::thread([this] { work(); });
}
Session::~Session() {
    shutdown();
}
void Session::shutdown() {
    stopping_ = true;
    ready_.notify_all();
    if (worker_.joinable())
        worker_.join();
    std::lock_guard lock(mutex_);
    baseline_job_.reset();
    reaction_job_.reset();
    inspect_job_.reset();
    completed_.clear();
    player_ = Player{};
    last_valid_.reset();
}
bool Session::current(const Job& job) const {
    if (stopping_ || job.generation != generation_)
        return false;
    if (job.kind == Kind::Baseline)
        return job.serial == baseline_serial_;
    if (job.kind == Kind::Reaction)
        return job.serial == reaction_serial_;
    return job.serial == inspect_serial_;
}
void Session::queue(Job job) {
    if (stopping_)
        return;
    std::lock_guard lock(mutex_);
    if (job.kind == Kind::Baseline)
        baseline_job_ = std::move(job);
    else if (job.kind == Kind::Reaction)
        reaction_job_ = std::move(job);
    else
        inspect_job_ = std::move(job);
    ready_.notify_one();
}
void Session::select(std::string personality, std::string style, std::int64_t now) {
    ++generation_;
    ++baseline_serial_;
    ++reaction_serial_;
    ++inspect_serial_;
    {
        std::lock_guard lock(mutex_);
        baseline_job_.reset();
        reaction_job_.reset();
        inspect_job_.reset();
        completed_.clear();
    }
    diagnostics_.clear();
    states_.clear();
    styles_.clear();
    submitted_events_.clear();
    pending_priority_ = -1;
    last_valid_.reset();
    player_.reset(personality, style, now);
    baseline("idle", 1, 0, now);
}
Decision Session::baseline(std::string state, std::uint64_t revision, int priority,
                           std::int64_t now, bool invalidate_reaction) {
    auto result = player_.context(state, revision, priority, now, invalidate_reaction);
    if (!result.accepted)
        return last_decision_ = result;
    ++reaction_serial_;
    ++inspect_serial_;
    pending_priority_ = -1;
    {
        std::lock_guard lock(mutex_);
        reaction_job_.reset();
        inspect_job_.reset();
    }
    if (result.reason == "context_revision_updated" && player_.baseline_ready())
        return last_decision_ = result;
    queue({Kind::Baseline,
           generation_,
           ++baseline_serial_,
           revision,
           player_.personality(),
           player_.style(),
           std::move(state),
           false,
           {}});
    return last_decision_ = result;
}
Decision Session::react(Reaction event, std::int64_t now) {
    if (stopping_)
        return last_decision_ = {false, "shutdown"};
    if (std::find(submitted_events_.begin(), submitted_events_.end(), event.event_id) !=
        submitted_events_.end())
        return last_decision_ = {false, "duplicate_event"};
    if (event.revision != player_.revision() || event.occurred_ms < 0 || event.occurred_ms > now ||
        event.ttl_ms < 0 || now - event.occurred_ms > event.ttl_ms)
        return last_decision_ = {false, "stale_context_or_event"};
    if (pending_priority_ > event.priority)
        return last_decision_ = {false, "pending_higher_priority"};
    pending_priority_ = event.priority;
    submitted_events_.push_back(event.event_id);
    while (submitted_events_.size() > 128)
        submitted_events_.pop_front();
    Job job{Kind::Reaction,
            generation_,
            ++reaction_serial_,
            event.revision,
            player_.personality(),
            player_.style(),
            event.state,
            false,
            event};
    queue(std::move(job));
    return last_decision_ = {true, "loading_reaction"};
}
void Session::inspect(std::string state, bool static_only, std::int64_t) {
    ++reaction_serial_;
    pending_priority_ = -1;
    queue({Kind::Inspect,
           generation_,
           ++inspect_serial_,
           player_.revision(),
           player_.personality(),
           player_.style(),
           std::move(state),
           static_only,
           {}});
}
bool Session::loading() const {
    std::lock_guard lock(mutex_);
    return in_flight_ || baseline_job_ || reaction_job_ || inspect_job_;
}
void Session::work() {
    while (!stopping_) {
        Job job;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(
                lock, [&] { return stopping_ || baseline_job_ || reaction_job_ || inspect_job_; });
            if (stopping_)
                break;
            if (baseline_job_) {
                job = std::move(*baseline_job_);
                baseline_job_.reset();
            } else if (reaction_job_) {
                job = std::move(*reaction_job_);
                reaction_job_.reset();
            } else {
                job = std::move(*inspect_job_);
                inspect_job_.reset();
            }
            in_flight_ = 1;
        }
        auto options = options_;
        options.cancelled = [this, job] { return !current(job); };
        options.persistent_context = job.kind == Kind::Baseline;
        auto clip =
            repository_.load(job.personality, job.style, job.state, job.static_only, options);
        {
            std::lock_guard lock(mutex_);
            if (current(job)) {
                completed_.push_back({job, std::move(clip)});
                while (completed_.size() > 3)
                    completed_.pop_front();
            }
            in_flight_ = 0;
        }
    }
}
Presentation Session::tick(std::int64_t now) {
    if (stopping_) {
        Presentation hidden;
        hidden.source = "shutdown";
        return hidden;
    }
    std::deque<Completion> ready;
    {
        std::lock_guard lock(mutex_);
        ready.swap(completed_);
    }
    for (auto& completion : ready) {
        const auto& job = completion.job;
        if (!current(job) || job.revision != player_.revision())
            continue;
        states_ = completion.clip->states;
        styles_ = completion.clip->styles;
        diagnostics_ = completion.clip->diagnostics;
        if (job.kind == Kind::Baseline)
            player_.install_baseline(completion.clip, job.revision, now);
        else if (job.kind == Kind::Reaction) {
            last_decision_ = player_.react(completion.clip, job.event, now);
            pending_priority_ = -1;
        } else
            player_.inspect(completion.clip, now);
    }
    auto frame = player_.tick(now);
    frame.loading = loading();
    if (frame.image)
        last_valid_ = frame.image;
    else if (!frame.loading && last_valid_) {
        frame.image = last_valid_;
        frame.source = "last_valid_frame";
    }
    if (!diagnostics_.empty())
        frame.diagnostics = diagnostics_;
    return frame;
}
} // namespace audiohole::avatar
