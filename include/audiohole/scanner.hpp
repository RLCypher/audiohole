#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace audiohole::scanner {
using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;

Json status(const std::string& value);
std::string utc_now();
std::string new_logical_id(const std::string& direction);
std::string safe_filename(const std::string& name);
std::string path_utf8(const fs::path& path);
fs::path utf8_path(const std::string& text);

struct Diagnostic {
  std::string code;
  std::string message;
  std::string context;
  std::string native_code;
};
void to_json(Json& json, const Diagnostic& diagnostic);

// These are observations, never portable identities or operational guarantees.
struct Endpoint {
  std::string endpoint_id;
  std::string direction;  // capture / render
  std::string device_name;
  std::string state;
  Json fingerprint = Json::object();
  Json local_identity = Json::object();
  Json driver = Json::object();
  Json reported_capabilities = Json::object();
  std::vector<Diagnostic> diagnostics;
};

struct Progress {
  std::string phase;
  std::string endpoint_id;
  std::size_t completed = 0;
  std::size_t total = 0;
  Json details = Json::object();
};
using ProgressSink = std::function<void(const Progress&)>;

struct Discovery {
  std::vector<Endpoint> endpoints;
  std::vector<Diagnostic> diagnostics;
  bool complete = false;
  bool cancelled = false;
  // Scoped capability queries do not establish absence of unselected endpoints.
  bool full_inventory = true;
};

using EndpointFilter = std::function<bool(const Endpoint&)>;

class EndpointProvider {
 public:
  virtual ~EndpointProvider() = default;
  // The filter runs after identity collection, BEFORE any capability queries.
  virtual Discovery discover(std::stop_token stop, const ProgressSink& progress,
                             const EndpointFilter& filter = {}) = 0;
};

// Only available on Windows; throws on other systems instead of faking an empty scan.
std::unique_ptr<EndpointProvider> make_windows_provider();

// A semantic merge is followed by a checked atomic file replacement. The expected
// bytes detect edits since load. This is not a lock against arbitrary text editors.
class AtomicWriter {
 public:
  virtual ~AtomicWriter() = default;
  virtual void write(const fs::path& path, const std::string& bytes,
                     const std::optional<std::string>& expected) = 0;
};
std::shared_ptr<AtomicWriter> make_atomic_writer();

struct ScanReport {
  std::string result = "failed";
  std::size_t discovered = 0;
  std::size_t saved = 0;
  std::vector<Diagnostic> diagnostics;
  std::size_t historical_skipped = 0;
};
void to_json(Json& json, const ScanReport& report);

struct ProbeTarget;
// Validated, immutable view of authoritative records. Loading normalizes legacy
// fields/filename authority in memory only; it never rewrites device JSON.
struct SavedDevice {
  fs::path path;
  Json record;
  Json local_binding = Json::object();
  std::string original_bytes;
};
struct InventorySnapshot {
  std::vector<SavedDevice> devices;
  std::vector<Diagnostic> diagnostics;
  bool complete = false;
};
struct IdentityResolution { std::string status; std::string logical_id; };
using IdentityResolver = std::function<IdentityResolution(const Endpoint&)>;
class DeviceStore {
 public:
  DeviceStore(fs::path portable_root, fs::path local_root,
              std::shared_ptr<AtomicWriter> writer = make_atomic_writer());
  ScanReport reconcile(const Discovery& discovery, const std::string& timestamp);
  // Metadata observations refresh known identities/availability without creating
  // records, replacing capability evidence, or advancing capability-scan dates.
  ScanReport refresh_metadata(const Discovery& discovery, const std::string& timestamp);
  InventorySnapshot inventory();
  // A user preference, not endpoint availability. Fresh read/merge plus checked
  // atomic replacement preserves external notes, extensions and identity.
  void set_hidden(const std::string& logical_id, bool hidden);
  // Explicit user action only. Retire scanner-owned records/history/bindings and
  // session evidence, retaining profile configuration and unrelated files. Failed
  // retirement rolls back without replacement; removal errors retain retired data
  // and return diagnostics. Never called on startup or cancellation.
  ScanReport clear_inventory();
  // User metadata only. The snapshot path/bytes reject stale editor writes.
  // Rename never replaces another device. Failed writes attempt a checked path
  // rollback; a concurrent external edit is retained and reported for recovery.
  void edit_metadata(const std::string& logical_id, const std::string& display_name,
    const std::string& notes, const std::string& expected_path, const std::string& expected_bytes);
  std::vector<ProbeTarget> probe_targets();
  void checkpoint_probe(const std::string& logical_id, const Json& attempt, bool terminal,
    bool refresh_summaries = true);
  // Derived views are regenerated once per device run; authoritative attempt and
  // device records remain durably checkpointed before/after each worker.
  void refresh_probe_summaries();
  IdentityResolver identity_resolver();
 private:
  fs::path portable_root_;
  fs::path local_root_;
  std::shared_ptr<AtomicWriter> writer_;
  ScanReport reconcile_impl(const Discovery&, const std::string&, bool metadata_only);
};

class BasicScanner {
 public:
  BasicScanner(EndpointProvider& provider, DeviceStore& store);
  ScanReport scan(std::stop_token stop = {}, const ProgressSink& progress = {});
 private:
  EndpointProvider& provider_;
  DeviceStore& store_;
};
}  // namespace audiohole::scanner
