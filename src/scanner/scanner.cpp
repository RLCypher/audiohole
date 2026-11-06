#include "audiohole/scanner.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace audiohole::scanner {
Json status(const std::string& value) { return Json{{"status", value}}; }

std::string path_utf8(const fs::path& path) {
  const auto utf8 = path.u8string();
  return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
fs::path utf8_path(const std::string& text) {
  return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string utc_now() {
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &now);
#else
  gmtime_r(&now, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string new_logical_id(const std::string& direction) {
  std::random_device random;
  std::ostringstream out;
  out << direction << '-' << std::hex << std::setfill('0');
  for (int i = 0; i < 16; ++i) out << std::setw(2) << (random() & 255);
  return out.str();
}

std::string safe_filename(const std::string& name) {
  std::string result = name;
  for (char& c : result) {
    if (static_cast<unsigned char>(c) < 32 || std::string("<>:\"/\\|?*").find(c) != std::string::npos)
      c = '_';
  }
  while (!result.empty() && (result.back() == '.' || result.back() == ' ')) result.pop_back();
  if (result.empty()) result = "Audio Device";
  std::string base = result.substr(0, result.find('.'));
  std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) { return std::toupper(c); });
  const bool reserved = base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" ||
    base == "CONIN$" || base == "CONOUT$" ||
    (base.size() == 4 && (base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") &&
     base[3] >= '1' && base[3] <= '9');
  if (reserved) result.insert(result.begin(), '_');
  // Leave room for disambiguating suffixes; never split a UTF-8 sequence.
  if (result.size() > 180) {
    std::size_t end = 180;
    while (end > 0 && (static_cast<unsigned char>(result[end]) & 0xc0) == 0x80) --end;
    result.resize(end);
  }
  while (!result.empty() && (result.back() == '.' || result.back() == ' ')) result.pop_back();
  if (result.empty()) result = "Audio Device";
  return result;
}

void to_json(Json& j, const Diagnostic& d) {
  j = Json{{"code", d.code}, {"message", d.message}, {"context", d.context}};
  if (!d.native_code.empty()) j["native_code"] = d.native_code;
  if (!d.endpoint_state.empty()) j["endpoint_state"] = d.endpoint_state;
  if (!d.api.empty()) j["api"] = d.api;
}
Diagnostic endpoint_lookup_failure(const Endpoint& endpoint, const std::string& api,
    const std::string& native_code, const std::string& message, bool missing_name) {
  const auto state = endpoint.state.empty() ? "unknown" : endpoint.state;
  const bool name_api = api == "IPropertyStore::GetValue(PKEY_Device_FriendlyName)";
  const bool metadata_api = api == "IMMDevice::OpenPropertyStore" ||
    name_api;
  // HRESULT_FROM_WIN32(ERROR_NOT_FOUND / ERROR_DEVICE_NOT_CONNECTED), then
  // AUDCLNT_E_DEVICE_INVALIDATED. Access denied and arbitrary errors are excluded.
  const bool missing = native_code == "0x80070490" || native_code == "0x8007048F" ||
    native_code == "0x88890004" ||
    (missing_name && native_code == "0x00000000" && name_api);
  const bool absent = state == "not_present" && !endpoint.endpoint_id.empty() &&
    (endpoint.direction == "capture" || endpoint.direction == "render") && metadata_api && missing;
  return {absent ? "absent_endpoint_metadata_unavailable" : "endpoint_query_failed",
    api + " | state: " + state + " | " + message,
    endpoint.endpoint_id, native_code, state, api};
}
void to_json(Json& j, const ScanReport& r) {
  j = Json{{"result", r.result}, {"discovered", r.discovered}, {"saved", r.saved},
           {"historical_skipped", r.historical_skipped}, {"diagnostics", r.diagnostics}};
}

BasicScanner::BasicScanner(EndpointProvider& provider, DeviceStore& store)
    : provider_(provider), store_(store) {}

ScanReport BasicScanner::scan(std::stop_token stop, const ProgressSink& progress) {
  if (stop.stop_requested()) return {"interrupted", 0, 0, {}};
  Discovery discovery;
  try {
    discovery = provider_.discover(stop, progress);
  } catch (const std::exception& e) {
    return {"failed", 0, 0, {{"discovery_failed", e.what(), {}, {}}}};
  }
  if (stop.stop_requested() || discovery.cancelled)
    return {"interrupted", discovery.endpoints.size(), 0, discovery.diagnostics};
  if (progress) progress({"reconciling", {}, 0, discovery.endpoints.size()});
  // Once persistence begins cancellation does not interrupt a file replacement.
  // Driver and filesystem calls are not advertised as forcibly cancelable.
  auto report = store_.reconcile(discovery, utc_now());
  if (progress) progress({"finished", {}, report.saved, discovery.endpoints.size()});
  return report;
}
}  // namespace audiohole::scanner
