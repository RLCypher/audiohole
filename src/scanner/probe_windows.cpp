#include "audiohole/deep_scan.hpp"
#include "windows_format.hpp"
#include "storage_internal.hpp"

#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace audiohole::scanner {
namespace {
template<class T> struct Ptr {
  T* p = nullptr;
  ~Ptr() { reset(); }
  void reset() { if (p) { p->Release(); p = nullptr; } }
  T** put() { reset(); return &p; }
  T* operator->() const { return p; }
};
struct AudioEvent {
  HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  ~AudioEvent() { if (handle) CloseHandle(handle); }
};
struct Apartment {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); }
};
std::string code(HRESULT hr) {
  std::ostringstream out;
  out << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << static_cast<std::uint32_t>(hr);
  return out.str();
}
void emit(const Json& value) { std::cout << value.dump() << '\n' << std::flush; }
Json windows_version() {
  using VersionFunction = LONG (WINAPI*)(OSVERSIONINFOW*);
  const auto module = GetModuleHandleW(L"ntdll.dll");
  const auto address = module ? GetProcAddress(module, "RtlGetVersion") : nullptr;
  VersionFunction function = nullptr;
  static_assert(sizeof(function) == sizeof(address));
  std::memcpy(&function, &address, sizeof(function));
  OSVERSIONINFOW info{}; info.dwOSVersionInfoSize = sizeof(info);
  if (function && function(&info) == 0)
    return Json{{"major", info.dwMajorVersion}, {"minor", info.dwMinorVersion}, {"build", info.dwBuildNumber}};
  return status("unavailable");
}

Json probe(const ProbeJob& job, HANDLE cancel) {
  Json outcome{{"classification", "inconclusive"}, {"cleanup", "unknown"},
    {"verification_scope", "buffer_processing"}, {"signal_verification", "not_tested"},
    {"windows_context", windows_version()}};
  auto stage = [&](const std::string& api, const std::string& phase, const Json& extra = Json::object()) {
    Json row{{"api", api}, {"phase", phase}, {"timestamp_utc", utc_now()}};
    for (auto it = extra.begin(); it != extra.end(); ++it) row[it.key()] = it.value();
    emit(Json{{"type", "stage"}, {"attempt_id", job.attempt_id}, {"stage", row}});
  };
  auto checked = [&](const std::string& api, HRESULT hr) {
    stage(api, "end", {{"hresult", code(hr)}});
    if (FAILED(hr)) { outcome["failed_api"] = api; outcome["hresult"] = code(hr); throw hr; }
  };
  // Do not emit/log/allocate on each successful buffer operation. Retain the
  // precise failing API if the bounded processing loop aborts.
  auto runtime_checked = [&](const char* api, HRESULT hr) {
    if (FAILED(hr)) { outcome["failed_api"] = api; outcome["hresult"] = code(hr); throw hr; }
  };
  Apartment apartment;
  AudioEvent audio_event;
  Ptr<IMMDeviceEnumerator> enumerator;
  Ptr<IMMDevice> device;
  Ptr<IMMEndpoint> endpoint;
  Ptr<IAudioClient> client;
  Ptr<IAudioClient3> client3;
  Ptr<IAudioRenderClient> render;
  Ptr<IAudioCaptureClient> capture;
  bool started = false;
  bool runtime_ok = false;
  bool cleanup_ok = true;
  try {
    checked("CoInitializeEx", apartment.hr);
    if (!audio_event.handle) throw std::runtime_error("Cannot create probe audio event");
    stage("ResolveEndpoint", "begin");
    checked("CreateEnumerator", CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.put())));
    const auto id = utf8_path(job.target.endpoint_id).wstring();
    checked("GetDevice", enumerator->GetDevice(id.c_str(), device.put()));
    DWORD state = 0;
    checked("GetState", device->GetState(&state));
    if (state != DEVICE_STATE_ACTIVE) throw std::runtime_error("Selected endpoint is no longer active");
    checked("GetEndpoint", device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void**>(endpoint.put())));
    EDataFlow flow = eAll;
    checked("GetDataFlow", endpoint->GetDataFlow(&flow));
    if ((job.target.direction == "capture" && flow != eCapture) || (job.target.direction == "render" && flow != eRender))
      throw std::runtime_error("Endpoint direction no longer matches the selected logical record");
    stage("ActivateAudioClient", "begin");
    checked("ActivateAudioClient", device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
      reinterpret_cast<void**>(client.put())));
    const auto& f = job.candidate.format;
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = f["format_tag"].get<WORD>();
    format.Format.nChannels = f["channel_count"].get<WORD>();
    format.Format.nSamplesPerSec = f["sample_rate_hz"].get<DWORD>();
    format.Format.wBitsPerSample = f["container_bits"].get<WORD>();
    format.Format.nBlockAlign = f["block_align_bytes"].get<WORD>();
    format.Format.nAvgBytesPerSec = f["average_bytes_per_second"].get<DWORD>();
    if (format.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
      format.Format.cbSize = 22;
      format.Samples.wValidBitsPerSample = f["valid_bits"].get<WORD>();
      format.dwChannelMask = f["channel_mask"].is_number_integer() ? f["channel_mask"].get<DWORD>() : 0;
      const auto guid = utf8_path(f["subformat_guid"].get<std::string>()).wstring();
      checked("ParseSubformat", CLSIDFromString(guid.c_str(), &format.SubFormat));
    }
    const auto mode = job.candidate.mode == "exclusive" ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
    WAVEFORMATEX* closest = nullptr;
    stage("IsFormatSupported", "begin", {{"mode", job.candidate.mode}, {"configuration", f}});
    const auto supported = client->IsFormatSupported(mode, &format.Format, mode == AUDCLNT_SHAREMODE_SHARED ? &closest : nullptr);
    Json query{{"hresult", code(supported)}, {"requested_format_supported", supported == S_OK}};
    if (closest) {
      try { query["closest_match"] = detail::describe_wave_format(closest, sizeof(WAVEFORMATEX) + closest->cbSize); }
      catch (...) { query["closest_match"] = status("failed"); }
      CoTaskMemFree(closest);
    }
    stage("IsFormatSupported", "end", query);
    if (supported == S_FALSE || supported == AUDCLNT_E_UNSUPPORTED_FORMAT) {
      outcome["classification"] = "failed"; outcome["support"] = "unsupported";
      outcome["failed_api"] = "IsFormatSupported"; outcome["hresult"] = code(supported);
    } else {
      if (FAILED(supported)) { outcome["failed_api"] = "IsFormatSupported"; outcome["hresult"] = code(supported); throw supported; }
      outcome["support"] = "reported_supported";
      if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) throw std::runtime_error("Probe canceled before initialization");
      stage("Initialize", "begin");
      if (mode == AUDCLNT_SHAREMODE_SHARED && SUCCEEDED(client->QueryInterface(__uuidof(IAudioClient3), reinterpret_cast<void**>(client3.put())))) {
        UINT32 def = 0, fundamental = 0, minimum = 0, maximum = 0;
        checked("GetSharedModeEnginePeriod", client3->GetSharedModeEnginePeriod(&format.Format, &def, &fundamental, &minimum, &maximum));
        outcome["requested_period_frames"] = def;
        checked("InitializeSharedAudioStream", client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, def, &format.Format, nullptr));
      } else if (mode == AUDCLNT_SHAREMODE_SHARED) {
        checked("Initialize", client->Initialize(mode, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, &format.Format, nullptr));
      } else {
        REFERENCE_TIME def = 0, minimum = 0;
        checked("GetDevicePeriod", client->GetDevicePeriod(&def, &minimum));
        auto period = std::max(def, minimum);
        outcome["requested_period_100ns"] = period;
        auto initialized = client->Initialize(mode, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, &format.Format, nullptr);
        stage("InitializeExclusive", "end", {{"hresult", code(initialized)}, {"requested_period_100ns", period}});
        if (initialized == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
          UINT32 aligned = 0;
          checked("GetAlignedBufferSize", client->GetBufferSize(&aligned));
          client.reset();
          checked("ReactivateAudioClient", device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(client.put())));
          period = static_cast<REFERENCE_TIME>((10000000ULL * aligned + format.Format.nSamplesPerSec / 2) / format.Format.nSamplesPerSec);
          stage("InitializeExclusiveAligned", "begin", {{"aligned_frames", aligned}, {"requested_period_100ns", period}});
          initialized = client->Initialize(mode, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, &format.Format, nullptr);
        }
        checked("InitializeExclusive", initialized);
      }
      outcome["initialization"] = "accepted";
      UINT32 buffer_frames = 0;
      checked("GetBufferSize", client->GetBufferSize(&buffer_frames));
      if (!buffer_frames || static_cast<std::uint64_t>(buffer_frames) * format.Format.nBlockAlign > 32 * 1024 * 1024)
        throw std::runtime_error("Negotiated buffer exceeds the test memory budget");
      outcome["actual_buffer_frames"] = buffer_frames;
      REFERENCE_TIME latency = 0;
      if (SUCCEEDED(client->GetStreamLatency(&latency))) outcome["api_reported_latency_100ns"] = latency;
      checked("SetEventHandle", client->SetEventHandle(audio_event.handle));
      if (flow == eRender) {
        checked("GetRenderService", client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(render.put())));
        BYTE* data = nullptr;
        checked("PrefillGetBuffer", render->GetBuffer(buffer_frames, &data));
        checked("PrefillSilentBuffer", render->ReleaseBuffer(buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT));
      } else checked("GetCaptureService", client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(capture.put())));
      stage("Start", "begin");
      checked("Start", client->Start()); started = true;
      const auto start = std::chrono::steady_clock::now();
      const auto deadline = start + std::chrono::milliseconds(job.duration_ms);
      std::uint64_t frames = 0, events = 0, discontinuities = 0, startup_discontinuities = 0, timestamp_errors = 0;
      bool canceled = false;
      stage("BufferProcessing", "begin");
      while (std::chrono::steady_clock::now() < deadline) {
        const HANDLE handles[]{cancel, audio_event.handle};
        const auto wait = WaitForMultipleObjects(2, handles, FALSE, 100);
        if (wait == WAIT_OBJECT_0) { canceled = true; break; }
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0 + 1) throw std::runtime_error("Audio event wait failed");
        ++events;
        if (flow == eRender) {
          UINT32 available = buffer_frames;
          if (mode == AUDCLNT_SHAREMODE_SHARED) {
            UINT32 padding = 0;
            runtime_checked("GetCurrentPadding", client->GetCurrentPadding(&padding));
            if (padding > buffer_frames) throw std::runtime_error("Invalid render padding");
            available -= padding;
          }
          if (available) {
            BYTE* data = nullptr;
            runtime_checked("RenderGetBuffer", render->GetBuffer(available, &data));
            runtime_checked("RenderReleaseBuffer", render->ReleaseBuffer(available, AUDCLNT_BUFFERFLAGS_SILENT));
            frames += available;
          }
        } else {
          const unsigned packet_budget = mode == AUDCLNT_SHAREMODE_SHARED ? 128 : 1;
          for (unsigned packets = 0; packets < packet_budget && std::chrono::steady_clock::now() < deadline; ++packets) {
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) { canceled = true; break; }
            // GetNextPacketSize is valid only in shared mode. Exclusive event
            // mode consumes one full packet directly after the event.
            if (mode == AUDCLNT_SHAREMODE_SHARED) {
              UINT32 available = 0;
              runtime_checked("GetNextPacketSize", capture->GetNextPacketSize(&available));
              if (!available) break;
            }
            BYTE* data = nullptr; UINT32 count = 0; DWORD flags = 0;
            const auto hr = capture->GetBuffer(&data, &count, &flags, nullptr, nullptr);
            runtime_checked("CaptureGetBuffer", hr);
            if (hr == AUDCLNT_S_BUFFER_EMPTY) break;
            if (!count || count > buffer_frames) throw std::runtime_error("Invalid capture packet size");
            if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
              if (!frames) ++startup_discontinuities; else ++discontinuities;
            }
            if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) ++timestamp_errors;
            frames += count;
            runtime_checked("CaptureReleaseBuffer", capture->ReleaseBuffer(count));
          }
        }
        if (canceled) break;
      }
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
      outcome["runtime_duration_ms"] = elapsed; outcome["serviced_frames"] = frames; outcome["audio_events"] = events;
      outcome["discontinuities"] = discontinuities; outcome["startup_discontinuities"] = startup_discontinuities;
      outcome["timestamp_errors"] = timestamp_errors; outcome["canceled"] = canceled;
      runtime_ok = !canceled && elapsed >= job.duration_ms && frames > 0 && events >= 3 && !discontinuities && !timestamp_errors;
      stage("BufferProcessing", "end", {{"runtime_result", runtime_ok ? "passed" : "inconclusive"},
        {"duration_ms", elapsed}, {"serviced_frames", frames}, {"audio_events", events}});
    }
  } catch (HRESULT hr) {
    outcome["hresult"] = code(hr); outcome["classification"] = hr == AUDCLNT_E_UNSUPPORTED_FORMAT ? "failed" : "inconclusive";
    if (!outcome.contains("failed_api")) outcome["failed_api"] = "BufferProcessing";
  } catch (const std::exception& e) { outcome["reason"] = e.what(); }
  stage("Cleanup", "begin");
  if (started) {
    const auto hr = client->Stop();
    stage("Stop", "end", {{"hresult", code(hr)}});
    cleanup_ok = SUCCEEDED(hr);
  }
  render.reset(); capture.reset(); client3.reset(); client.reset(); endpoint.reset(); device.reset(); enumerator.reset();
  outcome["cleanup"] = cleanup_ok ? "completed" : "failed";
  if (runtime_ok && cleanup_ok) outcome["classification"] = "verified";
  stage("Cleanup", "end", {{"result", outcome["cleanup"]}});
  return outcome;
}
}

int run_probe_worker() {
  try {
    if (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) != FILE_TYPE_PIPE || GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_PIPE)
      return 2;
    std::string input; char c = 0;
    while (std::cin.get(c) && c != '\n') {
      if (input.size() >= 32768) return 2;
      input.push_back(c);
    }
    const auto request = detail::parse_json(input);
    const auto job = parse_probe_job(request);
    const auto raw_handle = request.at("cancel_handle").get<std::uintptr_t>();
    HANDLE cancel = reinterpret_cast<HANDLE>(raw_handle);
    DWORD flags = 0;
    if (!cancel || !GetHandleInformation(cancel, &flags)) return 2;
    const auto result = probe(job, cancel);
    emit(Json{{"type", "result"}, {"attempt_id", job.attempt_id}, {"result", result}});
    return 0;
  } catch (...) { return 2; }
}
}
#else
namespace audiohole::scanner { int run_probe_worker() { return 2; } }
#endif
