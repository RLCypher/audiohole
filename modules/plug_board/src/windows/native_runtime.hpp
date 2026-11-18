#pragma once

#include "audiohole/plug_board/module.hpp"
#include "audiohole/plug_board/windows.hpp"
#include "shortcut_icon.hpp"
#include "support.hpp"
#include "window_icon.hpp"

#include <atomic>
#include <memory>

namespace audiohole::plug_board::windows_detail {

struct NativeCounters {
    std::atomic<std::uint64_t> full_enumerations{0}, session_events{0}, icon_extractions{0},
        icon_cache_hits{0}, policy_reads{0}, policy_writes{0}, process_snapshots{0}, dropped{0};
    std::atomic<unsigned> workers{0};
    std::atomic<bool> observing{false}, process_watching{false};
};

// These adapters own their workers and COM references. All callbacks carry copied
// observations/pixels/metadata. shutdown joins; no worker detaches or outlives its
// backend. Session callbacks only enqueue bounded MTA events and signal a handle.
class SessionMonitor final {
  public:
    SessionMonitor(BackendCallbacks, std::shared_ptr<NativeCounters>);
    ~SessionMonitor();
    void configure(const EndpointInventory&, std::uint64_t generation);
    void refresh();
    OperationResult shutdown();

  private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
class IconWorker final {
  public:
    IconWorker(BackendCallbacks, std::shared_ptr<NativeCounters>,
               std::filesystem::path cache_root = {});
    ~IconWorker();
    void request(const std::vector<ApplicationObservation>&, unsigned pixels,
                 std::uint64_t generation);
    void cancel_pending();
    void invalidate_sources();
    void shutdown();

  private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
class ProcessWatcher final {
  public:
    ProcessWatcher(BackendCallbacks, std::shared_ptr<NativeCounters>);
    ~ProcessWatcher();
    void set_identities(const std::vector<ApplicationIdentity>&);
    OperationResult shutdown();

  private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

// Query-limited identity checks. Writes are restricted to the current user and
// logon session, including when process-start events describe a shared machine.
bool current_user_process(HANDLE);
ApplicationIdentity canonical_file_identity(const ApplicationIdentity&);
std::vector<ProcessEvidence> find_current_user_processes(const std::vector<ApplicationIdentity>&);

// Called only on a Shell STA. It returns owned pixels with no PNG/file writes;
// the finite feasibility exporter remains available separately for evidence.
ApplicationImage resolve_icon_pixels(const ApplicationObservation&, unsigned pixels,
                                     const WindowIconSnapshot* = nullptr,
                                     const ShortcutIconSnapshot* = nullptr);
// Rescale owned cached RGBA on the Shell STA after a DPI change. No file reads,
// window handles or borrowed icon resources are required for a closed app.
ApplicationImage scale_icon_pixels(ApplicationImage, unsigned pixels);

} // namespace audiohole::plug_board::windows_detail
