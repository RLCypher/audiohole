#include "access_negotiation.hpp"
#include "audiohole/engine/runtime.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace audiohole::engine;
using audiohole::engine::detail::negotiate_access;

namespace {
int checks = 0;
void expect(bool value, const char* description) {
    ++checks;
    if (!value)
        throw std::runtime_error(description);
}

StreamConfiguration concrete_stream(const EndpointRequirements& required) {
    StreamConfiguration result;
    result.mode = *required.mode;
    result.client_format = {required.sample_rate_hz.value_or(48000),
                            required.channel_count.value_or(2),
                            required.sample_format.value_or(SampleFormat::Float32)};
    result.timing_sample_rate_hz = result.client_format.sample_rate_hz;
    result.allocated_buffer_frames = 480;
    result.applied_buffer_request_ms = required.buffer_ms;
    return result;
}

// Necessary lifecycle fixtures, never real or emulated hardware. The callback
// leaves a deliberately partial ownership token on failure to prove cleanup
// precedes a retry. The same policy helper is called by both Windows adapters.
struct Acquisition {
    int opens = 0, closes = 0;
    bool live = false, fail_exclusive = false, fail_shared = false;
    bool wrong_rate = false, wrong_buffer = false, wrong_mode = false;
    std::stop_source* cancel = nullptr;
    std::vector<AccessMode> modes;
    Result<StreamConfiguration> open(const EndpointRequirements& required) {
        expect(!live, "retry releases prior endpoint ownership before reopening");
        live = true;
        ++opens;
        modes.push_back(*required.mode);
        auto value = concrete_stream(required);
        if (*required.mode == AccessMode::Exclusive) {
            if (wrong_rate)
                value.client_format.sample_rate_hz = 44100;
            if (wrong_buffer)
                value.allocated_buffer_frames = 1056;
            if (wrong_mode)
                value.mode = AccessMode::Shared;
            if (cancel)
                cancel->request_stop();
            if (fail_exclusive)
                return {std::nullopt,
                        {{ErrorCode::BackendFailure, "InitializeAudioStream",
                          "Test-only exclusive refusal", "0x8889000A"}}};
        } else if (fail_shared) {
            return {std::nullopt,
                    {{ErrorCode::BackendFailure, "InitializeAudioStream",
                      "Test-only shared refusal", "0x88890008"}}};
        }
        return {value, {}};
    }
    void close() {
        live = false;
        ++closes;
    }
};

Result<StreamConfiguration> acquire(Acquisition& state, const EndpointRequirements& required,
                                    bool prefer, std::stop_token token = {},
                                    std::optional<Issue> conflict = std::nullopt) {
    return negotiate_access(
        required, prefer, token, "source", [&](const auto& r) { return state.open(r); },
        [&] { state.close(); }, conflict);
}

void access_contracts() {
    EndpointRequirements required;
    required.sample_rate_hz = 48000;
    required.channel_count = 2;
    required.sample_format = SampleFormat::Float32;
    required.buffer_ms = 10;
    const auto original = required;
    Acquisition success;
    auto result = acquire(success, required, true);
    expect(result.ok() && result.value->mode == AccessMode::Exclusive && success.opens == 1 &&
               success.closes == 0 && !result.value->access_fell_back &&
               result.value->access_attempts.size() == 1,
           "successful preferred Exclusive retains its one stopped acquisition");
    success.close();
    Acquisition fallback;
    fallback.fail_exclusive = true;
    const auto shared = acquire(fallback, required, true);
    expect(shared.ok() && shared.value->mode == AccessMode::Shared && fallback.opens == 2 &&
               fallback.closes == 1 && shared.value->access_fell_back &&
               shared.value->access_attempts[0].issues[0].native_code == "0x8889000A" &&
               !shared.value->access_attempts[0].succeeded &&
               shared.value->access_attempts[1].succeeded && required == original,
           "exclusive refusal is recorded and shared retries identical non-mode requirements");
    fallback.close();
    Acquisition both;
    both.fail_exclusive = both.fail_shared = true;
    result = acquire(both, required, true);
    expect(!result.ok() && result.issues.size() == 2 && both.opens == 2 && !both.live &&
               result.issues[0].location.find("Exclusive") != std::string::npos &&
               result.issues[1].location.find("Shared") != std::string::npos,
           "both failed attempts return complete structured errors with no retained ownership");

    required.mode = AccessMode::Exclusive;
    Acquisition forced;
    forced.fail_exclusive = true;
    result = acquire(forced, required, true);
    expect(!result.ok() && forced.opens == 1 && forced.closes == 1 &&
               forced.modes[0] == AccessMode::Exclusive &&
               result.issues[0].native_code == "0x8889000A",
           "forced Exclusive failure never tries Shared despite the preference flag");
    required.mode = AccessMode::Shared;
    Acquisition force_shared;
    result = acquire(force_shared, required, true);
    expect(result.ok() && force_shared.modes == std::vector<AccessMode>{AccessMode::Shared} &&
               result.value->access_policy == AccessPolicy::SharedRequired &&
               !result.value->access_fell_back,
           "forced Shared wins over exclusive preference");
    force_shared.close();
    required.mode.reset();
    Acquisition automatic;
    result = acquire(automatic, required, false);
    expect(result.ok() && automatic.modes == std::vector<AccessMode>{AccessMode::Shared},
           "Automatic without preference retains Shared behavior");
    automatic.close();

    for (int mismatch = 0; mismatch < 3; ++mismatch) {
        Acquisition altered;
        altered.wrong_rate = mismatch == 0;
        altered.wrong_buffer = mismatch == 1;
        altered.wrong_mode = mismatch == 2;
        result = acquire(altered, required, true);
        expect(
            result.ok() && result.value->access_fell_back && altered.closes == 1 &&
                result.value->client_format.sample_rate_hz == 48000 &&
                result.value->allocated_buffer_frames == 480 &&
                !result.value->access_attempts[0].issues.empty(),
            "an incompatible Exclusive tuple is released; Shared still honors every requirement");
        altered.close();
    }

    std::stop_source stopped;
    stopped.request_stop();
    Acquisition cancelled;
    result = acquire(cancelled, required, true, stopped.get_token());
    expect(!result.ok() && cancelled.opens == 0 && result.issues[0].code == ErrorCode::Cancelled,
           "pre-cancelled acquisition does not touch an endpoint");
    for (bool fails : {false, true}) {
        std::stop_source during;
        Acquisition mid;
        mid.cancel = &during;
        mid.fail_exclusive = fails;
        result = acquire(mid, required, true, during.get_token());
        expect(!result.ok() && mid.opens == 1 && mid.closes == 1 && !mid.live &&
                   result.issues[0].code == ErrorCode::Cancelled,
               "Stop after native success or refusal releases resources without a Shared retry");
    }
    Issue ownership{ErrorCode::ResourceConflict, "ownership", "Test-only capture reservation"};
    Acquisition reserved;
    result = acquire(reserved, required, true, {}, ownership);
    expect(result.ok() && reserved.modes == std::vector<AccessMode>{AccessMode::Shared} &&
               !result.value->access_attempts[0].native_open_attempted &&
               result.value->access_attempts[0].issues[0].code == ErrorCode::ResourceConflict,
           "known internal capture conflict is a reported preflight refusal, not a fabricated "
           "driver call");
    reserved.close();

    RouteRequest route;
    route.exclusive_mode_preferred = true;
    route.source = route.destination = required;
    NegotiatedRoute actual;
    actual.source = *shared.value;
    Acquisition exclusive;
    actual.destination = *acquire(exclusive, required, true).value;
    expect(check_negotiated(route, actual).ok(),
           "one endpoint may fall back while its sibling remains Exclusive");
    actual.source.access_attempts.clear();
    expect(!check_negotiated(route, actual).ok(), "unreported preferred fallback is rejected");
    actual.source = *shared.value;
    actual.clock_preference_applied = true;
    expect(!check_negotiated(route, actual).ok(),
           "saved clock metadata cannot masquerade as applied clock control");
    exclusive.close();
}

struct LifecycleEvidence {
    std::atomic<int> shared_source_attempts{0}, exclusive_source_attempts{0};
    std::atomic<int> reserved_capture_requests{0};
    bool refuse_exclusive = true;
};

class PolicySession final : public RouteSession {
  public:
    explicit PolicySession(NegotiatedRoute configuration)
        : configuration_(std::move(configuration)) {}
    NegotiatedRoute configuration() const override {
        return configuration_;
    }
    Result<bool> start(std::stop_token stop) override {
        return stop.stop_requested()
                   ? Result<bool>{std::nullopt,
                                  {{ErrorCode::Cancelled, "start", "Test-only cancellation"}}}
                   : Result<bool>{true, {}};
    }
    RouteObservation observe() override {
        RouteObservation result;
        result.transferred_frames = (frames_ += 480);
        return result;
    }
    void stop() noexcept override {}

  private:
    NegotiatedRoute configuration_;
    std::uint64_t frames_ = 0;
};

class PolicyAdapter final : public BackendAdapter {
  public:
    explicit PolicyAdapter(LifecycleEvidence& evidence) : evidence_(evidence) {}
    BackendDescriptor descriptor() const override {
        return {BackendId::Wasapi, true, "TEST ONLY; NO DEVICES"};
    }
    Result<std::unique_ptr<RouteSession>> open(const BoundRoute& route,
                                               std::stop_token stop) override {
        NegotiatedRoute configuration;
        for (bool input : {true, false}) {
            std::optional<Issue> conflict;
            if (input && route.request.shared_capture_required) {
                ++evidence_.reserved_capture_requests;
                conflict = Issue{ErrorCode::ResourceConflict, "ownership",
                                 "Test-only capture reservation"};
            }
            const auto opened = negotiate_access(
                input ? route.request.source : route.request.destination,
                route.request.exclusive_mode_preferred, stop, input ? "source" : "destination",
                [&](const EndpointRequirements& concrete) -> Result<StreamConfiguration> {
                    if (input) {
                        if (concrete.mode == AccessMode::Exclusive)
                            ++evidence_.exclusive_source_attempts;
                        else
                            ++evidence_.shared_source_attempts;
                    }
                    if (evidence_.refuse_exclusive && concrete.mode == AccessMode::Exclusive)
                        return {std::nullopt,
                                {{ErrorCode::BackendFailure, "InitializeAudioStream",
                                  "Test-only Exclusive refusal", "0x8889000A"}}};
                    return {concrete_stream(concrete), {}};
                },
                [] {}, conflict);
            if (!opened.ok())
                return {std::nullopt, opened.issues};
            (input ? configuration.source : configuration.destination) = *opened.value;
        }
        return {std::make_unique<PolicySession>(std::move(configuration)), {}};
    }

  private:
    LifecycleEvidence& evidence_;
};

template <class Predicate> void until(Predicate predicate) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= end)
            throw std::runtime_error("Access lifecycle timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void startup_receipts() {
    RouteRequest request;
    request.exclusive_mode_preferred = true;
    Acquisition source, destination;
    source.fail_exclusive = true;
    NegotiatedRoute previous;
    previous.source = *acquire(source, request.source, true).value;
    previous.destination = *acquire(destination, request.destination, true).value;
    const auto retry = audiohole::engine::detail::startup_shared_retry(request, previous);
    expect(retry && retry->source.mode == AccessMode::Shared &&
               retry->destination.mode == AccessMode::Shared,
           "startup retry retains an already-Shared endpoint instead of probing Exclusive again");
    source.close();
    destination.close();
    auto current = previous;
    current.source = *acquire(source, retry->source, true).value;
    current.destination = *acquire(destination, retry->destination, true).value;
    const Issue failure{ErrorCode::BackendFailure, "startup_progress", "Fixture zero transfer"};
    const auto receipt =
        audiohole::engine::detail::record_startup_fallback(request, previous, current, failure);
    expect(receipt.ok() &&
               current.source.access_attempts[0].issues[0].native_code == "0x8889000A" &&
               current.destination.access_attempts[0].issues[0].location ==
                   "destination/startup_progress",
           "startup receipt preserves the actual earlier open refusal and the new startup cause");
    current.destination.access_attempts.clear();
    expect(!audiohole::engine::detail::record_startup_fallback(request, previous, current, failure)
                .ok(),
           "a retry without auditable open receipts is rejected safely");
}

void lifecycle_errors_and_ownership() {
    Profile profile{"profile-test", "Access lifecycle", "", {}};
    Hole h;
    h.id = "hole-test";
    h.duplex = true;
    h.forward.source = "capture-a";
    h.forward.destination = "render-a";
    h.forward.exclusive_mode_preferred = true;
    h.reverse.source = "capture-b";
    h.reverse.destination = "render-b";
    h.reverse.advanced_tuning = true;
    h.reverse.exclusive_mode_preferred = true;
    h.reverse.preferences.common.mode = AccessMode::Exclusive;
    profile.holes.push_back(h);
    BindingSnapshot bindings{
        1,
        {{"capture-a", "native-capture-a", EndpointDirection::Capture, BindingState::Available},
         {"capture-b", "native-capture-b", EndpointDirection::Capture, BindingState::Available},
         {"render-a", "native-render-a", EndpointDirection::Render, BindingState::Available},
         {"render-b", "native-render-b", EndpointDirection::Render, BindingState::Available}}};
    LifecycleEvidence evidence;
    std::vector<std::unique_ptr<BackendAdapter>> adapters;
    adapters.push_back(std::make_unique<PolicyAdapter>(evidence));
    Runtime runtime(std::move(adapters));
    expect(runtime.start(profile, bindings).ok(),
           "valid duplex intent is admitted before native acquisition");
    until([&] { return runtime.snapshot().state == State::Degraded; });
    const auto snapshot = runtime.snapshot();
    expect(
        snapshot.paths[0].state == State::Running && snapshot.paths[1].state == State::Failed &&
            snapshot.paths[1].issues[0].native_code == "0x8889000A" &&
            snapshot.paths[1].issues[0].location.find("Exclusive") != std::string::npos &&
            evidence.shared_source_attempts == 1,
        "GUI snapshot preserves the healthy preferred route and reports forced-Exclusive failure");
    runtime.stop();
    until([&] { return runtime.snapshot().state == State::Stopped; });
    profile.holes[0].duplex = false;
    profile.holes[0].forward.advanced_tuning = true;
    profile.holes[0].forward.preferences.common.mode = AccessMode::Exclusive;
    expect(runtime.start(profile, bindings).ok(), "forced Exclusive simplex starts acquisition");
    until([&] { return runtime.snapshot().state == State::Failed; });
    expect(
        runtime.snapshot().paths[0].state == State::Failed && runtime.snapshot().armed,
        "forced Exclusive acquisition failure produces Failed while preserving intent until Stop");
    runtime.stop();
    until([&] { return runtime.snapshot().state == State::Stopped; });

    // Same logical capture in both directions: use compatible Shared
    // clients. This verifies planning only, never claims a hardware fanout run.
    profile.holes[0] = h;
    profile.holes[0].reverse.advanced_tuning = false;
    profile.holes[0].reverse.source = profile.holes[0].forward.source;
    expect(runtime.start(profile, bindings).ok(),
           "preferred ownership can resolve known repeated capture use");
    until([&] { return runtime.snapshot().state == State::Running; });
    const auto shared = runtime.snapshot();
    expect(evidence.reserved_capture_requests == 2 &&
               shared.paths[0].negotiated->source.mode == AccessMode::Shared &&
               !shared.paths[0].negotiated->source.access_attempts[0].native_open_attempted &&
               shared.paths[1].negotiated->source.mode == AccessMode::Shared,
           "known capture conflict falls back explicitly before incompatible Exclusive ownership");
    runtime.stop();
    until([&] { return runtime.snapshot().state == State::Stopped; });
    profile.holes[0].reverse.advanced_tuning = true;
    const auto conflict = runtime.start(profile, bindings);
    expect(!conflict.ok() && conflict.issues[0].code == ErrorCode::ResourceConflict,
           "a forced Exclusive claim on a reused physical capture rejects admission");
}
} // namespace

int main() {
    try {
        access_contracts();
        startup_receipts();
        lifecycle_errors_and_ownership();
        std::cout << checks << " portable access-policy checks passed; no hardware I/O.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
