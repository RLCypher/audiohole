#pragma once
#include "audiohole/scoped_scan.hpp"
#include "audiohole/scan_utility.hpp"

namespace audiohole::scanner {
struct TestSuiteOptions {
  fs::path output_parent;
  DeepLimits deep_limits{16, 2000, 10000, 120000, "both"};
  unsigned system_timeout_ms = 3600000;
  // Development tests must identify injected providers honestly. The delivered
  // executable has no simulated-provider option or automatic fallback.
  std::string data_source = "real_windows_mmdevice_wasapi";
};
struct TestSuiteResult {
  std::string result = "failed";
  fs::path directory;
  fs::path archive;
  Json manifest;
};
TestSuiteResult run_test_suite(const TestSuiteOptions& options, const ProviderFactory& provider_factory,
  const ProbeRunnerFactory& runner_factory, bool consent, std::stop_token stop = {}, const ProgressSink& progress = {});
// Archive only regular files, with bounded streaming I/O. Existing destinations
// and symlinks are refused. Throws; original result files remain on failure.
void zip_results(const fs::path& directory, const fs::path& archive);
}
