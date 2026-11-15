#pragma once

#include "profile.hpp"
#include <memory>
#include <stop_token>

namespace audiohole::engine {

// Host-selected validation tuning, deliberately outside the profile schema.
// Baseline preserves 0.2 scheduling. Low requests smaller automatic buffers
// from native WASAPI only; it never weakens explicit profile requirements.
enum class LatencyPreset { Baseline, Low };
enum class SchedulingTask { Automatic, Audio, ProAudio };
enum class AudioPriority { Automatic, Normal, High };
struct WorkerSettings {
    SchedulingTask scheduling = SchedulingTask::Automatic;
    AudioPriority priority = AudioPriority::Automatic;
    bool operator==(const WorkerSettings&) const = default;
};

// Descriptor for an actual adapter implementation. Merely recognizing its JSON
// selector does not mean this build implements or has validated that adapter.
struct BackendDescriptor {
    BackendId id = BackendId::Wasapi;
    bool available = false;
    std::string detail;
};

// Automatic prefers the native comparison baseline, then miniaudio, and never
// chooses Experimental. Explicit selection never substitutes another adapter.
// This checks registration only, not device/format support or runtime health.
Result<BackendId> select_backend(BackendId requested,
                                 const std::vector<BackendDescriptor>& available);

enum class AccessPolicy { AutomaticShared, SharedRequired, ExclusiveRequired, ExclusivePreferred };
const char* name(AccessPolicy) noexcept;
AccessPolicy access_policy(const EndpointRequirements&, bool exclusive_preferred) noexcept;

struct AccessAttempt {
    AccessMode mode = AccessMode::Shared;
    // Acceptance includes startup progress when the supervisor has checked it.
    // A successful native open can later be rejected before its first transfer;
    // issues then records startup_progress rather than an invented HRESULT.
    bool succeeded = false;
    // False for a known ownership conflict rejected before touching the device.
    bool native_open_attempted = true;
    std::vector<Issue> issues;
};

struct StreamConfiguration {
    StreamFormat client_format;
    std::optional<StreamFormat> native_format;
    AccessMode mode = AccessMode::Shared;
    AccessPolicy access_policy = AccessPolicy::AutomaticShared;
    std::optional<AccessMode> requested_mode;
    bool exclusive_mode_preferred = false;
    bool access_fell_back = false;
    std::vector<AccessAttempt> access_attempts;
    // Empty means unconfirmed. Zero must not masquerade as known telemetry.
    std::optional<std::uint32_t> period_frames;
    std::optional<std::uint32_t> allocated_buffer_frames;
    std::optional<int> timing_sample_rate_hz;
    std::optional<int> applied_buffer_request_ms;
    std::optional<double> applied_period_request_ms;
    std::optional<double> applied_render_ahead_request_ms;
    // Client3's advertised bounds are capabilities, not confirmation that the
    // requested period was accepted. period_frames above reports acceptance.
    std::optional<std::uint32_t> shared_default_period_frames;
    std::optional<std::uint32_t> shared_minimum_period_frames;
    std::optional<std::uint32_t> shared_fundamental_period_frames;
    std::optional<std::uint32_t> shared_maximum_period_frames;
    std::optional<std::uint32_t> render_ahead_target_frames;
    std::string period_selection;
    std::string low_latency_fallback;
};

struct NegotiatedRoute {
    StreamConfiguration source;
    StreamConfiguration destination;
    BackendId backend = BackendId::Wasapi;
    std::string processing;
    std::string synchronization;
    std::string selection_reason;
    std::optional<std::uint32_t> fifo_target_source_frames;
    std::optional<std::uint32_t> fifo_capacity_source_frames;
    std::optional<double> requested_queue_target_ms;
    std::string preferred_clock_device = "automatic";
    bool clock_preference_applied = false;
};

// Last observed FIFO-starvation event, not a complete event trace. Frame indexes
// belong to the processing pipeline; native QPC is in 100-ns units. Optional
// native context is absent for adapters without that observation. No samples or
// acoustic-latency claim are included. A host can detect skipped events using
// sequence and the cumulative underrun counter.
struct UnderrunEvent {
    std::uint64_t sequence = 0;
    std::uint64_t rendered_frame_index = 0;
    std::uint64_t source_published_frames = 0;
    std::uint32_t source_frames_available_before = 0;
    std::uint32_t requested_output_frames = 0;
    std::uint32_t produced_output_frames = 0;
    std::uint32_t controller_target_source_frames = 0;
    double requested_correction_ppm = 0;
    std::optional<std::uint64_t> render_qpc_100ns;
    std::optional<std::uint64_t> last_capture_device_position_frames;
    std::optional<std::uint64_t> last_capture_qpc_100ns;
    std::optional<std::uint64_t> capture_delivery_gap_last_us;
    std::optional<std::uint64_t> capture_packet_age_last_us;
    std::optional<std::uint32_t> native_padding_before_frames;
    std::optional<std::uint32_t> native_render_request_frames;
};

// A counter is absent until a real implementation supplies an observation.
// Silence is valid input; positive frames are not proof of audible playback,
// full-rate progress, low latency or a healthy wireless link.
struct RouteObservation {
    std::optional<std::uint64_t> captured_frames;
    std::optional<std::uint64_t> rendered_frames;
    std::optional<std::uint64_t> underruns;
    std::optional<std::uint64_t> overruns;
    std::optional<std::uint64_t> transferred_frames;
    std::optional<std::uint64_t> discontinuities;
    std::optional<std::uint64_t> startup_discontinuities;
    std::optional<std::uint64_t> timestamp_errors;
    std::optional<std::uint32_t> queued_frames;
    std::optional<std::uint32_t> controller_target_source_frames;
    std::optional<std::uint32_t> capture_packet_min_frames;
    std::optional<std::uint32_t> capture_packet_max_frames;
    std::optional<std::uint32_t> render_request_min_frames;
    std::optional<std::uint32_t> render_request_max_frames;
    std::optional<double> correction_ppm;
    std::optional<double> measured_clock_drift_ppm;
    // Native worker observations; absent for adapters that do not supply them.
    // Packet age is first-frame capture QPC to receipt, not acoustic latency.
    // Padding is submitted-but-unconsumed client audio, not wireless buffering.
    std::optional<std::uint64_t> capture_delivery_gap_max_us;
    std::optional<std::uint32_t> capture_drain_max_frames;
    std::optional<std::uint64_t> capture_packet_age_last_us;
    std::optional<std::uint64_t> capture_packet_age_max_us;
    std::optional<std::uint32_t> render_padding_before_frames;
    // Before + successfully submitted frames; Windows can consume audio during
    // the call. This is not a second, post-submission GetCurrentPadding query.
    std::optional<std::uint32_t> render_padding_after_submit_estimate_frames;
    std::optional<std::uint64_t> render_empty_wakes;
    // Native scheduling diagnostics, not counts of audible glitches. A bounded
    // capture wait spends existing Shared padding rather than inserting silence.
    std::optional<std::uint64_t> render_capture_waits;
    std::optional<std::uint64_t> render_capture_wait_timeouts;
    std::optional<bool> capture_wait_timer_available;
    std::optional<std::uint64_t> native_capture_wakes;
    std::optional<std::uint64_t> native_render_wakes;
    std::optional<std::uint64_t> native_wait_timeouts;
    // Initial native preroll and additional zero prefix during pipeline priming
    // are counted separately from pipeline-rendered/valid transferred frames.
    std::optional<std::uint64_t> native_initial_silence_frames;
    std::optional<std::uint64_t> native_startup_padding_silence_frames;
    std::optional<std::uint64_t> native_submitted_frames;
    std::optional<std::uint32_t> native_render_request_max_frames;
    std::optional<UnderrunEvent> last_underrun;
    std::optional<std::uint64_t> dropped_underrun_contexts;
    std::string worker_scheduling;
    std::vector<Issue> issues;
};

// Future streaming-adapter seam. All calls are lifecycle-worker operations,
// never GUI-thread I/O or audio callbacks. An adapter owns its threads, native
// handles, preallocated buffers and synchronization state. It must honor stop
// promptly where APIs permit and must not publish directly to widgets.
//
// open() below returns a STOPPED owned session; start() starts its data path.
// stop() is idempotent, joins producers, and releases all resources. Destruction
// must provide equivalent cleanup. Snapshots contain owned copies, not borrowed
// audio buffers. A future supervisor correlates these with operation generations.
class RouteSession {
  public:
    virtual ~RouteSession() = default;
    virtual NegotiatedRoute configuration() const = 0;
    virtual Result<bool> start(std::stop_token) = 0;
    virtual RouteObservation observe() = 0;
    virtual void stop() noexcept = 0;
};

class BackendAdapter {
  public:
    virtual ~BackendAdapter() = default;
    virtual BackendDescriptor descriptor() const = 0;
    virtual Result<std::unique_ptr<RouteSession>> open(const BoundRoute&, std::stop_token) = 0;
};

// Validate an opened route before committing it to a live session. Explicit
// requirements are not fallback hints. Without an effective forced mode, the
// independent preference allows Exclusive followed by a reported Shared retry.
// Otherwise Automatic remains Shared. Buffer request application
// is distinct from scheduling period and latency. Supplied requests must be
// confirmed unchanged; driver-aligned allocation is reported separately in its
// own sample-rate domain and need not match the requested duration.
Result<NegotiatedRoute> check_negotiated(const RouteRequest&, const NegotiatedRoute&);

// Validate one concrete attempt before accepting it or retrying another mode.
// The mode in requirements is the attempted mode; all other explicit parameters
// remain unchanged. No device I/O, migration or fallback is performed here.
Result<StreamConfiguration> check_stream(const EndpointRequirements&, const StreamConfiguration&,
                                         std::string_view location);

} // namespace audiohole::engine
