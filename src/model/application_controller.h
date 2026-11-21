#pragma once
#include "model/controller.h"
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>

namespace audiohole {
struct StartRouting {
    std::string profile_id;
    bool arm_waiting = false;
};
struct StopRouting {};
struct LoadProfile {
    std::string profile_id;
};
struct SaveConfiguration {
    Profile profile;
    bool apply = false;
};
struct DiscoverDevices {
    bool advanced = false;
    ScanScope scope = ScanScope::Update;
    std::string selected_device;
    ScanOptions options;
};
struct ScannerAction {
    ScannerActionRequest request;
};
struct UpdateBuffer {
    std::string hole_id;
    bool reverse = false;
    int milliseconds = 0;
    std::string profile_id{}; // Empty targets the active profile only.
};
struct RequestStatus {};
struct RequestDevices {};
struct RequestDiagnostics {};
struct StartBeep {
    std::string destination;
};
struct CancelBeep {};
using Command = std::variant<StartRouting, StopRouting, LoadProfile, SaveConfiguration,
                             DiscoverDevices, UpdateBuffer, RequestStatus, RequestDevices,
                             RequestDiagnostics, StartBeep, CancelBeep, ScannerAction>;
const char* command_id(const Command&);
enum class Outcome { Accepted, Completed, Rejected, Failed, Cancelled, TimedOut };
struct CommandResult {
    std::uint64_t request_id = 0;
    std::string command;
    Outcome outcome = Outcome::Rejected;
    Result result{false, {}};
    std::uint64_t revision = 0;
    bool simulated = false;
    std::optional<Profile> loaded_profile;
};
enum class InformationState { Available, Unknown, Unavailable, Unsupported, Stale };
struct Metric {
    InformationState status = InformationState::Unavailable;
    std::optional<double> value;
};
struct Telemetry {
    std::uint64_t revision = 0;
    Metric latency_ms, underruns, overruns, applied_buffer_ms;
    int scan_percent = 0;
    bool simulated = false;
};
struct Baseline {
    std::string expression = "idle";
    int priority = 0;
    std::uint64_t revision = 0;
};
Baseline presentation_baseline(const Snapshot&);
struct SubscriptionFilter {
    bool events = true, results = true, telemetry = true;
};
struct UpdateBatch {
    std::shared_ptr<const Snapshot> state;
    std::vector<Event> events;
    std::vector<CommandResult> results;
    std::optional<Telemetry> telemetry;
    bool closed = false;
    bool gap = false; // A bounded mailbox overflowed; resynchronize using state.
};
// Consumers pull on the owning control thread. No callbacks or raw consumer
// pointers survive unsubscribe; a moved/destroyed subscription releases its slot.
class Subscription {
  public:
    Subscription() = default;
    Subscription(Subscription&&) noexcept = default;
    Subscription& operator=(Subscription&&) noexcept = default;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    UpdateBatch poll();
    void reset();

  private:
    struct Mailbox;
    std::shared_ptr<Mailbox> mailbox_;
    explicit Subscription(std::shared_ptr<Mailbox>);
    friend class ApplicationController;
};
// In-process contract v1. Decorates the existing backend, never owns a second
// routing state machine. All methods except post() are control-thread confined.
// post() is a bounded, nonblocking handoff for NON-real-time adapters only.
class ApplicationController final : public Controller {
  public:
    explicit ApplicationController(std::unique_ptr<Controller>);
    ~ApplicationController() override;
    CommandResult submit(Command);
    struct PostResult {
        bool accepted;
        std::uint64_t request_id;
    };
    PostResult post(Command);
    Subscription subscribe(SubscriptionFilter = {});
    const Snapshot& snapshot() const override;
    std::string last_edit_error() const override;
    Result save(Profile, bool) override;
    Result update_buffer(const std::string&, const std::string&, bool, int) override;
    std::string create_profile(const std::string&, const std::string& = {}) override;
    Result remove_profile(const std::string&) override;
    Hole make_hole() override;
    Result activate(const std::string&, bool = false) override;
    void stop() override;
    Result schedule(const std::string&, int) override;
    void advance(int) override;
    Result update_endpoint(const Endpoint&) override;
    void fail_path(const std::string&, bool) override;
    void recover() override;
    void monitor(bool) override;
    Result check_level(const std::string&) override;
    void wait_for_devices(bool) override;
    Result beep(const std::string&) override;
    void cancel_beep() override;
    void clear_diagnostics() override;
    void set_logging(bool) override;
    void scan(bool, bool = false, ScanScope = ScanScope::Update, const std::string& = {},
              const ScanOptions& = {}) override;
    Result scanner_action(const ScannerActionRequest&) override;

  private:
    struct Posted {
        std::uint64_t id;
        Command command;
    };
    struct Pending {
        CommandResult result;
        std::string target;
        int remaining_ms = 30000;
        std::uint64_t starting_event_sequence = 0;
    };
    std::unique_ptr<Controller> backend_;
    std::shared_ptr<const Snapshot> published_state_;
    std::thread::id owner_;
    std::vector<std::weak_ptr<Subscription::Mailbox>> subscribers_;
    std::vector<Pending> pending_;
    std::mutex posted_mutex_;
    std::deque<Posted> posted_;
    std::uint64_t next_request_ = 1, published_revision_ = UINT64_MAX, last_event_ = 0;
    void require_owner() const;
    std::uint64_t request_id();
    CommandResult execute(Command, std::uint64_t);
    void publish(std::optional<CommandResult> = {});
    void settle(int elapsed_ms);
    void cancel_pending(const std::string& command, const std::string& detail);
    Result record(const char*, Result);
};
} // namespace audiohole
