#pragma once

#include "audiohole/engine/backend.hpp"
#include "audiohole/engine/channel_mapping.hpp"
#include <array>
#include <atomic>
#include <limits>
#include <vector>

namespace audiohole::engine::detail {

// Optional, fixed-size context copied only when a render call starves. Native
// adapters obtain these values on their I/O worker, never by calling into the
// supervisor. Miniaudio leaves the native fields absent rather than inventing
// equivalent WASAPI observations. No context construction allocates.
struct RenderTiming {
    std::optional<std::uint64_t> render_qpc_100ns;
    std::optional<std::uint64_t> last_capture_device_position_frames;
    std::optional<std::uint64_t> last_capture_qpc_100ns;
    std::optional<std::uint64_t> capture_delivery_gap_last_us;
    std::optional<std::uint64_t> capture_packet_age_last_us;
    std::optional<std::uint32_t> native_padding_before_frames;
    std::optional<std::uint32_t> native_render_request_frames;
};

// Validation-only processing prototype, not a selected production converter.
// One capture producer and one destination consumer own a bounded SPSC queue.
// Rates/widths are fixed at construction; no callback allocation or locks.
// Native and library I/O share this implementation so comparisons can isolate
// device access. Experimental selects an alternate occupancy controller.
class StreamPipeline {
  public:
    StreamPipeline(StreamFormat source, StreamFormat destination,
                   const std::vector<ChannelRoute>& map, bool experimental,
                   double target_milliseconds = 20, bool explicit_target = false);
    bool push(const void* input, std::size_t frames, bool silent = false) noexcept;
    // Returns valid source-derived output frames, including valid source silence.
    // A short return can mean priming or starvation; counters distinguish them.
    std::size_t render(void* output, std::size_t frames, const RenderTiming& = {}) noexcept;
    // Destination-consumer thread only. Conservative lookahead at the existing
    // maximum correction; no phase/controller/queue mutation and no observation
    // mailbox consumption. False during priming: priming silence is intentional.
    bool would_starve(std::size_t output_frames) const noexcept;
    // One non-RT observer owns telemetry consumption/cache. Audio processing
    // stays SPSC; observe() must not be called from multiple concurrent readers.
    RouteObservation observe();
    void discontinuity(bool startup = false) noexcept {
        (startup ? startup_discontinuities_ : discontinuities_)
            .fetch_add(1, std::memory_order_relaxed);
    }
    void timestamp_error() noexcept {
        timestamp_errors_.fetch_add(1, std::memory_order_relaxed);
    }
    std::uint32_t capacity_frames() const noexcept {
        return capacity_;
    }
    std::uint32_t target_frames() const noexcept {
        return target_;
    }

  private:
    StreamFormat source_, destination_;
    ChannelMapping mapping_;
    std::uint32_t capacity_ = 0, target_ = 0;
    std::size_t input_stride_ = 0, output_stride_ = 0;
    std::vector<float> queue_;
    // Capture writes head, render writes tail; release publishes sample access.
    std::atomic<std::uint64_t> head_{0}, tail_{0};
    std::atomic<std::uint64_t> captured_{0}, rendered_{0}, transferred_{0};
    std::atomic<std::uint64_t> underruns_{0}, overruns_{0}, discontinuities_{0};
    std::atomic<std::uint64_t> startup_discontinuities_{0}, timestamp_errors_{0};
    std::atomic<std::int64_t> correction_ppb_{0};
    // Each min/max pair has ONE writer (its endpoint callback); observers only
    // load. Updating them needs no CAS loop, locks, allocation or sample logging.
    std::atomic<std::uint32_t> packet_min_{std::numeric_limits<std::uint32_t>::max()},
        packet_max_{0};
    std::atomic<std::uint32_t> render_min_{std::numeric_limits<std::uint32_t>::max()},
        render_max_{0};
    std::atomic<std::uint32_t> active_target_{0};
    std::atomic<bool> capacity_insufficient_{false};
    // A second bounded SPSC mailbox moves starvation context from render to the
    // single observer. Slots are ordinary POD owned by exactly one side; release
    // publication/reclamation prevents overwrite while a reader copies. Full
    // mailboxes drop NEW telemetry, never block routing. The observer retains
    // the last event so a slower host snapshot does not lose it on the next poll.
    std::array<UnderrunEvent, 8> underrun_events_{};
    std::atomic<std::uint64_t> event_write_{0}, event_read_{0}, dropped_events_{0};
    std::optional<UnderrunEvent> last_observed_underrun_; // Observer-owned only.
    // Consumer-only controller/resampling state. A phase accumulator preserves
    // fractional progress between arbitrary endpoint block sizes.
    double phase_ = 0, filtered_error_ = 0, integral_ = 0;
    std::uint32_t previous_target_ = 0;
    bool primed_ = false, experimental_ = false, explicit_target_ = false;
};

// No cross-thread device ownership is exposed. Concrete adapters create their
// own workers and report unavailable on unsupported platforms/builds.
std::unique_ptr<BackendAdapter> make_wasapi_adapter(bool experimental = false,
                                                    LatencyPreset = LatencyPreset::Baseline,
                                                    WorkerSettings = {});
std::unique_ptr<BackendAdapter> make_miniaudio_adapter(WorkerSettings = {});
unsigned queue_target_milliseconds(const NegotiatedRoute&, LatencyPreset = LatencyPreset::Baseline);

} // namespace audiohole::engine::detail
