#include "runner.hpp"
#include "fixture_process.hpp"
#include "support.hpp"

#include <algorithm>

namespace audiohole::plug_board::harness {
using namespace windows_detail;
namespace {

std::filesystem::path locate_fixture(const ProbeRequest& request) {
    for (const auto& candidate : {request.executable_directory / L"bin/plug_board_fixture.exe",
                                  request.executable_directory / L"plug_board_fixture.exe"}) {
        if (std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error(
        "Extract the complete package so its bin/plug_board_fixture.exe is available.");
}

std::string operation_id() {
    return "fixture-" + std::to_string(GetCurrentProcessId()) + "-" +
           std::to_string(GetTickCount64());
}

void execute(const ProbeRequest& request, EvidenceDirectory& evidence, ProbeRunResult& result,
             std::stop_token stop) {
    result.inspection = inspect_windows_sessions();
    evidence.write(L"inspection.json", inspection_json(*result.inspection));
    const auto& inspection = *result.inspection;
    if (stop.stop_requested()) {
        result.exit_code = 4;
        result.message = "Inspection cancelled. Results collected so far were saved.";
        return;
    }
    if (request.action == ProbeAction::Inspect) {
        result.icons = export_windows_icons(group_applications(inspection.snapshot),
                                            evidence.root() / L"icons", 48, stop);
        evidence.write(L"icons.json", icons_json(result.icons));
        result.exit_code = inspection.snapshot.complete && !stop.stop_requested() ? 0 : 4;
        result.message =
            stop.stop_requested() ? "Inspection cancelled. Collected results were saved."
            : inspection.snapshot.complete
                ? "Playback applications refreshed."
                : "Inspection is incomplete. See the application details and saved results.";
        return;
    }
    if (request.action == ProbeAction::ProcessStarts) {
        std::unique_ptr<FixtureProcess> fixture;
        const auto probe = probe_process_start_notifications(
            [&] {
                fixture = std::make_unique<FixtureProcess>(locate_fixture(request), true);
                return fixture->token();
            },
            std::chrono::seconds(5));
        result.details = watcher_json(probe);
        const bool standard_user =
            inspection.environment.privilege_known && !inspection.environment.elevated;
        result.details["standard_user_context_confirmed"] = standard_user;
        evidence.write(L"process-start.json", result.details);
        if (fixture) {
            fixture->stop();
        }
        result.exit_code = probe.subscription.ok && probe.identity_verified &&
                                   !inspection.environment.wine && standard_user
                               ? 0
                               : 3;
        result.message = result.exit_code == 0
                             ? "Application-start detection passed for the test application."
                             : "Application-start detection was not confirmed. " + probe.detail;
        return;
    }
    // Capability remains read-only. Unsupported environments never launch the
    // routing fixture and never reach recovery preparation or a preference setter.
    auto capability_probe = make_windows_policy_adapter({});
    result.capability = capability_probe->capability();
    evidence.write(L"capability.json", operation_json(*result.capability));
    capability_probe.reset();
    if (!result.capability->ok) {
        result.exit_code = 3;
        result.message = result.capability->detail;
        result.details = operation_json(*result.capability);
        return;
    }
    if (request.action == ProbeAction::Capability) {
        result.exit_code = 0;
        result.message =
            "Windows routing interface is available. Run the output test to verify stream routing.";
        result.details = operation_json(*result.capability);
        return;
    }
    if (request.action == ProbeAction::Recovery) {
        auto recovery = read_recovery_plan(request.recovery_file);
        FixtureProcess fixture(locate_fixture(request), true);
        if (recovery.originals[0].process.executable_path != fixture.token().executable_path) {
            throw std::runtime_error(
                "Recovery belongs to a different fixture path. Use the original package location.");
        }
        recovery.originals[0].process = fixture.token();
        recovery.operation_id = operation_id();
        auto policy = make_windows_policy_adapter({fixture.token()});
        const auto recovered = recover_policy_preferences(recovery, *policy, evidence);
        fixture.stop();
        result.details = result_json(recovered);
        evidence.write(L"summary.json", result.details);
        result.exit_code = recovered.outcome == "recovered" ? 0 : 5;
        result.message =
            result.exit_code == 0
                ? "The test application's original preferences were restored."
                : "Recovery needs attention. Keep the saved recovery file. " + recovered.detail;
        return;
    }
    const auto selected = std::find_if(
        inspection.endpoints.begin(), inspection.endpoints.end(), [&](const auto& endpoint) {
            return endpoint.endpoint_id == request.endpoint_id && endpoint.available;
        });
    if (selected == inspection.endpoints.end() ||
        std::count_if(inspection.endpoints.begin(), inspection.endpoints.end(),
                      [](const auto& endpoint) { return endpoint.available; }) < 2) {
        throw std::runtime_error(
            "Choose an available output. The routing test needs two playback outputs.");
    }
    FixtureProcess fixture(locate_fixture(request));
    auto policy = make_windows_policy_adapter({fixture.token()});
    const ExperimentRequest experiment{operation_id(), request.endpoint_id, {fixture.token()}};
    std::size_t observation = 0;
    ExperimentHooks hooks;
    hooks.observe = [&] {
        const auto state = inspect_windows_sessions();
        const wchar_t* names[]{L"before.json", L"after-apply.json", L"after-recreation.json"};
        evidence.write(names[observation++], inspection_json(state));
        return observe_playback(state.snapshot, {fixture.token()});
    };
    hooks.recreate_fixture_streams = [&] { fixture.recreate_streams(); };
    const auto tested = run_policy_experiment(experiment, *policy, evidence, hooks, stop);
    // Keep the disposable process alive through restoration/readback, then wait
    // for its own streams and process to close before publishing completion.
    fixture.stop();
    result.details = result_json(tested);
    const bool initially_different =
        tested.before.complete &&
        std::any_of(tested.before.active_endpoint_ids.begin(),
                    tested.before.active_endpoint_ids.end(),
                    [&](const auto& endpoint) { return endpoint != request.endpoint_id; });
    const bool standard_user =
        inspection.environment.privilege_known && !inspection.environment.elevated;
    const bool verified =
        tested.outcome == "accepted" && tested.restoration_complete && initially_different &&
        compare_playback(tested.after_recreation, request.endpoint_id) == PlaybackMatch::Matches &&
        !inspection.environment.wine && standard_user;
    result.details["native_routing_verified_for_this_fixture_run"] = verified;
    result.details["standard_user_context_confirmed"] = standard_user;
    result.details["fixture_cleanup_confirmed"] = true;
    result.details["temporary_endpoint_id"] = request.endpoint_id;
    evidence.write(L"summary.json", result.details);
    result.exit_code = verified ? 0 : tested.restoration_complete ? 4 : 5;
    result.message =
        !tested.restoration_complete
            ? "Restoration needs attention. Keep the recovery file and use Recover test."
        : tested.outcome == "cancelled" ? "Test cancelled. Original preferences were restored."
        : verified                      ? "Routing test passed. Original preferences were restored."
        : tested.temporary_preferences_accepted
            ? "Windows accepted the preferences, but stream routing was not confirmed. Originals "
              "were restored."
            : "Routing was not confirmed. Original preferences were restored.";
    if (!tested.detail.empty()) {
        result.message += " " + tested.detail;
    }
}

} // namespace

ProbeRunResult run_probe(const ProbeRequest& request, std::stop_token stop) {
    ProbeRunResult result;
    result.action = request.action;
    result.output_directory = request.output_directory;
    try {
        EvidenceDirectory evidence(request.output_directory);
        result.results_available = true;
        execute(request, evidence, result, stop);
    } catch (const std::exception& error) {
        result.exit_code = 2;
        result.message = error.what();
    } catch (...) {
        result.exit_code = 2;
        result.message =
            "The inspection or test failed unexpectedly. Keep any saved recovery record.";
    }
    return result;
}

} // namespace audiohole::plug_board::harness
