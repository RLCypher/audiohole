#include "platform/windows/diagnostics.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace audiohole::windows {
namespace {
constexpr GUID pcm_subtype{1, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
constexpr GUID float_subtype{3, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
template <class T> struct Com {
    T* p = nullptr;
    ~Com() {
        if (p)
            p->Release();
    }
    T** put() {
        return &p;
    }
    T* operator->() const {
        return p;
    }
};
struct Handle {
    HANDLE value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ~Handle() {
        if (value)
            CloseHandle(value);
    }
};
struct Wave {
    WAVEFORMATEX* p = nullptr;
    ~Wave() {
        CoTaskMemFree(p);
    }
};
void check(HRESULT result) {
    if (FAILED(result)) {
        char text[100]{};
        std::snprintf(text, sizeof(text), "Selected device check failed (HRESULT 0x%08lX).",
                      static_cast<unsigned long>(result));
        throw std::runtime_error(text);
    }
}
std::wstring wide(const std::string& value) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0);
    if (!count)
        throw std::runtime_error("Invalid device binding.");
    std::wstring result(count, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), count);
    return result;
}
// One owner worker, all COM/client/buffer access on that worker. It runs only
// during an explicit check. The GUI samples a scalar peak at 10 Hz; audio bytes
// never cross this boundary. Event waits are bounded to 30 ms for cancellation.
// This is a temporary control-side worker, not a real-time routing callback:
// COM calls, buffer setup and a scalar-reading mutex are allowed here. There is
// no GUI/file/JSON work, logging, recording, clock synchronization or recovery.
// Peak sampling uses native frame counts; timestamps are intentionally not used
// to infer latency. Invalidation terminates this session without retargeting.
class Session final : public DiagnosticService {
    mutable std::mutex mutex_;
    DiagnosticReading state_;
    std::atomic<bool> busy_{false};
    std::jthread worker_;
    void detail(const std::string& value) {
        std::lock_guard lock(mutex_);
        state_.detail = value;
    }
    void run(std::stop_token stop, std::string endpoint, bool capture) {
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(initialized)) {
            std::lock_guard lock(mutex_);
            state_.detail = "Cannot initialize the device-check worker.";
            state_.failed = true;
            state_.active = false;
            busy_ = false;
            return;
        }
        {
            Handle event;
            Com<IMMDeviceEnumerator> enumerator;
            Com<IMMDevice> device;
            Com<IAudioClient> client;
            Com<IAudioRenderClient> render;
            Com<IAudioCaptureClient> input;
            Wave wave;
            bool started = false;
            try {
                if (stop.stop_requested())
                    throw std::runtime_error("Device check cancelled.");
                check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**>(enumerator.put())));
                check(enumerator->GetDevice(wide(endpoint).c_str(), device.put()));
                DWORD state = 0;
                check(device->GetState(&state));
                if (state != DEVICE_STATE_ACTIVE)
                    throw std::runtime_error("Selected device is unavailable.");
                Com<IMMEndpoint> direction;
                check(device->QueryInterface(__uuidof(IMMEndpoint),
                                             reinterpret_cast<void**>(direction.put())));
                EDataFlow flow;
                check(direction->GetDataFlow(&flow));
                if (flow != (capture ? eCapture : eRender))
                    throw std::runtime_error("Selected endpoint has the wrong direction.");
                if (stop.stop_requested())
                    throw std::runtime_error("Device check cancelled.");
                check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(client.put())));
                check(client->GetMixFormat(&wave.p));
                const auto& format = *wave.p;
                bool floating = format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
                if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && format.cbSize >= 22) {
                    const auto& extended = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wave.p);
                    floating = IsEqualGUID(extended.SubFormat, float_subtype);
                    if (!floating && !IsEqualGUID(extended.SubFormat, pcm_subtype))
                        throw std::runtime_error("Unsupported device-check sample format.");
                } else if (!floating && format.wFormatTag != WAVE_FORMAT_PCM)
                    throw std::runtime_error("Unsupported device-check sample format.");
                unsigned valid_bits = format.wBitsPerSample;
                if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                    valid_bits = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wave.p)
                                     ->Samples.wValidBitsPerSample;
                if (!valid_bits || valid_bits > format.wBitsPerSample)
                    throw std::runtime_error("Invalid mix-format precision.");
                const int bytes = format.wBitsPerSample / 8;
                if (format.nSamplesPerSec < 8000 || format.nSamplesPerSec > 384000 ||
                    !format.nChannels || format.nChannels > 64 ||
                    format.nBlockAlign != format.nChannels * bytes ||
                    (floating ? bytes != 4 : (bytes != 2 && bytes != 3 && bytes != 4)))
                    throw std::runtime_error(
                        "Selected device mix format cannot be checked by this build.");
                if (!event.value)
                    throw std::runtime_error("Cannot create device-check event.");
                // Shared only: a diagnostic must not force an exclusive takeover
                // or change a device's Windows-configured format or volume.
                if (stop.stop_requested())
                    throw std::runtime_error("Device check cancelled.");
                check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, wave.p, nullptr));
                check(client->SetEventHandle(event.value));
                UINT32 frames = 0;
                check(client->GetBufferSize(&frames));
                if (!frames || frames > format.nSamplesPerSec ||
                    static_cast<std::uint64_t>(frames) * format.nBlockAlign > 8 * 1024 * 1024)
                    throw std::runtime_error("Unbounded device-check buffer rejected.");
                if (capture)
                    check(client->GetService(__uuidof(IAudioCaptureClient),
                                             reinterpret_cast<void**>(input.put())));
                else
                    check(client->GetService(__uuidof(IAudioRenderClient),
                                             reinterpret_cast<void**>(render.put())));
                std::uint64_t emitted = 0;
                auto fill = [&](BYTE* data, UINT32 count) {
                    std::memset(data, 0, static_cast<std::size_t>(count) * format.nBlockAlign);
                    for (UINT32 frame = 0; frame < count && emitted < format.nSamplesPerSec;
                         ++frame, ++emitted) {
                        const double fade = std::min(
                            {1.0, (emitted + 1.0) / (format.nSamplesPerSec * .01),
                             (format.nSamplesPerSec - emitted) / (format.nSamplesPerSec * .01)});
                        const double sample =
                            .0316227766 * fade *
                            std::sin(6.283185307179586 * 1000 * emitted / format.nSamplesPerSec);
                        for (unsigned channel = 0;
                             channel < std::min<unsigned>(2, format.nChannels); ++channel) {
                            auto* out = data + frame * format.nBlockAlign + channel * bytes;
                            if (floating) {
                                const float value = static_cast<float>(sample);
                                std::memcpy(out, &value, 4);
                            } else {
                                const auto value = static_cast<std::int32_t>(
                                    sample * std::ldexp(1.0, format.wBitsPerSample - 1));
                                for (int byte = 0; byte < bytes; ++byte)
                                    out[byte] = static_cast<BYTE>(
                                        (static_cast<std::uint32_t>(value) &
                                         (~0u << (format.wBitsPerSample - valid_bits))) >>
                                        (byte * 8));
                            }
                        }
                    }
                };
                if (!capture) {
                    BYTE* data = nullptr;
                    check(render->GetBuffer(frames, &data));
                    fill(data, frames);
                    check(render->ReleaseBuffer(frames, 0));
                }
                if (!stop.stop_requested()) {
                    check(client->Start());
                    started = true;
                    detail(capture ? "Checking capture level."
                                   : "Playing a quiet one-second test beep.");
                }
                const auto began = std::chrono::steady_clock::now();
                while (started && !stop.stop_requested()) {
                    if (!capture &&
                        std::chrono::steady_clock::now() - began > std::chrono::seconds(3))
                        break;
                    const auto wait = WaitForSingleObject(event.value, 30);
                    if (wait == WAIT_FAILED)
                        check(HRESULT_FROM_WIN32(GetLastError()));
                    if (wait != WAIT_OBJECT_0)
                        continue;
                    if (capture) {
                        float peak = 0;
                        bool observed = false;
                        UINT32 available = 0;
                        check(input->GetNextPacketSize(&available));
                        unsigned packets = 0;
                        while (available && packets++ < 128 && !stop.stop_requested()) {
                            BYTE* data = nullptr;
                            UINT32 count = 0;
                            DWORD flags = 0;
                            check(input->GetBuffer(&data, &count, &flags, nullptr, nullptr));
                            observed = true;
                            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && count <= frames) {
                                for (std::size_t sample = 0;
                                     sample < static_cast<std::size_t>(count) * format.nChannels;
                                     ++sample) {
                                    const BYTE* at = data + sample * bytes;
                                    float value = 0;
                                    if (floating)
                                        std::memcpy(&value, at, 4);
                                    else {
                                        std::uint32_t packed = 0;
                                        for (int b = 0; b < bytes; ++b)
                                            packed |= static_cast<std::uint32_t>(at[b]) << (b * 8);
                                        if (bytes < 4 && (at[bytes - 1] & 128))
                                            packed |= ~((1u << (bytes * 8)) - 1u);
                                        value = static_cast<float>(
                                            static_cast<std::int32_t>(packed) /
                                            std::ldexp(1.0, format.wBitsPerSample - 1));
                                    }
                                    if (std::isfinite(value))
                                        peak = std::max(peak, std::abs(value));
                                }
                            }
                            check(input->ReleaseBuffer(count));
                            check(input->GetNextPacketSize(&available));
                        }
                        if (observed) {
                            std::lock_guard lock(mutex_);
                            state_.peak = std::min(1.f, peak);
                        }
                    } else {
                        UINT32 padding = 0;
                        check(client->GetCurrentPadding(&padding));
                        if (emitted >= format.nSamplesPerSec && padding == 0)
                            break;
                        if (padding > frames)
                            throw std::runtime_error("Invalid device-check padding.");
                        const UINT32 available = frames - padding;
                        if (available && emitted < format.nSamplesPerSec) {
                            BYTE* data = nullptr;
                            check(render->GetBuffer(available, &data));
                            fill(data, available);
                            check(render->ReleaseBuffer(available, 0));
                        }
                    }
                }
                detail(stop.stop_requested() ? "Device check cancelled."
                                             : "Test beep finished; confirm whether you heard it "
                                               "on the selected device.");
            } catch (const std::exception& error) {
                detail(error.what());
                std::lock_guard lock(mutex_);
                state_.failed = !stop.stop_requested();
            }
            if (started) {
                const auto stopped = client->Stop();
                if (FAILED(stopped)) {
                    try {
                        check(stopped);
                    } catch (const std::exception& error) {
                        detail(error.what());
                    }
                }
            }
        }
        CoUninitialize();
        {
            std::lock_guard lock(mutex_);
            state_.peak.reset();
            state_.active = false;
        }
        busy_ = false;
    }

  public:
    ~Session() override {
        cancel();
        if (worker_.joinable())
            worker_.join();
    }
    bool start(const std::string& id, const std::string& endpoint, bool capture) override {
        if (busy_.exchange(true))
            return false;
        if (worker_.joinable())
            worker_.join(); // Prior run has finished and released its clients.
        {
            std::lock_guard lock(mutex_);
            state_ = {true, capture, {}, id, "Opening selected device..."};
        }
        try {
            worker_ = std::jthread(
                [this, endpoint, capture](std::stop_token stop) { run(stop, endpoint, capture); });
        } catch (const std::exception&) {
            std::lock_guard lock(mutex_);
            state_.active = false;
            state_.detail = "Cannot start the device-check worker.";
            busy_ = false;
            return false;
        }
        return true;
    }
    void cancel() override {
        if (worker_.joinable())
            worker_.request_stop();
    }
    DiagnosticReading reading() const override {
        std::lock_guard lock(mutex_);
        return state_;
    }
};
} // namespace
std::unique_ptr<DiagnosticService> make_diagnostic_service() {
    return std::make_unique<Session>();
}
} // namespace audiohole::windows
