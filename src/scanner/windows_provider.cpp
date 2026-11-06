#include "audiohole/scanner.hpp"

#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <propvarutil.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>

namespace audiohole::scanner {
namespace {
template<class T> class ComPtr {
 public:
  ~ComPtr() { if (value_) value_->Release(); }
  ComPtr() = default;
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  T* get() const { return value_; }
  T* operator->() const { return value_; }
  T** put() { if (value_) value_->Release(); value_ = nullptr; return &value_; }
 private:
  T* value_ = nullptr;
};
struct Prop {
  PROPVARIANT value;
  Prop() { PropVariantInit(&value); }
  ~Prop() { PropVariantClear(&value); }
};
struct TaskMemory {
  void* pointer = nullptr;
  ~TaskMemory() { CoTaskMemFree(pointer); }
};
struct ComApartment {
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  ~ComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};

std::string utf8(const wchar_t* value) {
  if (!value) return {};
  const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 0) throw std::runtime_error("Windows returned an invalid Unicode string");
  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, result.data(), size, nullptr, nullptr);
  result.pop_back();
  return result;
}
std::string hex_code(HRESULT hr) {
  std::ostringstream out;
  out << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
      << static_cast<std::uint32_t>(hr);
  return out.str();
}
std::string guid_text(const GUID& guid) {
  wchar_t buffer[40]{};
  StringFromGUID2(guid, buffer, 40);
  return utf8(buffer);
}
void error(Endpoint& endpoint, const std::string& operation, HRESULT hr) {
  wchar_t* message = nullptr;
  const auto size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(hr), 0,
    reinterpret_cast<LPWSTR>(&message), 0, nullptr);
  std::string description;
  if (size && message) description = utf8(message);
  if (message) LocalFree(message);
  while (!description.empty() && (description.back() == '\r' || description.back() == '\n')) description.pop_back();
  endpoint.diagnostics.push_back({"windows_query_failed", operation + (description.empty() ? "" : ": " + description),
                                  endpoint.endpoint_id, hex_code(hr), endpoint.state, operation});
}
std::string property_string(IPropertyStore* store, const PROPERTYKEY& key) {
  Prop prop;
  if (FAILED(store->GetValue(key, &prop.value)) || prop.value.vt != VT_LPWSTR) return {};
  return utf8(prop.value.pwszVal);
}
Json property_guid(IPropertyStore* store, const PROPERTYKEY& key) {
  Prop prop;
  if (FAILED(store->GetValue(key, &prop.value)) || prop.value.vt != VT_CLSID || !prop.value.puuid)
    return status("unavailable");
  return guid_text(*prop.value.puuid);
}

constexpr GUID pcm_guid{1, 0, 0x0010, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
constexpr GUID float_guid{3, 0, 0x0010, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
constexpr PROPERTYKEY stable_id_key{{0x1da5d803, 0xd492, 0x4edd,
  {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 12};

Json format_json(const WAVEFORMATEX* format, std::size_t size) {
  if (!format || size < sizeof(WAVEFORMATEX)) throw std::runtime_error("Truncated WAVEFORMATEX");
  WAVEFORMATEX wave{};
  std::memcpy(&wave, format, sizeof(wave));
  if (wave.cbSize > 4096 || sizeof(WAVEFORMATEX) + wave.cbSize > size)
    throw std::runtime_error("Invalid WAVEFORMATEX extension size");
  if (!wave.nSamplesPerSec || !wave.nChannels || !wave.nBlockAlign)
    throw std::runtime_error("Invalid audio format dimensions");
  Json result{{"sample_rate_hz", wave.nSamplesPerSec}, {"container_bits", wave.wBitsPerSample},
              {"valid_bits", wave.wBitsPerSample}, {"channel_count", wave.nChannels},
              {"channel_mask", status("unknown")}, {"sample_format", status("unknown")},
              {"format_tag", wave.wFormatTag}, {"block_align_bytes", wave.nBlockAlign},
              {"average_bytes_per_second", wave.nAvgBytesPerSec}};
  bool pcm = wave.wFormatTag == WAVE_FORMAT_PCM;
  bool floating = wave.wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
  unsigned bits = wave.wBitsPerSample;
  if (wave.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    if (size < sizeof(WAVEFORMATEXTENSIBLE) || wave.cbSize < 22)
      throw std::runtime_error("Truncated WAVEFORMATEXTENSIBLE");
    WAVEFORMATEXTENSIBLE ext{};
    std::memcpy(&ext, format, sizeof(ext));
    bits = ext.Samples.wValidBitsPerSample;
    if (bits > wave.wBitsPerSample) throw std::runtime_error("Valid bits exceed container width");
    result["valid_bits"] = bits;
    result["channel_mask"] = ext.dwChannelMask ? Json(ext.dwChannelMask) : status("unknown");
    result["subformat_guid"] = guid_text(ext.SubFormat);
    pcm = IsEqualGUID(ext.SubFormat, pcm_guid);
    floating = IsEqualGUID(ext.SubFormat, float_guid);
  }
  if (pcm) {
    if (bits == 16 && wave.wBitsPerSample == 16) result["sample_format"] = "pcm_s16";
    else if (bits == 24 && wave.wBitsPerSample == 24) result["sample_format"] = "pcm_s24_packed";
    else if (bits == 24 && wave.wBitsPerSample == 32) result["sample_format"] = "pcm_s24_in_32";
    else if (bits == 32 && wave.wBitsPerSample == 32) result["sample_format"] = "pcm_s32";
  } else if (floating && wave.wBitsPerSample == 32) result["sample_format"] = "float32";
  return result;
}

Json property_format(IPropertyStore* store, const PROPERTYKEY& key, Endpoint& endpoint) {
  Prop prop;
  auto hr = store->GetValue(key, &prop.value);
  if (FAILED(hr)) { error(endpoint, "Read configured endpoint format", hr); return status("unavailable"); }
  if (prop.value.vt != VT_BLOB || !prop.value.blob.pBlobData) return status("unavailable");
  try {
    if (prop.value.blob.cbSize > sizeof(WAVEFORMATEX) + 4096)
      throw std::runtime_error("Endpoint format blob exceeds the size limit");
    // Property blobs need not be aligned for WAVEFORMATEX; copy before accessing.
    std::vector<unsigned char> bytes(prop.value.blob.pBlobData,
                                     prop.value.blob.pBlobData + prop.value.blob.cbSize);
    return format_json(reinterpret_cast<const WAVEFORMATEX*>(bytes.data()), bytes.size());
  } catch (const std::exception& e) {
    endpoint.diagnostics.push_back({"invalid_format_blob", e.what(), endpoint.endpoint_id, {}});
    return status("failed");
  }
}

std::vector<unsigned char> dev_property(DEVINST node, const DEVPROPKEY& key, DEVPROPTYPE expected) {
  DEVPROPTYPE type = 0;
  ULONG size = 0;
  auto result = CM_Get_DevNode_PropertyW(node, &key, &type, nullptr, &size, 0);
  if (result != CR_BUFFER_SMALL || size > 1024 * 1024 || !size) return {};
  std::vector<unsigned char> bytes(size + sizeof(wchar_t) * 2, 0);
  result = CM_Get_DevNode_PropertyW(node, &key, &type, bytes.data(), &size, 0);
  if (result != CR_SUCCESS || type != expected) return {};
  return bytes;
}
std::string dev_string(DEVINST node, const DEVPROPKEY& key) {
  const auto bytes = dev_property(node, key, DEVPROP_TYPE_STRING);
  return bytes.empty() ? std::string{} : utf8(reinterpret_cast<const wchar_t*>(bytes.data()));
}
Json string_or_unavailable(const std::string& value) { return value.empty() ? status("unavailable") : Json(value); }

void hardware_properties(IMMDevice* device, IMMDeviceEnumerator* enumerator,
                         const std::string& endpoint_instance, Endpoint& endpoint) {
  std::string instance = endpoint_instance;
  ComPtr<IDeviceTopology> topology;
  if (SUCCEEDED(device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(topology.put())))) {
    ComPtr<IConnector> connector;
    TaskMemory adapter_id;
    if (SUCCEEDED(topology->GetConnector(0, connector.put())) &&
        SUCCEEDED(connector->GetDeviceIdConnectedTo(reinterpret_cast<LPWSTR*>(&adapter_id.pointer)))) {
      ComPtr<IMMDevice> adapter;
      ComPtr<IPropertyStore> properties;
      if (SUCCEEDED(enumerator->GetDevice(static_cast<LPCWSTR>(adapter_id.pointer), adapter.put())) &&
          SUCCEEDED(adapter->OpenPropertyStore(STGM_READ, properties.put()))) {
        const auto mapped = property_string(properties.get(), PKEY_Device_InstanceId);
        if (!mapped.empty()) instance = mapped;
      }
    }
  }
  endpoint.local_identity["device_instance_id"] = string_or_unavailable(instance);
  if (instance.empty()) return;
  const auto wide = utf8_path(instance).wstring();
  DEVINST node = 0;
  if (CM_Locate_DevNodeW(&node, const_cast<DEVINSTID_W>(wide.c_str()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
    return;

  // Walk a bounded ancestor chain to find the hardware devnode. Keep the source
  // instance so software-endpoint and hardware-driver metadata are not conflated.
  for (int depth = 0; depth < 16; ++depth) {
    const auto ids = dev_property(node, DEVPKEY_Device_HardwareIds, DEVPROP_TYPE_STRING_LIST);
    if (!ids.empty()) {
      Json hardware_ids = Json::array();
      const auto* p = reinterpret_cast<const wchar_t*>(ids.data());
      while (*p) {
        hardware_ids.push_back(utf8(p));
        p += wcslen(p) + 1;
      }
      endpoint.fingerprint["hardware_ids"] = hardware_ids;
      endpoint.fingerprint["manufacturer"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_Manufacturer));
      endpoint.fingerprint["product"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_DeviceDesc));
      std::array<wchar_t, MAX_DEVICE_ID_LEN> id{};
      if (CM_Get_Device_IDW(node, id.data(), static_cast<ULONG>(id.size()), 0) == CR_SUCCESS)
        endpoint.local_identity["driver_device_instance_id"] = utf8(id.data());
      endpoint.driver["provider"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_DriverProvider));
      endpoint.driver["version"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_DriverVersion));
      endpoint.driver["inf_name"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_DriverInfPath));
      endpoint.driver["service"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_Service));
      endpoint.driver["name"] = string_or_unavailable(dev_string(node, DEVPKEY_Device_DriverDesc));
      const auto date = dev_property(node, DEVPKEY_Device_DriverDate, DEVPROP_TYPE_FILETIME);
      if (date.size() >= sizeof(FILETIME)) {
        FILETIME filetime{}; SYSTEMTIME systemtime{};
        std::memcpy(&filetime, date.data(), sizeof(filetime));
        if (FileTimeToSystemTime(&filetime, &systemtime)) {
          std::ostringstream formatted;
          formatted << std::setfill('0') << std::setw(4) << systemtime.wYear << '-'
                    << std::setw(2) << systemtime.wMonth << '-' << std::setw(2) << systemtime.wDay;
          endpoint.driver["date"] = formatted.str();
        }
      }
      endpoint.driver["association_status"] = "available";
      // Parse documented USB hardware-ID components, never the opaque MMDevice ID.
      for (const auto& item : hardware_ids) {
        auto value = item.get<std::string>();
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::toupper(c); });
        for (const auto& pair : {std::pair{"VID_", "vendor_id"}, {"PID_", "product_id"}, {"MI_", "interface_number"}}) {
          const auto pos = value.find(pair.first);
          const std::size_t width = std::string(pair.first) == "MI_" ? 2 : 4;
          if (pos != std::string::npos && pos + std::strlen(pair.first) + width <= value.size()) {
            const auto part = value.substr(pos + std::strlen(pair.first), width);
            if (std::all_of(part.begin(), part.end(), [](unsigned char c) { return std::isxdigit(c); }))
              endpoint.fingerprint["usb"][pair.second] = part;
          }
        }
      }
      // PnP instance-ID suffixes may be port-derived. Do not call them serial numbers.
      return;
    }
    DEVINST parent = 0;
    if (CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) break;
    node = parent;
  }
}

void capabilities(IMMDevice* device, Endpoint& endpoint, std::stop_token stop, const ProgressSink& progress) {
  auto stage = [&](const std::string& api) {
    if (progress) progress({"api_begin", endpoint.endpoint_id, 0, 0, {{"api", api}}});
  };
  auto& caps = endpoint.reported_capabilities;
  ComPtr<IAudioClient> client;
  stage("Activate IAudioClient");
  auto hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.put()));
  if (FAILED(hr)) { error(endpoint, "Activate IAudioClient for read-only queries", hr); return; }
  TaskMemory mix;
  stage("GetMixFormat");
  hr = client->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&mix.pointer));
  if (FAILED(hr)) { error(endpoint, "GetMixFormat", hr); return; }
  const auto* wave = static_cast<WAVEFORMATEX*>(mix.pointer);
  if (!wave || wave->cbSize > 4096) throw std::runtime_error("Invalid mix format size");
  caps["default_mix_format"] = format_json(wave, sizeof(WAVEFORMATEX) + wave->cbSize);
  caps["shared_mode"] = "available"; // query availability, not tested streaming

  REFERENCE_TIME default_period = 0, minimum_period = 0;
  stage("GetDevicePeriod");
  hr = client->GetDevicePeriod(&default_period, &minimum_period);
  if (SUCCEEDED(hr)) caps["device_periods"] = Json{{"default_shared_period_100ns", default_period},
                                                 {"minimum_exclusive_period_100ns", minimum_period}};
  else error(endpoint, "GetDevicePeriod", hr);

  ComPtr<IAudioClient3> client3;
  if (SUCCEEDED(client->QueryInterface(__uuidof(IAudioClient3), reinterpret_cast<void**>(client3.put())))) {
    UINT32 def = 0, fundamental = 0, minimum = 0, maximum = 0;
    stage("GetSharedModeEnginePeriod");
    hr = client3->GetSharedModeEnginePeriod(wave, &def, &fundamental, &minimum, &maximum);
    if (SUCCEEDED(hr)) caps["shared_engine_periods"] = Json{{"format", caps["default_mix_format"]},
      {"default_frames", def}, {"fundamental_frames", fundamental}, {"minimum_frames", minimum}, {"maximum_frames", maximum}};
    else error(endpoint, "GetSharedModeEnginePeriod", hr);
    TaskMemory current;
    UINT32 period = 0;
    stage("GetCurrentSharedModeEnginePeriod");
    hr = client3->GetCurrentSharedModeEnginePeriod(reinterpret_cast<WAVEFORMATEX**>(&current.pointer), &period);
    if (SUCCEEDED(hr)) {
      const auto* current_wave = static_cast<WAVEFORMATEX*>(current.pointer);
      if (current_wave && current_wave->cbSize <= 4096)
        caps["current_shared_engine"] = Json{{"format", format_json(current_wave, sizeof(WAVEFORMATEX) + current_wave->cbSize)},
                                             {"period_frames", period}};
    } else error(endpoint, "GetCurrentSharedModeEnginePeriod", hr);
  }

  // Bounded query-only coverage: these are candidates, never advertised lists.
  // At most 32 API calls. No Initialize, Start, buffer allocation, or test audio.
  Json queries = Json::array();
  std::set<std::string> requested;
  auto query = [&](const WAVEFORMATEX* candidate, std::size_t size) {
    const auto description = format_json(candidate, size);
    for (const auto mode : {AUDCLNT_SHAREMODE_SHARED, AUDCLNT_SHAREMODE_EXCLUSIVE}) {
      if (stop.stop_requested() || queries.size() >= 32) return;
      const auto key = description.dump() + std::to_string(mode);
      if (!requested.insert(key).second) continue;
      TaskMemory closest;
      if (progress) progress({"format_query_begin", endpoint.endpoint_id, queries.size(), 32,
        {{"api", "IsFormatSupported"}, {"mode", mode == AUDCLNT_SHAREMODE_SHARED ? "shared" : "exclusive"},
         {"configuration", description}}});
      const auto code = client->IsFormatSupported(mode, candidate,
        mode == AUDCLNT_SHAREMODE_SHARED ? reinterpret_cast<WAVEFORMATEX**>(&closest.pointer) : nullptr);
      Json result{{"configuration", description}, {"mode", mode == AUDCLNT_SHAREMODE_SHARED ? "shared" : "exclusive"},
                  {"stage", "capability_query"}, {"evidence", "reported"},
                  {"support", code == S_OK ? "supported" : code == S_FALSE || code == AUDCLNT_E_UNSUPPORTED_FORMAT ? "unsupported" : "unknown"},
                  {"hresult", hex_code(code)}, {"stream_verification", "not_tested"}};
      if (code == S_FALSE && closest.pointer) {
        const auto* match = static_cast<WAVEFORMATEX*>(closest.pointer);
        result["closest_match"] = format_json(match, sizeof(WAVEFORMATEX) + match->cbSize);
      }
      queries.push_back(std::move(result));
      if (progress) progress({"format_query_end", endpoint.endpoint_id, queries.size(), 32, queries.back()});
    }
  };
  query(wave, sizeof(WAVEFORMATEX) + wave->cbSize);
  const auto channels = wave->nChannels;
  const auto& mix_json = caps["default_mix_format"];
  DWORD mask = mix_json["channel_mask"].is_number_unsigned() ? mix_json["channel_mask"].get<DWORD>() : 0;
  if (channels <= 32 && wave->nSamplesPerSec <= 768000)
  for (const auto rate : {wave->nSamplesPerSec, DWORD(44100), DWORD(48000), DWORD(96000)}) {
    // Representation, container width, and valid bits remain independent.
    for (const auto representation : {0, 1, 2, 3, 4}) {
      const WORD bits = representation == 0 ? 16 : representation == 1 ? 24 : 32;
      WAVEFORMATEXTENSIBLE candidate{};
      candidate.Format = {WAVE_FORMAT_EXTENSIBLE, channels, rate,
        static_cast<DWORD>(rate * channels * (bits / 8)), static_cast<WORD>(channels * (bits / 8)), bits, 22};
      candidate.Samples.wValidBitsPerSample = representation == 2 ? 24 : bits;
      candidate.dwChannelMask = mask;
      candidate.SubFormat = representation == 4 ? float_guid : pcm_guid;
      query(&candidate.Format, sizeof(candidate));
      if (channels <= 2 && representation != 2) {
        candidate.Format.wFormatTag = representation == 4 ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
        candidate.Format.cbSize = 0;
        query(&candidate.Format, sizeof(WAVEFORMATEX));
      }
    }
  }
  caps["format_queries"] = std::move(queries);
  caps["format_query_coverage"] = "bounded_candidates_not_exhaustive";
}

class WindowsProvider final : public EndpointProvider {
 public:
  Discovery discover(std::stop_token stop, const ProgressSink& progress, const EndpointFilter& filter = {}) override {
    // Identity must be resolved before touching any format/capability interfaces.
    Discovery result;
    result.full_inventory = !filter;
    ComApartment apartment;
    // A GUI may call from an existing STA. All COM objects stay on this call's thread.
    if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE)
      throw std::runtime_error("COM initialization failed: " + hex_code(apartment.result));
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (progress) progress({"api_begin", {}, 0, 0, {{"api", "Create MMDeviceEnumerator"}}});
    auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.put()));
    if (FAILED(hr)) throw std::runtime_error("MMDeviceEnumerator creation failed: " + hex_code(hr));
    ComPtr<IMMDeviceCollection> collection;
    if (progress) progress({"api_begin", {}, 0, 0, {{"api", "EnumAudioEndpoints (all states)"}}});
    hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATEMASK_ALL, collection.put());
    if (FAILED(hr)) throw std::runtime_error("Endpoint enumeration failed: " + hex_code(hr));
    UINT count = 0;
    hr = collection->GetCount(&count);
    if (FAILED(hr)) throw std::runtime_error("Endpoint count failed: " + hex_code(hr));
    result.complete = true;
    for (UINT i = 0; i < count; ++i) {
      if (stop.stop_requested()) { result.complete = false; result.cancelled = true; break; }
      Endpoint endpoint;
      std::string operation = "IMMDeviceCollection::Item";
      bool missing_name = false;
      bool direct_api = true;
      hr = S_OK;
      try {
        ComPtr<IMMDevice> device;
        hr = collection->Item(i, device.put());
        if (FAILED(hr)) throw std::runtime_error("Read endpoint failed: " + hex_code(hr));
        TaskMemory id;
        operation = "IMMDevice::GetId";
        hr = device->GetId(reinterpret_cast<LPWSTR*>(&id.pointer));
        if (FAILED(hr)) throw std::runtime_error("GetId failed: " + hex_code(hr));
        endpoint.endpoint_id = utf8(static_cast<LPCWSTR>(id.pointer));
        ComPtr<IMMEndpoint> direction;
        operation = "IMMDevice::QueryInterface(IMMEndpoint)";
        hr = device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void**>(direction.put()));
        if (FAILED(hr)) throw std::runtime_error("Read endpoint direction failed: " + hex_code(hr));
        EDataFlow flow = eAll;
        operation = "IMMEndpoint::GetDataFlow";
        hr = direction->GetDataFlow(&flow);
        if (FAILED(hr) || (flow != eCapture && flow != eRender)) throw std::runtime_error("Invalid endpoint direction");
        endpoint.direction = flow == eCapture ? "capture" : "render";
        DWORD state = 0;
        operation = "IMMDevice::GetState";
        hr = device->GetState(&state);
        endpoint.state = SUCCEEDED(hr) ? state == DEVICE_STATE_ACTIVE ? "active" : state == DEVICE_STATE_DISABLED
          ? "disabled" : state == DEVICE_STATE_NOTPRESENT ? "not_present" : state == DEVICE_STATE_UNPLUGGED
          ? "unplugged" : "unknown" : "unknown";
        if (FAILED(hr)) error(endpoint, "GetState", hr);
        ComPtr<IPropertyStore> properties;
        operation = "IMMDevice::OpenPropertyStore";
        hr = device->OpenPropertyStore(STGM_READ, properties.put());
        if (FAILED(hr)) throw std::runtime_error("OpenPropertyStore failed: " + hex_code(hr));
        operation = "IPropertyStore::GetValue(PKEY_Device_FriendlyName)";
        Prop friendly_name;
        hr = properties->GetValue(PKEY_Device_FriendlyName, &friendly_name.value);
        if (FAILED(hr)) throw std::runtime_error("Windows endpoint name query failed");
        if (friendly_name.value.vt == VT_LPWSTR)
          endpoint.device_name = utf8(friendly_name.value.pwszVal);
        if (endpoint.device_name.empty()) {
          missing_name = friendly_name.value.vt == VT_EMPTY || friendly_name.value.vt == VT_NULL ||
            friendly_name.value.vt == VT_LPWSTR;
          throw std::runtime_error("Windows endpoint name unavailable; property type " +
            std::to_string(friendly_name.value.vt));
        }
        if (progress) progress({"querying", endpoint.endpoint_id, i, count});
        operation = "Read endpoint identity properties";
        direct_api = false;
        endpoint.local_identity["container_id"] = property_guid(properties.get(), PKEY_Device_ContainerId);
        // StableId is optional (Windows 11 24H2+). Use its published key so
        // Windows 10 builds do not require the newest SDK's symbolic constant.
        endpoint.local_identity["stable_id"] = string_or_unavailable(property_string(properties.get(), stable_id_key));
        endpoint.fingerprint = Json{{"manufacturer", status("unavailable")}, {"product", status("unavailable")},
          {"usb", {{"vendor_id", status("unavailable")}, {"product_id", status("unavailable")},
                    {"serial_number", status("unavailable")}, {"interface_number", status("unavailable")}}},
          {"endpoint", {{"interface_name", string_or_unavailable(property_string(properties.get(), PKEY_DeviceInterface_FriendlyName))},
                        {"role", status("unavailable")}}}, {"hardware_ids", status("unavailable")}};
        endpoint.driver = Json{{"name", status("unavailable")}, {"provider", status("unavailable")},
          {"version", status("unavailable")}, {"date", status("unavailable")}, {"inf_name", status("unavailable")},
          {"service", status("unavailable")}, {"association_status", "unknown"}};
        endpoint.fingerprint["endpoint"]["role"] =
          string_or_unavailable(property_string(properties.get(), PKEY_AudioEndpoint_Association));
        if (progress) progress({"api_begin", endpoint.endpoint_id, i, count, {{"api", "Read hardware and driver properties"}}});
        operation = "Read hardware and driver properties";
        hardware_properties(device.get(), enumerator.get(), property_string(properties.get(), PKEY_Device_InstanceId), endpoint);
        if (filter && !filter(endpoint)) {
          result.full_inventory = false;
          if (progress) progress({"capabilities_skipped", endpoint.endpoint_id, i + 1, count});
          continue;
        }
        endpoint.reported_capabilities = Json{{"sample_rates_hz", status("unavailable")},
          {"sample_formats", status("unavailable")}, {"channel_counts", status("unavailable")},
          {"shared_mode", "unknown"}, {"exclusive_mode", "unknown"},
          {"default_mix_format", status("unavailable")}, {"device_periods", status("unavailable")},
          {"shared_engine_periods", status("unavailable")}, {"current_shared_engine", status("unavailable")},
          {"format_queries", status("not_tested")}};
        operation = "Read cached audio format properties";
        endpoint.reported_capabilities["configured_device_format"] =
          property_format(properties.get(), PKEY_AudioEngine_DeviceFormat, endpoint);
        endpoint.reported_capabilities["oem_device_format"] =
          property_format(properties.get(), PKEY_AudioEngine_OEMFormat, endpoint);
        Prop event_mode;
        endpoint.reported_capabilities["event_driven_mode"] = status("unavailable");
        if (SUCCEEDED(properties->GetValue(PKEY_AudioEndpoint_Supports_EventDriven_Mode, &event_mode.value)) &&
            event_mode.value.vt == VT_UI4)
          endpoint.reported_capabilities["event_driven_mode"] = event_mode.value.ulVal ? "supported" : "unsupported";
        if (state == DEVICE_STATE_ACTIVE && !stop.stop_requested()) {
          operation = "Read active endpoint capabilities";
          capabilities(device.get(), endpoint, stop, progress);
        }
        result.endpoints.push_back(std::move(endpoint));
      } catch (const std::exception& e) {
        const auto diagnostic = endpoint_lookup_failure(endpoint, operation,
          direct_api ? hex_code(hr) : "", e.what(), missing_name);
        const bool absent = diagnostic.code == "absent_endpoint_metadata_unavailable";
        if (progress) progress({absent ? "absent_endpoint_observation" : "endpoint_lookup_failed",
          endpoint.endpoint_id, i + 1, count, Json(diagnostic)});
        // A positively absent endpoint with no cached name is not a new device.
        // Omitting it from a complete full inventory still marks its retained
        // binding not_present. Other states/errors keep absence inference blocked.
        if (!absent) {
          result.complete = false;
          result.diagnostics.push_back(diagnostic);
        }
      }
      if (progress) progress({"enumerating", {}, i + 1, count});
    }
    return result;
  }
};
}
namespace detail {
Json describe_wave_format(const WAVEFORMATEX* format, std::size_t size) { return format_json(format, size); }
}
std::unique_ptr<EndpointProvider> make_windows_provider() { return std::make_unique<WindowsProvider>(); }
}  // namespace audiohole::scanner
#else
namespace audiohole::scanner {
std::unique_ptr<EndpointProvider> make_windows_provider() {
  throw std::runtime_error("Windows endpoint scanning requires Windows 10/11; no scan was performed");
}
}
#endif
