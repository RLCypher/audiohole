#pragma once

#include "audiohole/plug_board/policy_experiment.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>

namespace audiohole::plug_board {

struct WindowsEnvironment {
    unsigned major = 0;
    unsigned minor = 0;
    unsigned build = 0;
    std::optional<std::uint32_t> update_build_revision;
    bool workstation = false;
    bool wine = false;
    bool elevated = false;
    bool privilege_known = false;
    std::string detail;
};
struct RenderEndpoint {
    std::string endpoint_id;
    std::string display_name;
    bool available = false;
    unsigned native_state = 0;
};
struct ProcessMetadata {
    OperationResult result;
    ProcessToken token;
    ApplicationIdentity identity;
    std::string display_name;
};
struct WindowsInspection {
    WindowsEnvironment environment;
    std::vector<RenderEndpoint> endpoints;
    std::array<std::optional<std::string>, 3> default_endpoints;
    SessionSnapshot snapshot;
};

// On-demand, non-UI inspection only. Each call owns its COM apartment/managers,
// returns copied values and releases all session interfaces before returning.
// This standalone host supplies endpoint inventory; production integration will
// instead supply AudioHole's existing inventory. No capability probes or streams
// are opened by inspection, and nothing runs repeatedly in the background.
WindowsEnvironment inspect_windows_environment();
ProcessMetadata inspect_process(std::uint32_t pid);
WindowsInspection inspect_windows_sessions(bool include_sessions = true);

// Construction and every call/destruction belong to one non-UI thread. The
// adapter accepts only the supplied exact process tokens and Windows 11's known
// private interface IID. Wine/older OS/interface failure returns Unsupported.
// Availability is a capability observation, not a tested Windows support claim.
enum class PolicyScope { FixtureExperiment, UserAssignment };
std::unique_ptr<PolicyAdapter>
make_windows_policy_adapter(std::vector<ProcessToken> allowlist,
                            PolicyScope scope = PolicyScope::FixtureExperiment,
                            std::function<void(bool write)> native_call_observer = {});

class PlatformBackend;
// A host supplies its own inventory. Only the standalone launcher enables the
// optional MMDevice inventory-notification shim; this is not a capability scan.
std::unique_ptr<PlatformBackend>
make_windows_backend(bool standalone_inventory_notifications = false);

struct IconExport {
    std::string observation_key;
    std::string source;
    std::filesystem::path file;
    bool application_specific = false;
    OperationResult result;
    // Owned RGBA values avoid file reads or icon extraction on the GUI thread.
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    std::string window_icon_kind{}, shortcut_match{}, shortcut_path{}, taskbar_app_user_model_id{};
};

// One owned Shell STA worker handles this finite batch, with a per-batch identity
// cache. The caller must already be a non-UI harness/worker: this function joins
// that worker before returning. GUI integration must use an asynchronous queue.
// PNG files are evidence of actual Windows icon resolution, never generated art.
std::vector<IconExport> export_windows_icons(const std::vector<ApplicationObservation>&,
                                             const std::filesystem::path& directory,
                                             unsigned pixels = 48, std::stop_token = {});

struct ProcessStartProbe {
    OperationResult subscription;
    bool target_observed = false;
    bool identity_verified = false;
    std::size_t unrelated_events = 0;
    std::string detail;
};

// Subscribe before invoking start_target, then wait for its verified creation
// event. The callback owns the disposable target and keeps it alive through this
// call. WMI event waits do not poll process/session inventories or require a
// separate persistent service. Permission failure is reported without elevation.
ProcessStartProbe
probe_process_start_notifications(const std::function<ProcessToken()>& start_target,
                                  std::chrono::milliseconds timeout);

} // namespace audiohole::plug_board
