#include "access_negotiation.hpp"
#include "stream_pipeline.hpp"

#if defined(_WIN32) && defined(AUDIOHOLE_ENGINE_MINIAUDIO)
#include <atomic>
#include <cstring>
#include <miniaudio.h>
#include <stdexcept>
#include <windows.h>

namespace audiohole::engine::detail {
namespace {
struct LibraryFailure {
    Issue issue;
};

void checked(ma_result result, const char* stage) {
    if (result != MA_SUCCESS)
        throw LibraryFailure{
            {ErrorCode::BackendFailure, stage,
             std::string(stage) + " failed: miniaudio result " + std::to_string(result) + ".",
             std::to_string(result)}};
}
ma_format format(SampleFormat value) {
    switch (value) {
    case SampleFormat::Float32:
        return ma_format_f32;
    case SampleFormat::PcmS16:
        return ma_format_s16;
    case SampleFormat::PcmS32:
        return ma_format_s32;
    default:
        // The pinned research reproduces a packed-s24 shuffle defect. This
        // adapter intentionally rejects that component path, and ma_format_s32
        // must not masquerade as a 24-valid-bit container request.
        throw LibraryFailure{{ErrorCode::UnsupportedRequirement, "sample_format",
                              "This miniaudio prototype supports float32/pcm_s16/pcm_s32. "
                              "Use native WASAPI for packed or 24-in-32 requests."}};
    }
}
SampleFormat format(ma_format value) {
    switch (value) {
    case ma_format_f32:
        return SampleFormat::Float32;
    case ma_format_s16:
        return SampleFormat::PcmS16;
    case ma_format_s32:
        return SampleFormat::PcmS32;
    default:
        throw LibraryFailure{{ErrorCode::UnsupportedRequirement, "native_format",
                              "Native library format uses an unreviewed converter path."}};
    }
}

class MiniaudioSession final : public RouteSession {
  public:
    explicit MiniaudioSession(BoundRoute route) : route_(std::move(route)) {}
    ~MiniaudioSession() override {
        stop();
    }

    Result<bool> initialize(std::stop_token stop_token) {
        try {
            if (stop_token.stop_requested())
                return {std::nullopt, {{ErrorCode::Cancelled, "open", "Opening cancelled."}}};
            // Explicit WASAPI-only backend list: no silent WinMM, default-device,
            // or null-backend fallback. Separate devices avoid stock duplex's
            // blocking coupling and common callback sample-rate restriction.
            const ma_backend backend = ma_backend_wasapi;
            checked(ma_context_init(&backend, 1, nullptr, &context_), "ma_context_init");
            context_ready_ = true;
            const auto source = open_endpoint(capture_, capture_ready_, route_.source.endpoint_id,
                                              route_.request.source, true, stop_token);
            if (!source.ok())
                return {std::nullopt, source.issues};
            if (stop_token.stop_requested())
                return {std::nullopt, {{ErrorCode::Cancelled, "open", "Opening cancelled."}}};
            const auto destination =
                open_endpoint(playback_, playback_ready_, route_.destination.endpoint_id,
                              route_.request.destination, false, stop_token);
            if (!destination.ok())
                return {std::nullopt, destination.issues};
            configuration_.source = *source.value;
            configuration_.destination = *destination.value;
            configuration_.backend = BackendId::Miniaudio;
            configuration_.preferred_clock_device = route_.request.preferred_clock_device;
            configuration_.processing =
                "Validation float32 pipeline plus library native-format conversion";
            configuration_.synchronization =
                "Prototype occupancy PI; application linear adaptive SRC";
            configuration_.selection_reason =
                "Explicit miniaudio request; WASAPI-only device access";
            const auto validation = check_negotiated(route_.request, configuration_);
            if (!validation.ok())
                return {std::nullopt, validation.issues};
            pipeline_ = std::make_unique<StreamPipeline>(
                configuration_.source.client_format, configuration_.destination.client_format,
                route_.request.channel_map, false, queue_target_milliseconds(configuration_));
            configuration_.fifo_target_source_frames = pipeline_->target_frames();
            configuration_.fifo_capacity_source_frames = pipeline_->capacity_frames();
            return {true, {}};
        } catch (const LibraryFailure& failure) {
            return {std::nullopt, {failure.issue}};
        } catch (const std::exception& error) {
            return {std::nullopt, {{ErrorCode::BackendFailure, "miniaudio", error.what()}}};
        }
    }
    NegotiatedRoute configuration() const override {
        return configuration_;
    }
    Result<bool> start(std::stop_token stop_token) override {
        if (stop_token.stop_requested())
            return {std::nullopt, {{ErrorCode::Cancelled, "start", "Start cancelled."}}};
        try {
            notifications_enabled_.store(true, std::memory_order_release);
            checked(ma_device_start(&capture_), "ma_capture_start");
            checked(ma_device_start(&playback_), "ma_playback_start");
            return {true, {}};
        } catch (const LibraryFailure& failure) {
            return {std::nullopt, {failure.issue}};
        }
    }
    RouteObservation observe() override {
        auto result = pipeline_->observe();
        // The library callback does not expose native packet flags. Absence is
        // honest telemetry; a fabricated zero would suggest those flags passed.
        result.discontinuities.reset();
        result.startup_discontinuities.reset();
        result.timestamp_errors.reset();
        if (interrupted_.load(std::memory_order_acquire))
            result.issues.push_back({ErrorCode::BackendFailure, "miniaudio.notification",
                                     "Explicit endpoint stopped, was interrupted or rerouted."});
        return result;
    }
    void stop() noexcept override {
        // The lifecycle owner alone calls this. Mark shutdown before uninit so
        // normal stopped notifications cannot be mistaken for device failure.
        // uninit joins callbacks before the queue/context/session is destroyed.
        stopping_.store(true, std::memory_order_release);
        if (capture_ready_) {
            ma_device_uninit(&capture_);
            capture_ready_ = false;
        }
        if (playback_ready_) {
            ma_device_uninit(&playback_);
            playback_ready_ = false;
        }
        if (context_ready_) {
            ma_context_uninit(&context_);
            context_ready_ = false;
        }
    }

  private:
    BoundRoute route_;
    ma_context context_{};
    ma_device capture_{}, playback_{};
    bool context_ready_ = false, capture_ready_ = false, playback_ready_ = false;
    std::atomic<bool> stopping_{false}, interrupted_{false}, notifications_enabled_{false};
    NegotiatedRoute configuration_;
    std::unique_ptr<StreamPipeline> pipeline_;

    static void data(ma_device* device, void* output, const void* input, ma_uint32 frames) {
        auto* self = static_cast<MiniaudioSession*>(device->pUserData);
        if (!self->pipeline_)
            return;
        // Audio callbacks: only preallocated queue/sample processing and
        // lock-free counters. No control/JSON/GUI/I/O/logging/teardown here.
        if (device->type == ma_device_type_capture && input)
            (void)self->pipeline_->push(input, frames);
        if (device->type == ma_device_type_playback && output)
            self->pipeline_->render(output, frames);
    }
    static void notification(const ma_device_notification* event) {
        auto* self = static_cast<MiniaudioSession*>(event->pDevice->pUserData);
        // Stopped-device cleanup during mode negotiation is expected, not an
        // interruption of live routing. Enable notifications before the first
        // actual Start so a failure during activation is still observed.
        if (self->notifications_enabled_.load(std::memory_order_acquire) &&
            !self->stopping_.load(std::memory_order_acquire) &&
            (event->type == ma_device_notification_type_stopped ||
             event->type == ma_device_notification_type_rerouted ||
             event->type == ma_device_notification_type_interruption_began))
            self->interrupted_.store(true, std::memory_order_release);
    }
    Result<StreamConfiguration> open_endpoint(ma_device& device, bool& ready,
                                              const std::string& binding,
                                              const EndpointRequirements& required, bool input,
                                              std::stop_token stop) {
        std::optional<Issue> conflict;
        if (input && route_.request.shared_capture_required)
            conflict = Issue{ErrorCode::ResourceConflict, "ownership",
                             "This capture endpoint has multiple consumers; Exclusive ownership "
                             "is unavailable for this profile."};
        return negotiate_access(
            required, route_.request.exclusive_mode_preferred, stop,
            input ? "source" : "destination",
            [&](const EndpointRequirements& concrete) -> Result<StreamConfiguration> {
                try {
                    initialize_device(device, ready, binding, concrete, input);
                    return {describe(device, input, concrete), {}};
                } catch (const LibraryFailure& failure) {
                    return {std::nullopt, {failure.issue}};
                }
            },
            [&] {
                // ma_device_init owns its failure cleanup. Successful but
                // requirement-incompatible devices are uninitialized before a
                // retry. Both operations stay on the lifecycle worker; no data
                // callback can run until start() after negotiation completes.
                if (ready) {
                    ma_device_uninit(&device);
                    ready = false;
                }
                device = {};
            },
            conflict);
    }

    void initialize_device(ma_device& device, bool& ready, const std::string& binding,
                           const EndpointRequirements& required, bool input) {
        ma_device_id id{};
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, binding.data(),
                                               static_cast<int>(binding.size()), nullptr, 0);
        if (length < 1 || length >= 64)
            throw LibraryFailure{{ErrorCode::UnsupportedRequirement, "endpoint_id",
                                  "Endpoint ID does not fit miniaudio's native ID field."}};
        static_assert(sizeof(id.wasapi[0]) == sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, binding.data(),
                            static_cast<int>(binding.size()), reinterpret_cast<wchar_t*>(id.wasapi),
                            length);
        auto config =
            ma_device_config_init(input ? ma_device_type_capture : ma_device_type_playback);
        config.sampleRate = static_cast<ma_uint32>(required.sample_rate_hz.value_or(0));
        config.periodSizeInMilliseconds = static_cast<ma_uint32>(required.buffer_ms.value_or(0));
        config.noFixedSizedCallback = MA_TRUE;
        config.noClip = MA_TRUE;
        config.wasapi.noAutoStreamRouting = MA_TRUE;
        config.wasapi.usage = ma_wasapi_usage_pro_audio;
        config.dataCallback = data;
        config.notificationCallback = notification;
        config.pUserData = this;
        const auto share = required.mode.value_or(AccessMode::Shared) == AccessMode::Shared
                               ? ma_share_mode_shared
                               : ma_share_mode_exclusive;
        if (input) {
            config.capture.pDeviceID = &id;
            config.capture.format = format(required.sample_format.value_or(SampleFormat::Float32));
            config.capture.channels = static_cast<ma_uint32>(required.channel_count.value_or(0));
            config.capture.shareMode = share;
        } else {
            config.playback.pDeviceID = &id;
            config.playback.format = format(required.sample_format.value_or(SampleFormat::Float32));
            config.playback.channels = static_cast<ma_uint32>(required.channel_count.value_or(0));
            config.playback.shareMode = share;
        }
        checked(ma_device_init(&context_, &config, &device),
                input ? "ma_capture_init" : "ma_playback_init");
        ready = true;
    }
    static StreamConfiguration describe(const ma_device& device, bool input,
                                        const EndpointRequirements& required) {
        StreamConfiguration result;
        result.client_format = {
            static_cast<int>(device.sampleRate),
            static_cast<int>(input ? device.capture.channels : device.playback.channels),
            format(input ? device.capture.format : device.playback.format)};
        // Reject packed native conversion too; do not assume a requested f32
        // client format makes the known upstream packed-s24 path unreachable.
        result.native_format = StreamFormat{
            static_cast<int>(input ? device.capture.internalSampleRate
                                   : device.playback.internalSampleRate),
            static_cast<int>(input ? device.capture.internalChannels
                                   : device.playback.internalChannels),
            format(input ? device.capture.internalFormat : device.playback.internalFormat)};
        result.mode =
            (input ? device.capture.shareMode : device.playback.shareMode) == ma_share_mode_shared
                ? AccessMode::Shared
                : AccessMode::Exclusive;
        result.allocated_buffer_frames = input ? device.wasapi.actualBufferSizeInFramesCapture
                                               : device.wasapi.actualBufferSizeInFramesPlayback;
        result.timing_sample_rate_hz = result.native_format->sample_rate_hz;
        result.period_frames = input ? device.capture.internalPeriodSizeInFrames
                                     : device.playback.internalPeriodSizeInFrames;
        result.applied_buffer_request_ms = required.buffer_ms;
        // Both period/allocation counts use the INTERNAL rate, reported above.
        return result;
    }
};

class MiniaudioAdapter final : public BackendAdapter {
  public:
    BackendDescriptor descriptor() const override {
        return {BackendId::Miniaudio, true,
                "miniaudio 0.11.25 WASAPI I/O; restricted converter paths"};
    }
    Result<std::unique_ptr<RouteSession>> open(const BoundRoute& route,
                                               std::stop_token stop) override {
        auto session = std::make_unique<MiniaudioSession>(route);
        const auto result = session->initialize(stop);
        if (!result.ok())
            return {std::nullopt, result.issues};
        return {std::move(session), {}};
    }
};
} // namespace
std::unique_ptr<BackendAdapter> make_miniaudio_adapter() {
    return std::make_unique<MiniaudioAdapter>();
}
} // namespace audiohole::engine::detail
#else
namespace audiohole::engine::detail {
std::unique_ptr<BackendAdapter> make_miniaudio_adapter() {
    return {};
}
} // namespace audiohole::engine::detail
#endif
