#pragma once

#include "audiohole/plug_board/library.hpp"
#include "audiohole/plug_board/storage.hpp"

#include <array>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace audiohole::plug_board {

struct EndpointDescriptor {
    std::string logical_id;
    std::string display_name;
    std::string windows_id;
    bool available = false;
    bool ambiguous = false;
    bool playback = true;
};
struct EndpointInventory {
    std::uint64_t revision = 0;
    std::vector<EndpointDescriptor> endpoints;
    std::array<std::optional<std::string>, 3> default_windows_endpoints;
};
struct ProfileDescriptor {
    std::string id;
    std::string display_name;
};
struct ProcessEvidence {
    ApplicationIdentity identity;
    ProcessToken token;
};

enum class Severity { Information, Warning, Error };
struct DiagnosticEvent {
    std::string utc{};
    Severity severity = Severity::Information;
    std::string code{};
    std::string message{};
    std::string operation_id{};
    std::string board_id{};
    std::string socket_id{};
    std::string application_id{};
    Document details = Document::object();
};

enum class PolicyState {
    NotManaged,
    PendingStorage,
    PendingProcess,
    PendingDevice,
    Applying,
    Accepted,
    Partial,
    Rejected,
    Unsupported,
    Stale
};
enum class ObservationState { Unknown, Matches, Differs, Multiple, Stale, Observed };
const char* policy_state_name(PolicyState);
const char* observation_state_name(ObservationState);

struct RolePolicyResult {
    ProcessToken process;
    AudioRole role = AudioRole::Console;
    PreferenceRead before;
    OperationResult write;
    PreferenceRead readback;
    bool write_attempted = false;
    bool accepted = false;
};
struct LivePolicyReport {
    PolicyState state = PolicyState::PendingProcess;
    std::string operation_id;
    std::string detail;
    std::vector<RolePolicyResult> roles;
    std::optional<std::string> target_windows_id;
    std::uint64_t generation = 0;
};
struct ApplicationImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::uint8_t> rgba;
    bool application_specific = false;
    std::string source;
    OperationResult result;
    Document evidence = Document::object();
};
struct ActivityHistory {
    std::optional<std::int64_t> last_active_utc_ms;
    std::vector<std::string> last_active_windows_endpoints;
    bool clock_uncertain = false;
};
struct ApplicationView {
    std::string id;
    std::string display_name;
    std::string socket_id;
    Activity activity = Activity::Inactive;
    std::optional<std::int64_t> inactive_seconds;
    ActivityHistory history;
    std::optional<ApplicationObservation> observation;
    bool observation_fresh = false;
    ObservationState observed_routing = ObservationState::Unknown;
    LivePolicyReport policy;
    std::shared_ptr<const ApplicationImage> image;
};

// Operation counts are distinct from measured process I/O. Native backends
// increment these at the actual API boundary, including resynchronizations.
struct PlatformMetrics {
    std::uint64_t full_session_enumerations = 0;
    std::uint64_t session_events = 0;
    std::uint64_t icon_extractions = 0;
    std::uint64_t icon_cache_hits = 0;
    std::uint64_t policy_reads = 0;
    std::uint64_t policy_writes = 0;
    std::uint64_t process_snapshots = 0;
    std::uint64_t dropped_events = 0;
    unsigned owned_workers = 0;
    bool discovery_active = false;
    bool process_watch_active = false;
    std::optional<std::uint64_t> process_sample_tick_ms;
    std::optional<std::uint64_t> process_cpu_100ns;
    std::optional<std::uint64_t> private_bytes;
    std::optional<std::uint64_t> working_set_bytes;
    std::optional<std::uint64_t> handles;
    std::optional<std::uint64_t> io_write_operations;
};

struct RawEndpointInventory {
    struct Endpoint {
        std::string windows_id;
        std::string display_name;
        bool available = false;
    };
    std::vector<Endpoint> endpoints;
    std::array<std::optional<std::string>, 3> defaults;
};

// Callbacks only enqueue copied values. Backend callbacks can run on native
// workers; they never touch widgets, storage, policy objects or an audio callback.
// A backend must cancel subscriptions and join every owned worker before its
// shutdown() returns, after which it must never invoke these callbacks again.
struct BackendCallbacks {
    std::function<void(std::uint64_t, SessionSnapshot, std::string)> observations;
    std::function<void()> inventory_changed;
    std::function<void(ProcessEvidence)> process_started;
    std::function<void(std::uint64_t, std::string, ApplicationImage)> icon_ready;
    std::function<void(DiagnosticEvent)> diagnostic;
};

// Construct, initialize, call and destroy on the module's NON-UI control thread.
// Discovery/icon/WMI workers have their own COM apartments. The supplied endpoint
// inventory is authoritative; full session discovery is visibility-scoped.
class PlatformBackend {
  public:
    virtual ~PlatformBackend() = default;
    virtual bool simulated() const = 0;
    virtual OperationResult initialize(BackendCallbacks) = 0;
    virtual OperationResult capability() = 0;
    virtual Document environment() const {
        return {{"source", simulated() ? "simulation" : "platform"}};
    }
    virtual ApplicationIdentity canonical_identity(const ApplicationIdentity& identity) {
        return identity;
    }
    // Native hosts also exclude their exact running image, even after renaming.
    // This decision never follows an application's user-editable display label.
    virtual bool internal_application(const ApplicationIdentity& identity) const {
        return is_audiohole_application(identity);
    }
    // Only the standalone host shim calls inventory(); integrated hosts supply
    // their existing scanner projection through Module::set_inventory instead.
    virtual RawEndpointInventory inventory() = 0;
    // Three read-only MMDevice default-role lookups, never a second scanner.
    virtual std::array<std::optional<std::string>, 3> default_endpoints() {
        return {};
    }
    virtual void observe(bool visible, const EndpointInventory&, std::uint64_t generation) = 0;
    virtual void refresh_observation() = 0;
    // Finite current-process lookup. The module calls watch_processes first;
    // taking a subset snapshot must preserve the complete subscription.
    virtual std::vector<ProcessEvidence>
    find_processes(const std::vector<ApplicationIdentity>&) = 0;
    // Event-driven for all managed identities, including accepted assignments.
    // Empty disables monitoring; no inactivity age expires a saved assignment.
    virtual void watch_processes(const std::vector<ApplicationIdentity>& identities) = 0;
    // Optional, disposable machine-local presentation cache. Native backends do
    // all cache I/O on their icon worker; this path never contains routing intent.
    virtual void set_icon_cache_root(const std::filesystem::path&) {}
    virtual void request_icons(const std::vector<ApplicationObservation>&, unsigned pixels,
                               std::uint64_t generation) = 0;
    virtual LivePolicyReport apply_policy(const std::vector<ProcessToken>&,
                                          const std::optional<std::string>& windows_endpoint,
                                          std::uint64_t generation, std::stop_token) = 0;
    virtual PlatformMetrics metrics(bool sample_process_resources) = 0;
    virtual void shutdown() = 0;
};

enum class ModulePhase { Loading, Ready, ReadOnlyError, Stopping, Stopped };
struct CommandResult {
    bool ok = false;
    bool durable_intent = false;
    std::string code;
    std::string message;
    std::string operation_id;
    std::string created_id;
    std::uint64_t revision = 0;
};
struct ModuleSnapshot {
    std::uint64_t publication = 0;
    ModulePhase phase = ModulePhase::Loading;
    std::shared_ptr<const Library> library;
    std::string selected_board_id;
    EndpointInventory inventory;
    std::vector<ProfileDescriptor> profiles;
    std::optional<std::string> active_profile_id;
    std::vector<ApplicationView> applications;
    std::vector<DiagnosticEvent> diagnostics;
    OperationResult capability;
    OperationResult storage;
    OperationResult diagnostic_storage;
    std::optional<CommandResult> last_command;
    PlatformMetrics platform_metrics;
    std::uint64_t library_commits = 0;
    std::uint64_t diagnostic_flushes = 0;
    std::uint64_t callbacks_dropped = 0;
    bool visible = false;
    bool simulated = false;
    bool catalog_write_pending = false;
    bool diagnostics_write_pending = false;
    std::string observed_utc;
    Document environment = Document::object();
};

// Value-only diagnostic projections. Export is not an importable configuration
// file. Default redaction removes application paths, app IDs, session IDs and raw
// Windows device identifiers, including paths embedded in native error strings.
Document diagnostic_document(const DiagnosticEvent&, bool include_sensitive = false);
Document snapshot_document(const ModuleSnapshot&, bool include_sensitive = false);

struct ModuleOptions {
    std::filesystem::path data_root;
    std::filesystem::path local_root;
    std::function<std::unique_ptr<PlatformBackend>()> backend;
    std::function<std::unique_ptr<CheckedStorage>(const std::filesystem::path&)> storage =
        make_disk_storage;
    std::vector<ProfileDescriptor> profiles;
    std::optional<std::string> initial_active_profile;
    bool standalone_inventory = false;
    bool resolve_system_defaults = false;
    // Isolated tutorial: no library/history/log/icon-cache file access and only
    // a simulated backend. The normal checked-storage behavior is unchanged.
    bool memory_only = false;
    unsigned icon_pixels = 32;
    // Called on the service thread. Host logging must be non-audio and must not
    // synchronously re-enter Module. The default ring/file diagnostics still run.
    std::function<void(const DiagnosticEvent&)> diagnostic_sink;
};

// Owns one control thread and every backend/storage lifetime. Public methods
// enqueue bounded work and never execute native policy, JSON or filesystem I/O
// on their caller. Snapshots are immutable, owned values. Observer callbacks run
// on the control thread; views must enqueue GUI events with a lifetime-safe sink.
// Unsubscribe prevents future copies but an already-dispatched callback may finish.
// No method belongs on a real-time audio callback.
class Module final {
  public:
    explicit Module(ModuleOptions);
    ~Module();
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    std::shared_ptr<const ModuleSnapshot> snapshot() const;
    std::future<CommandResult> submit(Command, std::optional<std::uint64_t> expected_revision = {},
                                      std::optional<std::string> expected_selected_board = {});
    void set_visible(bool);
    void set_inventory(EndpointInventory);
    void set_profiles(std::vector<ProfileDescriptor>);
    void profile_activated(std::optional<std::string>, std::uint64_t committed_generation);
    void profile_deleted(const std::string&);
    void set_icon_pixels(unsigned);
    void request_images(std::vector<std::string> application_ids);
    void sample_diagnostics();
    // Copied UI evidence joins the bounded diagnostic ring on the control
    // thread. It never changes routing intent or represents a native receipt.
    void record_view_event(std::string code, std::string message,
                           Document details = Document::object(),
                           Severity severity = Severity::Information);
    std::uint64_t subscribe(std::function<void(std::shared_ptr<const ModuleSnapshot>)>);
    void unsubscribe(std::uint64_t);
    std::future<CommandResult> reload_library();
    std::future<CommandResult> restore_library(const std::filesystem::path&, bool confirmed);
    std::future<OperationResult> export_diagnostics(const std::filesystem::path&,
                                                    bool include_sensitive,
                                                    bool overwrite_confirmed = false);
    std::shared_future<OperationResult> begin_shutdown();

  private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace audiohole::plug_board
