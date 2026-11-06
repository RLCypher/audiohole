#pragma once
#include "model/scanner_types.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace audiohole {
// D-019 public states. Resolving is internal to Starting, never a ninth state.
enum class State { Stopped, Ready, Starting, Running, Degraded, Recovering, Failed, Stopping };
const char* name(State state);
enum class Availability { Available, Unavailable, Ambiguous, Untested, Unknown };
const char* name(Availability availability);
// Exact Windows descriptor when supplied by the scanner. An absent mask remains
// unknown; channel count alone never establishes speaker positions.
struct WaveFormat {
    int container_bits = 0, valid_bits = 0, format_tag = 0;
    std::optional<std::uint32_t> channel_mask;
    std::string subformat_guid;
    int block_align_bytes = 0, average_bytes_per_second = 0;
    bool operator==(const WaveFormat&) const = default;
};
struct Format {
    int rate = 48000, channels = 2;
    std::string representation = "float32";
    std::optional<WaveFormat> wave{};
    bool operator==(const Format&) const = default;
    std::string label() const;
};
enum class FormatSupport { Unknown, Supported, Unsupported };
struct FormatCapability {
    Format format;
    std::string mode; // Shared or Exclusive; never a union of both modes.
    FormatSupport support = FormatSupport::Unknown;
};
struct Endpoint {
    std::string id, display_name, windows_name, notes;
    bool capture = false;
    Availability availability = Availability::Available;
    std::vector<Format> formats;
    bool hidden = false;
    std::shared_ptr<const DeviceInspection> inspection{};
    std::vector<FormatCapability> capabilities{};
};
struct Settings {
    bool automatic = true;
    Format format;
    int buffer_ms = 6;
    std::string mode = "Automatic";
    bool operator==(const Settings&) const = default;
};
struct Direction {
    std::string source, destination;
    Settings settings;
    // Readable hints only. Logical IDs remain authoritative after a rename.
    std::string source_name{}, destination_name{};
    bool operator==(const Direction&) const = default;
};
struct Hole {
    std::string id, display_name, notes;
    bool enabled = true, duplex = false;
    Direction forward, reverse;
    bool operator==(const Hole&) const = default;
};
struct Profile {
    std::string id, display_name, notes;
    std::vector<Hole> holes;
    bool operator==(const Profile&) const = default;
};
struct PathStatus {
    std::string hole_id;
    bool reverse = false;
    State state = State::Stopped;
    std::string detail;
    // Configuration confirmations from this backend, not measured latency.
    // Preview values are explicitly simulated; absent means unconfirmed.
    std::optional<int> requested_buffer_ms{}, applied_buffer_ms{};
};
enum class Severity { Info, Warning, Error, Critical };
struct Event {
    std::uint64_t sequence;
    std::string type, detail;
    Severity severity = Severity::Info;
};
// Read-only view owned by the controller. References/pointers into its vectors
// must not survive a mutating command. No audio buffers or measured telemetry.
enum class ScanScope { Update, Full, Selected };
enum class OperatingMode { Preview, Test };
// Command availability, never evidence that hardware opened or audio flowed.
struct BackendCapabilities {
    bool simulated = true, routing = true, scanning = true, beep = true, monitoring = true;
    bool persistence = false, live_buffer = false;
};

struct Snapshot {
    OperatingMode operating_mode = OperatingMode::Preview;
    BackendCapabilities capabilities;
    std::vector<Endpoint> endpoints;
    std::vector<Profile> profiles;
    std::string active_profile, pending_profile;
    State state = State::Stopped;
    bool armed = false, monitoring = false, scanning = false;
    int pending_ms = 0, scan_percent = 0;
    std::string beep_destination;
    std::vector<PathStatus> paths;
    std::vector<Event> events;
    std::uint64_t revision = 0;
    ScannerStatus scanner;
};
struct Result {
    bool ok;
    std::string detail;
};
struct Recommendation {
    std::vector<Format> compatible;
    std::string explanation;
};
// Prepared on the control side. No scan matrices, notes, JSON, widget state or
// guessed identity matching enter an engine start request. The future supervisor
// must resolve logical IDs to unambiguous local bindings and initialize streams.
struct RoutingPathRequest {
    std::string hole_id, source, destination, mode;
    bool reverse = false;
    Format format;
    int buffer_ms = 0;
};
struct PreparedProfile {
    std::string profile_id;
    std::vector<RoutingPathRequest> paths;
};
// Returns a complete request on success; leaves output unchanged on failure.
Result prepare_profile(const Snapshot&, const Profile&, PreparedProfile&);
// Full-save enrichment of name hints and cached automatic candidates. Does not
// change IDs, manual overrides or drafts without matching evidence.
void describe_profile(const Snapshot&, Profile&);
const Endpoint* endpoint(const Snapshot&, const std::string& id);
// Offered new bindings. Existing references are resolved separately, including
// hidden/missing records. Borrowed pointers expire when the snapshot mutates.
std::vector<const Endpoint*> configuration_devices(const Snapshot&);
const Profile* profile(const Snapshot&, const std::string& id);
Recommendation recommend(const Snapshot&, const Direction&);
Result validate(const Snapshot&, const Profile&);
// Internal, in-process GUI seam; not API.md's final public schema/transport.
// Calls and snapshots belong to the control/GUI thread. The scanner owns its
// worker and marshals immutable updates back through this boundary;
// it must not run audio callbacks on the GUI thread or expose their buffers here.
// Commands return explicit failures rather than silently substituting endpoints.
class Controller {
  public:
    virtual ~Controller() = default;
    // Borrowed until the next mutation; reading never advances simulated time.
    virtual const Snapshot& snapshot() const = 0;
    virtual std::string last_edit_error() const {
        return {};
    }
    // Save changes to backend-owned records (session Preview, persistent Test). Apply validates and
    // requests activation; failure preserves live routing. Unconfigured drafts can be saved without
    // Apply.
    virtual Result save(Profile, bool apply) = 0;
    // Commit only this saved direction's buffer preference, preserving other
    // staged GUI edits. The simulator also confirms a matching active direction;
    // configuration-only backends never claim an engine application.
    virtual Result update_buffer(const std::string& profile_id, const std::string& hole_id,
                                 bool reverse, int milliseconds);
    // Creates fresh logical IDs, including every duplicated hole. Test commits to disk.
    virtual std::string create_profile(const std::string& name,
                                       const std::string& copy_from = {}) = 0;
    // Rejects deletion of an active or pending profile; callers confirm user intent.
    virtual Result remove_profile(const std::string&) = 0;
    // Returns a new unconfigured editor value; does not arm or save it.
    virtual Hole make_hole() = 0;
    // Validates before replacing the live plan. An unavailable replacement retains
    // the old session unless the user explicitly asks to arm waiting resources.
    // This conservative simulator behavior does not resolve O-002.
    virtual Result activate(const std::string&, bool arm_waiting = false) = 0;
    // Disarms immediately and cancels every pending operation. Transition to
    // Stopped completes on advance(); no stale work can rearm the session.
    virtual void stop() = 0;
    // Replaces the pending startup deadline. No OS startup registration occurs.
    virtual Result schedule(const std::string&, int milliseconds) = 0;
    // Advances deterministic logical time, never wall-clock measurements.
    // Nonpositive durations are ignored. No sleeping or blocking work occurs.
    virtual void advance(int milliseconds) = 0;
    // Edits display name, notes and simulated availability only; identity and
    // windows_name are immutable. Availability changes reevaluate live directions.
    virtual Result update_endpoint(const Endpoint&) = 0;
    // Simulation controls. Failure is directional; recovery never arms a stopped session.
    virtual void fail_path(const std::string& hole, bool reverse) = 0;
    virtual void recover() = 0;
    // Temporary observation flag only; caller clears it on panel/window closure.
    virtual void monitor(bool enabled) = 0;
    // Simulates a bounded interval for one explicit playback endpoint. Rejects
    // missing/ambiguous/capture endpoints and exclusive conflicts. Emits NO SOUND.
    virtual Result beep(const std::string& destination) = 0;
    virtual void cancel_beep() = 0;
    // GUI-thread command: clears all retained session diagnostics, including hidden
    // filtered entries. Preserves event sequence monotonicity and all routing,
    // profile, device, scan, startup and observation state; no disk files touched.
    virtual void clear_diagnostics() = 0;
    // Simulated progress only. Advanced consent and scope are presentation concerns;
    // no fixture capability is promoted to experimental/verified hardware evidence.
    virtual void scan(bool enabled, bool advanced = false, ScanScope scope = ScanScope::Update,
                      const std::string& selected_device = {}, const ScanOptions& options = {}) = 0;
    virtual Result scanner_action(const ScannerActionRequest&) {
        return {false, "Scanner inspection actions are unavailable."};
    }
};
// Owns all session data and a copied live plan, so saved edits cannot mutate
// active routing. Only advance() completes pending transitions. This deterministic
// single-threaded fixture is replaceable and implements no Windows audio APIs.
class SimulatedController final : public Controller {
  public:
    SimulatedController();
    const Snapshot& snapshot() const override {
        return state_;
    }
    Result save(Profile, bool apply) override;
    Result update_buffer(const std::string&, const std::string&, bool, int) override;
    std::string create_profile(const std::string&, const std::string& copy_from = {}) override;
    Result remove_profile(const std::string&) override;
    Hole make_hole() override;
    Result activate(const std::string&, bool arm_waiting = false) override;
    void stop() override;
    Result schedule(const std::string&, int) override;
    void advance(int) override;
    Result update_endpoint(const Endpoint&) override;
    void fail_path(const std::string&, bool) override;
    void recover() override;
    void monitor(bool) override;
    Result beep(const std::string&) override;
    void cancel_beep() override;
    void clear_diagnostics() override;
    void scan(bool, bool advanced = false, ScanScope scope = ScanScope::Update,
              const std::string& selected_device = {}, const ScanOptions& options = {}) override;
    Result scanner_action(const ScannerActionRequest&) override;

  private:
    Snapshot state_;
    Profile live_;
    std::vector<std::string> failures_;
    std::uint64_t next_id_ = 10, sequence_ = 0;
    int transition_ms_ = 0, beep_ms_ = 0, scan_ms_ = 0;
    void event(std::string type, std::string detail);
    void refresh_paths(bool reconnecting = false);
};
} // namespace audiohole
