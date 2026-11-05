#pragma once
#include "audiohole/scanner.hpp"

namespace audiohole::scanner {
struct ProbeTarget {
  std::string logical_id;
  std::string endpoint_id;
  std::string display_name;
  std::string direction;
  Json record;
};
struct DeepLimits {
  std::size_t max_attempts = 8;
  unsigned duration_ms = 2000;
  unsigned attempt_timeout_ms = 10000;
  unsigned total_timeout_ms = 120000;
  std::string mode = "shared";
};
struct ProbeCandidate {
  Json format;
  std::string mode;
  std::string provenance;
};
struct ProbeJob {
  std::string attempt_id;
  ProbeTarget target;
  ProbeCandidate candidate;
  unsigned duration_ms = 2000;
  unsigned timeout_ms = 10000;
};
void validate_deep_limits(const DeepLimits& limits);
void validate_probe_format(const Json& format);
std::vector<ProbeCandidate> plan_deep_candidates(const ProbeTarget& target, const DeepLimits& limits);
Json probe_job_json(const ProbeJob& job);
ProbeJob parse_probe_job(const Json& json);

using ProbeStageSink = std::function<void(const Json&)>;
class ProbeRunner {
 public:
  virtual ~ProbeRunner() = default;
  virtual Json execute(const ProbeJob& job, std::stop_token stop, const ProbeStageSink& stage) = 0;
};
std::unique_ptr<ProbeRunner> make_process_probe_runner(const fs::path& executable);
// Private child protocol: bounded stdin/stdout pipes only, never a public control listener.
int run_probe_worker();

class DeviceStore;
struct DeepReport {
  std::string result = "failed";
  std::string logical_id;
  std::size_t attempted = 0;
  std::size_t verified = 0;
  std::size_t accepted = 0;
  std::size_t failed = 0;
  std::size_t inconclusive = 0;
  std::vector<Diagnostic> diagnostics;
  std::size_t planned = 0;
  Json attempts = Json::array();
};
void to_json(Json& json, const DeepReport& report);
DeepReport run_deep_scan(DeviceStore& store, ProbeRunner& runner, const ProbeTarget& target,
                         const DeepLimits& limits, bool consent, std::stop_token stop = {},
                         const ProgressSink& progress = {});

struct DeepBatchReport {
  std::string result = "running";
  Json devices = Json::array();
  std::vector<Diagnostic> diagnostics;
};
void to_json(Json& json, const DeepBatchReport& report);
using DeepBatchSink = std::function<void(const DeepBatchReport&)>;
// The system deadline covers all devices; each device also retains its own budget.
DeepBatchReport run_deep_scan_batch(DeviceStore& store, ProbeRunner& runner,
  const std::vector<ProbeTarget>& targets, const DeepLimits& limits, unsigned system_timeout_ms,
  bool consent, std::stop_token stop = {}, const ProgressSink& progress = {}, const DeepBatchSink& checkpoint = {});
}
