#include "audiohole/plug_board/policy_experiment.hpp"

#include <gtest/gtest.h>
#include <map>
#include <stdexcept>

namespace audiohole::plug_board {
namespace {

using PreferenceKey = std::pair<std::uint32_t, AudioRole>;
const ProcessToken first{100, 5000, "C:/Fixture/fixture.exe"};
const ProcessToken second{200, 6000, "C:/Fixture/fixture.exe"};

// This fake deliberately distinguishes accepted policy from observed playback.
// It does not enumerate Windows sessions and is never hardware validation.
class FakePolicy final : public PolicyAdapter {
  public:
    std::map<PreferenceKey, std::optional<std::string>> values;
    std::vector<std::pair<PreferenceKey, std::optional<std::string>>> writes;
    std::function<OperationResult(const ProcessToken&, AudioRole,
                                  const std::optional<std::string>&)>
        on_write;
    std::function<void(const ProcessToken&, AudioRole)> on_read;
    bool available = true;
    bool alive = true;
    bool read_fails = false;

    FakePolicy() {
        for (const auto& process : {first, second}) {
            values[{process.pid, AudioRole::Console}] = "original-console";
            values[{process.pid, AudioRole::Multimedia}] = std::nullopt;
            values[{process.pid, AudioRole::Communications}] = "original-comms";
        }
    }
    bool simulated() const override {
        return true;
    }
    OperationResult capability() override {
        return {available, "capability", {}};
    }
    OperationResult validate_process(const ProcessToken&) override {
        return {alive, "live_process", {}};
    }
    PreferenceRead read(const ProcessToken& process, AudioRole role) override {
        if (on_read) {
            on_read(process, role);
        }
        return {{!read_fails, "read", {}}, values[{process.pid, role}]};
    }
    OperationResult write(const ProcessToken& process, AudioRole role,
                          const std::optional<std::string>& endpoint) override {
        writes.push_back({{process.pid, role}, endpoint});
        if (on_write) {
            return on_write(process, role, endpoint);
        }
        values[{process.pid, role}] = endpoint;
        return {true, "write", {}};
    }
};

class FakeEvidence final : public ExperimentEvidence {
  public:
    bool prepare_ok = true;
    bool finish_ok = true;
    bool prepared = false;
    bool finished = false;
    RecoveryPlan recovery;
    OperationResult prepare(const RecoveryPlan& value) override {
        recovery = value;
        prepared = prepare_ok;
        return {prepare_ok, "journal", {}};
    }
    OperationResult finish(const ExperimentResult&) override {
        finished = true;
        return {finish_ok, "evidence", {}};
    }
};

ExperimentRequest request(std::vector<ProcessToken> processes = {first}) {
    return {"operation-one", "temporary-target", std::move(processes)};
}

TEST(PlugBoardPolicy, EveryRoleAndProcessRestoresItsOwnOriginalIncludingDefault) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    policy.on_write = [&](const auto& process, auto role, const auto& endpoint) {
        EXPECT_TRUE(evidence.prepared)
            << "No native mutation may precede durable recovery evidence";
        policy.values[{process.pid, role}] = endpoint;
        return OperationResult{true, "write", {}};
    };
    const auto result = run_policy_experiment(request({first, second}), policy, evidence);
    EXPECT_TRUE(result.simulated);
    EXPECT_EQ(result.outcome, "accepted");
    EXPECT_TRUE(result.temporary_preferences_accepted);
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(result.roles.size(), 6u);
    EXPECT_EQ(policy.writes.size(), 12u);
    EXPECT_EQ(policy.values, originals);
    EXPECT_TRUE(evidence.finished);
    ASSERT_EQ(evidence.recovery.originals.size(), 2u);
    EXPECT_FALSE(evidence.recovery.originals[0].endpoints[1].has_value());
}

TEST(PlugBoardPolicy, UnavailableAdapterNeverWrites) {
    FakePolicy policy;
    FakeEvidence evidence;
    policy.available = false;
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_EQ(result.outcome, "failed");
    EXPECT_TRUE(policy.writes.empty());
    EXPECT_FALSE(evidence.prepared);
}

TEST(PlugBoardPolicy, MissingOrDuplicateProcessTokensAreRejectedBeforeMutation) {
    for (const auto& processes :
         {std::vector<ProcessToken>{}, std::vector<ProcessToken>{first, first},
          std::vector<ProcessToken>{{0, 5000, "image.exe"}},
          std::vector<ProcessToken>{{100, 0, "image.exe"}}}) {
        FakePolicy policy;
        FakeEvidence evidence;
        run_policy_experiment(request(processes), policy, evidence);
        EXPECT_TRUE(policy.writes.empty());
    }
}

TEST(PlugBoardPolicy, SnapshotOrJournalFailureNeverWrites) {
    for (const bool read_fails : {false, true}) {
        FakePolicy policy;
        FakeEvidence evidence;
        policy.read_fails = read_fails;
        evidence.prepare_ok = false;
        const auto result = run_policy_experiment(request(), policy, evidence);
        EXPECT_FALSE(result.temporary_preferences_accepted);
        EXPECT_TRUE(policy.writes.empty());
    }
}

TEST(PlugBoardPolicy, SecondRoleRefusalRestoresTheFirstRole) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    policy.on_write = [&](const auto& process, auto role, const auto& endpoint) {
        if (role == AudioRole::Multimedia && endpoint == "temporary-target") {
            return OperationResult{false, "native_refused", {}};
        }
        policy.values[{process.pid, role}] = endpoint;
        return OperationResult{true, "write", {}};
    };
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_EQ(result.outcome, "partial");
    EXPECT_FALSE(result.temporary_preferences_accepted);
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

TEST(PlugBoardPolicy, FailedSetterThatMutatedStillGetsRestored) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    policy.on_write = [&](const auto& process, auto role, const auto& endpoint) {
        policy.values[{process.pid, role}] = endpoint;
        return OperationResult{endpoint != "temporary-target", "failed_after_mutation", {}};
    };
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_EQ(result.outcome, "rejected");
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

TEST(PlugBoardPolicy, SuccessfulHresultWithoutMatchingReadbackIsRejected) {
    FakePolicy policy;
    FakeEvidence evidence;
    policy.on_write = [](const auto&, auto, const auto&) {
        return OperationResult{true, "accepted_without_effect", {}};
    };
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_EQ(result.outcome, "rejected");
    EXPECT_FALSE(result.temporary_preferences_accepted);
}

TEST(PlugBoardPolicy, AcceptedPreferenceDoesNotInventAStreamMove) {
    FakePolicy policy;
    FakeEvidence evidence;
    ExperimentHooks hooks;
    hooks.observe = [] { return PlaybackObservation{{"old-stream-endpoint"}, true}; };
    const auto result = run_policy_experiment(request(), policy, evidence, hooks);
    EXPECT_TRUE(result.temporary_preferences_accepted);
    EXPECT_EQ(compare_playback(result.after_apply, "temporary-target"), PlaybackMatch::Differs);
    EXPECT_TRUE(result.restoration_complete);
}

TEST(PlugBoardPolicy, StreamRecreationIsObservedIndependently) {
    FakePolicy policy;
    FakeEvidence evidence;
    bool recreated = false;
    ExperimentHooks hooks;
    hooks.observe = [&] {
        return PlaybackObservation{{recreated ? "temporary-target" : "old-stream-endpoint"}, true};
    };
    hooks.recreate_fixture_streams = [&] { recreated = true; };
    const auto result = run_policy_experiment(request(), policy, evidence, hooks);
    EXPECT_EQ(compare_playback(result.after_apply, "temporary-target"), PlaybackMatch::Differs);
    EXPECT_EQ(compare_playback(result.after_recreation, "temporary-target"),
              PlaybackMatch::Matches);
}

TEST(PlugBoardPolicy, ObservationExceptionCannotSkipRestoration) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    int observations = 0;
    ExperimentHooks hooks;
    hooks.observe = [&]() -> PlaybackObservation {
        if (++observations > 1) {
            throw std::runtime_error("observer stopped");
        }
        return {{"original"}, true};
    };
    const auto result = run_policy_experiment(request(), policy, evidence, hooks);
    EXPECT_EQ(result.outcome, "failed");
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

TEST(PlugBoardPolicy, ExternalPreferenceChangeIsPreservedAndRecoveryRemainsExplicit) {
    FakePolicy policy;
    FakeEvidence evidence;
    ExperimentHooks hooks;
    hooks.recreate_fixture_streams = [&] {
        policy.values[{first.pid, AudioRole::Console}] = "external-user-choice";
    };
    const auto result = run_policy_experiment(request(), policy, evidence, hooks);
    EXPECT_EQ(result.outcome, "recovery_required");
    EXPECT_FALSE(result.restoration_complete);
    EXPECT_EQ(policy.values.at({first.pid, AudioRole::Console}), "external-user-choice");
    EXPECT_EQ(result.roles[0].restore, RestoreState::Conflict);
}

TEST(PlugBoardPolicy, ProcessExitNeverTurnsIntoSuccessfulRestoration) {
    FakePolicy policy;
    FakeEvidence evidence;
    ExperimentHooks hooks;
    hooks.recreate_fixture_streams = [&] { policy.alive = false; };
    const auto result = run_policy_experiment(request(), policy, evidence, hooks);
    EXPECT_EQ(result.outcome, "recovery_required");
    EXPECT_FALSE(result.restoration_complete);
    EXPECT_EQ(policy.writes.size(), 3u);
}

TEST(PlugBoardPolicy, RestoreRefusalDoesNotLoseTheRecoveryPlan) {
    FakePolicy policy;
    FakeEvidence evidence;
    policy.on_write = [&](const auto& process, auto role, const auto& endpoint) {
        if (endpoint != "temporary-target") {
            return OperationResult{false, "restore_refused", {}};
        }
        policy.values[{process.pid, role}] = endpoint;
        return OperationResult{true, "write", {}};
    };
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_EQ(result.outcome, "recovery_required");
    EXPECT_TRUE(evidence.prepared);
    EXPECT_TRUE(evidence.finished);
    EXPECT_FALSE(result.restoration_complete);
}

TEST(PlugBoardPolicy, CancellationBetweenWritesRestoresTouchedRoles) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    std::stop_source cancellation;
    policy.on_write = [&](const auto& process, auto role, const auto& endpoint) {
        policy.values[{process.pid, role}] = endpoint;
        cancellation.request_stop();
        return OperationResult{true, "write", {}};
    };
    const auto result =
        run_policy_experiment(request(), policy, evidence, {}, cancellation.get_token());
    EXPECT_EQ(result.outcome, "cancelled");
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

TEST(PlugBoardPolicy, EvidenceCompletionFailureDoesNotSkipPreferenceRestoration) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    evidence.finish_ok = false;
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_EQ(result.outcome, "evidence_failed");
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

TEST(PlugBoardPolicy, SharedApplicationPreferenceAcrossPidsDoesNotLookLikeExternalConflict) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    policy.on_write = [&](const auto&, auto role, const auto& endpoint) {
        // Windows can resolve both PIDs to the same application preference.
        policy.values[{first.pid, role}] = endpoint;
        policy.values[{second.pid, role}] = endpoint;
        return OperationResult{true, "shared_application_write", {}};
    };
    const auto result = run_policy_experiment(request({first, second}), policy, evidence);
    EXPECT_TRUE(result.temporary_preferences_accepted);
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

TEST(PlugBoardPolicy, AliasedWindowsRolesAreStillReadAndRestoredSeparately) {
    FakePolicy policy;
    FakeEvidence evidence;
    for (auto role : render_roles) {
        policy.values[{first.pid, role}] = "shared-original";
    }
    const auto originals = policy.values;
    policy.on_write = [&](const auto& process, auto, const auto& endpoint) {
        for (auto role : render_roles) {
            policy.values[{process.pid, role}] = endpoint;
        }
        return OperationResult{true, "shared_role_write", {}};
    };
    const auto result = run_policy_experiment(request(), policy, evidence);
    EXPECT_TRUE(result.temporary_preferences_accepted);
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
}

RecoveryPlan interrupted_plan() {
    OriginalPreferences original;
    original.process = first;
    original.endpoints = {"original-console", std::nullopt, "original-comms"};
    return {"interrupted-operation", "temporary-target", {original}};
}

TEST(PlugBoardRecovery, InterruptedRolesRestoreWithoutAnotherTemporaryExperiment) {
    FakePolicy policy;
    FakeEvidence evidence;
    const auto originals = policy.values;
    for (auto role : render_roles) {
        policy.values[{first.pid, role}] = "temporary-target";
    }
    const auto result = recover_policy_preferences(interrupted_plan(), policy, evidence);
    EXPECT_EQ(result.outcome, "recovered");
    EXPECT_TRUE(result.recovery_operation);
    EXPECT_TRUE(result.restoration_complete);
    EXPECT_EQ(policy.values, originals);
    EXPECT_EQ(policy.writes.size(), 3u);
}

TEST(PlugBoardRecovery, AlreadyOriginalRolesAreIdempotentAndExternalChangesSurvive) {
    FakePolicy policy;
    FakeEvidence evidence;
    policy.values[{first.pid, AudioRole::Console}] = "user-changed-after-crash";
    const auto result = recover_policy_preferences(interrupted_plan(), policy, evidence);
    EXPECT_EQ(result.outcome, "recovery_required");
    EXPECT_FALSE(result.restoration_complete);
    EXPECT_TRUE(policy.writes.empty());
    EXPECT_EQ(policy.values.at({first.pid, AudioRole::Console}), "user-changed-after-crash");
}

TEST(PlugBoardRecovery, JournalFailureCannotClaimRestorationOrPerformWrites) {
    FakePolicy policy;
    FakeEvidence evidence;
    evidence.prepare_ok = false;
    const auto result = recover_policy_preferences(interrupted_plan(), policy, evidence);
    EXPECT_EQ(result.outcome, "recovery_required");
    EXPECT_FALSE(result.restoration_complete);
    EXPECT_TRUE(policy.writes.empty());
}

TEST(PlugBoardRecovery, DeadProcessDoesNotReuseHistoricalPid) {
    FakePolicy policy;
    FakeEvidence evidence;
    policy.alive = false;
    const auto result = recover_policy_preferences(interrupted_plan(), policy, evidence);
    EXPECT_EQ(result.outcome, "recovery_required");
    EXPECT_TRUE(policy.writes.empty());
    EXPECT_FALSE(evidence.prepared);
}

} // namespace
} // namespace audiohole::plug_board
