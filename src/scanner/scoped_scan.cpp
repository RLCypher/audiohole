#include "audiohole/scoped_scan.hpp"
#include <algorithm>
#include <stdexcept>

namespace audiohole::scanner {
void to_json(Json& j, const ScopedReport& r) {
  j = Json{{"scope", r.scope}, {"basic_scan", r.scan}, {"identity_observations", r.observed},
    {"known_devices_skipped", r.known_skipped}, {"ambiguous_devices_skipped", r.ambiguous_skipped},
    {"selected_endpoint_ids", r.selected_endpoint_ids}, {"metadata_updated", r.metadata_updated},
    {"inventory_complete", r.inventory_complete}, {"selected_target_complete", r.selected_target_complete}};
}
ScopedReport run_scoped_basic(EndpointProvider& provider, DeviceStore& store,
    const std::string& scope, const std::string& logical_id, std::stop_token stop, const ProgressSink& progress) {
  ScopedReport report; report.scope = scope;
  try {
    if (scope != "full" && scope != "update" && scope != "device") throw std::invalid_argument("Invalid scan scope");
    if (scope == "device" && logical_id.empty()) throw std::invalid_argument("Per-device scope requires a logical ID");
    if (stop.stop_requested()) { report.scan.result = "interrupted"; return report; }
    const auto resolve = store.identity_resolver();
    Discovery identities;
    auto discovery = provider.discover(stop, progress, [&](const Endpoint& endpoint) {
      ++report.observed;
      const auto identity = resolve(endpoint);
      if (scope != "device" || (identity.status == "known" && identity.logical_id == logical_id))
        identities.endpoints.push_back(endpoint);
      const bool selected = scope == "full" ||
        (scope == "update" && identity.status == "new" && endpoint.state != "not_present") ||
        (scope == "device" && identity.status == "known" && identity.logical_id == logical_id);
      if (identity.status == "ambiguous" && scope != "full") ++report.ambiguous_skipped;
      if (!selected && identity.status == "known") ++report.known_skipped;
      if (selected) report.selected_endpoint_ids.push_back(endpoint.endpoint_id);
      const char* capability_queries = "skipped";
      if (selected) capability_queries = endpoint.state == "active" ? "selected" : "metadata_only";
      if (progress) progress({"scope_selection", endpoint.endpoint_id, report.observed, 0,
        {{"scope", scope}, {"identity", identity.status}, {"logical_id", identity.logical_id},
         {"device_name", endpoint.device_name}, {"direction", endpoint.direction}, {"state", endpoint.state},
         {"capability_queries", capability_queries},
         {"fingerprint", endpoint.fingerprint}, {"local_identity", endpoint.local_identity}, {"driver", endpoint.driver}}});
      return selected;
    });
    discovery.full_inventory = scope == "full";
    report.inventory_complete = discovery.complete;
    identities.complete = discovery.complete;
    identities.cancelled = discovery.cancelled;
    identities.full_inventory = scope != "device";
    identities.diagnostics = discovery.diagnostics;
    if (progress) for (const auto& endpoint : discovery.endpoints)
      progress({"selected_endpoint_observation", endpoint.endpoint_id, 0, discovery.endpoints.size(),
        {{"device_name", endpoint.device_name}, {"state", endpoint.state}, {"fingerprint", endpoint.fingerprint},
         {"driver", endpoint.driver}, {"reported_capabilities", endpoint.reported_capabilities}, {"diagnostics", endpoint.diagnostics}}});
    if (stop.stop_requested() || discovery.cancelled) { report.scan.result = "interrupted"; return report; }
    if (scope == "device" && report.selected_endpoint_ids.size() != 1) {
      report.scan.diagnostics.push_back({"target_unavailable_or_ambiguous", "Selected device was not uniquely identified; no substitute was scanned", logical_id, {}});
      return report;
    }
    // An unrelated identity/name failure makes the global inventory incomplete,
    // but does not erase a completed selected endpoint's capability timestamp.
    // This scoped copy never establishes absence for unselected endpoints.
    auto selected = discovery;
    report.selected_target_complete = scope == "device" && discovery.endpoints.size() == 1 &&
      discovery.endpoints.front().diagnostics.empty() &&
      std::none_of(discovery.diagnostics.begin(), discovery.diagnostics.end(), [&](const auto& d) {
        return d.context.empty() || d.context == discovery.endpoints.front().endpoint_id;
      });
    if (report.selected_target_complete) selected.complete = true;
    report.scan = store.reconcile(selected, utc_now());
    if (!discovery.complete && report.scan.result == "completed") report.scan.result = "partially_completed";
    if (report.scan.result == "completed" || report.scan.result == "partially_completed") {
      const auto refreshed = store.refresh_metadata(identities, utc_now());
      report.metadata_updated = refreshed.saved;
      for (const auto& diagnostic : refreshed.diagnostics)
        if (std::none_of(report.scan.diagnostics.begin(), report.scan.diagnostics.end(), [&](const auto& existing) {
              return existing.code == diagnostic.code && existing.context == diagnostic.context && existing.message == diagnostic.message;
            })) report.scan.diagnostics.push_back(diagnostic);
      if (refreshed.result != "completed" && report.scan.result == "completed")
        report.scan.result = "partially_completed";
    }
    if (report.ambiguous_skipped) {
      report.scan.diagnostics.push_back({"identity_ambiguity", "Ambiguous endpoint identities were skipped; they were not treated as new devices", {}, {}});
      if (report.scan.result == "completed") report.scan.result = "partially_completed";
    }
  } catch (const std::exception& e) {
    report.scan.result = "failed";
    report.scan.diagnostics.push_back({"scoped_scan_failed", e.what(), {}, {}});
  }
  return report;
}
}
