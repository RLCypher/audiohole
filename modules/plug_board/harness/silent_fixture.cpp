#include "support.hpp"

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace {
using namespace audiohole::plug_board::windows_detail;

// Every stream belongs to this disposable process, requests a normal shared
// default endpoint and runs silence. This produces Windows Active sessions
// without playing a beep, recording input or using AudioHole's routing engine.
struct SilentStream {
    ComPointer<IAudioClient> client;
    ComPointer<IAudioRenderClient> render;
    Handle ready;
    UINT32 capacity = 0;

    SilentStream() = default;
    SilentStream(const SilentStream&) = delete;
    SilentStream& operator=(const SilentStream&) = delete;
    SilentStream(SilentStream&&) noexcept = default;
    ~SilentStream() {
        if (client) {
            client->Stop();
        }
    }

    void open(IMMDeviceEnumerator* enumerator, ERole role) {
        ComPointer<IMMDevice> device;
        require(enumerator->GetDefaultAudioEndpoint(eRender, role, device.put()),
                "Fixture default endpoint");
        require(device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, client.put_void()),
                "Fixture audio client");
        WAVEFORMATEX* raw_format = nullptr;
        require(client->GetMixFormat(&raw_format), "Fixture shared mix format");
        std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> format(raw_format, CoTaskMemFree);
        // This 100 ms allocation request is a conservative fixture choice, not
        // a latency measurement or a production AudioHole buffering policy.
        constexpr REFERENCE_TIME buffer_request_100ns = 1000000;
        require(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   buffer_request_100ns, 0, format.get(), nullptr),
                "Initialize silent fixture stream");
        ready.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        if (!ready) {
            throw std::runtime_error("Could not create fixture audio event.");
        }
        require(client->SetEventHandle(ready.get()), "Set fixture audio event");
        require(client->GetService(IID_IAudioRenderClient, render.put_void()),
                "Get fixture render service");
        require(client->GetBufferSize(&capacity), "Get fixture buffer allocation");
        BYTE* buffer = nullptr;
        require(render->GetBuffer(capacity, &buffer), "Prime fixture silence");
        require(render->ReleaseBuffer(capacity, AUDCLNT_BUFFERFLAGS_SILENT),
                "Commit fixture silence");
        require(client->Start(), "Start silent fixture stream");
    }

    HRESULT fill_silence() {
        // The steady-state fill path performs no allocation, logging, JSON,
        // filesystem work, locks or GUI calls. Windows owns the borrowed buffer;
        // ReleaseBuffer completes each successful GetBuffer on this same thread.
        UINT32 padding = 0;
        auto result = client->GetCurrentPadding(&padding);
        if (FAILED(result) || padding > capacity) {
            return FAILED(result) ? result : E_UNEXPECTED;
        }
        const auto frames = capacity - padding;
        if (frames == 0) {
            return S_OK;
        }
        BYTE* buffer = nullptr;
        result = render->GetBuffer(frames, &buffer);
        if (FAILED(result)) {
            return result;
        }
        return render->ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
    }
};

class SilentAudio {
  public:
    ~SilentAudio() {
        stop();
    }
    void start() {
        stop();
        failure_.store(S_OK);
        stop_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!stop_) {
            throw std::runtime_error("Could not create fixture stop event.");
        }
        std::promise<std::string> startup;
        auto ready = startup.get_future();
        worker_ = std::thread([this, startup = std::move(startup)]() mutable {
            bool announced = false;
            try {
                Apartment apartment;
                require(apartment.result(), "Initialize fixture audio MTA");
                ComPointer<IMMDeviceEnumerator> enumerator;
                require(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                         __uuidof(IMMDeviceEnumerator), enumerator.put_void()),
                        "Fixture enumerator");
                std::vector<SilentStream> streams;
                streams.reserve(3);
                for (const auto role : {eConsole, eMultimedia, eCommunications}) {
                    streams.emplace_back();
                    streams.back().open(enumerator.get(), role);
                }
                HANDLE events[]{stop_.get(), streams[0].ready.get(), streams[1].ready.get(),
                                streams[2].ready.get()};
                startup.set_value({});
                announced = true;
                for (;;) {
                    const auto event = WaitForMultipleObjects(4, events, FALSE, INFINITE);
                    if (event == WAIT_OBJECT_0) {
                        break;
                    }
                    if (event < WAIT_OBJECT_0 + 1 || event > WAIT_OBJECT_0 + 3) {
                        failure_.store(E_UNEXPECTED);
                        break;
                    }
                    const auto result = streams[event - WAIT_OBJECT_0 - 1].fill_silence();
                    if (FAILED(result)) {
                        failure_.store(result);
                        break;
                    }
                }
                // Stream Stop and all COM releases occur on their owning worker
                // before its apartment disappears. No detached audio thread remains.
            } catch (const std::exception& error) {
                failure_.store(E_FAIL);
                if (!announced) {
                    startup.set_value(error.what());
                }
            }
        });
        const auto error = ready.get();
        if (!error.empty()) {
            stop();
            throw std::runtime_error(error);
        }
    }
    void stop() {
        if (worker_.joinable()) {
            SetEvent(stop_.get());
            worker_.join();
        }
        stop_.reset();
    }
    HRESULT failure() const {
        return failure_.load();
    }

  private:
    Handle stop_;
    std::thread worker_;
    std::atomic<HRESULT> failure_{S_OK};
};

} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        const bool metadata_only = argc == 2 && std::wstring(argv[1]) == L"--metadata-only";
        if (argc > 1 && !metadata_only) {
            throw std::runtime_error("This disposable fixture is launched by plug_board_probe.");
        }
        SilentAudio audio;
        if (!metadata_only) {
            audio.start();
        }
        std::cout << "ready\n" << std::flush;
        std::string command;
        while (std::getline(std::cin, command)) {
            if (command == "stop") {
                break;
            }
            if (command != "recreate" || metadata_only) {
                throw std::runtime_error("Unexpected private fixture command.");
            }
            audio.start();
            std::cout << "recreated\n" << std::flush;
        }
        audio.stop();
        require(audio.failure(), "Silent fixture render loop");
        return 0;
    } catch (const std::exception& error) {
        std::cout << "error " << error.what() << '\n' << std::flush;
        return 1;
    }
}
