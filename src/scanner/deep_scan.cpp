#include "audiohole/deep_scan.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <set>
#include <stdexcept>

namespace audiohole::scanner {
namespace {
unsigned number(const Json& object, const char* key) {
  if (!object.contains(key) || !object[key].is_number_integer() || object[key] < 0 || object[key] > 100000000)
    throw std::invalid_argument(std::string("Invalid format field: ") + key);
  return object[key].get<unsigned>();
}
Json candidate_format(unsigned rate, unsigned channels, unsigned bits, unsigned valid,
                       const std::string& representation, const Json& mask, bool extensible) {
  Json result{{"sample_rate_hz", rate}, {"container_bits", bits}, {"valid_bits", valid},
    {"channel_count", channels}, {"channel_mask", mask}, {"sample_format", representation},
    {"format_tag", extensible ? 65534 : representation == "float32" ? 3 : 1},
    {"block_align_bytes", channels * bits / 8}, {"average_bytes_per_second", rate * channels * bits / 8}};
  if (extensible) result["subformat_guid"] = representation == "float32"
    ? "{00000003-0000-0010-8000-00AA00389B71}" : "{00000001-0000-0010-8000-00AA00389B71}";
  return result;
}
std::string classification(const Json& attempt, unsigned duration) {
  const auto value = attempt.value("classification", std::string{});
  if (value != "verified" && value != "accepted" && value != "failed" && value != "inconclusive")
    throw std::runtime_error("Invalid probe outcome");
  if (value == "verified" && (attempt.value("cleanup", "") != "completed" ||
      attempt.value("verification_scope", "") != "buffer_processing" ||
      number(attempt, "runtime_duration_ms") < duration || number(attempt, "serviced_frames") == 0 ||
      number(attempt, "audio_events") < 3))
    throw std::runtime_error("Probe verification lacks completed cleanup or scope");
  return value;
}
}

void validate_deep_limits(const DeepLimits& l) {
  if (l.max_attempts < 1 || l.max_attempts > 64 || l.duration_ms < 250 || l.duration_ms > 30000 ||
      l.attempt_timeout_ms < l.duration_ms + 1000 || l.attempt_timeout_ms > 120000 ||
      l.total_timeout_ms < 1000 || l.total_timeout_ms > 600000 ||
      (l.mode != "shared" && l.mode != "exclusive" && l.mode != "both"))
    throw std::invalid_argument("Invalid Deep Scan budget, duration, timeout, or mode");
}

void validate_probe_format(const Json& f) {
  if (!f.is_object()) throw std::invalid_argument("Probe format must be an object");
  const auto rate = number(f, "sample_rate_hz"), channels = number(f, "channel_count");
  const auto bits = number(f, "container_bits"), valid = number(f, "valid_bits");
  const auto tag = number(f, "format_tag");
  if (rate < 8000 || rate > 384000 || channels < 1 || channels > 32 ||
      (bits != 16 && bits != 24 && bits != 32) || valid == 0 || valid > bits ||
      (tag != 1 && tag != 3 && tag != 65534)) throw std::invalid_argument("Probe format exceeds PCM test scope");
  const auto representation = f.value("sample_format", std::string{});
  const bool integer = (representation == "pcm_s16" && bits == 16 && valid == 16) ||
    (representation == "pcm_s24_packed" && bits == 24 && valid == 24) ||
    (representation == "pcm_s24_in_32" && bits == 32 && valid == 24) ||
    (representation == "pcm_s32" && bits == 32 && valid == 32);
  const bool floating = representation == "float32" && bits == 32 && valid == 32;
  if ((!integer && !floating) || (tag == 1 && !integer) || (tag == 3 && !floating) ||
      (tag != 65534 && (channels > 2 || valid != bits))) throw std::invalid_argument("Inconsistent PCM descriptor");
  if (tag == 65534) {
    const auto guid = f.value("subformat_guid", std::string{});
    if (guid != (floating ? "{00000003-0000-0010-8000-00AA00389B71}" : "{00000001-0000-0010-8000-00AA00389B71}"))
      throw std::invalid_argument("Inconsistent subformat GUID");
  }
  if (!f.contains("channel_mask")) throw std::invalid_argument("Missing channel-mask evidence");
  if (f["channel_mask"].is_number_integer()) {
    if (f["channel_mask"] < 0 || f["channel_mask"] > 0xffffffffULL) throw std::invalid_argument("Invalid channel mask");
    const auto mask = f["channel_mask"].get<std::uint32_t>();
    if (mask && std::popcount(mask) != static_cast<int>(channels)) throw std::invalid_argument("Channel mask/count mismatch");
  } else if (!f["channel_mask"].is_object() || f["channel_mask"].value("status", "") != "unknown")
    throw std::invalid_argument("Invalid unknown channel mask");
  if (number(f, "block_align_bytes") != channels * bits / 8 ||
      number(f, "average_bytes_per_second") != rate * channels * bits / 8)
    throw std::invalid_argument("Invalid format alignment or byte rate");
}

std::vector<ProbeCandidate> plan_deep_candidates(const ProbeTarget& target, const DeepLimits& limits) {
  validate_deep_limits(limits);
  std::vector<ProbeCandidate> result;
  std::set<std::string> keys;
  const auto& caps = target.record.at("reported_capabilities");
  Json seeds = Json::array();
  auto add = [&](const Json& format, const std::string& provenance) {
    try { validate_probe_format(format); } catch (const std::exception&) { return; }
    const auto normalized = candidate_format(number(format, "sample_rate_hz"), number(format, "channel_count"),
      number(format, "container_bits"), number(format, "valid_bits"), format["sample_format"],
      format["channel_mask"].is_number_integer() ? format["channel_mask"] : status("unknown"), number(format, "format_tag") == 65534);
    for (const auto* mode : {"shared", "exclusive"}) {
      if (limits.mode != "both" && limits.mode != mode) continue;
      const auto key = std::string(mode) + normalized.dump();
      if (keys.insert(key).second && result.size() < limits.max_attempts)
        result.push_back({normalized, mode, provenance});
    }
  };
  for (const auto* key : {"configured_device_format", "default_mix_format"}) {
    if (!caps.contains(key)) continue;
    try { validate_probe_format(caps[key]); } catch (const std::exception&) { continue; }
    seeds.push_back(caps[key]); add(caps[key], std::string("windows_") + key);
  }
  if (seeds.empty()) return result;
  const auto base = seeds.back();
  const auto channels = number(base, "channel_count");
  const auto mask = base["channel_mask"];
  const std::vector<unsigned> rates{number(base, "sample_rate_hz"), 44100, 48000, 96000, 88200, 192000};
  // Interleave rates across representation rounds, instead of spending every
  // attempt on the first rate. The plan is finite and its cap is user-visible.
  for (unsigned representation = 0; representation < 5; ++representation) {
    for (const auto rate : rates) {
      const unsigned bits = representation == 0 ? 16 : representation == 1 ? 24 : 32;
      const unsigned valid = representation == 2 ? 24 : bits;
      const char* names[]{"pcm_s16", "pcm_s24_packed", "pcm_s24_in_32", "pcm_s32", "float32"};
      add(candidate_format(rate, channels, bits, valid, names[representation], mask, true), "generated_candidate");
    }
  }
  for (const auto& seed : seeds) if (channels <= 2 && number(seed, "valid_bits") == number(seed, "container_bits"))
    add(candidate_format(number(seed, "sample_rate_hz"), number(seed, "channel_count"),
      number(seed, "container_bits"), number(seed, "valid_bits"), seed["sample_format"], status("unknown"), false),
      "legacy_descriptor_variant");
  return result;
}

Json probe_job_json(const ProbeJob& job) {
  return Json{{"protocol_version", 1}, {"consent", true}, {"attempt_id", job.attempt_id},
    {"logical_id", job.target.logical_id}, {"endpoint_id", job.target.endpoint_id}, {"direction", job.target.direction},
    {"mode", job.candidate.mode}, {"format", job.candidate.format}, {"provenance", job.candidate.provenance},
    {"duration_ms", job.duration_ms}, {"timeout_ms", job.timeout_ms}};
}
ProbeJob parse_probe_job(const Json& j) {
  if (!j.is_object() || j.value("protocol_version", 0) != 1 || !j.value("consent", false))
    throw std::invalid_argument("Invalid or unconsented probe request");
  ProbeJob job;
  job.attempt_id = j.at("attempt_id").get<std::string>();
  job.target.logical_id = j.at("logical_id").get<std::string>();
  job.target.endpoint_id = j.at("endpoint_id").get<std::string>();
  job.target.direction = j.at("direction").get<std::string>();
  job.candidate = {j.at("format"), j.at("mode"), j.at("provenance")};
  job.duration_ms = number(j, "duration_ms");
  job.timeout_ms = number(j, "timeout_ms");
  validate_deep_limits({1, job.duration_ms, job.timeout_ms, job.timeout_ms, job.candidate.mode});
  validate_probe_format(job.candidate.format);
  if (job.attempt_id.empty() || safe_filename(job.attempt_id) != job.attempt_id ||
      job.target.logical_id.empty() || job.target.endpoint_id.empty() || job.target.endpoint_id.size() > 4096 ||
      (job.target.direction != "capture" && job.target.direction != "render"))
    throw std::invalid_argument("Invalid probe target identity");
  return job;
}
void to_json(Json& j, const DeepReport& r) {
  j = Json{{"result", r.result}, {"logical_id", r.logical_id}, {"attempted", r.attempted},
    {"planned", r.planned},
    {"verified", r.verified}, {"accepted", r.accepted}, {"failed", r.failed}, {"inconclusive", r.inconclusive},
    {"verification_scope", "bounded_buffer_processing_only"}, {"signal_verification", "not_tested"},
    {"diagnostics", r.diagnostics}, {"attempts", r.attempts}};
}
DeepReport run_deep_scan(DeviceStore& store, ProbeRunner& runner, const ProbeTarget& target,
                         const DeepLimits& limits, bool consent, std::stop_token stop, const ProgressSink& progress) {
  DeepReport report; report.logical_id = target.logical_id;
  try {
    validate_deep_limits(limits);
    if (!consent) throw std::invalid_argument("Deep Scan requires explicit disruption consent");
    if (stop.stop_requested()) { report.result = "interrupted"; return report; }
    const auto candidates = plan_deep_candidates(target, limits);
    report.planned = candidates.size();
    if (candidates.empty()) {
      report.diagnostics.push_back({"no_probe_candidates", "No validated PCM candidate is available for this endpoint", target.logical_id, {}});
      return report;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(limits.total_timeout_ms);
    for (const auto& candidate : candidates) {
      if (stop.stop_requested()) { report.result = "interrupted"; return report; }
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (remaining < static_cast<long long>(limits.duration_ms + 1000)) {
        report.result = "partially_completed";
        report.diagnostics.push_back({"budget_exhausted", "Deep Scan total deadline reached", target.logical_id, {}});
        return report;
      }
      ProbeJob job{new_logical_id("probe"), target, candidate, limits.duration_ms,
        static_cast<unsigned>(std::min<long long>(limits.attempt_timeout_ms, remaining))};
      Json attempt = probe_job_json(job);
      attempt.erase("endpoint_id"); // Local IDs stay in the machine-local/debug context.
      attempt.erase("consent");
      attempt["started_utc"] = utc_now(); attempt["classification"] = "inconclusive";
      attempt["status"] = "running"; attempt["signal_verification"] = "not_tested";
      attempt["verification_scope"] = "buffer_processing"; attempt["driver_context"] = target.record.at("driver");
      attempt["stages"] = Json::array();
      store.checkpoint_probe(target.logical_id, attempt, false);
      ++report.attempted;
      Json outcome;
      try {
        outcome = runner.execute(job, stop, [&](const Json& stage) {
          attempt["stages"].push_back(stage);
          store.checkpoint_probe(target.logical_id, attempt, false);
          if (progress) progress({"deep_stage", target.endpoint_id, report.attempted - 1, candidates.size(), stage});
        });
      } catch (const std::exception& e) {
        outcome = {{"classification", "inconclusive"}, {"cleanup", "unknown"}, {"reason", e.what()}};
      }
      std::string result;
      try { result = classification(outcome, limits.duration_ms); }
      catch (const std::exception& e) {
        outcome = {{"classification", "inconclusive"}, {"cleanup", "unknown"}, {"reason", e.what()}};
        result = "inconclusive";
      }
      for (auto it = outcome.begin(); it != outcome.end(); ++it) {
        if (it.key() == "classification" || it.key() == "cleanup" || it.key() == "reason" || it.key() == "stages" ||
            it.key() == "verification_scope" || it.key() == "signal_verification" || it.key() == "windows_context" ||
            it.key() == "runtime_duration_ms" || it.key() == "serviced_frames" || it.key() == "audio_events" ||
            it.key() == "initialization" || it.key() == "failed_api" || it.key() == "hresult" ||
            it.key() == "actual_buffer_frames" || it.key() == "api_reported_latency_100ns" || it.key() == "support" ||
            it.key() == "discontinuities" || it.key() == "startup_discontinuities" || it.key() == "timestamp_errors" ||
            it.key() == "requested_period_frames" || it.key() == "requested_period_100ns" || it.key() == "canceled" ||
            it.key() == "process_exit_observed" || it.key() == "worker_exit_code")
          attempt[it.key()] = it.value();
      }
      attempt["status"] = stop.stop_requested() ? "interrupted" : "completed";
      attempt["signal_verification"] = "not_tested";
      attempt["verification_scope"] = "buffer_processing";
      attempt["finished_utc"] = utc_now();
      store.checkpoint_probe(target.logical_id, attempt, true);
      auto summary = attempt;
      summary["stage_count"] = summary["stages"].size();
      summary.erase("stages");
      summary["history_file"] = "probe_history/" + job.attempt_id + ".json";
      report.attempts.push_back(std::move(summary));
      if (result == "verified") ++report.verified;
      else if (result == "accepted") ++report.accepted;
      else if (result == "failed") ++report.failed;
      else ++report.inconclusive;
      if (progress) progress({"deep_attempt_finished", target.endpoint_id, report.attempted, candidates.size(), attempt});
      if (outcome.value("cleanup", "unknown") != "completed") {
        report.result = stop.stop_requested() ? "interrupted" : "partially_completed";
        report.diagnostics.push_back({"cleanup_uncertain", "Further attempts stopped because worker cleanup is uncertain", target.logical_id, {}});
        return report;
      }
    }
    report.result = stop.stop_requested() ? "interrupted" : report.inconclusive ? "partially_completed" : "completed";
  } catch (const std::exception& e) {
    report.result = report.attempted ? "partially_completed" : "failed";
    report.diagnostics.push_back({"deep_scan_failed", e.what(), target.logical_id, {}});
  }
  return report;
}

void to_json(Json& j, const DeepBatchReport& r) {
  j = {{"result", r.result}, {"scope", "all_active_endpoints"}, {"devices_selected", r.devices.size()},
    {"devices_processed", 0}, {"devices_completed", 0}, {"devices_not_tested", 0},
    {"planned", 0}, {"attempted", 0}, {"verified", 0}, {"accepted", 0}, {"failed", 0}, {"inconclusive", 0},
    {"verification_scope", "bounded_buffer_processing_only"}, {"signal_verification", "not_tested"},
    {"devices", r.devices}, {"diagnostics", r.diagnostics}};
  for (const auto& device : r.devices) {
    if (device.value("result", "") == "not_tested") j["devices_not_tested"] = j["devices_not_tested"].get<std::size_t>() + 1;
    else if (device.value("result", "") != "running") j["devices_processed"] = j["devices_processed"].get<std::size_t>() + 1;
    if (device.value("result", "") == "completed") j["devices_completed"] = j["devices_completed"].get<std::size_t>() + 1;
    for (const auto* key : {"planned", "attempted", "verified", "accepted", "failed", "inconclusive"})
      j[key] = j[key].get<std::size_t>() + device.value(key, std::size_t{});
  }
}

DeepBatchReport run_deep_scan_batch(DeviceStore& store, ProbeRunner& runner,
    const std::vector<ProbeTarget>& targets, const DeepLimits& limits, unsigned system_timeout_ms,
    bool consent, std::stop_token stop, const ProgressSink& progress, const DeepBatchSink& checkpoint) {
  DeepBatchReport batch;
  try {
    validate_deep_limits(limits);
    if (!consent) throw std::invalid_argument("Full-system Deep Scan requires explicit disruption consent");
    if (system_timeout_ms < 1000 || system_timeout_ms > 7200000)
      throw std::invalid_argument("System timeout must be 1000..7200000 ms");
    std::set<std::string> ids, endpoints;
    for (const auto& target : targets) {
      if (target.logical_id.empty() || target.endpoint_id.empty() || !ids.insert(target.logical_id).second ||
          !endpoints.insert(target.endpoint_id).second) throw std::invalid_argument("Duplicate or invalid batch target");
      batch.devices.push_back(Json{{"logical_id", target.logical_id}, {"display_name", target.display_name},
        {"direction", target.direction}, {"result", "not_tested"}, {"reason", "Not reached in this run"},
        {"planned", plan_deep_candidates(target, limits).size()}});
    }
    if (targets.empty()) throw std::runtime_error("No active, uniquely bound endpoints are available for Deep Scan");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(system_timeout_ms);
    if (checkpoint) checkpoint(batch);
    bool incomplete = false;
    for (std::size_t i = 0; i < targets.size(); ++i) {
      if (stop.stop_requested()) { batch.result = "interrupted"; break; }
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (remaining < static_cast<long long>(limits.duration_ms + 1000)) {
        batch.result = "partially_completed";
        batch.diagnostics.push_back({"system_budget_exhausted", "Full-system Deep Scan deadline reached; remaining devices were not tested", {}, {}});
        break;
      }
      auto& entry = batch.devices[i];
      entry["result"] = "running"; entry.erase("reason");
      if (checkpoint) checkpoint(batch);
      if (progress) progress({"deep_device_started", targets[i].endpoint_id, i, targets.size(), entry});
      auto device_limits = limits;
      device_limits.total_timeout_ms = static_cast<unsigned>(std::min<long long>(limits.total_timeout_ms, remaining));
      const auto device = run_deep_scan(store, runner, targets[i], device_limits, true, stop, progress);
      const auto planned = entry["planned"];
      entry = Json(device);
      entry["planned"] = planned;
      entry["display_name"] = targets[i].display_name; entry["direction"] = targets[i].direction;
      incomplete = incomplete || device.result != "completed";
      if (progress) progress({"deep_device_finished", targets[i].endpoint_id, i + 1, targets.size(), entry});
      const bool unsafe = std::any_of(device.diagnostics.begin(), device.diagnostics.end(), [](const auto& d) {
        return d.code == "cleanup_uncertain" || d.code == "deep_scan_failed";
      });
      if (device.result == "interrupted" || stop.stop_requested()) batch.result = "interrupted";
      else if (unsafe) {
        batch.result = "partially_completed";
        batch.diagnostics.push_back({"batch_stopped_after_device_error", "Further devices were not tested after a worker cleanup or probe error", targets[i].logical_id, {}});
      }
      if (checkpoint) checkpoint(batch);
      if (batch.result != "running") break;
    }
    if (batch.result == "running") batch.result = incomplete ? "partially_completed" : "completed";
  } catch (const std::exception& e) {
    batch.result = "failed";
    batch.diagnostics.push_back({"deep_batch_failed", e.what(), {}, {}});
  }
  return batch;
}
}
