#include "storage_internal.hpp"
#include "audiohole/deep_scan.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace audiohole::scanner {
namespace {
struct Record {
  fs::path path;
  std::string original;
  Json data;
};

std::string text(const Json& object, const std::string& key) {
  if (object.is_object() && object.contains(key) && object[key].is_string())
    return object[key].get<std::string>();
  return {};
}
void object_field(Json& j, const std::string& key) {
  if (!j.contains(key)) j[key] = Json::object();
  if (!j[key].is_object()) throw std::runtime_error("Expected object: " + key);
}
Json ordered_record(const Json& input) {
  Json result = Json::object();
  for (const auto* field : {"display_name", "device_name", "hidden", "notes", "logical_id",
                           "scan_history", "identity", "reported_capabilities",
                           "probed_capabilities", "driver", "diagnostics", "metadata"})
    if (input.contains(field)) result[field] = input[field];
  for (auto it = input.begin(); it != input.end(); ++it)
    if (!result.contains(it.key())) result[it.key()] = it.value();
  return result;
}

bool valid_timestamp(const Json& value) {
  if (value.is_object()) {
    const auto state = text(value, "status");
    return state == "unknown" || state == "unavailable" || state == "not_tested" || state == "not_applicable";
  }
  if (!value.is_string()) return false;
  static const std::regex pattern(R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(\.\d+)?Z$)");
  std::smatch parts;
  const auto string = value.get<std::string>();
  if (!std::regex_match(string, parts, pattern)) return false;
  const std::chrono::year_month_day date{std::chrono::year(std::stoi(parts[1])),
    std::chrono::month(static_cast<unsigned>(std::stoi(parts[2]))),
    std::chrono::day(static_cast<unsigned>(std::stoi(parts[3])))};
  return date.ok() && std::stoi(parts[4]) < 24 && std::stoi(parts[5]) < 60 && std::stoi(parts[6]) < 60;
}

Json normalize(Json j, const fs::path& path) {
  if (!j.is_object()) throw std::runtime_error("Device record must be an object");
  if (!j.contains("metadata") || !j["metadata"].is_object() ||
      !j["metadata"].contains("schema_version") || !j["metadata"]["schema_version"].is_number_integer())
    throw std::runtime_error("Missing integer metadata.schema_version");
  const auto version = j["metadata"]["schema_version"];
  if (version != 1 && version != 2) throw std::runtime_error("Unsupported device schema version");
  if (j["metadata"].contains("record_type") && j["metadata"]["record_type"] != "audio_device")
    throw std::runtime_error("Unexpected record_type");
  object_field(j, "identity");
  if (version == 1) {
    if (j.contains("windows_name")) {
      if (!j["windows_name"].is_string()) throw std::runtime_error("Invalid legacy windows_name");
      if (j.contains("device_name") && j["device_name"] != j["windows_name"])
        throw std::runtime_error("Conflicting windows_name and device_name");
      j["device_name"] = j["windows_name"];
      j.erase("windows_name");
    }
    if (j["identity"].contains("logical_id")) {
      if (j.contains("logical_id") && j["logical_id"] != j["identity"]["logical_id"])
        throw std::runtime_error("Conflicting logical IDs");
      j["logical_id"] = j["identity"]["logical_id"];
      j["identity"].erase("logical_id");
    }
    object_field(j, "scan_history");
    auto& history = j["scan_history"];
    if (history.contains("last_scanned")) {
      if (history.contains("last_attempt") && history["last_attempt"] != history["last_scanned"])
        throw std::runtime_error("Conflicting legacy scan timestamps");
      history["last_attempt"] = history["last_scanned"];
      history["last_scanned"] = text(history, "last_scan_result") == "completed"
        ? history["last_attempt"] : status("unknown");
    }
    history["first_scanned_origin"] = "legacy_first_completed_scan";
  }
  for (const auto* key : {"logical_id", "device_name"})
    if (text(j, key).empty()) throw std::runtime_error(std::string("Missing nonempty string: ") + key);
  const auto direction = text(j["identity"], "direction");
  if (direction != "capture" && direction != "render") throw std::runtime_error("Invalid endpoint direction");
  const auto directory = path_utf8(path.parent_path().filename());
  if ((direction == "capture" && directory != "recording") ||
      (direction == "render" && directory != "playback"))
    throw std::runtime_error("Endpoint direction conflicts with record directory");
  if (!j.contains("notes")) j["notes"] = "";
  if (!j.contains("hidden")) j["hidden"] = false;
  if (!j["hidden"].is_boolean()) throw std::runtime_error("hidden must be a boolean");
  if (!j["notes"].is_string()) throw std::runtime_error("notes must be a string");
  const auto name = path_utf8(path.stem());
  if (name.empty() || name.back() == '.' || name.back() == ' ' ||
      name.find_first_of("<>:\"/\\|?*") != std::string::npos ||
      std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }) ||
      (name.size() <= 180 && safe_filename(name) != name))
    throw std::runtime_error("Filename is not Windows-portable");
  j["display_name"] = name;
  for (const auto* key : {"scan_history", "reported_capabilities", "probed_capabilities", "driver"})
    object_field(j, key);
  object_field(j["identity"], "fingerprint");
  for (const auto* key : {"first_scanned", "last_scanned", "last_attempt", "last_successful_scan"})
    if (!j["scan_history"].contains(key)) j["scan_history"][key] = status("unknown");
  for (const auto* key : {"first_scanned", "last_scanned", "last_attempt", "last_successful_scan", "last_easy_scan", "last_advanced_scan"})
    if (j["scan_history"].contains(key) && !valid_timestamp(j["scan_history"][key]))
      throw std::runtime_error(std::string("Invalid UTC scan timestamp: ") + key);
  for (const auto* key : {"sample_rates_hz", "sample_formats", "channel_counts", "default_mix_format"})
    if (!j["reported_capabilities"].contains(key)) j["reported_capabilities"][key] = status("unavailable");
  for (const auto* key : {"shared_mode", "exclusive_mode"})
    if (!j["reported_capabilities"].contains(key)) j["reported_capabilities"][key] = "unknown";
  for (const auto* key : {"scan_status", "verified_configurations", "failed_configurations", "inconclusive_configurations"})
    if (!j["probed_capabilities"].contains(key)) j["probed_capabilities"][key] =
      std::string(key) == "scan_status" ? Json("not_tested") : status("not_tested");
  if (!j["driver"].contains("association_status")) j["driver"]["association_status"] = "unknown";
  j["metadata"]["schema_version"] = 2;
  j["metadata"]["record_type"] = "audio_device";
  return ordered_record(j);
}

bool same_filename(const fs::path& a, const fs::path& b) {
#ifdef _WIN32
  const auto aw = a.wstring(), bw = b.wstring();
  return CompareStringOrdinal(aw.c_str(), -1, bw.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
  auto aa = path_utf8(a), bb = path_utf8(b);
  auto lower = [](unsigned char c) { return std::tolower(c); };
  std::transform(aa.begin(), aa.end(), aa.begin(), lower);
  std::transform(bb.begin(), bb.end(), bb.begin(), lower);
  return aa == bb;
#endif
}

bool serial_conflict(const Json& a, const Json& b) {
  const auto as = a.contains("usb") ? text(a["usb"], "serial_number") : std::string{};
  const auto bs = b.contains("usb") ? text(b["usb"], "serial_number") : std::string{};
  return !as.empty() && !bs.empty() && as != bs;
}
bool strong_match(const Json& a, const Json& b) {
  // Deliberately conservative until D-036 is validated against real hardware.
  if (!a.contains("usb") || !b.contains("usb") || !a.contains("endpoint") || !b.contains("endpoint")) return false;
  for (const auto* field : {"serial_number", "vendor_id", "product_id", "interface_number"}) {
    const auto value = text(a["usb"], field);
    if (value.empty() || value != text(b["usb"], field)) return false;
  }
  const auto role = text(a["endpoint"], "role");
  return !role.empty() && role == text(b["endpoint"], "role");
}
bool possible_match(const Json& record, const Endpoint& endpoint) {
  const auto& a = record["identity"]["fingerprint"];
  const auto& b = endpoint.fingerprint;
  if (serial_conflict(a, b)) return false;
  if (text(record, "device_name") == endpoint.device_name ||
      text(record["identity"], "current_device_name") == endpoint.device_name) return true;
  if (!a.contains("usb") || !b.contains("usb")) return false;
  for (const auto* field : {"vendor_id", "product_id"})
    if (text(a["usb"], field).empty() || text(a["usb"], field) != text(b["usb"], field)) return false;
  return !text(a, "product").empty() && text(a, "product") == text(b, "product");
}
void merge_observation(Json& destination, const Json& observation) {
  // Unknown/user extensions survive; only observed, scanner-owned keys change.
  for (auto it = observation.begin(); it != observation.end(); ++it) {
    if (it.value().is_object() && !it.value().contains("status") && destination.contains(it.key()) &&
        destination[it.key()].is_object() && !destination[it.key()].contains("status"))
      merge_observation(destination[it.key()], it.value());
    else destination[it.key()] = it.value();
  }
}
bool observation_changed(const Json& previous, const Json& observation) {
  for (auto it = observation.begin(); it != observation.end(); ++it) {
    if (!previous.contains(it.key())) return true;
    if (it.value().is_object() && previous[it.key()].is_object() && !it.value().contains("status")) {
      if (observation_changed(previous[it.key()], it.value())) return true;
    } else if (previous[it.key()] != it.value()) return true;
  }
  return false;
}
void append_change(Json& object, const std::string& key, const Json& old_value,
                   const std::string& timestamp) {
  if (!object.contains(key)) object[key] = Json::array();
  if (!object[key].is_array()) throw std::runtime_error("History must be an array: " + key);
  object[key].push_back(Json{{"observed_before", timestamp}, {"value", old_value}});
}
void save(AtomicWriter& writer, const fs::path& path, const Json& data,
          const std::optional<std::string>& expected) {
  writer.write(path, data.dump(2) + "\n", expected);
}
}

DeviceStore::DeviceStore(fs::path portable_root, fs::path local_root, std::shared_ptr<AtomicWriter> writer)
    : portable_root_(std::move(portable_root)), local_root_(std::move(local_root)), writer_(std::move(writer)) {
  if (!writer_) throw std::invalid_argument("Atomic writer is required");
  if (fs::absolute(portable_root_).lexically_normal() == fs::absolute(local_root_).lexically_normal())
    throw std::invalid_argument("Portable and machine-local roots must differ");
}

ScanReport DeviceStore::reconcile(const Discovery& discovery, const std::string& timestamp) {
  return reconcile_impl(discovery, timestamp, false);
}
ScanReport DeviceStore::refresh_metadata(const Discovery& discovery, const std::string& timestamp) {
  return reconcile_impl(discovery, timestamp, true);
}
ScanReport DeviceStore::reconcile_impl(const Discovery& discovery, const std::string& timestamp, bool metadata_only) {
  ScanReport report{"failed", discovery.endpoints.size(), 0, discovery.diagnostics};
  if (discovery.cancelled) { report.result = "interrupted"; return report; }
  if (!discovery.complete && discovery.endpoints.empty()) {
    if (!discovery.full_inventory) report.result = "partially_completed";
    return report;
  }
  try {
    if (!valid_timestamp(Json(timestamp))) throw std::runtime_error("Invalid scan timestamp");
    detail::StoreLock portable_lock(portable_root_);
    detail::StoreLock local_lock(local_root_);
    std::vector<Record> records;
    std::set<std::string> ids;
    bool invalid = false;
    for (const auto* directory : {"recording", "playback"}) {
      const auto folder = portable_root_ / "devices" / directory;
      if (!fs::exists(folder)) continue;
      std::vector<fs::path> paths;
      for (const auto& entry : fs::directory_iterator(folder)) {
        auto extension = path_utf8(entry.path().extension());
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (extension == ".json") paths.push_back(entry.path());
      }
      std::sort(paths.begin(), paths.end());
      for (const auto& path : paths) {
        try {
          const auto original = detail::read_optional(path).value();
          auto data = normalize(detail::parse_json(original), path);
          if (!ids.insert(text(data, "logical_id")).second) throw std::runtime_error("Duplicate logical_id");
          for (const auto& r : records)
            if (same_filename(r.path, path)) throw std::runtime_error("Case-insensitive filename conflict");
          records.push_back({path, original, std::move(data)});
        } catch (const std::exception& e) {
          report.diagnostics.push_back({"invalid_record", e.what(), path_utf8(path), {}});
          invalid = true;
        }
      }
    }
    // An unreadable identity record could match any discovered endpoint. Do not
    // create duplicates or overwrite summaries until the inventory is trustworthy.
    if (invalid) return report;

    const auto cache_path = local_root_ / "device_bindings.json";
    const auto cache_original = detail::read_optional(cache_path);
    Json cache = cache_original ? detail::parse_json(*cache_original)
      : Json{{"metadata", {{"schema_version", 1}}}, {"bindings", Json::object()}};
    if (!cache.is_object() || !cache.contains("metadata") || !cache["metadata"].is_object() ||
        !cache["metadata"].contains("schema_version") || cache["metadata"]["schema_version"] != 1 || !cache.contains("bindings") ||
        !cache["bindings"].is_object()) throw std::runtime_error("Invalid machine-local binding cache");
    auto& bindings = cache["bindings"];
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
      if (!it.value().is_object() || text(it.value(), "endpoint_id").empty() ||
          (text(it.value(), "direction") != "capture" && text(it.value(), "direction") != "render"))
        throw std::runtime_error("Invalid endpoint binding: " + it.key());
      if (discovery.complete && discovery.full_inventory) it.value()["state"] = "not_present";
    }

    const auto old_count = records.size();
    std::vector<std::optional<std::size_t>> matches(discovery.endpoints.size());
    std::map<std::size_t, std::size_t> claims;
    std::set<std::size_t> rejected;
    std::set<std::size_t> historical;
    std::set<std::string> endpoint_ids;
    for (std::size_t i = 0; i < discovery.endpoints.size(); ++i) {
      const auto& endpoint = discovery.endpoints[i];
      report.diagnostics.insert(report.diagnostics.end(), endpoint.diagnostics.begin(), endpoint.diagnostics.end());
      if (endpoint.endpoint_id.empty() || endpoint.device_name.empty() ||
          (endpoint.direction != "capture" && endpoint.direction != "render") ||
          !endpoint.fingerprint.is_object() || !endpoint.local_identity.is_object() ||
          !endpoint.driver.is_object() || !endpoint.reported_capabilities.is_object() ||
          !endpoint_ids.insert(endpoint.endpoint_id).second)
        throw std::runtime_error("Invalid or duplicate endpoint observation");
      std::vector<std::size_t> local, strong, possible;
      for (std::size_t n = 0; n < old_count; ++n) {
        const auto& record = records[n].data;
        if (text(record["identity"], "direction") != endpoint.direction) continue;
        const auto& fingerprint = record["identity"]["fingerprint"];
        if (serial_conflict(fingerprint, endpoint.fingerprint)) continue;
        const auto id = text(record, "logical_id");
        if (bindings.contains(id) && text(bindings[id], "direction") == endpoint.direction) {
          const auto stable = text(endpoint.local_identity, "stable_id");
          if (text(bindings[id], "endpoint_id") == endpoint.endpoint_id ||
              (endpoint.state != "not_present" && !stable.empty() && text(bindings[id], "stable_id") == stable)) local.push_back(n);
        }
        if (endpoint.state != "not_present" && strong_match(fingerprint, endpoint.fingerprint)) strong.push_back(n);
        if (possible_match(record, endpoint)) possible.push_back(n);
      }
      if (endpoint.state == "not_present" && local.empty()) {
        historical.insert(i);
        ++report.historical_skipped;
        continue;
      }
      const auto& candidates = !local.empty() ? local : !strong.empty() ? strong : possible;
      if (candidates.size() == 1 && (!local.empty() || !strong.empty())) {
        matches[i] = candidates[0];
        ++claims[candidates[0]];
      } else if (!candidates.empty()) {
        rejected.insert(i);
        report.diagnostics.push_back({"identity_ambiguity", "Device association requires confirmation", endpoint.endpoint_id, {}});
      }
    }
    for (std::size_t i = 0; i < matches.size(); ++i) {
      if (matches[i] && claims[*matches[i]] != 1) {
        rejected.insert(i);
        report.diagnostics.push_back({"identity_ambiguity", "Multiple endpoints claim the same logical record",
                                      discovery.endpoints[i].endpoint_id, {}});
      }
    }

    std::set<std::size_t> changed;
    for (std::size_t i = 0; i < discovery.endpoints.size(); ++i) {
      if (rejected.contains(i) || historical.contains(i)) continue;
      const auto& endpoint = discovery.endpoints[i];
      if (!matches[i]) {
        if (metadata_only) continue; // New endpoints require an explicit capability scan.
        const auto folder = portable_root_ / "devices" / (endpoint.direction == "capture" ? "recording" : "playback");
        const auto base = safe_filename(endpoint.device_name);
        fs::path path;
        for (std::size_t suffix = 1;; ++suffix) {
          auto name = base + (suffix == 1 ? "" : " (" + std::to_string(suffix) + ")");
          path = folder / utf8_path(name + ".json");
          const bool taken = fs::exists(path) || std::any_of(records.begin(), records.end(),
            [&](const auto& r) { return same_filename(path, r.path); });
          if (!taken) break;
        }
        std::string id;
        do { id = new_logical_id(endpoint.direction); } while (!ids.insert(id).second);
        Json data{{"display_name", path_utf8(path.stem())}, {"device_name", endpoint.device_name},
                  {"notes", ""}, {"logical_id", id},
                  {"scan_history", {{"first_scanned", timestamp}, {"first_scanned_origin", "initial_discovery"}}},
                  {"identity", {{"direction", endpoint.direction}, {"fingerprint", Json::object()}}},
                  {"reported_capabilities", Json::object()},
                  {"probed_capabilities", {{"scan_status", "not_tested"},
                    {"verified_configurations", status("not_tested")},
                    {"failed_configurations", status("not_tested")},
                    {"inconclusive_configurations", status("not_tested")}}},
                  {"driver", Json::object()},
                  {"metadata", {{"schema_version", 2}, {"record_type", "audio_device"}}}};
        matches[i] = records.size();
        records.push_back({path, {}, normalize(std::move(data), path)});
      }
      auto& record = records[*matches[i]].data;
      auto& history = record["scan_history"];
      if (!history.contains("first_scanned")) {
        history["first_scanned"] = status("unknown"); // do not invent the discovery time of an imported record
        history["first_scanned_origin"] = "unavailable_in_imported_record";
      }
      if (!metadata_only) {
        history["last_attempt"] = timestamp;
        if (discovery.complete) history["last_scanned"] = timestamp;
        history["last_scan_type"] = "basic";
        history["last_easy_scan"] = timestamp;
        history["last_scan_result"] = discovery.complete ? "completed" : "partially_completed";
        if (discovery.complete) history["last_successful_scan"] = timestamp;
      }
      auto& identity = record["identity"];
      const auto old_name = text(identity, "current_device_name");
      if (!old_name.empty() && old_name != endpoint.device_name)
        append_change(identity, "name_history", old_name, timestamp);
      identity["current_device_name"] = endpoint.device_name;
      if (*matches[i] < old_count && !identity["fingerprint"].empty() && observation_changed(identity["fingerprint"], endpoint.fingerprint))
        append_change(history, "fingerprint_history", identity["fingerprint"], timestamp);
      merge_observation(identity["fingerprint"], endpoint.fingerprint);
      auto& driver = record["driver"];
      if (*matches[i] < old_count && !driver.empty() && driver != endpoint.driver) {
        // Compare only observed keys; user extensions must not manufacture a change.
        if (observation_changed(driver, endpoint.driver)) {
          append_change(history, "driver_history", driver, timestamp);
          record["probed_capabilities"]["evidence_currency"] = "requires_revalidation";
        }
      }
      merge_observation(driver, endpoint.driver);
      if (!metadata_only) {
        merge_observation(record["reported_capabilities"], endpoint.reported_capabilities);
        record["diagnostics"] = endpoint.diagnostics;
      }
      const auto id = text(record, "logical_id");
      if (!bindings.contains(id)) bindings[id] = Json::object();
      auto& binding = bindings[id];
      merge_observation(binding, endpoint.local_identity);
      binding["endpoint_id"] = endpoint.endpoint_id;
      binding["direction"] = endpoint.direction;
      binding["state"] = endpoint.state;
      binding["last_seen"] = timestamp;
      binding["match_reason"] = *matches[i] >= old_count ? "new_endpoint" : "local_binding_or_durable_fingerprint";
      record = ordered_record(record);
      changed.insert(*matches[i]);
    }

    // Bindings precede records so a failed record write leaves, at worst, an
    // ignored orphan cache entry. A successful new record always has a binding.
    save(*writer_, cache_path, cache, cache_original);
    for (std::size_t n = 0; n < records.size(); ++n) {
      auto& record = records[n];
      const auto bytes = record.data.dump(2) + "\n";
      if (changed.contains(n) || (discovery.full_inventory && !metadata_only && bytes != record.original)) {
        try {
          writer_->write(record.path, bytes, record.original.empty() ? std::nullopt
                                                                   : std::optional(record.original));
          if (changed.contains(n)) ++report.saved;
        } catch (const std::exception& e) {
          report.diagnostics.push_back({"write_failure", e.what(), path_utf8(record.path), {}});
          // Summaries must not describe changes which were not committed.
          report.result = report.saved ? "partially_completed" : "failed";
          return report;
        }
      }
    }
    for (const auto* direction : {"capture", "render"}) {
      Json summary{{"generated", true}, {"source", "individual_device_records"},
                   {"schema_version", 2}, {"devices", Json::array()}};
      for (const auto& record : records) {
        if (text(record.data["identity"], "direction") != direction) continue;
        auto item = record.data;
        const auto id = text(item, "logical_id");
        item["availability"] = bindings.contains(id) ? Json(text(bindings[id], "state")) : status("unknown");
        summary["devices"].push_back(std::move(item));
      }
      const auto path = portable_root_ / "devices" /
        (std::string(direction) == "capture" ? "recording_devices.json" : "playback_devices.json");
      save(*writer_, path, summary, detail::read_optional(path));
    }
    report.result = discovery.complete && rejected.empty() && report.diagnostics.empty()
      ? "completed" : "partially_completed";
  } catch (const std::exception& e) {
    report.diagnostics.push_back({"store_failure", e.what(), path_utf8(portable_root_), {}});
    report.result = report.saved ? "partially_completed" : "failed";
  }
  return report;
}

namespace {
std::vector<Record> probe_records(const fs::path& root) {
  std::vector<Record> records;
  std::set<std::string> ids;
  std::uintmax_t bytes_read = 0;
  for (const auto* directory : {"recording", "playback"}) {
    const auto folder = root / "devices" / directory;
    if (!fs::exists(folder)) continue;
    for (const auto& entry : fs::directory_iterator(folder)) {
      auto extension = path_utf8(entry.path().extension());
      std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
      if (extension != ".json") continue;
      if (records.size() >= 4096) throw std::runtime_error("Device inventory exceeds 4096 records");
      try {
        const auto original = detail::read_optional(entry.path()).value();
        bytes_read += original.size();
        if (bytes_read > 128ULL * 1024 * 1024) throw std::runtime_error("Device inventory exceeds 128 MiB");
        auto record = normalize(detail::parse_json(original), entry.path());
        if (!ids.insert(text(record, "logical_id")).second) throw std::runtime_error("Duplicate logical ID in probe inventory");
        for (const auto& existing : records)
          if (same_filename(existing.path, entry.path())) throw std::runtime_error("Case-insensitive filename conflict");
        records.push_back({entry.path(), original, std::move(record)});
      } catch (const std::exception& error) {
        throw std::runtime_error(path_utf8(entry.path()) + ": " + error.what());
      }
    }
  }
  return records;
}
Json probe_bindings(const fs::path& root) {
  const auto bytes = detail::read_optional(root / "device_bindings.json");
  if (!bytes) throw std::runtime_error("Run Basic Scan before Deep Scan");
  const auto cache = detail::parse_json(*bytes);
  if (!cache.contains("metadata") || cache["metadata"].value("schema_version", 0) != 1 ||
      !cache.contains("bindings") || !cache["bindings"].is_object()) throw std::runtime_error("Invalid local probe bindings");
  return cache["bindings"];
}
}

InventorySnapshot DeviceStore::inventory() {
  InventorySnapshot result;
  try {
    detail::StoreLock portable_lock(portable_root_);
    detail::StoreLock local_lock(local_root_);
    const auto records = probe_records(portable_root_);
    const auto bindings = fs::exists(local_root_ / "device_bindings.json")
      ? probe_bindings(local_root_) : Json::object();
    for (const auto& record : records) {
      const auto id = text(record.data, "logical_id");
      auto binding = bindings.contains(id) ? bindings.at(id) : Json::object();
      if (!binding.is_object() || (!binding.empty() &&
          (text(binding, "endpoint_id").empty() || text(binding, "direction") != text(record.data["identity"], "direction"))))
        throw std::runtime_error("Invalid local binding for saved device: " + id);
      result.devices.push_back({record.path, record.data, std::move(binding), record.original});
    }
    std::sort(result.devices.begin(), result.devices.end(), [](const auto& a, const auto& b) {
      return text(a.record["identity"], "direction") + text(a.record, "display_name") <
             text(b.record["identity"], "direction") + text(b.record, "display_name");
    });
    result.complete = true;
  } catch (const std::exception& error) {
    result.devices.clear();
    result.diagnostics.push_back({"inventory_unavailable", error.what(), path_utf8(portable_root_), {}});
  }
  return result;
}
void DeviceStore::set_hidden(const std::string& logical_id, bool hidden) {
  detail::StoreLock portable_lock(portable_root_);
  auto records = probe_records(portable_root_);
  auto found = std::find_if(records.begin(), records.end(), [&](const auto& record) {
    return text(record.data, "logical_id") == logical_id;
  });
  if (found == records.end()) throw std::runtime_error("Saved device no longer exists");
  found->data["hidden"] = hidden;
  writer_->write(found->path, ordered_record(found->data).dump(2) + "\n", found->original);
}

void DeviceStore::edit_metadata(const std::string &logical_id, const std::string &display_name,
                                const std::string &notes, const std::string &expected_path,
                                const std::string &expected_bytes) {
  if (display_name.empty() || display_name.size() > 180 ||
      safe_filename(display_name) != display_name)
    throw std::invalid_argument("Use a nonempty Windows-compatible device name (up to 180 UTF-8 "
                                "bytes); avoid reserved names, slashes and trailing spaces/dots");
  if (notes.size() > 65536)
    throw std::invalid_argument("Device notes exceed 64 KiB");
  detail::StoreLock portable_lock(portable_root_);
  auto records = probe_records(portable_root_);
  auto found = std::find_if(records.begin(), records.end(), [&](const auto &record) {
    return text(record.data, "logical_id") == logical_id;
  });
  if (found == records.end())
    throw std::runtime_error("Device no longer exists; reload saved devices");
  if (path_utf8(found->path) != expected_path || found->original != expected_bytes)
    throw std::runtime_error("Device changed externally since it was loaded; reload before saving");
  const auto destination = found->path.parent_path() / utf8_path(display_name + ".json");
  for (const auto &other : records)
    if (text(other.data, "logical_id") != logical_id && same_filename(other.path, destination))
      throw std::runtime_error(
          "Another device already uses that filename; choose a different name");
  const bool renamed = destination != found->path;
  if (renamed && fs::exists(destination) && !fs::equivalent(found->path, destination))
    throw std::runtime_error("Destination already exists; no file was replaced");
  auto updated = found->data;
  updated["display_name"] = display_name;
  updated["notes"] = notes;
  const auto bytes = ordered_record(updated).dump(2) + "\n";
  // Move the single canonical record first, avoiding a duplicate logical ID.
  // Contents are then atomically replaced; interruptions can retain the new
  // filename with old notes. Never roll back over an intervening external edit.
  if (renamed)
    detail::move_no_replace(found->path, destination);
  try {
    writer_->write(destination, bytes, found->original);
  } catch (const std::exception &error) {
    if (renamed) {
      try {
        if (detail::read_optional(destination) != std::optional(found->original))
          throw std::runtime_error("Renamed record was changed externally");
        detail::move_no_replace(destination, found->path);
      } catch (const std::exception &recovery) {
        throw std::runtime_error(std::string(error.what()) +
                                 "; rename rollback could not finish: " + recovery.what() +
                                 ". Reload saved devices and inspect " + path_utf8(destination));
      }
    }
    throw;
  }
}

std::vector<ProbeTarget> DeviceStore::probe_targets() {
  detail::StoreLock portable_lock(portable_root_);
  detail::StoreLock local_lock(local_root_);
  const auto records = probe_records(portable_root_);
  const auto bindings = probe_bindings(local_root_);
  std::vector<ProbeTarget> targets;
  std::set<std::string> endpoint_ids;
  for (const auto& record : records) {
    const auto id = text(record.data, "logical_id");
    if (!bindings.contains(id) || text(bindings[id], "state") != "active") continue;
    const auto endpoint = text(bindings[id], "endpoint_id");
    if (endpoint.empty() || text(bindings[id], "direction") != text(record.data["identity"], "direction") ||
        !endpoint_ids.insert(endpoint).second) throw std::runtime_error("Invalid or ambiguous active probe binding");
    targets.push_back({id, endpoint, text(record.data, "display_name"), text(record.data["identity"], "direction"), record.data});
  }
  std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) {
    return a.direction + a.display_name < b.direction + b.display_name;
  });
  return targets;
}

IdentityResolver DeviceStore::identity_resolver() {
  detail::StoreLock portable_lock(portable_root_);
  detail::StoreLock local_lock(local_root_);
  auto records = probe_records(portable_root_);
  auto bindings = fs::exists(local_root_ / "device_bindings.json") ? probe_bindings(local_root_) : Json::object();
  for (const auto& binding : bindings) {
    if (!binding.is_object() || text(binding, "endpoint_id").empty() ||
        (text(binding, "direction") != "capture" && text(binding, "direction") != "render"))
      throw std::runtime_error("Invalid endpoint binding in identity resolver");
  }
  return [records = std::move(records), bindings = std::move(bindings)](const Endpoint& endpoint) {
    std::vector<std::string> local, strong, possible;
    for (const auto& saved : records) {
      const auto& record = saved.data;
      if (text(record["identity"], "direction") != endpoint.direction) continue;
      const auto& fingerprint = record["identity"]["fingerprint"];
      if (serial_conflict(fingerprint, endpoint.fingerprint)) continue;
      const auto id = text(record, "logical_id");
      if (bindings.contains(id) && text(bindings[id], "direction") == endpoint.direction) {
        const auto stable = text(endpoint.local_identity, "stable_id");
        if (text(bindings[id], "endpoint_id") == endpoint.endpoint_id ||
            (endpoint.state != "not_present" && !stable.empty() && text(bindings[id], "stable_id") == stable)) local.push_back(id);
      }
      if (endpoint.state != "not_present" && strong_match(fingerprint, endpoint.fingerprint)) strong.push_back(id);
      if (possible_match(record, endpoint)) possible.push_back(id);
    }
    // Match reconciliation's historical-instance policy. An unseen not_present
    // alias is not an ambiguous/new device eligible for an Update scan.
    if (endpoint.state == "not_present" && local.empty()) return IdentityResolution{"historical", {}};
    const auto& candidates = !local.empty() ? local : !strong.empty() ? strong : possible;
    if (candidates.size() == 1 && (!local.empty() || !strong.empty())) return IdentityResolution{"known", candidates[0]};
    return IdentityResolution{candidates.empty() ? "new" : "ambiguous", {}};
  };
}

void DeviceStore::checkpoint_probe(const std::string& logical_id, const Json& attempt, bool terminal) {
  const auto attempt_id = text(attempt, "attempt_id");
  if (attempt_id.empty() || safe_filename(attempt_id) != attempt_id || text(attempt, "logical_id") != logical_id ||
      attempt.contains("endpoint_id")) throw std::runtime_error("Invalid portable probe checkpoint");
  detail::StoreLock portable_lock(portable_root_);
  detail::StoreLock local_lock(local_root_);
  auto records = probe_records(portable_root_);
  const auto match = std::find_if(records.begin(), records.end(), [&](const auto& r) { return text(r.data, "logical_id") == logical_id; });
  if (match == records.end()) throw std::runtime_error("Probe device record no longer exists");
  auto& data = match->data;
  auto& caps = data["probed_capabilities"];
  if (!caps.contains("test_attempts")) caps["test_attempts"] = Json::array();
  if (!caps["test_attempts"].is_array()) throw std::runtime_error("test_attempts must be an array");
  auto& attempts = caps["test_attempts"];
  auto old = std::find_if(attempts.begin(), attempts.end(), [&](const auto& a) { return text(a, "attempt_id") == attempt_id; });
  if (old == attempts.end()) attempts.push_back(attempt);
  else merge_observation(*old, attempt);
  const auto history_name = attempt_id + ".json";
  const auto history_path = portable_root_ / "probe_history" / utf8_path(history_name);
  caps["scan_status"] = terminal ? text(attempt, "status") : "running";
  caps["latest_attempt_id"] = attempt_id;
  if (terminal) {
    const auto result = text(attempt, "classification");
    const auto list = result == "verified" ? "verified_configurations" : result == "accepted" ? "accepted_configurations" :
      result == "failed" ? "failed_configurations" : "inconclusive_configurations";
    if (!caps.contains(list) || (caps[list].is_object() && text(caps[list], "status") == "not_tested")) caps[list] = Json::array();
    if (!caps[list].is_array()) throw std::runtime_error("Probe configuration history is not an array");
    auto existing = std::find_if(caps[list].begin(), caps[list].end(), [&](const auto& a) { return text(a, "attempt_id") == attempt_id; });
    Json summary{{"attempt_id", attempt_id}, {"mode", attempt.at("mode")}, {"configuration", attempt.at("format")},
      {"result", result}, {"verification_scope", "buffer_processing"}, {"signal_verification", "not_tested"},
      {"history_file", "probe_history/" + history_name}};
    if (existing == caps[list].end()) caps[list].push_back(std::move(summary)); else merge_observation(*existing, summary);
    data["scan_history"]["last_advanced_scan"] = attempt.at("finished_utc");
  }
  save(*writer_, history_path, attempt, detail::read_optional(history_path));
  save(*writer_, match->path, ordered_record(data), match->original);
  if (!terminal) return;
  const auto bindings = probe_bindings(local_root_);
  for (const auto* direction : {"capture", "render"}) {
    Json summary{{"generated", true}, {"source", "individual_device_records"}, {"schema_version", 2}, {"devices", Json::array()}};
    for (const auto& record : records) {
      if (text(record.data["identity"], "direction") != direction) continue;
      auto item = record.data;
      const auto id = text(item, "logical_id");
      item["availability"] = bindings.contains(id) ? Json(text(bindings[id], "state")) : status("unknown");
      summary["devices"].push_back(std::move(item));
    }
    const auto path = portable_root_ / "devices" /
      (std::string(direction) == "capture" ? "recording_devices.json" : "playback_devices.json");
    save(*writer_, path, summary, detail::read_optional(path));
  }
}
}  // namespace audiohole::scanner
