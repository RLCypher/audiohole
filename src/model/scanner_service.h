#pragma once
#include "model/controller.h"
#include "model/scan_session_store.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace audiohole::scanner {
class EndpointProvider;
class ProbeRunner;
} // namespace audiohole::scanner
namespace audiohole {
struct ScannerServiceOptions {
    std::filesystem::path portable_root, local_root, probe_executable;
    std::string source = "real_windows_mmdevice_wasapi";
    std::string build_id = "unknown";
    ScanSessionLimits session_limits;
};
struct ScannerFactories {
    std::function<std::unique_ptr<scanner::EndpointProvider>()> provider;
    std::function<std::unique_ptr<scanner::ProbeRunner>()> runner;
};
struct ScannerUpdate {
    std::uint64_t generation = 0;
    bool terminal = false, inventory_valid = false;
    int percent = -1;
    std::string phase, result, detail, session_path;
    std::vector<Endpoint> devices;
    std::shared_ptr<const BundleImportPlan> import_plan{};
    bool profiles_imported = false;
};
// One owned worker, one operation at a time. Public calls are control-thread only;
// producers never touch widgets/controller state. Progress is bounded; full rich
// evidence is retained separately on disk. Destruction cancels and joins work.
// Basic Windows API cancellation is cooperative; Deep work uses the private
// process runner's deadlines/cleanup protocol, never detached driver threads.
class ScannerService {
  public:
    explicit ScannerService(ScannerServiceOptions, ScannerFactories = {});
    ~ScannerService();
    ScannerService(const ScannerService&) = delete;
    ScannerService& operator=(const ScannerService&) = delete;
    std::uint64_t start_scan(bool deep, ScanScope, const std::string& logical_id,
                             const ScanOptions&);
    std::uint64_t action(const ScannerActionRequest&);
    void set_logging(bool);
    void cancel();
    void shutdown();
    bool busy() const;
    std::vector<ScannerUpdate> drain();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace audiohole
