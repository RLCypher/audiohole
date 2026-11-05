#pragma once
#include "audiohole/scanner.hpp"
#include "audiohole/deep_scan.hpp"

namespace audiohole::scanner {
struct UtilityOptions {
  fs::path results_root;
  fs::path portable_root;
  fs::path local_root;
  fs::path debug_log;
  bool help = false;
  bool deep = false;
  bool confirm_disruption = false;
  std::string logical_device;
  DeepLimits deep_limits;
  bool deep_all = false;
  unsigned system_timeout_ms = 1800000;
};
// Explicit paths use the caller's working directory. No-argument results are
// anchored beside the executable, independent of Explorer's working directory.
UtilityOptions utility_options(const std::vector<fs::path>& arguments, const fs::path& executable_directory);
struct UtilityResult {
  ScanReport report;
  fs::path debug_log;
  fs::path report_json;
  Json deep_report = status("not_tested");
};
using ProviderFactory = std::function<std::unique_ptr<EndpointProvider>()>;
using ProbeRunnerFactory = std::function<std::unique_ptr<ProbeRunner>()>;
struct DeepInteraction {
  std::function<std::string(const std::vector<ProbeTarget>&)> select_target;
  std::function<bool(const ProbeTarget&, const DeepLimits&)> confirm;
  std::function<bool(const std::vector<ProbeTarget>&, const DeepLimits&, unsigned)> confirm_all;
};
UtilityResult run_scan_utility(const UtilityOptions& options, const ProviderFactory& factory,
                              std::stop_token stop = {}, const ProgressSink& progress = {},
                              const ProbeRunnerFactory& probe_factory = {}, const DeepInteraction& interaction = {});
}
