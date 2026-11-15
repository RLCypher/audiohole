#pragma once
#include "model/configuration_controller.h"
#include <atomic>
#include <chrono>
#include <thread>

// Software-only fixtures for contract tests and the explicitly requested
// --engine-smoke process. Ordinary application and Tutorial never use these.
namespace audiohole::testing_engine {
struct Control {
    std::atomic<int> opens{0}, releases{0};
    std::atomic<bool> block_open{false}, block_cleanup{false}, cleaning{false};
    bool fail_return = false, refuse_exclusive = false;
};
class Session final : public engine::RouteSession {
  public:
    Session(std::shared_ptr<Control> control, engine::BoundRoute route, engine::BackendId backend)
        : control_(std::move(control)), route_(std::move(route)), backend_(backend) {}
    ~Session() override {
        stop();
    }
    engine::NegotiatedRoute configuration() const override {
        engine::NegotiatedRoute result;
        result.backend = backend_;
        auto stream = [&](const engine::EndpointRequirements& request) {
            engine::StreamConfiguration value;
            value.client_format = {request.sample_rate_hz.value_or(48000),
                                   request.channel_count.value_or(2),
                                   request.sample_format.value_or(engine::SampleFormat::Float32)};
            value.mode = request.mode.value_or(route_.request.exclusive_mode_preferred
                                                   ? engine::AccessMode::Exclusive
                                                   : engine::AccessMode::Shared);
            value.requested_mode = request.mode;
            value.exclusive_mode_preferred = route_.request.exclusive_mode_preferred;
            value.access_policy =
                engine::access_policy(request, route_.request.exclusive_mode_preferred);
            value.access_attempts = {{value.mode, true, true, {}}};
            value.applied_buffer_request_ms = request.buffer_ms;
            value.timing_sample_rate_hz = value.client_format.sample_rate_hz;
            const auto requested = (std::uint64_t(value.client_format.sample_rate_hz) *
                                        request.buffer_ms.value_or(10) +
                                    999) /
                                   1000;
            value.allocated_buffer_frames =
                static_cast<std::uint32_t>((requested + 479) / 480 * 480);
            value.period_frames = 240;
            return value;
        };
        result.source = stream(route_.request.source);
        result.destination = stream(route_.request.destination);
        result.processing = "Software fixture; no audio";
        result.selection_reason = "Explicit test adapter";
        return result;
    }
    engine::Result<bool> start(std::stop_token stop) override {
        started_ = !stop.stop_requested();
        return {started_, {}};
    }
    engine::RouteObservation observe() override {
        engine::RouteObservation out;
        if (started_)
            frames_ += 480;
        out.captured_frames = out.rendered_frames = out.transferred_frames = frames_;
        out.underruns = out.overruns = 0;
        return out;
    }
    void stop() noexcept override {
        if (released_)
            return;
        released_ = true;
        control_->cleaning = true;
        while (control_->block_cleanup)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++control_->releases;
    }

  private:
    std::shared_ptr<Control> control_;
    engine::BoundRoute route_;
    engine::BackendId backend_;
    bool started_ = false, released_ = false;
    std::uint64_t frames_ = 0;
};
class Adapter final : public engine::BackendAdapter {
  public:
    explicit Adapter(std::shared_ptr<Control> control,
                     engine::BackendId backend = engine::BackendId::Wasapi)
        : control_(std::move(control)), backend_(backend) {}
    engine::BackendDescriptor descriptor() const override {
        return {backend_, true, "Software contract fixture; no audio"};
    }
    engine::Result<std::unique_ptr<engine::RouteSession>> open(const engine::BoundRoute& request,
                                                               std::stop_token stop) override {
        ++control_->opens;
        while (control_->block_open && !stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (stop.stop_requested())
            return {std::nullopt, {{engine::ErrorCode::Cancelled, "fixture", "Start cancelled"}}};
        if ((control_->fail_return && request.request.direction == engine::DirectionId::Reverse) ||
            (control_->refuse_exclusive &&
             request.request.destination.mode == engine::AccessMode::Exclusive))
            return {std::nullopt,
                    {{engine::ErrorCode::BackendFailure, "fixture/open", "Requested mode refused",
                      "TEST-EXCLUSIVE"}}};
        return {std::make_unique<Session>(control_, request, backend_), {}};
    }

  private:
    std::shared_ptr<Control> control_;
    engine::BackendId backend_;
};
inline Profile profile() {
    Profile p;
    p.id = "engine-profile";
    p.display_name = "Engine contract fixture";
    p.notes = "Notes stay saved; RØDE";
    Hole h;
    h.id = "engine-hole";
    h.display_name = "Two-way fixture";
    h.duplex = true;
    h.forward.source = "capture-forward";
    h.forward.destination = "render-forward";
    h.reverse.source = "capture-return";
    h.reverse.destination = "render-return";
    p.holes.push_back(h);
    return p;
}
class Store final : public ConfigurationPersistence {
  public:
    ConfigurationRecords records;
    int saves = 0;
    bool reject_save = false;
    Store() {
        records.profiles.push_back(profile());
        for (const auto& id :
             {"capture-forward", "render-forward", "capture-return", "render-return"}) {
            Endpoint e;
            e.id = e.display_name = e.windows_name = id;
            e.capture = std::string(id).starts_with("capture");
            auto info = std::make_shared<DeviceInspection>();
            info->diagnostic_endpoint_id = std::string("local-") + id;
            e.inspection = info;
            records.devices.push_back(std::move(e));
        }
    }
    Result load(ConfigurationRecords& out) override {
        out = records;
        return {true, "Loaded software fixture"};
    }
    Result save(const ConfigurationRecords& in) override {
        if (reject_save)
            return {false, "Injected profile write failure"};
        records = in;
        ++saves;
        return {true, "Saved software fixture"};
    }
};
inline std::unique_ptr<RoutingService> service(std::shared_ptr<Control> control) {
    return make_routing_service([control] {
        std::vector<std::unique_ptr<engine::BackendAdapter>> adapters;
        adapters.push_back(std::make_unique<Adapter>(control));
        return std::make_unique<engine::Runtime>(std::move(adapters));
    });
}
} // namespace audiohole::testing_engine
