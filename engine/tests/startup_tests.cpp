#include "access_negotiation.hpp"
#include "audiohole/engine/runtime.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace audiohole::engine;
using namespace audiohole::engine::detail;

namespace {
int checks = 0;
void expect(bool value, const char* message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class Predicate> void until(Predicate predicate) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= end)
            throw std::runtime_error("Startup lifecycle fixture timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// Lifecycle-only fixtures reproduce successful open followed by zero transfer.
// They exercise the real supervisor and negotiation receipts, not a device,
// driver or simulated Windows scheduling environment. None ships in the host.
struct Evidence {
    std::atomic<unsigned> opens{0}, live{0}, stopped{0};
    std::atomic<bool> release_entered{false}, allow_release{false};
    bool stall_shared = false, refuse_shared = false, block_first_release = false;
    bool fail_exclusive_start = false;
    bool fail_after_transfer = false;
};
class StartupSession final : public RouteSession {
  public:
    StartupSession(Evidence& evidence, NegotiatedRoute configuration)
        : evidence_(evidence), configuration_(std::move(configuration)) {
        ++evidence_.live;
    }
    ~StartupSession() override {
        stop();
    }
    NegotiatedRoute configuration() const override {
        return configuration_;
    }
    Result<bool> start(std::stop_token token) override {
        if (evidence_.fail_exclusive_start &&
            configuration_.destination.mode == AccessMode::Exclusive)
            return {std::nullopt,
                    {{ErrorCode::BackendFailure, "StartRender", "Fixture Start refusal",
                      "0x8889000A"}}};
        return token.stop_requested()
                   ? Result<bool>{std::nullopt, {{ErrorCode::Cancelled, "start", "Fixture stop"}}}
                   : Result<bool>{true, {}};
    }
    RouteObservation observe() override {
        RouteObservation result;
        if (evidence_.fail_after_transfer && captured_ >= 480) {
            result.issues.push_back(
                {ErrorCode::BackendFailure, "observe", "Fixture running-stream failure"});
            return result; // Deliberately omit counters AFTER previously valid progress.
        }
        result.captured_frames = (captured_ += 480);
        const bool stalled = (!evidence_.fail_after_transfer &&
                              configuration_.destination.mode == AccessMode::Exclusive) ||
                             evidence_.stall_shared;
        result.transferred_frames = stalled ? 0 : captured_;
        result.rendered_frames = stalled ? 480 : captured_;
        result.overruns = stalled ? captured_ / 480 : 0;
        return result;
    }
    void stop() noexcept override {
        if (stopped_)
            return;
        stopped_ = true;
        if (evidence_.block_first_release && evidence_.stopped == 0) {
            evidence_.release_entered = true;
            while (!evidence_.allow_release)
                std::this_thread::yield(); // Test-only, controlled by the owner thread below.
        }
        --evidence_.live;
        ++evidence_.stopped;
    }

  private:
    Evidence& evidence_;
    NegotiatedRoute configuration_;
    std::uint64_t captured_ = 0;
    bool stopped_ = false;
};
class StartupAdapter final : public BackendAdapter {
  public:
    explicit StartupAdapter(Evidence& evidence) : evidence_(evidence) {}
    BackendDescriptor descriptor() const override {
        return {BackendId::Wasapi, true, "STARTUP TEST FIXTURE, NO DEVICES"};
    }
    Result<std::unique_ptr<RouteSession>> open(const BoundRoute& route,
                                               std::stop_token token) override {
        if (evidence_.live != 0)
            return {std::nullopt,
                    {{ErrorCode::ContractViolation, "fixture", "Old ownership was not released"}}};
        ++evidence_.opens;
        NegotiatedRoute configuration;
        for (bool input : {true, false}) {
            const auto stream = negotiate_access(
                input ? route.request.source : route.request.destination,
                route.request.exclusive_mode_preferred, token, input ? "source" : "destination",
                [&](const EndpointRequirements& concrete) -> Result<StreamConfiguration> {
                    if (evidence_.refuse_shared && concrete.mode == AccessMode::Shared)
                        return {std::nullopt,
                                {{ErrorCode::BackendFailure, "InitializeAudioStream",
                                  "Fixture Shared refusal", "0x88890008"}}};
                    StreamConfiguration result;
                    result.mode = *concrete.mode;
                    result.client_format = {concrete.sample_rate_hz.value_or(48000),
                                            concrete.channel_count.value_or(2),
                                            concrete.sample_format.value_or(SampleFormat::Float32)};
                    result.timing_sample_rate_hz = result.client_format.sample_rate_hz;
                    result.period_frames = result.allocated_buffer_frames = 480;
                    result.applied_buffer_request_ms = concrete.buffer_ms;
                    return {result, {}};
                },
                [] {});
            if (!stream.ok())
                return {std::nullopt, stream.issues};
            (input ? configuration.source : configuration.destination) = *stream.value;
        }
        return {std::make_unique<StartupSession>(evidence_, std::move(configuration)), {}};
    }

  private:
    Evidence& evidence_;
};
Profile profile() {
    Profile result{"startup-test", "Startup fixture", "Preserve profile notes", {}};
    Hole hole;
    hole.id = "startup-hole";
    hole.forward.source = "capture-test";
    hole.forward.destination = "render-test";
    hole.forward.advanced_tuning = true;
    hole.forward.exclusive_mode_preferred = true;
    auto& common = hole.forward.preferences.common;
    common.sample_rate_hz = 48000;
    common.channel_count = 2;
    common.sample_format = SampleFormat::Float32;
    common.buffer_ms = 10;
    hole.forward.preferences.channel_map = {{0, 1}, {1, 0}};
    result.holes.push_back(hole);
    return result;
}
const BindingSnapshot bindings{
    1,
    {{"capture-test", "fixture-input", EndpointDirection::Capture, BindingState::Available},
     {"render-test", "fixture-output", EndpointDirection::Render, BindingState::Available}}};
std::vector<std::unique_ptr<BackendAdapter>> adapters(Evidence& evidence) {
    std::vector<std::unique_ptr<BackendAdapter>> result;
    result.push_back(std::make_unique<StartupAdapter>(evidence));
    return result;
}
void stop(Runtime& runtime, Evidence& evidence) {
    runtime.stop();
    until([&] { return runtime.snapshot().state == State::Stopped; });
    expect(!runtime.snapshot().armed && evidence.live == 0,
           "Stop joins all startup/retry ownership");
}

void startup_retry(bool force_source) {
    Evidence evidence;
    Runtime runtime(adapters(evidence));
    auto p = profile();
    if (force_source)
        p.holes[0].forward.preferences.source.mode = AccessMode::Exclusive;
    const auto original = prepare_profile(p);
    expect(original.ok() && runtime.start(p, bindings).ok(), "startup intent is admitted");
    until([&] {
        const auto snapshot = runtime.snapshot();
        return !snapshot.paths.empty() && snapshot.paths[0].progress_age_ms.value_or(0) >= 25;
    });
    expect(runtime.snapshot().paths[0].state == State::Starting,
           "zero-transfer startup has a visible advancing age before its deadline");
    until([&] { return runtime.snapshot().state == State::Running; });
    const auto snapshot = runtime.snapshot();
    const auto& actual = *snapshot.paths[0].negotiated;
    expect(evidence.opens == 2 && evidence.stopped == 1 && evidence.live == 1,
           "preferred startup stall releases the old route before exactly one reopen");
    expect(check_negotiated(original.value->routes[0], actual).ok(),
           "fallback retains explicit rate, format, buffer, map and forced source requirements");
    expect(actual.destination.mode == AccessMode::Shared && actual.destination.access_fell_back &&
               actual.destination.access_attempts.size() == 2 &&
               !actual.destination.access_attempts[0].succeeded &&
               actual.destination.access_attempts[0].issues[0].location ==
                   "destination/startup_progress" &&
               actual.destination.access_attempts[0].issues[0].native_code.empty() &&
               actual.destination.access_attempts[1].succeeded,
           "snapshot preserves failed Exclusive startup and successful Shared receipts");
    expect(actual.source.mode == (force_source ? AccessMode::Exclusive : AccessMode::Shared),
           "a forced source stays Exclusive during a preferred-destination retry");
    expect(p.holes[0].forward.preferences.common.mode == std::nullopt &&
               p.notes == "Preserve profile notes",
           "retry never rewrites profile intent or notes");
    stop(runtime, evidence);
}
void terminal_stalls(bool force_exclusive, bool shared) {
    Evidence evidence;
    evidence.stall_shared = true;
    Runtime runtime(adapters(evidence));
    auto p = profile();
    if (force_exclusive || shared)
        p.holes[0].forward.preferences.common.mode =
            shared ? AccessMode::Shared : AccessMode::Exclusive;
    expect(runtime.start(p, bindings).ok(), "zero-transfer terminal fixture starts");
    until([&] { return runtime.snapshot().state == State::Failed; });
    const auto snapshot = runtime.snapshot();
    expect(snapshot.armed && snapshot.paths[0].issues[0].location == "startup_progress" &&
               snapshot.paths[0].observation.transferred_frames == 0,
           "unusable startup becomes an actionable Failed state, even with captured frames");
    expect(evidence.opens == (force_exclusive || shared ? 1u : 2u) && evidence.live == 0,
           "strict modes never fall back and unsuccessful preferred retry never loops");
    stop(runtime, evidence);
}
void failed_retry() {
    Evidence evidence;
    evidence.refuse_shared = true;
    Runtime runtime(adapters(evidence));
    expect(runtime.start(profile(), bindings).ok(), "refused-retry fixture starts");
    until([&] { return runtime.snapshot().state == State::Failed; });
    const auto issues = runtime.snapshot().paths[0].issues;
    expect(evidence.opens == 2 && issues.size() >= 2 && issues[0].location == "startup_progress" &&
               issues[1].native_code == "0x88890008" && evidence.live == 0,
           "failed Shared reopen preserves both the startup cause and native refusal");
    stop(runtime, evidence);
}
void immediate_start_failure() {
    Evidence evidence;
    evidence.fail_exclusive_start = true;
    Runtime runtime(adapters(evidence));
    auto p = profile();
    expect(runtime.start(p, bindings).ok(), "native Start-failure fixture is admitted");
    until([&] { return runtime.snapshot().state == State::Running; });
    const auto snapshot = runtime.snapshot();
    expect(evidence.opens == 2 && evidence.stopped == 1 &&
               snapshot.paths[0].negotiated->destination.access_attempts[0].issues[0].native_code ==
                   "0x8889000A",
           "preferred Start refusal retries Shared with the actual failure code preserved");
    stop(runtime, evidence);
    p.holes[0].forward.preferences.common.mode = AccessMode::Exclusive;
    expect(runtime.start(p, bindings).ok(), "forced native Start-failure fixture is admitted");
    until([&] { return runtime.snapshot().state == State::Failed; });
    expect(evidence.opens == 3 && runtime.snapshot().paths[0].issues[0].native_code == "0x8889000A",
           "forced native Start refusal never triggers a Shared retry");
    stop(runtime, evidence);
}
void failure_after_transfer() {
    Evidence evidence;
    evidence.fail_after_transfer = true;
    Runtime runtime(adapters(evidence));
    expect(runtime.start(profile(), bindings).ok(), "flowing failure fixture starts");
    until([&] { return runtime.snapshot().state == State::Failed; });
    expect(evidence.opens == 1 && runtime.snapshot().paths[0].issues[0].location == "observe",
           "counter omission during a later failure never permits a live mode change");
    stop(runtime, evidence);
}
void stop_during_retry_release() {
    Evidence evidence;
    evidence.block_first_release = true;
    Runtime runtime(adapters(evidence));
    struct ReleaseGuard {
        Evidence& evidence;
        ~ReleaseGuard() {
            evidence.allow_release = true;
        }
    } release_guard{evidence}; // Unblocks cleanup even if an assertion throws.
    expect(runtime.start(profile(), bindings).ok(), "cancellable retry fixture starts");
    until([&] { return evidence.release_entered.load(); });
    runtime.stop();
    evidence.allow_release = true;
    until([&] { return runtime.snapshot().state == State::Stopped; });
    expect(evidence.opens == 1 && evidence.live == 0 && !runtime.snapshot().armed,
           "Stop arriving during old-route cleanup suppresses the pending reopen");
}
} // namespace

int main() {
    try {
        startup_retry(false);
        startup_retry(true);
        terminal_stalls(true, false);
        terminal_stalls(false, true);
        terminal_stalls(false, false);
        failed_retry();
        immediate_start_failure();
        failure_after_transfer();
        stop_during_retry_release();
        std::cout << checks << " startup lifecycle checks passed; no hardware I/O.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
