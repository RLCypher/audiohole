#include "access_negotiation.hpp"
#include "stream_pipeline.hpp"
#include "wasapi_policy.hpp"

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Establish Win32/COM types before SDK interfaces. Property keys used below
// are local constants; the SDK's function-discovery key catalog is not needed.
#include <windows.h>

#include "worker_scheduling.hpp"
#include <audioclient.h>
#include <avrt.h>
#include <cstring>
#include <ks.h>
#include <ksmedia.h>
#include <limits>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propvarutil.h>

namespace audiohole::engine::detail {
namespace {
// SDK-defined values are local constants so MinGW/MSVC do not require a
// separate translation unit exporting property/KS GUID definitions.
constexpr GUID pcm_subtype{
    0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr GUID float_subtype{
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr PROPERTYKEY device_format_key{
    {0xf19f064d, 0x082c, 0x4e27, {0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c}}, 0};
struct NativeFailure {
    Issue issue;
};
struct AccessFailure {
    std::vector<Issue> issues;
};

[[noreturn]] void native_failure(const char* stage, HRESULT code) {
    throw NativeFailure{native_open_issue(stage, static_cast<std::uint32_t>(code))};
}
void checked(HRESULT code, const char* stage) {
    if (FAILED(code))
        native_failure(stage, code);
}

// All COM pointers below are created, used and released on ONE owning STA
// route worker. Only copied configuration and atomic counters cross threads.
template <class T> class Com {
  public:
    ~Com() {
        if (value_)
            value_->Release();
    }
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    T* operator->() const noexcept {
        return value_;
    }
    T** put() noexcept {
        if (value_)
            value_->Release();
        value_ = nullptr;
        return &value_;
    }
    T* get() const noexcept {
        return value_;
    }

  private:
    T* value_ = nullptr;
};

class Event {
  public:
    Event() : handle_(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {
        if (!handle_)
            native_failure("CreateEvent", HRESULT_FROM_WIN32(GetLastError()));
    }
    ~Event() {
        CloseHandle(handle_);
    }
    HANDLE get() const noexcept {
        return handle_;
    }

  private:
    HANDLE handle_;
};

// Optional per-route high-resolution deadline for the short capture wait. A
// plain 1-ms wait timeout can round to a scheduler tick and exhaust playback
// padding. Do not change the global timer resolution. Unsupported/failed timer
// creation or arming retains immediate rendering and honest starvation counts.
class CaptureDeadline {
  public:
    CaptureDeadline()
        : handle_(CreateWaitableTimerExW(nullptr, nullptr, high_resolution_timer_flag,
                                         TIMER_MODIFY_STATE | SYNCHRONIZE)) {
        // 0x2 is CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Windows 10 1803+).
    }
    ~CaptureDeadline() {
        if (handle_)
            CloseHandle(handle_);
    }
    CaptureDeadline(const CaptureDeadline&) = delete;
    CaptureDeadline& operator=(const CaptureDeadline&) = delete;
    HANDLE get() const noexcept {
        return handle_;
    }
    bool arm(std::uint32_t milliseconds) noexcept {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(milliseconds) * 10000; // Relative 100-ns units.
        return handle_ && SetWaitableTimer(handle_, &due, 0, nullptr, nullptr, FALSE);
    }
    void cancel() noexcept {
        if (handle_)
            (void)CancelWaitableTimer(handle_);
    }

  private:
    static constexpr DWORD high_resolution_timer_flag = 0x00000002;
    HANDLE handle_ = nullptr; // Used and released by its owning route worker.
};

class Apartment {
  public:
    Apartment() {
        checked(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "CoInitializeEx");
    }
    ~Apartment() {
        CoUninitialize();
    }
};

std::wstring wide(const std::string& value) {
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        native_failure("DecodeEndpointId", E_INVALIDARG);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), length);
    return result;
}

StreamFormat describe(const WAVEFORMATEX* format) {
    if (!format || format->nChannels < 1 || format->nChannels > 256 || format->nSamplesPerSec < 1 ||
        format->nSamplesPerSec > 768000)
        throw NativeFailure{{ErrorCode::UnsupportedRequirement, "format",
                             "Endpoint reports a format outside prototype limits."}};
    GUID subformat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ? float_subtype : pcm_subtype;
    WORD valid = format->wBitsPerSample;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (format->cbSize < 22)
            native_failure("ReadExtensibleFormat", E_INVALIDARG);
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        subformat = ext->SubFormat;
        valid = ext->Samples.wValidBitsPerSample;
    } else if (format->wFormatTag != WAVE_FORMAT_PCM &&
               format->wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
        native_failure("ReadPcmFormat", AUDCLNT_E_UNSUPPORTED_FORMAT);
    }
    SampleFormat representation;
    if (IsEqualGUID(subformat, float_subtype) && format->wBitsPerSample == 32)
        representation = SampleFormat::Float32;
    else if (IsEqualGUID(subformat, pcm_subtype)) {
        if (format->wBitsPerSample == 16 && valid == 16)
            representation = SampleFormat::PcmS16;
        else if (format->wBitsPerSample == 24 && valid == 24)
            representation = SampleFormat::PcmS24Packed;
        else if (format->wBitsPerSample == 32 && valid == 24)
            representation = SampleFormat::PcmS24In32;
        else if (format->wBitsPerSample == 32 && valid == 32)
            representation = SampleFormat::PcmS32;
        else
            native_failure("ReadPcmPrecision", AUDCLNT_E_UNSUPPORTED_FORMAT);
    } else
        native_failure("ReadSubformat", AUDCLNT_E_UNSUPPORTED_FORMAT);
    return {static_cast<int>(format->nSamplesPerSec), format->nChannels, representation};
}

WAVEFORMATEXTENSIBLE wave_format(const StreamFormat& format, DWORD channel_mask) {
    const auto geometry = pcm_geometry(format);
    if (!geometry.ok())
        native_failure("BuildPcmFormat", E_INVALIDARG);
    WAVEFORMATEXTENSIBLE result{};
    result.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    result.Format.nChannels = static_cast<WORD>(format.channel_count);
    result.Format.nSamplesPerSec = static_cast<DWORD>(format.sample_rate_hz);
    result.Format.wBitsPerSample = static_cast<WORD>(geometry.value->container_bits);
    result.Format.nBlockAlign = static_cast<WORD>(geometry.value->block_align_bytes);
    result.Format.nAvgBytesPerSec = static_cast<DWORD>(geometry.value->average_bytes_per_second);
    result.Format.cbSize = 22;
    result.Samples.wValidBitsPerSample = static_cast<WORD>(geometry.value->valid_bits);
    result.dwChannelMask = channel_mask;
    result.SubFormat = format.sample_format == SampleFormat::Float32 ? float_subtype : pcm_subtype;
    return result;
}

struct Client {
    Com<IMMDevice> device;
    Com<IAudioClient> audio;
    Com<IAudioCaptureClient> capture;
    Com<IAudioRenderClient> render;
    Event event;
    StreamConfiguration configuration;
    UINT32 buffer_frames = 0;
    bool started = false;

    ~Client() {
        if (started)
            (void)audio->Stop();
    }

    void initialize(IMMDeviceEnumerator* enumerator, const LocalBinding& binding,
                    const EndpointRequirements& requested, bool input, LatencyPreset preset,
                    bool prefer_exclusive, std::stop_token stop, bool shared_capture_required) {
        std::optional<Issue> conflict;
        if (input && shared_capture_required)
            conflict = Issue{ErrorCode::ResourceConflict, "ownership",
                             "This capture endpoint has multiple consumers; Exclusive ownership "
                             "is unavailable for this profile."};
        const auto negotiated = negotiate_access(
            requested, prefer_exclusive, stop, input ? "source" : "destination",
            [&](const EndpointRequirements& concrete) -> Result<StreamConfiguration> {
                try {
                    initialize_attempt(enumerator, binding, concrete, input, preset, stop);
                    return {configuration, {}};
                } catch (const NativeFailure& failure) {
                    return {std::nullopt, {failure.issue}};
                }
            },
            [&] {
                // Acquisitions are stopped here. Every failed attempt releases
                // COM ownership on this same STA before a Shared retry begins.
                (void)capture.put();
                (void)render.put();
                (void)audio.put();
                (void)device.put();
                configuration = {};
                buffer_frames = 0;
                (void)ResetEvent(event.get());
            },
            conflict);
        if (!negotiated.ok()) {
            // Preserve the complete per-attempt failure list for the supervisor.
            throw AccessFailure{negotiated.issues};
        }
        configuration = *negotiated.value;
    }

    void initialize_attempt(IMMDeviceEnumerator* enumerator, const LocalBinding& binding,
                            const EndpointRequirements& requested, bool input,
                            LatencyPreset preset, std::stop_token stop) {
        const auto id = wide(binding.endpoint_id);
        checked(enumerator->GetDevice(id.c_str(), device.put()), "GetDevice");
        DWORD state = 0;
        checked(device->GetState(&state), "GetState");
        if (state != DEVICE_STATE_ACTIVE)
            native_failure("EndpointNotActive", AUDCLNT_E_DEVICE_INVALIDATED);
        Com<IMMEndpoint> endpoint;
        checked(
            device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void**>(endpoint.put())),
            "GetEndpointDirection");
        EDataFlow flow;
        checked(endpoint->GetDataFlow(&flow), "GetDataFlow");
        if (flow != (input ? eCapture : eRender))
            native_failure("EndpointDirectionChanged", E_INVALIDARG);
        checked(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(audio.put())),
                "ActivateAudioClient");
        WAVEFORMATEX* mix_raw = nullptr;
        checked(audio->GetMixFormat(&mix_raw), "GetMixFormat");
        const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mix(mix_raw, CoTaskMemFree);
        auto base = describe(mix.get());
        DWORD mask = 0;
        if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->cbSize >= 22)
            mask = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix.get())->dwChannelMask;
        configuration.mode = requested.mode.value_or(AccessMode::Shared);
        if (configuration.mode == AccessMode::Exclusive) {
            // Exclusive's native starting candidate comes from the configured
            // device format, not the shared float mix format. Manual fields
            // override it. Scanner hints can fill unspecified fields only;
            // explicit fields remain requirements across every candidate.
            Com<IPropertyStore> properties;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.put()))) {
                struct PropertyValue {
                    PROPVARIANT value;
                    PropertyValue() {
                        PropVariantInit(&value);
                    }
                    ~PropertyValue() {
                        PropVariantClear(&value);
                    }
                } property;
                auto& value = property.value;
                if (SUCCEEDED(properties->GetValue(device_format_key, &value)) &&
                    value.vt == VT_BLOB && value.blob.cbSize >= sizeof(WAVEFORMATEX)) {
                    const auto* configured =
                        reinterpret_cast<const WAVEFORMATEX*>(value.blob.pBlobData);
                    if (configured->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
                        value.blob.cbSize >= sizeof(WAVEFORMATEXTENSIBLE)) {
                        base = describe(configured);
                        mask = configured->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                                   ? reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(configured)
                                         ->dwChannelMask
                                   : 0;
                    }
                }
            }
        }
        configuration.native_format =
            base; // Reported mix/configured format, not hardware measurement.
        StreamFormat selected{requested.sample_rate_hz.value_or(base.sample_rate_hz),
                              requested.channel_count.value_or(base.channel_count),
                              requested.sample_format.value_or(base.sample_format)};
        if (selected.channel_count != base.channel_count)
            mask = 0; // Unknown custom channel order.
        auto format = wave_format(selected, mask);
        configuration.format_selection = "Live shared mix format with active preferences";
        const auto share = configuration.mode == AccessMode::Shared ? AUDCLNT_SHAREMODE_SHARED
                                                                    : AUDCLNT_SHAREMODE_EXCLUSIVE;
        HRESULT initialized = E_FAIL;
        configuration.period_selection =
            share == AUDCLNT_SHAREMODE_EXCLUSIVE ? "exclusive" : "legacy-shared";
        if (share == AUDCLNT_SHAREMODE_SHARED) {
            Com<IAudioClient3> low_latency;
            if (SUCCEEDED(audio->QueryInterface(__uuidof(IAudioClient3),
                                                reinterpret_cast<void**>(low_latency.put())))) {
                UINT32 normal = 0, fundamental = 0, minimum = 0, maximum = 0;
                if (SUCCEEDED(low_latency->GetSharedModeEnginePeriod(
                        &format.Format, &normal, &fundamental, &minimum, &maximum))) {
                    configuration.shared_default_period_frames = normal;
                    configuration.shared_fundamental_period_frames = fundamental;
                    configuration.shared_minimum_period_frames = minimum;
                    configuration.shared_maximum_period_frames = maximum;
                    const auto smallest = minimum_shared_period(fundamental, minimum, maximum);
                    const bool try_minimum = !requested.period_ms && preset == LatencyPreset::Low &&
                                             smallest.has_value();
                    auto chosen = try_minimum ? *smallest : normal;
                    if (requested.period_ms) {
                        const auto explicit_period =
                            requested_shared_period(*requested.period_ms, selected.sample_rate_hz,
                                                    fundamental, minimum, maximum);
                        if (!explicit_period)
                            throw NativeFailure{{ErrorCode::UnsupportedRequirement, "period_ms",
                                                 "Processing interval is not supported within "
                                                 "ordinary frame rounding."}};
                        chosen = *explicit_period;
                    }
                    if (!requested.buffer_ms) {
                        initialized = low_latency->InitializeSharedAudioStream(
                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, chosen, &format.Format, nullptr);
                        if (SUCCEEDED(initialized)) {
                            configuration.period_frames = chosen;
                            configuration.period_selection = requested.period_ms ? "explicit-period"
                                                             : try_minimum       ? "minimum"
                                                                                 : "default";
                        } else if (try_minimum) {
                            std::ostringstream reason;
                            reason << "Minimum shared period rejected: HRESULT 0x" << std::hex
                                   << std::uppercase << static_cast<std::uint32_t>(initialized)
                                   << "; retrying default shared period.";
                            configuration.low_latency_fallback = reason.str();
                            // A rejected minimum (including a period lock held by
                            // another shared stream) must not trigger Exclusive or
                            // change the PCM tuple. Re-create the client before a
                            // default-period retry; failed Initialize state is not
                            // assumed reusable. The old Client3 reference is dropped
                            // before releasing its IAudioClient reference.
                            (void)low_latency.put();
                            checked(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                                     reinterpret_cast<void**>(audio.put())),
                                    "ReactivateDefaultPeriodClient");
                            if (SUCCEEDED(audio->QueryInterface(
                                    __uuidof(IAudioClient3),
                                    reinterpret_cast<void**>(low_latency.put())))) {
                                initialized = low_latency->InitializeSharedAudioStream(
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK, normal, &format.Format,
                                    nullptr);
                                if (SUCCEEDED(initialized)) {
                                    configuration.period_frames = normal;
                                    configuration.period_selection =
                                        "default-after-minimum-rejected";
                                }
                            }
                        }
                    }
                    if (requested.period_ms && !requested.buffer_ms && FAILED(initialized)) {
                        auto issue = native_open_issue("InitializeExplicitSharedPeriod",
                                                       static_cast<std::uint32_t>(initialized));
                        issue.code = ErrorCode::UnsupportedRequirement;
                        throw NativeFailure{std::move(issue)};
                    }
                    if (preset == LatencyPreset::Low && !smallest)
                        configuration.low_latency_fallback =
                            "Invalid advertised minimum/fundamental period; used default attempt.";
                }
            }
            if (preset == LatencyPreset::Low && configuration.low_latency_fallback.empty() &&
                configuration.period_selection != "minimum")
                configuration.low_latency_fallback =
                    "Minimum-period negotiation unavailable; retained shared compatibility path.";
        }
        if (requested.period_ms && share == AUDCLNT_SHAREMODE_SHARED && !requested.buffer_ms &&
            !configuration.period_frames)
            throw NativeFailure{
                {ErrorCode::UnsupportedRequirement, "period_ms",
                 "The requested format has no controllable shared processing interval."}};
        if (FAILED(initialized)) {
            // Re-activate after any Client3 attempt. COM initialization state
            // must not be assumed reusable following a failed Initialize call.
            checked(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                     reinterpret_cast<void**>(audio.put())),
                    "ReactivateAudioClient");
            DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
            REFERENCE_TIME duration =
                requested.buffer_ms ? REFERENCE_TIME(*requested.buffer_ms) * 10000 : 0;
            REFERENCE_TIME periodicity = 0;
            if (share == AUDCLNT_SHAREMODE_SHARED) {
                flags |=
                    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            } else {
                const FormatHint initial{selected, AccessMode::Exclusive, mask};
                const auto chosen = select_exclusive_format(
                    requested, initial,
                    binding.format_hints, stop,
                    [&](const FormatHint& candidate) -> Result<bool> {
                        auto trial = wave_format(candidate.format,
                                                 candidate.channel_mask.value_or(0));
                        auto supported = audio->IsFormatSupported(share, &trial.Format, nullptr);
                        if (supported == AUDCLNT_E_UNSUPPORTED_FORMAT &&
                            candidate.format.channel_count <= 2 &&
                            trial.Samples.wValidBitsPerSample == trial.Format.wBitsPerSample) {
                            // Header aliases describe the SAME PCM tuple. Query
                            // them only after a format refusal, never to mask an
                            // ownership/permission/device error.
                            auto plain = trial;
                            plain.Format.wFormatTag =
                                candidate.format.sample_format == SampleFormat::Float32
                                    ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
                            plain.Format.cbSize = 0;
                            supported = audio->IsFormatSupported(share, &plain.Format, nullptr);
                            if (supported == S_OK)
                                trial = plain;
                        }
                        if (supported == AUDCLNT_E_UNSUPPORTED_FORMAT)
                            return {false, {}};
                        if (supported != S_OK)
                            return {std::nullopt,
                                    {native_open_issue("IsExclusiveFormatSupported",
                                                       static_cast<std::uint32_t>(supported))}};
                        format = trial;
                        return {true, {}};
                    });
                if (!chosen.ok())
                    throw NativeFailure{chosen.issues.front()};
                selected = chosen.value->format;
                configuration.format_selection = *chosen.value == initial
                    ? "Live configured format with active preferences; support queried"
                    : "Scanner format hint revalidated by live IsFormatSupported";
                if (requested.period_ms) {
                    const auto requested_duration =
                        static_cast<REFERENCE_TIME>(std::ceil(*requested.period_ms * 10000));
                    if (duration && std::abs(double(duration - requested_duration)) >
                                        10000000.0 / selected.sample_rate_hz)
                        throw NativeFailure{{ErrorCode::UnsupportedRequirement, "period_ms",
                                             "Exclusive event mode couples processing interval and "
                                             "device capacity."}};
                    duration = requested_duration;
                }
                if (!duration) {
                    REFERENCE_TIME normal = 0, minimum = 0;
                    checked(audio->GetDevicePeriod(&normal, &minimum), "GetDevicePeriod");
                    duration = std::max(normal, minimum);
                }
                periodicity = duration;
            }
            initialized =
                audio->Initialize(share, flags, duration, periodicity, &format.Format, nullptr);
            if (initialized == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
                UINT32 aligned = 0;
                checked(audio->GetBufferSize(&aligned), "GetAlignedBufferSize");
                duration = static_cast<REFERENCE_TIME>(
                    (10000000ULL * aligned + selected.sample_rate_hz - 1) /
                    selected.sample_rate_hz);
                checked(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                         reinterpret_cast<void**>(audio.put())),
                        "ActivateAlignedAudioClient");
                initialized = audio->Initialize(share, flags, duration,
                                                share == AUDCLNT_SHAREMODE_EXCLUSIVE ? duration : 0,
                                                &format.Format, nullptr);
            }
        }
        checked(initialized, "InitializeAudioStream");
        checked(audio->GetBufferSize(&buffer_frames), "GetBufferSize");
        if (buffer_frames == 0 || buffer_frames > 4 * 1024 * 1024)
            native_failure("BufferOutsidePrototypeBounds", E_INVALIDARG);
        configuration.client_format = selected;
        configuration.allocated_buffer_frames = buffer_frames;
        configuration.timing_sample_rate_hz = selected.sample_rate_hz;
        if (share == AUDCLNT_SHAREMODE_EXCLUSIVE)
            configuration.period_frames = buffer_frames;
        else if (!configuration.period_frames) {
            Com<IAudioClient3> current;
            WAVEFORMATEX* current_format = nullptr;
            UINT32 current_frames = 0;
            if (SUCCEEDED(audio->QueryInterface(__uuidof(IAudioClient3),
                                                reinterpret_cast<void**>(current.put()))) &&
                SUCCEEDED(
                    current->GetCurrentSharedModeEnginePeriod(&current_format, &current_frames))) {
                const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> owned(current_format,
                                                                                    CoTaskMemFree);
                if (current_format && current_frames)
                    configuration.period_frames = static_cast<UINT32>(
                        std::ceil(double(current_frames) * selected.sample_rate_hz /
                                  current_format->nSamplesPerSec));
            }
        }
        configuration.applied_period_request_ms = requested.period_ms;
        configuration.applied_render_ahead_request_ms = requested.render_ahead_ms;
        // This confirms the requested duration was supplied to Initialize.
        // Windows can quantize allocation; report its actual frames separately.
        configuration.applied_buffer_request_ms = requested.buffer_ms;
        if (requested.buffer_ms)
            configuration.period_selection = "explicit-buffer";
        if (!input) {
            configuration.render_ahead_target_frames = render_ahead_target(configuration, preset);
            if (!*configuration.render_ahead_target_frames ||
                *configuration.render_ahead_target_frames > buffer_frames ||
                (share == AUDCLNT_SHAREMODE_EXCLUSIVE &&
                 *configuration.render_ahead_target_frames != buffer_frames))
                throw NativeFailure{
                    {ErrorCode::UnsupportedRequirement, "render_ahead_ms",
                     "Playback lead cannot fit the device capacity or exclusive packet size."}};
        }
        checked(audio->SetEventHandle(event.get()), "SetEventHandle");
        if (input)
            checked(audio->GetService(__uuidof(IAudioCaptureClient),
                                      reinterpret_cast<void**>(capture.put())),
                    "GetCaptureService");
        else
            checked(audio->GetService(__uuidof(IAudioRenderClient),
                                      reinterpret_cast<void**>(render.put())),
                    "GetRenderService");
    }
};

// The route worker is the only writer. Non-RT observe() reads fixed-size atomic
// aggregates; no packet history, clock fitting, allocation, lock or logging is
// introduced in the capture/render loop. These observations are not a coherent
// sample-by-sample trace and do not establish acoustic latency or clock drift.
class NativeTiming {
  public:
    NativeTiming() {
        LARGE_INTEGER frequency{};
        if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0)
            frequency_ = frequency.QuadPart;
    }
    void capture(UINT64 position, UINT64 packet_qpc, DWORD flags, bool first_in_drain) noexcept {
        last_capture_position_.reset();
        last_capture_qpc_.reset();
        if (!(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) && packet_qpc) {
            last_capture_position_ = position;
            last_capture_qpc_ = packet_qpc;
        }
        const auto now = now_100ns();
        if (!now)
            return;
        const auto received = *now;
        if (first_in_drain) {
            if (last_delivery_ && received >= *last_delivery_) {
                const auto gap = (received - *last_delivery_) / 10;
                last_delivery_gap_ = gap;
                gap_max_.store(std::max(gap_max_.load(std::memory_order_relaxed), gap),
                               std::memory_order_relaxed);
                have_gap_.store(true, std::memory_order_relaxed);
            }
            last_delivery_ = received;
        }
        // WASAPI reports capture QPC in 100-ns units, not raw performance ticks.
        // Timestamp-error packets and impossible future timestamps are omitted.
        if (!(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) && packet_qpc &&
            received >= packet_qpc) {
            const auto age = (received - packet_qpc) / 10;
            age_last_.store(age, std::memory_order_relaxed);
            age_max_.store(std::max(age_max_.load(std::memory_order_relaxed), age),
                           std::memory_order_relaxed);
            have_age_.store(true, std::memory_order_relaxed);
        }
    }
    void drain(std::uint64_t frames) noexcept {
        const auto bounded = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(frames, std::numeric_limits<std::uint32_t>::max()));
        drain_max_.store(std::max(drain_max_.load(std::memory_order_relaxed), bounded),
                         std::memory_order_relaxed);
    }
    void render(std::uint32_t before, std::uint32_t submitted) noexcept {
        padding_before_.store(before, std::memory_order_relaxed);
        padding_after_.store(before + submitted, std::memory_order_relaxed);
        if (!before)
            empty_wakes_.fetch_add(1, std::memory_order_relaxed);
        have_padding_.store(true, std::memory_order_relaxed);
    }
    void initial_silence(std::uint32_t frames) noexcept {
        initial_silence_.store(frames, std::memory_order_relaxed);
        submitted_.fetch_add(frames, std::memory_order_relaxed);
        have_submission_.store(true, std::memory_order_relaxed);
    }
    void submitted(std::uint32_t frames, std::uint32_t startup_prefix) noexcept {
        request_max_.store(std::max(request_max_.load(std::memory_order_relaxed), frames),
                           std::memory_order_relaxed);
        submitted_.fetch_add(frames, std::memory_order_relaxed);
        startup_silence_.fetch_add(startup_prefix, std::memory_order_relaxed);
        have_submission_.store(true, std::memory_order_relaxed);
    }
    void wake(bool capture, bool render, bool timeout) noexcept {
        if (capture)
            capture_wakes_.fetch_add(1, std::memory_order_relaxed);
        if (render)
            render_wakes_.fetch_add(1, std::memory_order_relaxed);
        if (timeout)
            wait_timeouts_.fetch_add(1, std::memory_order_relaxed);
    }
    void capture_wait(bool timeout) noexcept {
        capture_waits_.fetch_add(1, std::memory_order_relaxed);
        if (timeout)
            capture_wait_timeouts_.fetch_add(1, std::memory_order_relaxed);
    }
    void capture_timer_available(bool available) noexcept {
        capture_timer_available_.store(available, std::memory_order_relaxed);
    }
    RenderTiming render_timing(std::optional<std::uint32_t> padding,
                               std::uint32_t requested) const noexcept {
        RenderTiming result;
        // QPC is sampled at pipeline-call entry, not at physical playback or
        // the exact sample within a block where starvation later occurs.
        result.render_qpc_100ns = now_100ns();
        result.last_capture_device_position_frames = last_capture_position_;
        result.last_capture_qpc_100ns = last_capture_qpc_;
        result.capture_delivery_gap_last_us = last_delivery_gap_;
        if (have_age_.load(std::memory_order_relaxed))
            result.capture_packet_age_last_us = age_last_.load(std::memory_order_relaxed);
        result.native_padding_before_frames = padding;
        result.native_render_request_frames = requested;
        return result;
    }
    void append(RouteObservation& result) const {
        result.native_capture_wakes = capture_wakes_.load(std::memory_order_relaxed);
        result.native_render_wakes = render_wakes_.load(std::memory_order_relaxed);
        result.native_wait_timeouts = wait_timeouts_.load(std::memory_order_relaxed);
        result.render_capture_waits = capture_waits_.load(std::memory_order_relaxed);
        result.render_capture_wait_timeouts =
            capture_wait_timeouts_.load(std::memory_order_relaxed);
        result.capture_wait_timer_available =
            capture_timer_available_.load(std::memory_order_relaxed);
        if (have_gap_.load(std::memory_order_relaxed))
            result.capture_delivery_gap_max_us = gap_max_.load(std::memory_order_relaxed);
        if (const auto maximum = drain_max_.load(std::memory_order_relaxed))
            result.capture_drain_max_frames = maximum;
        if (have_age_.load(std::memory_order_relaxed)) {
            result.capture_packet_age_last_us = age_last_.load(std::memory_order_relaxed);
            result.capture_packet_age_max_us = age_max_.load(std::memory_order_relaxed);
        }
        if (have_padding_.load(std::memory_order_relaxed)) {
            result.render_padding_before_frames = padding_before_.load(std::memory_order_relaxed);
            result.render_padding_after_submit_estimate_frames =
                padding_after_.load(std::memory_order_relaxed);
            result.render_empty_wakes = empty_wakes_.load(std::memory_order_relaxed);
        }
        if (have_submission_.load(std::memory_order_relaxed)) {
            result.native_initial_silence_frames = initial_silence_.load(std::memory_order_relaxed);
            result.native_startup_padding_silence_frames =
                startup_silence_.load(std::memory_order_relaxed);
            result.native_submitted_frames = submitted_.load(std::memory_order_relaxed);
            if (const auto maximum = request_max_.load(std::memory_order_relaxed))
                result.native_render_request_max_frames = maximum;
        }
    }

  private:
    LONGLONG frequency_ = 0;
    std::optional<std::uint64_t> last_delivery_; // Worker-owned QPC units / 100 ns.
    std::optional<std::uint64_t> last_delivery_gap_, last_capture_position_, last_capture_qpc_;
    std::atomic<std::uint64_t> gap_max_{0}, age_last_{0}, age_max_{0}, empty_wakes_{0};
    std::atomic<std::uint64_t> capture_wakes_{0}, render_wakes_{0}, wait_timeouts_{0};
    std::atomic<std::uint64_t> capture_waits_{0}, capture_wait_timeouts_{0};
    std::atomic<bool> capture_timer_available_{false};
    std::atomic<std::uint32_t> drain_max_{0}, padding_before_{0}, padding_after_{0};
    std::atomic<bool> have_gap_{false}, have_age_{false}, have_padding_{false};
    std::atomic<std::uint64_t> initial_silence_{0}, startup_silence_{0}, submitted_{0};
    std::atomic<std::uint32_t> request_max_{0};
    std::atomic<bool> have_submission_{false};

    std::optional<std::uint64_t> now_100ns() const noexcept {
        LARGE_INTEGER now{};
        if (!frequency_ || !QueryPerformanceCounter(&now) || now.QuadPart < 0)
            return std::nullopt;
        return static_cast<std::uint64_t>(static_cast<long double>(now.QuadPart) * 10000000 /
                                          frequency_);
    }
};

class WasapiSession final : public RouteSession {
  public:
    WasapiSession(BoundRoute route, bool experimental, LatencyPreset preset, WorkerSettings workers)
        : route_(std::move(route)), experimental_(experimental), preset_(preset),
          worker_scheduling_(workers) {}
    ~WasapiSession() override {
        stop();
    }

    Result<bool> initialize(std::stop_token cancellation) {
        worker_ = std::jthread([this](std::stop_token stop) { work(stop); });
        std::stop_callback cancel(cancellation, [this] {
            worker_.request_stop();
            condition_.notify_all();
        });
        std::unique_lock lock(mutex_);
        condition_.wait(lock, [&] { return ready_ || error_.has_value(); });
        if (error_)
            return {std::nullopt,
                    access_failures_.empty() ? std::vector<Issue>{*error_} : access_failures_};
        if (cancellation.stop_requested())
            return {std::nullopt, {{ErrorCode::Cancelled, "open", "Opening was cancelled."}}};
        return {true, {}};
    }
    NegotiatedRoute configuration() const override {
        std::lock_guard lock(mutex_);
        return configuration_;
    }
    Result<bool> start(std::stop_token stop) override {
        std::lock_guard lock(mutex_);
        if (stop.stop_requested())
            return {std::nullopt, {{ErrorCode::Cancelled, "start", "Start cancelled."}}};
        if (error_)
            return {std::nullopt,
                    access_failures_.empty() ? std::vector<Issue>{*error_} : access_failures_};
        go_ = true;
        condition_.notify_all();
        return {true, {}}; // Admission; processing is established by frame counters.
    }
    RouteObservation observe() override {
        auto result = pipeline_ ? pipeline_->observe() : RouteObservation{};
        timing_.append(result);
        result.worker_scheduling = worker_scheduling_.describe();
        std::lock_guard lock(mutex_);
        if (!access_failures_.empty())
            result.issues = access_failures_;
        else if (error_)
            result.issues.push_back(*error_);
        return result;
    }
    void stop() noexcept override {
        if (worker_.joinable()) {
            worker_.request_stop();
            condition_.notify_all();
            worker_.join();
        }
    }

  private:
    BoundRoute route_;
    bool experimental_;
    LatencyPreset preset_;
    WorkerScheduling worker_scheduling_;
    NativeTiming timing_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool ready_ = false, go_ = false;
    std::optional<Issue> error_;
    std::vector<Issue> access_failures_;
    NegotiatedRoute configuration_;
    std::unique_ptr<StreamPipeline> pipeline_;
    std::jthread worker_;

    void work(std::stop_token stop) noexcept {
        try {
            Apartment apartment;
            Event cancelled;
            CaptureDeadline capture_deadline;
            timing_.capture_timer_available(capture_deadline.get() != nullptr);
            std::stop_callback signal(stop, [&] {
                SetEvent(cancelled.get());
                condition_.notify_all();
            });
            Com<IMMDeviceEnumerator> enumerator;
            checked(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                     __uuidof(IMMDeviceEnumerator),
                                     reinterpret_cast<void**>(enumerator.put())),
                    "CreateDeviceEnumerator");
            Client source, destination;
            source.initialize(enumerator.get(), route_.source, route_.request.source, true, preset_,
                              route_.request.exclusive_mode_preferred, stop,
                              route_.request.shared_capture_required);
            if (stop.stop_requested())
                throw NativeFailure{{ErrorCode::Cancelled, "open", "Opening cancelled."}};
            destination.initialize(enumerator.get(), route_.destination, route_.request.destination,
                                   false, preset_, route_.request.exclusive_mode_preferred, stop,
                                   false);
            NegotiatedRoute negotiated;
            negotiated.source = source.configuration;
            negotiated.destination = destination.configuration;
            negotiated.backend = experimental_ ? BackendId::Experimental : BackendId::Wasapi;
            negotiated.preferred_clock_device = route_.request.preferred_clock_device;
            negotiated.processing = "Validation prototype: float32 processing, linear adaptive SRC";
            negotiated.synchronization =
                experimental_ ? "Experimental occupancy DLL" : "Prototype occupancy PI";
            negotiated.selection_reason =
                preset_ == LatencyPreset::Low
                    ? "Native low-latency validation preset; explicit requirements preserved"
                    : "Explicit selector or Automatic native comparison baseline";
            pipeline_ = std::make_unique<StreamPipeline>(
                source.configuration.client_format, destination.configuration.client_format,
                route_.request.channel_map, experimental_,
                route_.request.queue_target_ms ? *route_.request.queue_target_ms
                                               : queue_target_milliseconds(negotiated, preset_),
                route_.request.queue_target_ms.has_value());
            negotiated.requested_queue_target_ms = route_.request.queue_target_ms;
            negotiated.fifo_target_source_frames = pipeline_->target_frames();
            negotiated.fifo_capacity_source_frames = pipeline_->capacity_frames();
            const auto valid = check_negotiated(route_.request, negotiated);
            if (!valid.ok())
                throw NativeFailure{valid.issues.front()};
            {
                std::unique_lock lock(mutex_);
                configuration_ = std::move(negotiated);
                ready_ = true;
                condition_.notify_all();
                condition_.wait(lock, [&] { return go_ || stop.stop_requested(); });
            }
            if (stop.stop_requested())
                return;
            worker_scheduling_.start();
            struct ScheduleGuard {
                WorkerScheduling& value;
                ~ScheduleGuard() {
                    value.stop();
                }
            } schedule{worker_scheduling_};
            const auto render_target = *destination.configuration.render_ahead_target_frames;
            BYTE* prime = nullptr;
            checked(destination.render->GetBuffer(render_target, &prime), "PrimeRenderBuffer");
            checked(destination.render->ReleaseBuffer(render_target, AUDCLNT_BUFFERFLAGS_SILENT),
                    "ReleasePrimedRenderBuffer");
            timing_.initial_silence(render_target);
            // Validated once before streaming; callback offset arithmetic is
            // in native interleaved bytes, with no format allocation/query.
            const auto render_stride =
                pcm_geometry(destination.configuration.client_format).value->block_align_bytes;
            checked(source.audio->Start(), "StartCapture");
            source.started = true;
            checked(destination.audio->Start(), "StartRender");
            destination.started = true;
            const HANDLE events[]{cancelled.get(), source.event.get(), destination.event.get()};
            bool first_capture_packet = true;
            bool awaiting_first_transfer = true;
            // Bounded, allocation-free drainage shared by the ordinary wake
            // and the optional short capture wait. All COM stays on this worker;
            // no native output buffer is held while waiting for another packet.
            auto drain_capture = [&] {
                std::uint64_t drained_frames = 0;
                for (unsigned packet = 0; packet < 64 && !stop.stop_requested(); ++packet) {
                    UINT32 frames = 0;
                    checked(source.capture->GetNextPacketSize(&frames), "GetNextPacketSize");
                    if (!frames)
                        break;
                    BYTE* input = nullptr;
                    DWORD flags = 0;
                    UINT64 position = 0, qpc = 0;
                    checked(source.capture->GetBuffer(&input, &frames, &flags, &position, &qpc),
                            "CaptureGetBuffer");
                    // Position is device frames; QPC is in 100-ns units. This
                    // prototype does not estimate clock drift from timestamps.
                    timing_.capture(position, qpc, flags, drained_frames == 0);
                    drained_frames += frames;
                    if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                        pipeline_->discontinuity(first_capture_packet);
                    if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                        pipeline_->timestamp_error();
                    first_capture_packet = false;
                    (void)pipeline_->push(input, frames, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
                    checked(source.capture->ReleaseBuffer(frames), "CaptureReleaseBuffer");
                }
                timing_.drain(drained_frames);
            };
            while (!stop.stop_requested()) {
                const auto event = WaitForMultipleObjects(3, events, FALSE, 100);
                if (event == WAIT_OBJECT_0 || stop.stop_requested())
                    break;
                if (event == WAIT_FAILED)
                    native_failure("WaitAudioEvents", HRESULT_FROM_WIN32(GetLastError()));
                // WaitForMultipleObjects chooses the lowest signaled index.
                // Capture has priority there; also claim an already-signaled
                // render notification so repeated capture wakes cannot starve
                // output. Never fabricate an Exclusive wake from a timeout.
                bool render_ready = event == WAIT_OBJECT_0 + 2;
                if (!render_ready) {
                    const auto pending = WaitForSingleObject(destination.event.get(), 0);
                    if (pending == WAIT_FAILED)
                        native_failure("PollRenderEvent", HRESULT_FROM_WIN32(GetLastError()));
                    render_ready = pending == WAIT_OBJECT_0;
                }
                timing_.wake(event == WAIT_OBJECT_0 + 1, render_ready, event == WAIT_TIMEOUT);
                drain_capture();
                if (!render_ready || stop.stop_requested())
                    continue;
                UINT32 padding = 0;
                if (destination.configuration.mode == AccessMode::Shared)
                    checked(destination.audio->GetCurrentPadding(&padding), "GetRenderPadding");
                if (padding > destination.buffer_frames)
                    native_failure("InvalidRenderPadding", E_UNEXPECTED);
                const UINT32 frames =
                    render_fill_frames(destination.buffer_frames, padding, render_target);
                // Native padding is available only in Shared mode. Exclusive
                // event packets retain the full negotiated buffer contract.
                if (!frames) {
                    if (destination.configuration.mode == AccessMode::Shared)
                        timing_.render(padding, 0);
                    continue;
                }
                const auto wait_ms = capture_wait_milliseconds(destination.configuration, padding,
                                                               pipeline_->would_starve(frames));
                const bool wait_armed = wait_ms && capture_deadline.arm(wait_ms);
                if (wait_ms)
                    timing_.capture_timer_available(wait_armed);
                if (wait_armed) {
                    // At the observed starvation, Windows still had 12 ms of
                    // queued audio and the next 10-ms capture packet was due.
                    // Wait ONCE, at most 1 requested ms, then submit this same
                    // bounded block. Do not expand the write to a catch-up block
                    // or increase the FIFO target because time passed here.
                    // OS scheduling can overshoot any wait; refreshed padding
                    // and underrun/wait counters keep that limitation visible.
                    const HANDLE capture_events[]{cancelled.get(), source.event.get(),
                                                  capture_deadline.get()};
                    const auto waited = WaitForMultipleObjects(3, capture_events, FALSE, INFINITE);
                    const DWORD wait_error = waited == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
                    capture_deadline.cancel();
                    if (waited == WAIT_OBJECT_0 || stop.stop_requested())
                        break;
                    if (waited == WAIT_FAILED)
                        native_failure("WaitCaptureBeforeRender", HRESULT_FROM_WIN32(wait_error));
                    timing_.capture_wait(waited == WAIT_OBJECT_0 + 2);
                    drain_capture(); // Also check after timeout; notifications can race it.
                    checked(destination.audio->GetCurrentPadding(&padding),
                            "GetRenderPaddingAfterCaptureWait");
                    if (padding > destination.buffer_frames)
                        native_failure("InvalidRenderPaddingAfterCaptureWait", E_UNEXPECTED);
                }
                if (stop.stop_requested())
                    break;
                BYTE* output = nullptr;
                checked(destination.render->GetBuffer(frames, &output), "RenderGetBuffer");
                const auto plan =
                    startup_render_plan(destination.configuration, frames, awaiting_first_transfer);
                const auto prefix_bytes = std::size_t(plan.prefix_silence_frames) * render_stride;
                std::memset(output, 0, prefix_bytes);
                const auto context =
                    timing_.render_timing(destination.configuration.mode == AccessMode::Shared
                                              ? std::optional<std::uint32_t>(padding)
                                              : std::nullopt,
                                          frames);
                const auto produced =
                    pipeline_->render(output + prefix_bytes, plan.pipeline_frames, context);
                if (produced)
                    awaiting_first_transfer = false;
                checked(destination.render->ReleaseBuffer(frames, 0), "RenderReleaseBuffer");
                timing_.submitted(frames, plan.prefix_silence_frames);
                if (destination.configuration.mode == AccessMode::Shared)
                    timing_.render(padding, frames);
            }
            // Client destructors Stop/Release, then the STA unwinds. No COM
            // pointer is released on the GUI or supervisor thread.
        } catch (const AccessFailure& failure) {
            std::lock_guard lock(mutex_);
            // One failure remains the session admission error; retaining the
            // rest below allows GUI snapshots to distinguish both mode attempts.
            error_ = failure.issues.empty() ? Issue{ErrorCode::BackendFailure, "access",
                                                    "Endpoint acquisition failed."}
                                            : failure.issues.front();
            access_failures_ = failure.issues;
            condition_.notify_all();
        } catch (const NativeFailure& failure) {
            std::lock_guard lock(mutex_);
            error_ = failure.issue;
            condition_.notify_all();
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex_);
            error_ = Issue{ErrorCode::BackendFailure, "wasapi", error.what()};
            condition_.notify_all();
        } catch (...) {
            std::lock_guard lock(mutex_);
            error_ =
                Issue{ErrorCode::BackendFailure, "wasapi", "Unexpected native backend failure."};
            condition_.notify_all();
        }
    }
};

class WasapiAdapter final : public BackendAdapter {
  public:
    WasapiAdapter(bool experimental, LatencyPreset preset, WorkerSettings workers)
        : experimental_(experimental), preset_(preset), workers_(workers) {}
    BackendDescriptor descriptor() const override {
        return {experimental_ ? BackendId::Experimental : BackendId::Wasapi, true,
                "Windows event-driven I/O; unvalidated prototype processing"};
    }
    Result<std::unique_ptr<RouteSession>> open(const BoundRoute& route,
                                               std::stop_token stop) override {
        auto session = std::make_unique<WasapiSession>(route, experimental_, preset_, workers_);
        const auto initialized = session->initialize(stop);
        if (!initialized.ok())
            return {std::nullopt, initialized.issues};
        return {std::move(session), {}};
    }

  private:
    bool experimental_;
    LatencyPreset preset_;
    WorkerSettings workers_;
};
} // namespace

std::unique_ptr<BackendAdapter> make_wasapi_adapter(bool experimental, LatencyPreset preset,
                                                    WorkerSettings workers) {
    return std::make_unique<WasapiAdapter>(experimental, preset, workers);
}
} // namespace audiohole::engine::detail
#else
namespace audiohole::engine::detail {
std::unique_ptr<BackendAdapter> make_wasapi_adapter(bool, LatencyPreset, WorkerSettings) {
    return {};
}
} // namespace audiohole::engine::detail
#endif
