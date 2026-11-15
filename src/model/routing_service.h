#pragma once
#include "audiohole/engine/runtime.hpp"
#include "model/controller.h"
#include <functional>

namespace audiohole {
// GUI/control-thread seam. The implementation owns Runtime on a dedicated
// non-audio control thread; readings contain values only, never stream buffers.
// Start acknowledges a queued command. Its later admission result and actual
// route state are distinct. Stop supersedes every older command/publication.
struct RoutingReading {
    std::uint64_t generation = 0, revision = 0;
    bool ready = false, pending = false;
    std::vector<engine::BackendDescriptor> backends;
    std::shared_ptr<const engine::RuntimeSnapshot> state;
    std::vector<engine::Issue> admission_issues;
};
class RoutingService {
  public:
    virtual ~RoutingService() = default;
    virtual std::uint64_t start(engine::Profile, engine::BindingSnapshot) = 0;
    virtual std::uint64_t stop() = 0;
    virtual bool update_bindings(engine::BindingSnapshot, bool resume_ready = true) = 0;
    virtual void configure_workers(engine::WorkerSettings) {}
    virtual RoutingReading reading() const = 0;
};
// The factory runs on the service's thread. Injection is for portable contract
// tests; ordinary application startup uses the real platform adapters in Low.
// Destroy only after a confirmed Stopped reading: Runtime destruction joins
// lifecycle ownership and driver APIs have no hard cancellation deadline.
using RuntimeFactory = std::function<std::unique_ptr<engine::Runtime>()>;
std::unique_ptr<RoutingService> make_routing_service(RuntimeFactory = {});
engine::Result<engine::Profile> routing_profile(const Profile&);
engine::BindingSnapshot routing_bindings(const Snapshot&);
State routing_state(engine::State);
std::string routing_issues(const std::vector<engine::Issue>&);
// Actual configurations/counters and access receipts, never acoustic latency.
std::string routing_progress(const Snapshot&);
std::string routing_diagnostics(const engine::RuntimeSnapshot&, const Profile*);
} // namespace audiohole
