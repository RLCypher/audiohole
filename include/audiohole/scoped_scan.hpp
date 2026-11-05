#pragma once
#include "audiohole/deep_scan.hpp"

namespace audiohole::scanner {
struct ScopedReport {
  ScanReport scan;
  std::string scope;
  std::size_t observed = 0;
  std::size_t known_skipped = 0;
  std::size_t ambiguous_skipped = 0;
  std::vector<std::string> selected_endpoint_ids;
  std::size_t metadata_updated = 0;
  bool inventory_complete = false;
  bool selected_target_complete = false;
};
void to_json(Json& json, const ScopedReport& report);
ScopedReport run_scoped_basic(EndpointProvider& provider, DeviceStore& store,
  const std::string& scope, const std::string& logical_id = {},
  std::stop_token stop = {}, const ProgressSink& progress = {});
}
