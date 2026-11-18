#include "audiohole/plug_board/simulation.hpp"

#include <algorithm>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace audiohole::plug_board {

struct Simulation::State {
    mutable std::mutex mutex;
    std::condition_variable_any policy_ready;
    RawEndpointInventory outputs;
    SessionSnapshot sessions;
    std::vector<ProcessEvidence> processes;
    std::map<std::string, ApplicationImage> images;
    BackendCallbacks callbacks;
    OperationResult capability{true, "simulated_interface",
                               "Simulation: routing interface available."};
    std::optional<AudioRole> rejected;
    bool blocked = false;
    bool visible = false;
    std::uint64_t generation = 0;
    std::set<std::string> observed_endpoints;
    std::vector<ApplicationIdentity> pending;
    PlatformMetrics metrics;
    std::vector<std::optional<std::string>> targets;
    std::map<std::pair<std::string, AudioRole>, std::optional<std::string>> preferences;
};
namespace {
std::string process_key(const ProcessToken& token) {
    return std::to_string(token.pid) + ':' + std::to_string(token.creation_time) + ':' +
           token.executable_path;
}
SessionSnapshot filtered(const Simulation::State& state) {
    auto snapshot = state.sessions;
    std::erase_if(snapshot.sessions, [&](const auto& session) {
        return !state.observed_endpoints.contains(session.endpoint_id);
    });
    return snapshot;
}

class SimulatedBackend final : public PlatformBackend {
  public:
    explicit SimulatedBackend(std::shared_ptr<Simulation::State> state)
        : state_(std::move(state)) {}
    bool simulated() const override {
        return true;
    }
    OperationResult initialize(BackendCallbacks callbacks) override {
        owner_ = std::this_thread::get_id();
        std::lock_guard lock(state_->mutex);
        state_->callbacks = std::move(callbacks);
        return {true, "simulated", "Explicit simulation initialized."};
    }
    OperationResult capability() override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        return state_->capability;
    }
    RawEndpointInventory inventory() override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        return state_->outputs;
    }
    std::array<std::optional<std::string>, 3> default_endpoints() override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        return state_->outputs.defaults;
    }
    void observe(bool visible, const EndpointInventory& inventory,
                 std::uint64_t generation) override {
        check_thread();
        BackendCallbacks callbacks;
        SessionSnapshot snapshot;
        {
            std::lock_guard lock(state_->mutex);
            state_->visible = visible;
            state_->generation = generation;
            state_->metrics.discovery_active = visible;
            state_->observed_endpoints.clear();
            for (const auto& endpoint : inventory.endpoints)
                if (endpoint.playback && endpoint.available && !endpoint.ambiguous)
                    state_->observed_endpoints.insert(endpoint.windows_id);
            if (!visible)
                return;
            ++state_->metrics.full_session_enumerations;
            callbacks = state_->callbacks;
            snapshot = filtered(*state_);
        }
        if (callbacks.observations)
            callbacks.observations(generation, std::move(snapshot), utc_timestamp());
    }
    void refresh_observation() override {
        check_thread();
        BackendCallbacks callbacks;
        SessionSnapshot snapshot;
        std::uint64_t generation;
        {
            std::lock_guard lock(state_->mutex);
            if (!state_->visible)
                return;
            ++state_->metrics.full_session_enumerations;
            callbacks = state_->callbacks;
            snapshot = filtered(*state_);
            generation = state_->generation;
        }
        if (callbacks.observations)
            callbacks.observations(generation, std::move(snapshot), utc_timestamp());
    }
    std::vector<ProcessEvidence>
    find_processes(const std::vector<ApplicationIdentity>& identities) override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        ++state_->metrics.process_snapshots;
        std::vector<ProcessEvidence> result;
        for (const auto& process : state_->processes)
            if (std::find(identities.begin(), identities.end(), process.identity) !=
                identities.end())
                result.push_back(process);
        return result;
    }
    void watch_processes(const std::vector<ApplicationIdentity>& identities) override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        state_->pending = identities;
        state_->metrics.process_watch_active = !identities.empty();
    }
    void request_icons(const std::vector<ApplicationObservation>& applications, unsigned pixels,
                       std::uint64_t generation) override {
        check_thread();
        BackendCallbacks callbacks;
        {
            std::lock_guard lock(state_->mutex);
            callbacks = state_->callbacks;
            state_->metrics.icon_extractions += applications.size();
        }
        for (const auto& application : applications)
            if (callbacks.icon_ready) {
                ApplicationImage image;
                bool supplied = false;
                {
                    std::lock_guard lock(state_->mutex);
                    const auto found = state_->images.find(application.identity.value);
                    if (found != state_->images.end()) {
                        image = found->second;
                        supplied = true;
                    }
                }
                if (supplied) {
                    callbacks.icon_ready(generation, application.observation_key, std::move(image));
                    continue;
                }
                image.width = pixels;
                image.height = pixels;
                image.rgba.resize(pixels * pixels * 4, 128);
                for (std::size_t index = 3; index < image.rgba.size(); index += 4)
                    image.rgba[index] = 255;
                image.source = "simulation_placeholder";
                image.result = {true, "simulated_icon",
                                "Simulation placeholder; no real Windows icon was queried."};
                callbacks.icon_ready(generation, application.observation_key, std::move(image));
            }
    }
    LivePolicyReport apply_policy(const std::vector<ProcessToken>& processes,
                                  const std::optional<std::string>& target,
                                  std::uint64_t generation, std::stop_token stop) override {
        check_thread();
        std::unique_lock lock(state_->mutex);
        state_->targets.push_back(target);
        state_->policy_ready.wait(lock, stop, [&] { return !state_->blocked; });
        LivePolicyReport result;
        result.generation = generation;
        result.target_windows_id = target;
        result.operation_id = new_id("simulated-policy");
        std::size_t accepted = 0;
        for (const auto& process : processes)
            for (const auto role : render_roles) {
                if (stop.stop_requested()) {
                    result.state = PolicyState::Partial;
                    result.detail = "Simulation: superseded policy work stopped cooperatively.";
                    return result;
                }
                RolePolicyResult row;
                row.process = process;
                row.role = role;
                const auto live =
                    std::find_if(state_->processes.begin(), state_->processes.end(),
                                 [&](const auto& evidence) { return evidence.token == process; });
                if (live == state_->processes.end()) {
                    result.state = PolicyState::PendingProcess;
                    result.detail = "Simulation: the verified process is no longer available.";
                    return result;
                }
                auto& current = state_->preferences[{process_key(process), role}];
                ++state_->metrics.policy_reads;
                row.before = {{true, "simulated_read", {}}, current};
                if (current != target) {
                    row.write_attempted = true;
                    ++state_->metrics.policy_writes;
                    if (state_->rejected == role)
                        row.write = {false, "simulated_rejection",
                                     "Simulation: selected role rejected the preference."};
                    else {
                        current = target;
                        row.write = {true, "simulated_write", {}};
                    }
                    ++state_->metrics.policy_reads;
                } else
                    row.write = {true, "already_requested",
                                 "Simulation: preference already matches."};
                row.readback = {{true, "simulated_readback", {}}, current};
                row.accepted = row.write.ok && current == target;
                if (row.accepted)
                    ++accepted;
                result.roles.push_back(std::move(row));
            }
        result.state = !result.roles.empty() && accepted == result.roles.size()
                           ? PolicyState::Accepted
                       : accepted ? PolicyState::Partial
                                  : PolicyState::Rejected;
        result.detail =
            result.state == PolicyState::Accepted
                ? "Simulation: preferences accepted; playback observations remain independent."
                : "Simulation: one or more role preferences were rejected.";
        return result;
    }
    PlatformMetrics metrics(bool) override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        return state_->metrics;
    }
    void shutdown() override {
        check_thread();
        std::lock_guard lock(state_->mutex);
        state_->callbacks = {};
        state_->visible = false;
        state_->metrics.discovery_active = false;
        state_->metrics.process_watch_active = false;
    }

  private:
    std::shared_ptr<Simulation::State> state_;
    std::thread::id owner_;
    void check_thread() const {
        if (std::this_thread::get_id() != owner_)
            throw std::logic_error("Backend called outside its owned control thread.");
    }
};
} // namespace

Simulation::Simulation() : state_(std::make_shared<State>()) {}
void Simulation::set_outputs(RawEndpointInventory outputs) {
    std::function<void()> notify;
    {
        std::lock_guard lock(state_->mutex);
        state_->outputs = std::move(outputs);
        notify = state_->callbacks.inventory_changed;
    }
    if (notify)
        notify();
}
void Simulation::set_sessions(SessionSnapshot sessions) {
    BackendCallbacks callbacks;
    SessionSnapshot snapshot;
    std::uint64_t generation;
    {
        std::lock_guard lock(state_->mutex);
        state_->sessions = std::move(sessions);
        if (!state_->visible)
            return;
        ++state_->metrics.session_events;
        callbacks = state_->callbacks;
        snapshot = filtered(*state_);
        generation = state_->generation;
    }
    if (callbacks.observations)
        callbacks.observations(generation, std::move(snapshot), utc_timestamp());
}
void Simulation::set_processes(std::vector<ProcessEvidence> processes) {
    std::lock_guard lock(state_->mutex);
    state_->processes = std::move(processes);
}
void Simulation::process_started(ProcessEvidence process) {
    std::function<void(ProcessEvidence)> notify;
    {
        std::lock_guard lock(state_->mutex);
        state_->processes.push_back(process);
        if (std::find(state_->pending.begin(), state_->pending.end(), process.identity) !=
            state_->pending.end())
            notify = state_->callbacks.process_started;
    }
    if (notify)
        notify(std::move(process));
}
void Simulation::set_capability(OperationResult capability) {
    std::lock_guard lock(state_->mutex);
    state_->capability = std::move(capability);
}
void Simulation::reject_role(std::optional<AudioRole> role) {
    std::lock_guard lock(state_->mutex);
    state_->rejected = role;
}
void Simulation::block_policy(bool blocked) {
    {
        std::lock_guard lock(state_->mutex);
        state_->blocked = blocked;
    }
    state_->policy_ready.notify_all();
}
std::size_t Simulation::policy_calls() const {
    std::lock_guard lock(state_->mutex);
    return state_->targets.size();
}
std::vector<std::optional<std::string>> Simulation::requested_targets() const {
    std::lock_guard lock(state_->mutex);
    return state_->targets;
}
std::unique_ptr<PlatformBackend> make_simulated_backend(std::shared_ptr<Simulation> simulation) {
    return std::make_unique<SimulatedBackend>(simulation->state_);
}

void Simulation::set_images(std::map<std::string, ApplicationImage> images) {
    std::lock_guard lock(state_->mutex);
    state_->images = std::move(images);
}

std::shared_ptr<Simulation> make_demo_simulation() {
    auto simulation = std::make_shared<Simulation>();
    RawEndpointInventory outputs;
    outputs.endpoints = {{"sample-speakers", "Speakers (sample)", true},
                         {"sample-headphones", "Headphones (sample)", true},
                         {"sample-music", "Music (sample)", true}};
    outputs.defaults = {"sample-speakers", "sample-speakers", "sample-headphones"};
    simulation->set_outputs(outputs);
    SessionSnapshot snapshot;
    std::vector<ProcessEvidence> processes;
    auto add = [&](const std::string& name, const std::string& executable, unsigned pid,
                   const std::string& endpoint, SessionState state) {
        SessionObservation session;
        session.endpoint_id = endpoint;
        session.instance_id = executable + ':' + endpoint;
        session.display_name = name;
        session.identity = {IdentityKind::Win32Image, "C:\\Sample\\" + executable};
        session.process = ProcessToken{pid, 100000 + pid, session.identity.value};
        session.state = state;
        if (std::none_of(processes.begin(), processes.end(),
                         [&](const auto& value) { return value.token.pid == pid; }))
            processes.push_back({session.identity, *session.process});
        snapshot.sessions.push_back(std::move(session));
    };
    add("Discord (sample)", "Discord.exe", 10, "sample-headphones", SessionState::Active);
    add("Discord (sample)", "Discord.exe", 10, "sample-speakers", SessionState::Inactive);
    add("Game (sample)", "Game.exe", 20, "sample-speakers", SessionState::Active);
    add("Player (sample)", "Player.exe", 30, "sample-music", SessionState::Inactive);
    simulation->set_processes(processes);
    simulation->set_sessions(snapshot);
    return simulation;
}

} // namespace audiohole::plug_board
