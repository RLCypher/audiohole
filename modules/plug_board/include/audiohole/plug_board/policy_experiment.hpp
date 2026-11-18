#pragma once

#include "audiohole/plug_board/observations.hpp"

#include <array>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace audiohole::plug_board {

enum class AudioRole { Console, Multimedia, Communications };
inline constexpr std::array<AudioRole, 3> render_roles{AudioRole::Console, AudioRole::Multimedia,
                                                       AudioRole::Communications};
const char* role_name(AudioRole);

// Success means only that the named operation succeeded. A preference setter's
// HRESULT never proves playback moved; observation is reported independently.
struct OperationResult {
    bool ok = false;
    std::string code;
    std::string detail;
};
struct PreferenceRead {
    OperationResult result;
    // nullopt is Windows default, distinct from a failed/unknown read.
    std::optional<std::string> endpoint_id;
};

// Non-UI, non-audio, single-owner-thread interface. Implementations verify the
// exact token on every read/write and restrict writes to an explicit allowlist.
// Windows IDs are local adapter values, never portable device identities.
class PolicyAdapter {
  public:
    virtual ~PolicyAdapter() = default;
    virtual bool simulated() const = 0;
    virtual OperationResult capability() = 0;
    virtual OperationResult validate_process(const ProcessToken&) = 0;
    virtual PreferenceRead read(const ProcessToken&, AudioRole) = 0;
    virtual OperationResult write(const ProcessToken&, AudioRole,
                                  const std::optional<std::string>& endpoint_id) = 0;
};

struct OriginalPreferences {
    ProcessToken process;
    std::array<std::optional<std::string>, 3> endpoints;
};
struct RecoveryPlan {
    std::string operation_id;
    std::string temporary_endpoint_id;
    std::vector<OriginalPreferences> originals;
};

enum class RestoreState { NotAttempted, AlreadyOriginal, Restored, Conflict, Failed };
struct RoleExperiment {
    ProcessToken process;
    AudioRole role = AudioRole::Console;
    std::optional<std::string> original_endpoint_id;
    bool write_attempted = false;
    OperationResult write_result;
    PreferenceRead readback;
    bool preference_accepted = false;
    RestoreState restore = RestoreState::NotAttempted;
    OperationResult restore_result;
};

struct ExperimentResult {
    bool simulated = false;
    bool recovery_operation = false;
    std::string outcome = "not_attempted";
    std::string detail;
    bool journal_prepared = false;
    bool temporary_preferences_accepted = false;
    bool restoration_complete = true;
    std::vector<RoleExperiment> roles;
    PlaybackObservation before{{}, false};
    PlaybackObservation after_apply{{}, false};
    PlaybackObservation after_recreation{{}, false};
};

// A prepared, durable recovery record is mandatory before the first setter.
// finish appends evidence after restoration; it must not erase the original plan.
// Both operations may fail. Failure to prepare forbids ALL preference mutations.
class ExperimentEvidence {
  public:
    virtual ~ExperimentEvidence() = default;
    virtual OperationResult prepare(const RecoveryPlan&) = 0;
    virtual OperationResult finish(const ExperimentResult&) = 0;
};

struct ExperimentRequest {
    std::string operation_id;
    std::string temporary_endpoint_id;
    std::vector<ProcessToken> processes;
};
struct ExperimentHooks {
    // Both callbacks run synchronously on the calling NON-UI thread. Observation
    // is optional evidence and may throw; it must never prevent restoration.
    std::function<PlaybackObservation()> observe;
    // The native harness uses this to ask its own silent fixture to recreate its
    // streams. It must never restart, kill or inject into a user's application.
    std::function<void()> recreate_fixture_streams;
};

// Reversible feasibility experiment, not the future live-routing controller.
// Snapshot every original role, persist a recovery plan, temporarily apply the
// requested render endpoint, observe, then restore touched roles in reverse order.
// Before restoration, compare current values so an external change is preserved.
// Cancellation and ordinary exceptions trigger the same cleanup. A killed process,
// power loss or failed native cleanup cannot be repaired by a C++ scope guard;
// the durable plan remains available and the result must say recovery_required.
ExperimentResult run_policy_experiment(const ExperimentRequest&, PolicyAdapter&,
                                       ExperimentEvidence&, const ExperimentHooks& = {},
                                       std::stop_token = {});

// Retry an interrupted experiment's restoration, with the same compare-before-
// write rules and a fresh durable recovery copy. The adapter must independently
// verify every process token. The native harness may explicitly rebind a journal
// only to a new instance of its own exact disposable fixture executable; this is
// not automatic reassociation of arbitrary applications across PID reuse.
ExperimentResult recover_policy_preferences(const RecoveryPlan&, PolicyAdapter&,
                                            ExperimentEvidence&);

} // namespace audiohole::plug_board
