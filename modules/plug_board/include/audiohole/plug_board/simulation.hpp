#pragma once

#include "audiohole/plug_board/module.hpp"

namespace audiohole::plug_board {

// Explicit test/demo backend. This translation unit has no Windows API calls and
// cannot construct the native policy adapter. Samples never become native proof.
// Control methods are thread-safe; the module still owns every backend call.
class Simulation final {
  public:
    struct State;
    Simulation();
    void set_outputs(RawEndpointInventory);
    void set_sessions(SessionSnapshot);
    void set_processes(std::vector<ProcessEvidence>);
    void set_images(std::map<std::string, ApplicationImage>);
    void process_started(ProcessEvidence);
    void set_capability(OperationResult);
    void reject_role(std::optional<AudioRole>);
    void block_policy(bool);
    std::size_t policy_calls() const;
    std::vector<std::optional<std::string>> requested_targets() const;

  private:
    std::shared_ptr<State> state_;
    friend std::unique_ptr<PlatformBackend> make_simulated_backend(std::shared_ptr<Simulation>);
};

std::unique_ptr<PlatformBackend> make_simulated_backend(std::shared_ptr<Simulation>);
std::shared_ptr<Simulation> make_demo_simulation();

} // namespace audiohole::plug_board
