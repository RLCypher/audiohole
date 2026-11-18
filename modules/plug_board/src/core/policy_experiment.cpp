#include "audiohole/plug_board/policy_experiment.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <stdexcept>

namespace audiohole::plug_board {
namespace {

OperationResult failure(const std::string& code, const std::string& detail) {
    return {false, code, detail};
}

// Restoration is also invoked by the destructor if observation or evidence code
// throws. The guard borrows its collaborators; all outlive run_policy_experiment.
class RestorationGuard {
  public:
    RestorationGuard(PolicyAdapter& policy, ExperimentResult& result, std::string temporary,
                     bool recovering = false)
        : policy_(policy), result_(result), temporary_(std::move(temporary)),
          recovering_(recovering) {}

    ~RestorationGuard() {
        restore();
    }

    void restore() noexcept {
        if (restored_) {
            return;
        }
        restored_ = true;
        for (auto entry = result_.roles.rbegin(); entry != result_.roles.rend(); ++entry) {
            if (!entry->write_attempted && !recovering_) {
                continue;
            }
            // Mark uncertain BEFORE calling fallible adapter code. An exception
            // must not leave this role looking successfully restored.
            entry->restore = RestoreState::Failed;
            try {
                const auto live = policy_.validate_process(entry->process);
                if (!live.ok) {
                    entry->restore_result = live;
                    result_.restoration_complete = false;
                    continue;
                }
                const auto current = policy_.read(entry->process, entry->role);
                if (!current.result.ok) {
                    entry->restore_result = current.result;
                    result_.restoration_complete = false;
                    continue;
                }
                if (current.endpoint_id == entry->original_endpoint_id) {
                    entry->restore = RestoreState::AlreadyOriginal;
                    entry->restore_result = {true, "already_original", {}};
                    continue;
                }
                if (current.endpoint_id != std::optional<std::string>(temporary_)) {
                    entry->restore = RestoreState::Conflict;
                    entry->restore_result = failure(
                        "external_change",
                        "Another preference change was preserved; inspect recovery evidence.");
                    result_.restoration_complete = false;
                    continue;
                }
                entry->restore_result =
                    policy_.write(entry->process, entry->role, entry->original_endpoint_id);
                const auto readback = policy_.read(entry->process, entry->role);
                if (entry->restore_result.ok && readback.result.ok &&
                    readback.endpoint_id == entry->original_endpoint_id) {
                    entry->restore = RestoreState::Restored;
                } else {
                    result_.restoration_complete = false;
                    if (entry->restore_result.ok) {
                        entry->restore_result =
                            failure("restore_readback_failed",
                                    "Original preference was not confirmed by readback.");
                    }
                }
            } catch (...) {
                // No exception may escape a cleanup destructor. The durable
                // journal, failed state and aggregate flag retain the uncertainty.
                result_.restoration_complete = false;
            }
        }
    }

  private:
    PolicyAdapter& policy_;
    ExperimentResult& result_;
    std::string temporary_;
    bool restored_ = false;
    bool recovering_ = false;
};

void require_success(const OperationResult& result) {
    if (!result.ok) {
        throw std::runtime_error(result.code + ": " + result.detail);
    }
}

bool same_application_evidence(const ProcessToken& first, const ProcessToken& second) {
    if (!first.app_user_model_id.empty() || !second.app_user_model_id.empty()) {
        return !first.app_user_model_id.empty() &&
               first.app_user_model_id == second.app_user_model_id;
    }
    return first.executable_path == second.executable_path;
}

} // namespace

const char* role_name(AudioRole role) {
    switch (role) {
    case AudioRole::Console:
        return "console";
    case AudioRole::Multimedia:
        return "multimedia";
    case AudioRole::Communications:
        return "communications";
    }
    return "unknown";
}

ExperimentResult run_policy_experiment(const ExperimentRequest& request, PolicyAdapter& policy,
                                       ExperimentEvidence& evidence, const ExperimentHooks& hooks,
                                       std::stop_token stop) {
    ExperimentResult result;
    result.simulated = policy.simulated();
    RestorationGuard restoration(policy, result, request.temporary_endpoint_id);
    try {
        if (request.operation_id.empty() || request.temporary_endpoint_id.empty() ||
            request.processes.empty()) {
            throw std::runtime_error(
                "Provide an operation ID, endpoint and explicit process allowlist.");
        }
        require_success(policy.capability());
        RecoveryPlan recovery{request.operation_id, request.temporary_endpoint_id, {}};
        std::set<std::uint32_t> seen_pids;
        for (const auto& process : request.processes) {
            if (process.pid == 0 || process.creation_time == 0 || process.executable_path.empty() ||
                !seen_pids.insert(process.pid).second) {
                throw std::runtime_error("Process tokens must be complete, distinct and verified.");
            }
            require_success(policy.validate_process(process));
            OriginalPreferences original;
            original.process = process;
            for (std::size_t index = 0; index < render_roles.size(); ++index) {
                const auto preference = policy.read(process, render_roles[index]);
                require_success(preference.result);
                original.endpoints[index] = preference.endpoint_id;
                RoleExperiment role;
                role.process = process;
                role.role = render_roles[index];
                role.original_endpoint_id = preference.endpoint_id;
                result.roles.push_back(std::move(role));
            }
            recovery.originals.push_back(std::move(original));
        }
        if (stop.stop_requested()) {
            result.outcome = "cancelled";
            result.detail = "Cancelled before any preference mutation.";
            return result;
        }
        if (hooks.observe) {
            result.before = hooks.observe();
        } else {
            result.before.complete = false;
        }
        require_success(evidence.prepare(recovery));
        result.journal_prepared = true;

        std::size_t accepted = 0;
        for (auto& role : result.roles) {
            if (stop.stop_requested()) {
                break;
            }
            require_success(policy.validate_process(role.process));
            const auto current = policy.read(role.process, role.role);
            require_success(current.result);
            const bool own_shared_change =
                current.endpoint_id == std::optional<std::string>(request.temporary_endpoint_id) &&
                std::any_of(result.roles.begin(), result.roles.end(), [&](const auto& previous) {
                    return previous.preference_accepted &&
                           same_application_evidence(previous.process, role.process);
                });
            // Windows persists by application, although the API accepts a PID.
            // A prior call for the same verified app can update another PID or
            // aliased role. Its own exact requested value is not an external
            // conflict; any other intervening value still aborts application.
            if (current.endpoint_id != role.original_endpoint_id && !own_shared_change) {
                throw std::runtime_error(
                    "Preference changed since snapshot; no overwrite was attempted.");
            }
            // Even a failed setter might have affected Windows state. Include
            // every attempted call in rollback, not only successful HRESULTs.
            role.write_attempted = true;
            role.write_result =
                policy.write(role.process, role.role, request.temporary_endpoint_id);
            role.readback = policy.read(role.process, role.role);
            role.preference_accepted =
                role.write_result.ok && role.readback.result.ok &&
                role.readback.endpoint_id ==
                    std::optional<std::string>(request.temporary_endpoint_id);
            if (!role.preference_accepted) {
                break;
            }
            ++accepted;
        }
        result.temporary_preferences_accepted = accepted == result.roles.size();
        result.outcome = stop.stop_requested()                   ? "cancelled"
                         : result.temporary_preferences_accepted ? "accepted"
                         : accepted > 0                          ? "partial"
                                                                 : "rejected";
        if (hooks.observe) {
            result.after_apply = hooks.observe();
        } else {
            result.after_apply.complete = false;
        }
        if (!stop.stop_requested() && result.temporary_preferences_accepted &&
            hooks.recreate_fixture_streams) {
            hooks.recreate_fixture_streams();
            if (hooks.observe) {
                result.after_recreation = hooks.observe();
            }
        } else {
            result.after_recreation.complete = false;
        }
    } catch (const std::exception& error) {
        result.outcome = "failed";
        result.detail = error.what();
    } catch (...) {
        result.outcome = "failed";
        result.detail = "Unexpected adapter or observer exception; restoration was attempted.";
    }
    restoration.restore();
    if (!result.restoration_complete) {
        result.outcome = "recovery_required";
    }
    if (result.journal_prepared) {
        try {
            const auto finished = evidence.finish(result);
            if (!finished.ok) {
                result.detail += " Final evidence write failed; original recovery record retained.";
                if (result.restoration_complete) {
                    result.outcome = "evidence_failed";
                }
            }
        } catch (...) {
            result.detail += " Final evidence write threw; original recovery record retained.";
            if (result.restoration_complete) {
                result.outcome = "evidence_failed";
            }
        }
    }
    return result;
}

ExperimentResult recover_policy_preferences(const RecoveryPlan& recovery, PolicyAdapter& policy,
                                            ExperimentEvidence& evidence) {
    ExperimentResult result;
    result.simulated = policy.simulated();
    result.recovery_operation = true;
    result.restoration_complete = false;
    try {
        if (recovery.operation_id.empty() || recovery.temporary_endpoint_id.empty() ||
            recovery.originals.empty()) {
            throw std::runtime_error(
                "Recovery record is missing its operation, endpoint or originals.");
        }
        require_success(policy.capability());
        std::set<std::uint32_t> seen_pids;
        for (const auto& original : recovery.originals) {
            if (original.process.pid == 0 || original.process.creation_time == 0 ||
                original.process.executable_path.empty() ||
                !seen_pids.insert(original.process.pid).second) {
                throw std::runtime_error("Recovery targets must be complete and distinct.");
            }
            require_success(policy.validate_process(original.process));
            for (std::size_t index = 0; index < render_roles.size(); ++index) {
                RoleExperiment role;
                role.process = original.process;
                role.role = render_roles[index];
                role.original_endpoint_id = original.endpoints[index];
                result.roles.push_back(std::move(role));
            }
        }
        require_success(evidence.prepare(recovery));
        result.journal_prepared = true;
        result.restoration_complete = true;
        RestorationGuard restoration(policy, result, recovery.temporary_endpoint_id, true);
        restoration.restore();
        result.outcome = result.restoration_complete ? "recovered" : "recovery_required";
        require_success(evidence.finish(result));
    } catch (const std::exception& error) {
        result.outcome = result.restoration_complete ? "failed" : "recovery_required";
        result.detail = error.what();
    } catch (...) {
        result.outcome = result.restoration_complete ? "failed" : "recovery_required";
        result.detail = "Recovery failed; retain both original and retry journals.";
    }
    return result;
}

} // namespace audiohole::plug_board
