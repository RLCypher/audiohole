#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace audiohole::engine {

// All public functions in this file run on a non-audio control/lifecycle thread.
// They allocate and return owned values. No JSON library, GUI class, scanner
// type, COM pointer or persistent Windows binding appears in the public model.
enum class ErrorCode {
    InvalidDocument,
    UnsupportedVersion,
    InvalidConfiguration,
    UnsupportedBackend,
    BackendUnavailable,
    UnsupportedRequirement,
    UnresolvedEndpoint,
    AmbiguousEndpoint,
    UnavailableEndpoint,
    DirectionMismatch,
    ResourceConflict,
    Busy,
    Cancelled,
    BackendFailure,
    ContractViolation
};

// location is a JSON Pointer or a documented runtime field path. Hosts can
// associate a failure with an editor control without parsing English messages.
struct Issue {
    ErrorCode code = ErrorCode::InvalidConfiguration;
    std::string location;
    std::string detail;
    std::string native_code{};
};

// A failed operation has no partially usable output. Result owns its strings
// and payload, so a GUI can copy/move it across a non-real-time transport.
template <class T> struct Result {
    std::optional<T> value;
    std::vector<Issue> issues;
    [[nodiscard]] bool ok() const noexcept {
        return value.has_value() && issues.empty();
    }
};

enum class AccessMode { Shared, Exclusive };
enum class SampleFormat { Float32, PcmS16, PcmS24Packed, PcmS24In32, PcmS32 };
enum class BackendId { Automatic, Miniaudio, Wasapi, Experimental };
enum class DirectionId { Forward, Reverse };

const char* name(AccessMode) noexcept;
const char* name(SampleFormat) noexcept;
const char* name(BackendId) noexcept;
std::optional<BackendId> backend_id(std::string_view) noexcept;

// Absence means inherit/Automatic, never an unknown measured capability.
// buffer_ms is a requested duration, not a measured route latency. An adapter
// must report its application of the request separately from allocated buffers.
struct EndpointRequirements {
    std::optional<int> sample_rate_hz;
    std::optional<int> channel_count;
    std::optional<SampleFormat> sample_format;
    std::optional<AccessMode> mode;
    std::optional<int> buffer_ms;
    std::optional<double> period_ms, render_ahead_ms;
    bool operator==(const EndpointRequirements&) const = default;
};

// Zero-based PCM frame indexes, not physical hardware port identifiers.
struct ChannelRoute {
    int source = 0;
    int destination = 0;
    bool operator==(const ChannelRoute&) const = default;
};

struct Preferences {
    EndpointRequirements common;
    EndpointRequirements source;
    EndpointRequirements destination;
    std::vector<ChannelRoute> channel_map;
    // Saved metadata only. No clock switching or latency promise is implied.
    std::string preferred_clock_device = "automatic";
    std::optional<double> queue_target_ms;
    bool operator==(const Preferences&) const = default;
};

struct Direction {
    bool advanced_tuning = false;
    // Independent of advanced tuning; effective forced modes still win.
    bool exclusive_mode_preferred = false;
    std::string source;
    std::string destination;
    std::string source_name;
    std::string destination_name;
    Preferences preferences;
    bool operator==(const Direction&) const = default;
};

struct Hole {
    std::string id;
    std::string display_name;
    std::string notes;
    bool enabled = true;
    bool duplex = false;
    // Preserve unknown selectors on load; preparation rejects one when enabled.
    // Disabling audio overrides does not disable the backend request.
    std::string routing_engine = "automatic";
    Direction forward;
    Direction reverse;
    bool operator==(const Hole&) const = default;
};

struct Profile {
    std::string id;
    std::string display_name;
    std::string notes;
    std::vector<Hole> holes;
    int source_schema_version = 4;
    bool operator==(const Profile&) const = default;
};

// Read-only consumer of audiohole-profile v3, with v1/v2 loading compatibility,
// maximum 2 MiB/depth 16. Legacy hole flags are projected onto both directions;
// v3 direction flags and hole.routing alone are authoritative for their fields.
// Never opens devices, saves/renames files, migrates legacy aggregates, rewrites
// notes, or consults old settings.format/mode/buffer_ms/automatic projections.
// Unknown fields are nonoperational; the host retains the original document for
// editing/preservation. Do not use this typed projection as a serializer.
Result<Profile> parse_profile(std::string_view utf8_json);

// Resolve one property at a time. This never fills in an automatic candidate:
// concrete stream selection belongs to an actual backend, not the profile.
EndpointRequirements inherit(const EndpointRequirements& common,
                             const EndpointRequirements& endpoint);

struct RouteRequest {
    std::string profile_id;
    std::string hole_id;
    DirectionId direction = DirectionId::Forward;
    std::string source_id;
    std::string destination_id;
    BackendId backend = BackendId::Automatic;
    bool exclusive_mode_preferred = false;
    // Runtime planning constraint, never a saved profile preference. Multiple
    // consumers of one capture endpoint cannot each own it exclusively.
    bool shared_capture_required = false;
    std::string preferred_clock_device = "automatic";
    EndpointRequirements source;
    EndpointRequirements destination;
    std::vector<ChannelRoute> channel_map;
    std::optional<double> queue_target_ms;
};

struct PreparedProfile {
    std::string profile_id;
    std::size_t enabled_holes = 0;
    std::vector<RouteRequest> routes;
};

// Validate typed input too: hosts need not enter through JSON. Missing physical
// devices and missing Deep evidence do not prevent preparing portable intent.
// Saved drafts can parse but fail preparation if enabled assignments are empty.
// Effective overrides only are validated here; retained inactive values remain
// untouched. At most eight enabled holes / sixteen directional paths may start.
Result<PreparedProfile> prepare_profile(const Profile&);

enum class EndpointDirection { Capture, Render };
enum class BindingState { Available, Unavailable, Ambiguous, Unknown };

// Interleaved client PCM representation. Native layout is tracked separately;
// channel count alone does not identify physical speaker positions or buses.
struct StreamFormat {
    int sample_rate_hz = 0;
    int channel_count = 0;
    SampleFormat sample_format = SampleFormat::Float32;
    bool operator==(const StreamFormat&) const = default;
};

// Optional scanner evidence copied with an exact local binding, never saved
// profile intent. A backend must query current support before using a hint.
// The host supplies only positive, noncontradictory evidence; absence is valid.
struct FormatHint {
    StreamFormat format;
    AccessMode mode = AccessMode::Shared;
    std::optional<std::uint32_t> channel_mask;
    bool operator==(const FormatHint&) const = default;
};

// A host-produced, machine-local observation. endpoint_id is an opaque backend
// binding, never a portable identity and never a guessed match by display name.
struct LocalBinding {
    std::string logical_id;
    std::string endpoint_id;
    EndpointDirection direction = EndpointDirection::Capture;
    BindingState state = BindingState::Unknown;
    std::vector<FormatHint> format_hints{};
};

struct BindingSnapshot {
    std::uint64_t revision = 0;
    std::vector<LocalBinding> endpoints;
};

struct BoundRoute {
    RouteRequest request;
    LocalBinding source;
    LocalBinding destination;
    std::uint64_t binding_revision = 0;
};

// Resolves an exact logical ID once in an immutable observation. No scanning,
// matching, fallback or side effects. Caller must still recheck on opening and
// reject superseded operation generations. A bound route is not running audio.
Result<BoundRoute> bind_route(const RouteRequest&, const BindingSnapshot&);

// Derived PCM storage geometry for StreamFormat above. WAVE tags and GUIDs
// remain backend-owned, and an absent native channel layout stays unknown.
struct PcmGeometry {
    int container_bits = 0;
    int valid_bits = 0;
    std::uint32_t block_align_bytes = 0;
    std::uint64_t average_bytes_per_second = 0;
};
Result<PcmGeometry> pcm_geometry(const StreamFormat&);

// Check a backend's actual client format against every active requirement.
// Availability, access mode, timing and synchronization need separate checks.
Result<StreamFormat> check_format(const EndpointRequirements&, const StreamFormat&,
                                  std::string_view location);

} // namespace audiohole::engine
