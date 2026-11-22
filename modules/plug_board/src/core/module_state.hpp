#pragma once

#include "audiohole/plug_board/module.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace audiohole::plug_board {

// Private actor state. Only inboxes, observer registration, requested cancellation
// and the immutable published pointer are shared; all remaining fields belong to
// the control thread. Native snapshots coalesce in one bounded slot, retaining
// observed Active hints so a short-lived game remains Recently Active.
struct Module::Implementation {
    using Clock = std::chrono::steady_clock;
    struct Task {
        std::function<void()> run;
        std::function<void()> cancel;
        bool interrupt_policy = false;
    };
    struct ActiveHint {
        ApplicationIdentity identity;
        std::string observation_key;
        std::string display_name;
        std::int64_t utc_ms = 0;
        std::vector<std::string> endpoints;
    };
    struct ObservationInbox {
        std::uint64_t generation = 0;
        SessionSnapshot snapshot;
        std::string utc;
        std::map<std::string, ActiveHint> active;
    };
    struct RuntimeApplication {
        std::optional<ApplicationObservation> observation;
        std::vector<ProcessToken> processes;
        ActivityHistory history;
        LivePolicyReport policy;
        bool fresh = false;
        bool was_active = false;
        std::optional<Clock::time_point> last_active_monotonic;
        std::string attempted_fingerprint;
        std::string identity_signature;
        std::string icon_signature;
        std::string provisional_key;
        std::uint64_t last_seek_generation = 0;
    };
    struct CachedImage {
        std::shared_ptr<const ApplicationImage> image;
        std::uint64_t used = 0;
    };

    ModuleOptions options;
    std::atomic<std::shared_ptr<const ModuleSnapshot>> published;
    std::atomic<bool> closing{false};
    std::atomic<std::uint64_t> dropped{0};
    std::stop_source policy_stop;
    std::optional<std::stop_source> in_flight_policy_stop;
    std::mutex inbox_mutex;
    std::condition_variable wake;
    std::deque<Task> commands;
    std::deque<std::function<void()>> events;
    std::optional<ObservationInbox> observation_inbox;
    std::optional<bool> visible_inbox;
    std::optional<EndpointInventory> inventory_inbox;
    std::optional<std::vector<ProfileDescriptor>> profiles_inbox;
    std::optional<std::pair<std::optional<std::string>, std::uint64_t>> profile_inbox;
    std::optional<unsigned> pixels_inbox;
    bool sample_inbox = false;
    bool native_inventory_inbox = false;
    bool resync_needed = false;
    std::mutex observers_mutex;
    std::map<std::uint64_t, std::function<void(std::shared_ptr<const ModuleSnapshot>)>> observers;
    std::uint64_t next_observer = 1;
    std::promise<OperationResult> shutdown_promise;
    std::shared_future<OperationResult> shutdown_future = shutdown_promise.get_future().share();

    // Control-thread-owned resources and intent. Never mutate a published Library.
    std::unique_ptr<PlatformBackend> backend;
    std::unique_ptr<CheckedStorage> storage;
    std::unique_ptr<CheckedStorage> device_storage;
    std::optional<std::string> device_bytes;
    std::optional<std::string> expected_bytes;
    std::shared_ptr<const Library> library;
    std::string selected;
    ModulePhase phase = ModulePhase::Loading;
    EndpointInventory inventory;
    std::vector<ProfileDescriptor> profiles;
    std::optional<std::string> active_profile;
    std::uint64_t profile_generation = 0;
    std::uint64_t discovery_generation = 0;
    std::uint64_t routing_generation = 1;
    std::uint64_t publication = 0;
    std::uint64_t library_commits = 0;
    std::uint64_t diagnostic_flushes = 0;
    bool visible = false;
    bool simulation = false;
    PlatformMetrics last_metrics;
    Document platform_environment = Document::object();
    bool catalog_dirty = false;
    bool history_dirty = false;
    bool diagnostics_dirty = false;
    bool history_writable = true;
    bool diagnostics_writable = true;
    bool local_history_loaded = false;
    unsigned icon_pixels = 24;
    OperationResult capability;
    OperationResult storage_result;
    OperationResult diagnostic_storage_result{
        true, "memory_only", "Diagnostics are retained in memory until the next flush."};
    std::optional<CommandResult> last_command;
    std::map<std::string, RuntimeApplication> applications;
    std::map<std::string, std::string> identity_index;
    std::map<std::string, ApplicationIdentity> canonical_identity_cache;
    std::map<std::string, std::string> provisional_ids;
    std::map<std::string, CachedImage> images;
    // Request identity is separate from displayed pixels: keep the last good
    // image while a new process, reopened view or DPI change resolves its icon.
    std::map<std::string, std::string> image_requests;
    std::uint64_t image_sequence = 0;
    std::set<std::string> policy_pending;
    std::vector<ApplicationIdentity> watched;
    bool watcher_dirty = false;
    std::deque<DiagnosticEvent> diagnostics;
    std::map<std::string, std::string> standalone_devices;
    Document standalone_device_extra = Document::object();
    std::optional<Clock::time_point> flush_due;
    std::optional<Clock::time_point> recent_due;
    std::jthread control;

    explicit Implementation(ModuleOptions);
    ~Implementation();
    bool post(Task);
    void event(std::function<void()>);
    void observed(std::uint64_t, SessionSnapshot, std::string);
    void run();
    void initialize();
    void shutdown();
    bool pump_inboxes();
    void log(Severity, std::string code, std::string message, std::string app = {},
             Document details = Document::object(), std::string operation = {});
    void publish();
    void mark_flush();
    void flush();
    void rebuild_identity_index();
    OperationResult commit(Library);
    CommandResult execute(const Command&, std::optional<std::uint64_t>, std::optional<std::string>);
    CommandResult reload();
    CommandResult restore(const std::filesystem::path&, bool);
    void set_visibility(bool);
    void update_inventory(EndpointInventory);
    void refresh_standalone_inventory();
    void activate_profile(std::optional<std::string>, std::uint64_t);
    std::string register_application(const ApplicationIdentity&, const std::string& name,
                                     const std::string& provisional_key);
    void ingest(ObservationInbox);
    void process_started(ProcessEvidence);
    void icon_ready(std::uint64_t, const std::string&, ApplicationImage);
    void request_images(const std::vector<std::string>&);
    void reconcile(bool force, const std::string& only_application = {});
    void seek_pending();
    void update_watcher();
    void apply_one();
    std::string fingerprint(const std::string&, const EndpointTarget&,
                            const std::optional<std::string>&) const;
    const Socket* socket_for(const std::string&) const;
    bool managed(const ApplicationRecord&) const;
    void load_local_state();
    OperationResult export_to(const std::filesystem::path&, bool, bool);
};

inline std::string catalog_signature(const ApplicationRecord& record) {
    Document aliases = Document::array();
    for (const auto& alias : record.verified_aliases)
        aliases.push_back({static_cast<int>(alias.kind), alias.value});
    return Document{{"kind", static_cast<int>(record.identity.kind)},
                    {"value", record.identity.value},
                    {"aliases", std::move(aliases)}}
        .dump();
}

} // namespace audiohole::plug_board
