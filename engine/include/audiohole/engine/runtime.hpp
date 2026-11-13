#pragma once

#include "backend.hpp"
#include <memory>

namespace audiohole::engine {

enum class State { Stopped, Ready, Starting, Running, Degraded, Recovering, Failed, Stopping };
const char* name(State) noexcept;

struct PathSnapshot {
    std::string hole_id;
    DirectionId direction = DirectionId::Forward;
    State state = State::Ready;
    std::optional<NegotiatedRoute> negotiated;
    RouteObservation observation;
    // Age from start admission before first transfer, then since the last
    // advancing transfer count. This is not end-to-end audio latency.
    std::optional<std::uint64_t> progress_age_ms;
    std::vector<Issue> issues;
};

struct RuntimeSnapshot {
    std::uint64_t operation = 0;
    std::uint64_t revision = 0;
    std::string profile_id;
    State state = State::Stopped;
    bool armed = false;
    std::vector<PathSnapshot> paths;
};

// Standalone module: no GUI/simulator link, no public transport, no JSON on its
// workers. The host parses/prepares on its own non-audio thread and supplies an
// immutable binding snapshot. Commands and snapshots belong to the thread that
// constructed Runtime. Native opening/polling/cleanup run on owned workers.
//
// start() returns command admission, not flowing audio. stop() requests cleanup;
// wait for snapshot.state == Stopped before a new Start. Destruction joins all
// work. Driver API calls are cooperatively cancellable only; no unsafe detached
// threads or hard cleanup deadline is claimed. Snapshots allocate/copy and must
// never be polled from an audio callback.
//
// Validation prototype limitations: explicit Stop/Start is required for edits,
// changed bindings or device recovery. This does not replace the GUI supervisor
// or implement transactional profile switching. Healthy sibling paths continue.
// Before first transfer, a backend startup failure or 2-s zero-transfer watchdog
// can retry unforced preferred Exclusive endpoints in Shared, once per route,
// after joining old ownership. Strict modes fail instead. There is no mode
// change after first transfer; the timeout is validation policy, not a production
// SLA. Driver opening/cleanup can block cooperative cancellation beyond it.
class Runtime {
  public:
    Runtime();
    // Validation-only native scheduling comparison; default remains Baseline.
    // Miniaudio retains its existing settings under either preset.
    explicit Runtime(LatencyPreset);
    // Optional adapter injection for module hosts and portable lifecycle tests.
    // The validation executable uses the default real-platform factory only.
    explicit Runtime(std::vector<std::unique_ptr<BackendAdapter>>);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    std::vector<BackendDescriptor> backends() const;
    Result<std::uint64_t> start(const Profile&, const BindingSnapshot&);
    void stop();
    RuntimeSnapshot snapshot() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace audiohole::engine
