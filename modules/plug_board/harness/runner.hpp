#pragma once

#include "evidence.hpp"
#include <stop_token>

namespace audiohole::plug_board::harness {

enum class ProbeAction { Inspect, Capability, ProcessStarts, RoutingTest, Recovery };

struct ProbeRequest {
    ProbeAction action = ProbeAction::Inspect;
    std::filesystem::path output_directory;
    // The frontend supplies its executable directory; the fixture may be beside
    // a CLI executable or in bin/ below the top-level graphical launcher.
    std::filesystem::path executable_directory;
    std::string endpoint_id;
    std::filesystem::path recovery_file;
};

// Owned values only. The worker returns snapshots, RGBA icons and result data;
// no widget, process handle or COM interface crosses into the GUI thread.
struct ProbeRunResult {
    ProbeAction action = ProbeAction::Inspect;
    int exit_code = 2;
    std::string message;
    std::filesystem::path output_directory;
    std::optional<WindowsInspection> inspection;
    bool results_available = false;
    std::vector<IconExport> icons;
    std::optional<OperationResult> capability;
    Json details = Json::object();
    // Set only by the injected UI smoke executor, never by the native runner.
    bool simulated = false;
};

// Synchronous NON-UI entry point shared by both frontends. It owns finite native
// work, saves results automatically, catches ordinary errors and preserves the
// original experiment's cleanup/evidence semantics. The GUI runs it on one owned
// worker and waits asynchronously for completion before normal window closure.
ProbeRunResult run_probe(const ProbeRequest&, std::stop_token = {});

} // namespace audiohole::plug_board::harness
