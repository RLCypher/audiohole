#include "model/routing_service.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>

namespace audiohole {
namespace {
using Json = nlohmann::json;
const char* issue_code(engine::ErrorCode code) {
    switch (code) {
    case engine::ErrorCode::InvalidDocument:
        return "InvalidDocument";
    case engine::ErrorCode::UnsupportedVersion:
        return "UnsupportedVersion";
    case engine::ErrorCode::InvalidConfiguration:
        return "InvalidConfiguration";
    case engine::ErrorCode::UnsupportedBackend:
        return "UnsupportedBackend";
    case engine::ErrorCode::BackendUnavailable:
        return "BackendUnavailable";
    case engine::ErrorCode::UnsupportedRequirement:
        return "UnsupportedRequirement";
    case engine::ErrorCode::UnresolvedEndpoint:
        return "UnresolvedEndpoint";
    case engine::ErrorCode::AmbiguousEndpoint:
        return "AmbiguousEndpoint";
    case engine::ErrorCode::UnavailableEndpoint:
        return "UnavailableEndpoint";
    case engine::ErrorCode::DirectionMismatch:
        return "DirectionMismatch";
    case engine::ErrorCode::ResourceConflict:
        return "ResourceConflict";
    case engine::ErrorCode::Busy:
        return "Busy";
    case engine::ErrorCode::Cancelled:
        return "Cancelled";
    case engine::ErrorCode::BackendFailure:
        return "BackendFailure";
    case engine::ErrorCode::ContractViolation:
        return "ContractViolation";
    }
    return "UnknownIssue";
}
Json preferences_json(const EndpointPreferences& p) {
    Json out = Json::object();
    if (p.sample_rate_hz)
        out["sample_rate_hz"] = *p.sample_rate_hz;
    if (p.channel_count)
        out["channel_count"] = *p.channel_count;
    if (p.sample_format)
        out["sample_format"] = *p.sample_format;
    if (p.mode)
        out["mode"] = *p.mode;
    if (p.buffer_ms)
        out["buffer_ms"] = *p.buffer_ms;
    if (p.period_ms)
        out["period_ms"] = *p.period_ms;
    if (p.render_ahead_ms)
        out["render_ahead_ms"] = *p.render_ahead_ms;
    return out;
}
Json direction_json(const Direction& d) {
    const auto& p = d.settings.preferences;
    auto map = Json::array();
    for (const auto& channel : p.channel_map)
        map.push_back({{"source", channel.source}, {"destination", channel.destination}});
    return {{"source", d.source},
            {"destination", d.destination},
            {"source_name", d.source_name},
            {"destination_name", d.destination_name},
            {"advanced_tuning", d.advanced_tuning},
            {"exclusive_mode_preferred", d.exclusive_mode_preferred},
            {"settings",
             {{"preferences",
               {{"queue_target_ms", p.queue_target_ms ? Json(*p.queue_target_ms) : Json(nullptr)},
                {"common", preferences_json(p.common)},
                {"source", preferences_json(p.source)},
                {"destination", preferences_json(p.destination)},
                {"channel_map", map},
                {"preferred_clock_device", p.preferred_clock_device}}}}}};
}
class Service final : public RoutingService {
  public:
    explicit Service(RuntimeFactory factory)
        : worker_(
              [this, factory = std::move(factory)](std::stop_token stop) { run(stop, factory); }) {}
    ~Service() override {
        worker_.request_stop();
        wake_.notify_all();
        // The GUI's asynchronous exit gate waits for engine Stopped before
        // destroying this owner. Tests also release all native/fake sessions.
        worker_.join();
    }
    std::uint64_t start(engine::Profile profile, engine::BindingSnapshot bindings) override {
        std::lock_guard lock(mutex_);
        if (!reading_.ready || reading_.pending ||
            (reading_.state &&
             (reading_.state->armed || reading_.state->state == engine::State::Stopping)))
            return 0;
        const auto generation = ++reading_.generation;
        reading_.pending = true;
        reading_.admission_issues.clear();
        command_ = Command{generation, false, std::move(profile), std::move(bindings), workers_};
        wake_.notify_all();
        return generation;
    }
    std::uint64_t stop() override {
        std::lock_guard lock(mutex_);
        const auto generation = ++reading_.generation;
        bindings_.reset();
        if (!reading_.ready) {
            reading_.pending = false;
            reading_.admission_issues.clear();
            command_.reset();
            reading_.state = std::make_shared<const engine::RuntimeSnapshot>();
            ++reading_.revision;
            return generation;
        }
        reading_.pending = true;
        command_ = Command{generation, true, {}, {}, {}};
        wake_.notify_all();
        return generation;
    }
    bool update_bindings(engine::BindingSnapshot bindings, bool resume_ready) override {
        std::lock_guard lock(mutex_);
        if (!reading_.ready || reading_.pending || !reading_.state || !reading_.state->armed ||
            reading_.state->state == engine::State::Stopping)
            return false;
        bindings_ = std::pair{std::move(bindings), resume_ready};
        wake_.notify_all();
        return true;
    }
    void configure_workers(engine::WorkerSettings workers) override {
        std::lock_guard lock(mutex_);
        workers_ = workers;
    }
    RoutingReading reading() const override {
        std::lock_guard lock(mutex_);
        return reading_;
    }

  private:
    struct Command {
        std::uint64_t generation;
        bool stop;
        engine::Profile profile;
        engine::BindingSnapshot bindings;
        engine::WorkerSettings workers;
    };
    engine::WorkerSettings workers_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    RoutingReading reading_;
    std::optional<Command> command_;
    std::optional<std::pair<engine::BindingSnapshot, bool>> bindings_;
    std::jthread worker_; // Last member: every referenced field is initialized first.
    void run(std::stop_token stop, const RuntimeFactory& factory) noexcept {
        try {
            auto runtime =
                factory ? factory() : std::make_unique<engine::Runtime>(engine::LatencyPreset::Low);
            if (!runtime)
                throw std::runtime_error("Engine factory returned no runtime.");
            {
                std::lock_guard lock(mutex_);
                reading_.backends = runtime->backends();
                reading_.state =
                    std::make_shared<const engine::RuntimeSnapshot>(runtime->snapshot());
                reading_.ready = true;
                ++reading_.revision;
            }
            std::uint64_t generation = 0;
            while (!stop.stop_requested()) {
                std::optional<Command> command;
                std::optional<std::pair<engine::BindingSnapshot, bool>> bindings;
                {
                    std::unique_lock lock(mutex_);
                    wake_.wait_for(lock, std::chrono::milliseconds(50), [&] {
                        return command_.has_value() || bindings_.has_value() ||
                               stop.stop_requested();
                    });
                    command.swap(command_);
                    bindings.swap(bindings_);
                }
                if (stop.stop_requested())
                    break;
                std::vector<engine::Issue> issues;
                if (command) {
                    generation = command->generation;
                    if (command->stop)
                        runtime->stop();
                    else {
                        if (!factory)
                            runtime = std::make_unique<engine::Runtime>(engine::LatencyPreset::Low,
                                                                        command->workers);
                        auto accepted = runtime->start(command->profile, command->bindings);
                        if (!accepted.ok())
                            issues = std::move(accepted.issues);
                    }
                }
                if (!command && bindings)
                    runtime->update_bindings(bindings->first, bindings->second);
                auto state = std::make_shared<const engine::RuntimeSnapshot>(runtime->snapshot());
                std::lock_guard lock(mutex_);
                if (generation != reading_.generation)
                    continue;
                reading_.state = std::move(state);
                if (command) {
                    reading_.admission_issues = std::move(issues);
                    reading_.pending = false;
                }
                ++reading_.revision;
            }
            runtime->stop();
            // Destruction and all potentially blocking driver joins stay on
            // this control thread; no detached ownership or forced termination.
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex_);
            reading_.ready = false;
            reading_.pending = false;
            reading_.admission_issues = {
                {engine::ErrorCode::BackendFailure, "engine", error.what()}};
            ++reading_.revision;
        }
    }
};
} // namespace
std::unique_ptr<RoutingService> make_routing_service(RuntimeFactory factory) {
    return std::make_unique<Service>(std::move(factory));
}
engine::Result<engine::Profile> routing_profile(const Profile& p) {
    auto holes = Json::array();
    for (const auto& h : p.holes)
        holes.push_back({{"id", h.id},
                         {"display_name", h.display_name},
                         {"notes", h.notes},
                         {"enabled", h.enabled},
                         {"duplex", h.duplex},
                         {"routing", h.routing_engine},
                         {"forward", direction_json(h.forward)},
                         {"reverse", direction_json(h.reverse)}});
    // A transient operational projection only. Existing persistence retains
    // original JSON/unknown fields and is never rewritten from this value.
    return engine::parse_profile(Json{
        {"format", "audiohole-profile"},
        {"schema_version", 4},
        {"profile",
         {{"id", p.id}, {"display_name", p.display_name}, {"notes", p.notes}, {"holes", holes}}}}
                                     .dump());
}
engine::BindingSnapshot routing_bindings(const Snapshot& s) {
    engine::BindingSnapshot out;
    out.revision = s.scanner.inventory_revision;
    for (const auto& device : s.endpoints) {
        engine::LocalBinding binding;
        binding.logical_id = device.id;
        binding.direction =
            device.capture ? engine::EndpointDirection::Capture : engine::EndpointDirection::Render;
        binding.state =
            device.availability == Availability::Ambiguous     ? engine::BindingState::Ambiguous
            : device.availability == Availability::Available   ? engine::BindingState::Available
            : device.availability == Availability::Unavailable ? engine::BindingState::Unavailable
                                                               : engine::BindingState::Unknown;
        if (device.inspection)
            binding.endpoint_id = device.inspection->diagnostic_endpoint_id;
        if (binding.endpoint_id.empty() && binding.state == engine::BindingState::Available)
            binding.state = engine::BindingState::Unknown;
        // Carry advisory scanner tuples with their exact binding, not inside a
        // profile or as forced preferences. Unknown/negative/contradictory
        // evidence is excluded; WASAPI still queries current support at Start.
        // Bound the projection and the later native format search separately.
        for (const auto& capability : device.capabilities) {
            if (binding.format_hints.size() >= 128)
                break;
            if (capability.support != FormatSupport::Supported || capability.mode != "Exclusive" ||
                std::any_of(device.capabilities.begin(), device.capabilities.end(),
                            [&](const auto& other) {
                                return other.format == capability.format &&
                                       other.mode == capability.mode &&
                                       other.support == FormatSupport::Unsupported;
                            }))
                continue;
            const auto& f = capability.format;
            const auto representation = f.representation == "PCM16" || f.representation == "int16"
                                            ? "pcm_s16"
                                            : f.representation;
            std::optional<engine::SampleFormat> sample;
            for (const auto encoding : {engine::SampleFormat::Float32, engine::SampleFormat::PcmS16,
                                        engine::SampleFormat::PcmS24Packed,
                                        engine::SampleFormat::PcmS24In32,
                                        engine::SampleFormat::PcmS32})
                if (representation == engine::name(encoding))
                    sample = encoding;
            if (!sample)
                continue;
            engine::FormatHint hint{{f.rate, f.channels, *sample},
                                    capability.mode == "Exclusive" ? engine::AccessMode::Exclusive
                                                                    : engine::AccessMode::Shared,
                                    f.wave ? f.wave->channel_mask : std::nullopt};
            const auto geometry = engine::pcm_geometry(hint.format);
            if (!geometry.ok() ||
                (f.wave && ((f.wave->container_bits > 0 &&
                             f.wave->container_bits != geometry.value->container_bits) ||
                            (f.wave->valid_bits > 0 &&
                             f.wave->valid_bits != geometry.value->valid_bits))) ||
                (hint.channel_mask && *hint.channel_mask &&
                 std::popcount(*hint.channel_mask) != f.channels))
                continue;
            if (std::find(binding.format_hints.begin(), binding.format_hints.end(), hint) ==
                binding.format_hints.end())
                binding.format_hints.push_back(std::move(hint));
        }
        out.endpoints.push_back(std::move(binding));
    }
    return out;
}
State routing_state(engine::State state) {
    switch (state) {
    case engine::State::Stopped:
        return State::Stopped;
    case engine::State::Ready:
        return State::Ready;
    case engine::State::Starting:
        return State::Starting;
    case engine::State::Running:
        return State::Running;
    case engine::State::Degraded:
        return State::Degraded;
    case engine::State::Recovering:
        return State::Recovering;
    case engine::State::Failed:
        return State::Failed;
    case engine::State::Stopping:
        return State::Stopping;
    }
    return State::Failed;
}
std::string routing_issues(const std::vector<engine::Issue>& issues) {
    std::string out;
    for (const auto& issue : issues) {
        if (!out.empty())
            out += "\n";
        out +=
            "[" + std::string(issue_code(issue.code)) + "] " + issue.location + ": " + issue.detail;
        if (!issue.native_code.empty())
            out += " [" + issue.native_code + "]";
    }
    return out;
}
std::string routing_progress(const Snapshot& state) {
    const auto* intent =
        state.routing_intent ? state.routing_intent.get() : profile(state, state.active_profile);
    if (!intent || !state.armed)
        return {};
    unsigned total = 0, running = 0;
    for (const auto& hole : intent->holes) {
        if (!hole.enabled)
            continue;
        ++total;
        const auto path_running = [&](bool reverse) {
            return std::any_of(state.paths.begin(), state.paths.end(), [&](const auto& p) {
                return p.hole_id == hole.id && p.reverse == reverse && p.state == State::Running;
            });
        };
        if (path_running(false) && (!hole.duplex || path_running(true)))
            ++running;
    }
    return std::to_string(running) + "/" + std::to_string(total) + " running";
}
std::string routing_diagnostics(const engine::RuntimeSnapshot& state, const Profile* profile) {
    std::ostringstream out;
    out << "Engine 0.6 | Automatic native latency policy: Low\n";
    auto format = [](const engine::StreamFormat& f) {
        return std::to_string(f.sample_rate_hz) + " Hz / " + std::to_string(f.channel_count) +
               " ch / " + engine::name(f.sample_format);
    };
    for (const auto& path : state.paths) {
        std::string title = path.hole_id;
        if (profile)
            for (const auto& hole : profile->holes)
                if (hole.id == path.hole_id)
                    title = hole.display_name;
        out << title
            << (path.direction == engine::DirectionId::Reverse ? " / Return: " : " / Forward: ")
            << engine::name(path.state) << '\n';
        if (profile)
            for (const auto& hole : profile->holes)
                if (hole.id == path.hole_id) {
                    const auto& direction = path.direction == engine::DirectionId::Reverse
                                                ? hole.reverse
                                                : hole.forward;
                    out << "Requested engine: " << hole.routing_engine
                        << " | Advanced tuning: " << (direction.advanced_tuning ? "enabled" : "off")
                        << '\n';
                    for (const auto& [label, overrides] :
                         {std::pair{"Capture request", &direction.settings.preferences.source},
                          std::pair{"Playback request",
                                    &direction.settings.preferences.destination}}) {
                        const auto p = direction.advanced_tuning
                                           ? inherit_preferences(
                                                 direction.settings.preferences.common, *overrides)
                                           : EndpointPreferences{};
                        out << label << ": Rate "
                            << (p.sample_rate_hz ? std::to_string(*p.sample_rate_hz) : "Automatic")
                            << " | Channels "
                            << (p.channel_count ? std::to_string(*p.channel_count) : "Automatic")
                            << " | Format " << p.sample_format.value_or("Automatic") << " | Mode "
                            << p.mode.value_or("Automatic") << " | Buffer "
                            << (p.buffer_ms ? std::to_string(*p.buffer_ms) + " ms" : "Automatic")
                            << " | Exclusive preferred: "
                            << (direction.exclusive_mode_preferred ? "yes" : "no") << '\n';
                    }
                }
        if (path.negotiated) {
            const auto& n = *path.negotiated;
            out << "Engine: " << engine::name(n.backend) << " | " << n.selection_reason << '\n';
            auto stream = [&](const char* label, const engine::StreamConfiguration& s) {
                out << label << ": " << format(s.client_format) << " | " << engine::name(s.mode)
                    << " | Policy: " << engine::name(s.access_policy);
                out << " | Requested: "
                    << (s.requested_mode ? engine::name(*s.requested_mode) : "Automatic")
                    << " | Exclusive preferred: " << (s.exclusive_mode_preferred ? "yes" : "no");
                if (s.allocated_buffer_frames) {
                    out << " | Allocated buffer: " << *s.allocated_buffer_frames << " frames";
                    if (s.timing_sample_rate_hz && *s.timing_sample_rate_hz > 0)
                        out << " ("
                            << 1000.0 * *s.allocated_buffer_frames / *s.timing_sample_rate_hz
                            << " ms)";
                }
                if (s.period_frames)
                    out << " | Period: " << *s.period_frames << " frames";
                if (s.applied_buffer_request_ms) {
                    out << " | Buffer request supplied: " << *s.applied_buffer_request_ms << " ms";
                    if (s.allocated_buffer_frames && s.timing_sample_rate_hz)
                        out << " | Capacity adjustment: "
                            << 1000.0 * *s.allocated_buffer_frames / *s.timing_sample_rate_hz -
                                   *s.applied_buffer_request_ms
                            << " ms";
                }
                if (s.applied_period_request_ms)
                    out << " | Processing interval requested: " << *s.applied_period_request_ms
                        << " ms";
                if (s.applied_render_ahead_request_ms)
                    out << " | Playback lead requested: " << *s.applied_render_ahead_request_ms
                        << " ms";
                if (s.render_ahead_target_frames && s.timing_sample_rate_hz)
                    out << " | Playback lead: "
                        << 1000.0 * *s.render_ahead_target_frames / *s.timing_sample_rate_hz
                        << " ms";
                if (s.access_fell_back)
                    out << " | Preferred Exclusive fell back to Shared";
                if (!s.format_selection.empty())
                    out << " | Format selection: " << s.format_selection;
                out << '\n';
                for (const auto& attempt : s.access_attempts) {
                    out << "  " << engine::name(attempt.mode) << ": "
                        << (attempt.succeeded ? "accepted" : "refused")
                        << (attempt.native_open_attempted ? " (native open attempted)"
                                                          : " (ownership check; no native open)")
                        << '\n';
                    if (!attempt.issues.empty())
                        out << "  " << routing_issues(attempt.issues) << '\n';
                }
            };
            stream("Capture", n.source);
            stream("Playback", n.destination);
            if (n.requested_queue_target_ms)
                out << "Routing queue requested: " << *n.requested_queue_target_ms << " ms\n";
            if (n.fifo_target_source_frames)
                out << "Routing queue target: "
                    << 1000.0 * *n.fifo_target_source_frames / n.source.client_format.sample_rate_hz
                    << " ms\n";
            out << "Processing: " << n.processing << " | Synchronization: " << n.synchronization
                << '\n';
        }
        const auto& o = path.observation;
        if (!o.worker_scheduling.empty())
            out << "Audio worker: " << o.worker_scheduling << '\n';
        auto counter = [&](const char* label, const auto& value) {
            if (value)
                out << label << ": " << *value << " | ";
        };
        counter("Captured frames", o.captured_frames);
        counter("Rendered frames", o.rendered_frames);
        counter("Transferred frames", o.transferred_frames);
        counter("FIFO queued frames", o.queued_frames);
        counter("Native capture wakes", o.native_capture_wakes);
        counter("Native render wakes", o.native_render_wakes);
        counter("Native wait timeouts", o.native_wait_timeouts);
        counter("Capture waits", o.render_capture_waits);
        counter("Capture wait timeouts", o.render_capture_wait_timeouts);
        counter("Underruns", o.underruns);
        counter("Overruns", o.overruns);
        counter("Correction ppm", o.correction_ppm);
        counter("Progress age (ms)", path.progress_age_ms);
        out << '\n';
        if (!path.issues.empty())
            out << routing_issues(path.issues) << '\n';
        if (!o.issues.empty())
            out << routing_issues(o.issues) << '\n';
        out << '\n';
    }
    return out.str();
}
} // namespace audiohole
