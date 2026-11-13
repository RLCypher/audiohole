#include "audiohole/engine/runtime.hpp"
#include "access_negotiation.hpp"
#include "stream_pipeline.hpp"

#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace audiohole::engine {
namespace {
// Validation watchdog, not an accepted production latency/quality threshold.
// Two seconds substantially exceeds this prototype's supported priming budget
// (queue_target_milliseconds rejects >1 s). It bounds a live zero-transfer
// session after start() admission, not a potentially blocking driver API call.
constexpr auto startup_progress_deadline = std::chrono::seconds(2);
} // namespace
const char* name(State state) noexcept {
    switch (state) {
    case State::Stopped:
        return "Stopped";
    case State::Ready:
        return "Ready";
    case State::Starting:
        return "Starting";
    case State::Running:
        return "Running";
    case State::Degraded:
        return "Degraded";
    case State::Recovering:
        return "Recovering";
    case State::Failed:
        return "Failed";
    case State::Stopping:
        return "Stopping";
    }
    return "Invalid";
}

struct Runtime::Impl {
    const std::thread::id owner = std::this_thread::get_id();
    mutable std::mutex mutex;
    RuntimeSnapshot state;
    std::vector<std::unique_ptr<BackendAdapter>> adapters;
    std::jthread worker;

    explicit Impl(LatencyPreset preset) {
        if (auto native = detail::make_wasapi_adapter(false, preset))
            adapters.push_back(std::move(native));
        if (auto library = detail::make_miniaudio_adapter())
            adapters.push_back(std::move(library));
        if (auto experimental = detail::make_wasapi_adapter(true, preset))
            adapters.push_back(std::move(experimental));
    }
    explicit Impl(std::vector<std::unique_ptr<BackendAdapter>> supplied)
        : adapters(std::move(supplied)) {
        for (const auto& adapter : adapters)
            if (!adapter)
                throw std::invalid_argument("Null backend adapter registration.");
    }
    ~Impl() {
        // Join before destroying adapters, snapshots, locks or this object.
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
    }
    void check_thread() const {
        if (std::this_thread::get_id() != owner)
            throw std::logic_error("Engine commands/snapshots require the owning control thread.");
    }
    std::vector<BackendDescriptor> descriptors() const {
        std::vector<BackendDescriptor> result;
        for (const auto id : {BackendId::Wasapi, BackendId::Miniaudio, BackendId::Experimental}) {
            BackendDescriptor description{id, false,
                                          "No adapter compiled for this platform/build."};
            for (const auto& adapter : adapters)
                if (adapter->descriptor().id == id)
                    description = adapter->descriptor();
            result.push_back(std::move(description));
        }
        return result;
    }
    BackendAdapter* adapter(BackendId id) {
        for (const auto& item : adapters)
            if (item->descriptor().id == id)
                return item.get();
        return nullptr;
    }
    void publish(RuntimeSnapshot snapshot, std::uint64_t generation) {
        std::lock_guard lock(mutex);
        if (state.operation != generation)
            return; // Future hosts can supersede safely.
        snapshot.revision = state.revision + 1;
        // A pending Stop must not be overwritten by late open/progress results.
        if (state.state == State::Stopping && snapshot.state != State::Stopped)
            snapshot.state = State::Stopping;
        state = std::move(snapshot);
    }
    void work(std::stop_token stop, std::uint64_t generation, PreparedProfile prepared,
              BindingSnapshot bindings) noexcept {
        RuntimeSnapshot next;
        next.operation = generation;
        next.profile_id = prepared.profile_id;
        next.armed = true;
        next.state = State::Starting;
        std::vector<std::unique_ptr<RouteSession>> sessions(prepared.routes.size());
        std::vector<std::uint64_t> previous_progress(prepared.routes.size());
        std::vector<std::chrono::steady_clock::time_point> progress_time(prepared.routes.size());
        std::vector<bool> startup_retried(prepared.routes.size(), false);
        next.paths.resize(prepared.routes.size());
        try {
            const auto available = descriptors();
            // Used for initial activation and at most one PRE-transfer Shared
            // retry. This is lifecycle-worker code: allocations and joining old
            // sessions never run on an endpoint's audio worker/callback.
            auto activate = [&](std::size_t i, const RouteRequest& request,
                                const NegotiatedRoute* previous = nullptr,
                                const Issue* startup_failure = nullptr) {
                auto& path = next.paths[i];
                path.hole_id = request.hole_id;
                path.direction = request.direction;
                path.state = State::Starting;
                path.issues.clear();
                path.progress_age_ms.reset();
                path.observation = {};
                previous_progress[i] = 0;
                publish(next, generation);
                if (stop.stop_requested())
                    return;
                auto bound = bind_route(request, bindings);
                if (!bound.ok()) {
                    path.state = State::Ready;
                    path.issues = std::move(bound.issues);
                    return;
                }
                const auto selected = select_backend(request.backend, available);
                if (!selected.ok()) {
                    path.state = State::Failed;
                    path.issues = selected.issues;
                    return;
                }
                auto opened = adapter(*selected.value)->open(*bound.value, stop);
                if (!opened.ok()) {
                    path.state = State::Failed;
                    path.issues = std::move(opened.issues);
                    if (startup_failure)
                        path.issues.insert(path.issues.begin(), *startup_failure);
                    return;
                }
                sessions[i] = std::move(*opened.value);
                if (stop.stop_requested())
                    return; // Terminal cleanup releases it without starting.
                auto configuration = sessions[i]->configuration();
                configuration.preferred_clock_device = request.preferred_clock_device;
                auto checked = check_negotiated(request, configuration);
                if (checked.ok() && previous && startup_failure) {
                    checked = detail::record_startup_fallback(prepared.routes[i], *previous,
                                                              configuration, *startup_failure);
                }
                if (!checked.ok()) {
                    path.state = State::Failed;
                    path.issues = checked.issues;
                    sessions[i]->stop();
                    sessions[i].reset();
                    return;
                }
                path.negotiated = configuration;
                const auto started = sessions[i]->start(stop);
                if (!started.ok() || !started.value.value_or(false)) {
                    path.state = State::Failed;
                    path.issues = started.issues;
                    if (path.issues.empty())
                        path.issues.push_back(
                            {ErrorCode::ContractViolation, "start",
                             "Backend returned false without a failure explanation."});
                    sessions[i]->stop();
                    sessions[i].reset();
                }
                // Starts from actual backend admission for this route/retry.
                // No hardware-API timeout is implied; another slow route open
                // may delay observation of this deadline on the same supervisor.
                progress_time[i] = std::chrono::steady_clock::now();
            };
            auto retry_startup = [&](std::size_t i, const Issue& failure) {
                auto& path = next.paths[i];
                if (startup_retried[i] || !path.negotiated || stop.stop_requested())
                    return false;
                const auto previous = *path.negotiated;
                const auto retry = detail::startup_shared_retry(prepared.routes[i], previous);
                if (!retry)
                    return false;
                // Joining also covers a backend that reported a startup error
                // while its worker was unwinding. Never reopen before release.
                if (sessions[i]) {
                    sessions[i]->stop();
                    sessions[i].reset();
                }
                if (stop.stop_requested())
                    return false;
                startup_retried[i] = true;
                activate(i, *retry, &previous, &failure);
                return true;
            };
            for (std::size_t i = 0; i < prepared.routes.size() && !stop.stop_requested(); ++i) {
                activate(i, prepared.routes[i]);
                const auto& path = next.paths[i];
                if (path.state == State::Failed && !path.issues.empty() &&
                    path.issues[0].code == ErrorCode::BackendFailure) {
                    const auto failure = path.issues[0]; // activate() may replace the vector.
                    (void)retry_startup(i, failure);
                }
            }
            while (!stop.stop_requested()) {
                std::size_t running = 0, failed = 0, ready = 0, starting = 0, degraded = 0;
                for (std::size_t i = 0; i < next.paths.size(); ++i) {
                    auto& path = next.paths[i];
                    if (sessions[i]) {
                        path.observation = sessions[i]->observe();
                        const auto count = path.observation.transferred_frames.value_or(0);
                        if (count > previous_progress[i]) {
                            previous_progress[i] = count;
                            progress_time[i] = std::chrono::steady_clock::now();
                        }
                        // Error-only observations must not erase earlier real
                        // progress and incorrectly permit a live mode change.
                        const bool has_transferred = previous_progress[i] > 0;
                        path.progress_age_ms = static_cast<std::uint64_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - progress_time[i])
                                .count());
                        if (!path.observation.issues.empty()) {
                            const auto issues = path.observation.issues;
                            // A real native Start/service failure before the
                            // first transfer is also failed startup. Cancellation
                            // and contract violations never become mode retries.
                            if (!(!has_transferred && issues[0].code == ErrorCode::BackendFailure &&
                                  retry_startup(i, issues[0]))) {
                                path.state = State::Failed;
                                path.issues = issues;
                                if (sessions[i]) {
                                    sessions[i]->stop();
                                    sessions[i].reset();
                                }
                            }
                        } else if (!has_transferred &&
                                   std::chrono::steady_clock::now() - progress_time[i] >=
                                       startup_progress_deadline) {
                            const Issue failure{
                                ErrorCode::BackendFailure, "startup_progress",
                                "The stream opened but transferred no captured audio frames "
                                "within the 2000-ms validation startup deadline. Silence still "
                                "requires frame progress; inspect endpoint wake/queue counters."};
                            if (!retry_startup(i, failure)) {
                                if (sessions[i]) {
                                    sessions[i]->stop();
                                    sessions[i].reset();
                                }
                                path.state = State::Failed;
                                path.issues.push_back(failure);
                            }
                        } else if (has_transferred && path.progress_age_ms.value_or(0) < 1000) {
                            path.state = State::Running;
                        } else if (has_transferred) {
                            // A one-second reporting freshness window is a
                            // validation diagnostic, not an accepted production
                            // health threshold or measured transport timeout.
                            path.state = State::Degraded;
                        }
                    }
                    if (path.state == State::Running)
                        ++running;
                    else if (path.state == State::Failed)
                        ++failed;
                    else if (path.state == State::Degraded)
                        ++degraded;
                    else if (path.state == State::Starting)
                        ++starting;
                    else
                        ++ready;
                }
                next.state = running == next.paths.size() ? State::Running
                             : running || degraded        ? State::Degraded
                             : failed                     ? State::Failed
                             : starting                   ? State::Starting
                             : ready                      ? State::Ready
                                                          : State::Stopped;
                publish(next, generation);
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        } catch (const std::exception& error) {
            for (auto& path : next.paths) {
                path.state = State::Failed;
                path.issues.push_back({ErrorCode::BackendFailure, "runtime", error.what()});
            }
            next.state = State::Failed;
            publish(next, generation);
        } catch (...) {
            for (auto& path : next.paths) {
                path.state = State::Failed;
                path.issues.push_back(
                    {ErrorCode::BackendFailure, "runtime", "Unexpected worker failure."});
            }
            next.state = State::Failed;
            publish(next, generation);
        }
        if (!stop.stop_requested() && next.state == State::Failed) {
            for (auto& session : sessions)
                if (session)
                    session->stop();
            sessions.clear();
            // Failure keeps activation intent armed until explicit Stop. This
            // prototype reports it; automatic recovery is not implemented yet.
            while (!stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        // Terminal Stopped is published only AFTER all adapters have joined and
        // released streams. Stop has precedence over pending Start and telemetry.
        for (auto& session : sessions)
            if (session)
                session->stop();
        sessions.clear();
        next.armed = false;
        next.state = State::Stopped;
        for (auto& path : next.paths)
            path.state = State::Stopped;
        publish(std::move(next), generation);
    }
};

Runtime::Runtime() : Runtime(LatencyPreset::Baseline) {}
Runtime::Runtime(LatencyPreset preset) : impl_(std::make_unique<Impl>(preset)) {}
Runtime::Runtime(std::vector<std::unique_ptr<BackendAdapter>> supplied)
    : impl_(std::make_unique<Impl>(std::move(supplied))) {}
Runtime::~Runtime() = default;
std::vector<BackendDescriptor> Runtime::backends() const {
    impl_->check_thread();
    return impl_->descriptors();
}
RuntimeSnapshot Runtime::snapshot() const {
    impl_->check_thread();
    std::lock_guard lock(impl_->mutex);
    return impl_->state;
}
Result<std::uint64_t> Runtime::start(const Profile& profile, const BindingSnapshot& bindings) {
    impl_->check_thread();
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->state.armed || impl_->state.state == State::Stopping)
            return {std::nullopt,
                    {{ErrorCode::Busy, "runtime",
                      "Stop the current session before starting another profile."}}};
    }
    auto prepared = prepare_profile(profile);
    if (!prepared.ok())
        return {std::nullopt, prepared.issues};
    const auto available = impl_->descriptors();
    for (const auto& request : prepared.value->routes) {
        const auto selected = select_backend(request.backend, available);
        if (!selected.ok())
            return {std::nullopt, selected.issues};
    }
    // Group known same-logical capture requests without touching hardware. Multiple
    // consumers of one capture endpoint require Shared ownership for compatible
    // independent capture clients in this prototype. Preferred Exclusive can
    // fall back on this known conflict; forced Exclusive still rejects admission.
    // Different logical IDs claiming one physical binding remain Ambiguous in
    // bind_route; mode preference must not silently merge those identities.
    for (std::size_t i = 0; i < prepared.value->routes.size(); ++i)
        for (std::size_t j = i + 1; j < prepared.value->routes.size(); ++j) {
            auto& a = prepared.value->routes[i];
            auto& b = prepared.value->routes[j];
            const bool same_capture = a.source_id == b.source_id;
            const bool same_render = a.destination_id == b.destination_id;
            if (same_render)
                return {std::nullopt,
                        {{ErrorCode::ResourceConflict, "routes",
                          "This prototype does not mix multiple routes into one destination."}}};
            if (same_capture &&
                (a.source.mode == AccessMode::Exclusive || b.source.mode == AccessMode::Exclusive))
                return {std::nullopt,
                        {{ErrorCode::ResourceConflict, "routes",
                          "Enabled routes request incompatible exclusive ownership."}}};
            if (same_capture)
                a.shared_capture_required = b.shared_capture_required = true;
        }
    if (impl_->worker.joinable())
        impl_->worker.join(); // Previous cleanup already completed.
    std::uint64_t generation;
    {
        std::lock_guard lock(impl_->mutex);
        generation = impl_->state.operation + 1;
        impl_->state =
            RuntimeSnapshot{generation,
                            impl_->state.revision + 1,
                            profile.id,
                            prepared.value->routes.empty() ? State::Stopped : State::Starting,
                            !prepared.value->routes.empty(),
                            {}};
    }
    if (!prepared.value->routes.empty())
        impl_->worker = std::jthread(
            [this, generation, request = *prepared.value, bindings](std::stop_token stop) {
                impl_->work(stop, generation, request, bindings);
            });
    return {generation, {}};
}
void Runtime::stop() {
    impl_->check_thread();
    {
        std::lock_guard lock(impl_->mutex);
        if (!impl_->state.armed && impl_->state.state == State::Stopped)
            return;
        impl_->state.state = State::Stopping;
        ++impl_->state.revision;
    }
    if (impl_->worker.joinable())
        impl_->worker.request_stop();
}
} // namespace audiohole::engine
