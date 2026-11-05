#include "model/application_controller.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace audiohole {
namespace {
constexpr std::size_t maximum_subscribers = 16, maximum_mail = 128, maximum_posts = 64;
} // namespace
const char* command_id(const Command& command) {
    static constexpr std::array names{"routing.start",      "routing.stop",     "profile.load",
                                      "configuration.save", "devices.discover", "buffer.update",
                                      "status.get",         "devices.get",      "diagnostics.get",
                                      "beep.start",         "beep.cancel",      "scanner.action"};
    return names.at(command.index());
}
Baseline presentation_baseline(const Snapshot& state) {
    Baseline result{"idle", 0, state.revision};
    switch (state.state) {
    case State::Failed:
        result.expression = "error";
        result.priority = 100;
        break;
    case State::Degraded:
    case State::Recovering:
        result.expression = "concerned";
        result.priority = 80;
        break;
    case State::Starting:
    case State::Stopping:
        result.expression = "working";
        result.priority = 20;
        break;
    default:
        break; // Healthy Running is relaxed; never permanent WORKING.
    }
    if (state.scanning && result.priority < 20) {
        result.expression = "working";
        result.priority = 20;
    }
    return result;
}
struct Subscription::Mailbox {
    UpdateBatch pending;
    SubscriptionFilter filter;
    std::thread::id owner;
};
Subscription::Subscription(std::shared_ptr<Mailbox> mailbox) : mailbox_(std::move(mailbox)) {}
UpdateBatch Subscription::poll() {
    if (!mailbox_)
        return {};
    if (mailbox_->owner != std::this_thread::get_id())
        throw std::logic_error("Poll on control thread only");
    auto result = std::move(mailbox_->pending);
    mailbox_->pending = {};
    return result;
}
void Subscription::reset() {
    mailbox_.reset();
}
ApplicationController::ApplicationController(std::unique_ptr<Controller> backend)
    : backend_(std::move(backend)), owner_(std::this_thread::get_id()) {
    if (!backend_)
        throw std::invalid_argument("Application backend required");
    published_state_ = std::make_shared<const Snapshot>(backend_->snapshot());
    published_revision_ = published_state_->revision;
    if (!published_state_->events.empty())
        last_event_ = published_state_->events.back().sequence;
}
ApplicationController::~ApplicationController() {
    // Host joins producers before destroying this owner; there is no detached
    // worker. Surviving pull subscriptions receive an explicit closure marker.
    for (auto& weak : subscribers_)
        if (auto mailbox = weak.lock())
            mailbox->pending.closed = true;
}
void ApplicationController::require_owner() const {
    if (owner_ != std::this_thread::get_id())
        throw std::logic_error("Controller calls belong to the control thread");
}
std::uint64_t ApplicationController::request_id() {
    std::lock_guard lock(posted_mutex_);
    return next_request_++;
}
const Snapshot& ApplicationController::snapshot() const {
    require_owner();
    return backend_->snapshot();
}
std::string ApplicationController::last_edit_error() const {
    require_owner();
    return backend_->last_edit_error();
}
Subscription ApplicationController::subscribe(SubscriptionFilter filter) {
    require_owner();
    std::erase_if(subscribers_, [](const auto& item) { return item.expired(); });
    if (subscribers_.size() >= maximum_subscribers)
        return {};
    auto mailbox = std::make_shared<Subscription::Mailbox>();
    mailbox->owner = owner_;
    mailbox->filter = filter;
    subscribers_.push_back(mailbox);
    // A new subscriber receives current immutable state without replaying historic
    // results; its event history is already available in that snapshot.
    mailbox->pending.state = std::make_shared<const Snapshot>(snapshot());
    if (filter.telemetry)
        mailbox->pending.telemetry = Telemetry{snapshot().revision,
                                               {},
                                               {},
                                               {},
                                               {},
                                               snapshot().scan_percent,
                                               snapshot().capabilities.simulated};
    return Subscription(std::move(mailbox));
}
void ApplicationController::publish(std::optional<CommandResult> result) {
    const auto& current = snapshot();
    const bool changed = current.revision != published_revision_;
    std::shared_ptr<const Snapshot> immutable;
    std::vector<Event> events;
    if (changed) {
        immutable = std::make_shared<const Snapshot>(current);
        published_state_ = immutable;
        for (const auto& event : current.events)
            if (event.sequence > last_event_)
                events.push_back(event);
        if (!current.events.empty())
            last_event_ = std::max(last_event_, current.events.back().sequence);
        published_revision_ = current.revision;
    }
    for (auto iterator = subscribers_.begin(); iterator != subscribers_.end();) {
        auto mailbox = iterator->lock();
        if (!mailbox) {
            iterator = subscribers_.erase(iterator);
            continue;
        }
        ++iterator;
        auto& pending = mailbox->pending;
        if (result && published_state_)
            pending.state = published_state_;
        if (changed) {
            pending.state = immutable; // Latest state/telemetry replaces older samples.
            if (mailbox->filter.telemetry)
                pending.telemetry = Telemetry{current.revision,
                                              {},
                                              {},
                                              {},
                                              {},
                                              current.scan_percent,
                                              current.capabilities.simulated};
            if (mailbox->filter.events) {
                pending.events.insert(pending.events.end(), events.begin(), events.end());
                if (pending.events.size() > maximum_mail) {
                    pending.events.erase(pending.events.begin(),
                                         pending.events.end() - maximum_mail);
                    pending.gap = true;
                }
            }
        }
        if (result && mailbox->filter.results) {
            if (pending.results.size() == maximum_mail) {
                pending.results.erase(pending.results.begin());
                pending.gap = true;
            }
            pending.results.push_back(*result);
        }
    }
}
ApplicationController::PostResult ApplicationController::post(Command command) {
    std::unique_lock lock(posted_mutex_, std::try_to_lock);
    if (!lock || posted_.size() >= maximum_posts)
        return {false, 0};
    const auto id = next_request_++;
    posted_.push_back({id, std::move(command)});
    return {true, id}; // Queue acceptance, not command acceptance or completion.
}
CommandResult ApplicationController::submit(Command command) {
    require_owner();
    return execute(std::move(command), request_id());
}
void ApplicationController::cancel_pending(const std::string& command, const std::string& detail) {
    for (auto iterator = pending_.begin(); iterator != pending_.end();) {
        if (command.empty() || iterator->result.command == command) {
            auto result = iterator->result;
            result.outcome = Outcome::Cancelled;
            result.result = {false, detail};
            result.revision = snapshot().revision;
            iterator = pending_.erase(iterator);
            publish(result);
        } else
            ++iterator;
    }
}
CommandResult ApplicationController::execute(Command command, std::uint64_t id) {
    CommandResult response{id,
                           command_id(command),
                           Outcome::Rejected,
                           {false, "Unsupported command"},
                           snapshot().revision,
                           snapshot().capabilities.simulated,
                           {}};
    std::string target;
    bool asynchronous = false;
    int budget_ms = 30000;
    if (pending_.size() >= maximum_posts && (std::holds_alternative<StartRouting>(command) ||
                                             std::holds_alternative<DiscoverDevices>(command) ||
                                             std::holds_alternative<ScannerAction>(command) ||
                                             std::holds_alternative<StartBeep>(command) ||
                                             (std::holds_alternative<SaveConfiguration>(command) &&
                                              std::get<SaveConfiguration>(command).apply))) {
        response.result.detail = "Too many pending commands; retry after completion.";
        publish(response);
        return response;
    }
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            const auto caps = snapshot().capabilities;
            if constexpr (std::is_same_v<T, StartRouting>) {
                if (!caps.routing) {
                    response.result.detail = "Routing backend unavailable";
                    return;
                }
                response.result = backend_->activate(value.profile_id, value.arm_waiting);
                target = value.profile_id;
                asynchronous = response.result.ok;
                if (response.result.ok) {
                    cancel_pending("routing.stop", "Superseded by a new activation");
                    cancel_pending("routing.start", "Superseded by a new activation");
                    cancel_pending("configuration.save", "Superseded by activation");
                    cancel_pending("beep.start", "Activation cancelled Test Beep");
                }
            } else if constexpr (std::is_same_v<T, StopRouting>) {
                if (!caps.routing) {
                    response.result.detail = "Routing backend unavailable";
                    return;
                }
                backend_->stop();
                cancel_pending({}, "Stop cancelled the operation");
                response.result = {true, "Stop requested; await authoritative Stopped state"};
                asynchronous = true;
            } else if constexpr (std::is_same_v<T, LoadProfile>) {
                const auto* found = profile(snapshot(), value.profile_id);
                if (!found) {
                    response.result.detail = "Profile not found";
                    return;
                }
                response.loaded_profile = *found;
                response.result = {true, "Saved profile loaded without activation"};
            } else if constexpr (std::is_same_v<T, SaveConfiguration>) {
                if (value.apply && !caps.routing) {
                    response.result.detail =
                        "Apply unavailable; save configuration without applying";
                    return;
                }
                response.result = backend_->save(value.profile, value.apply);
                target = value.profile.id;
                asynchronous = value.apply && response.result.ok;
                if (asynchronous) {
                    cancel_pending("routing.stop", "Superseded by Apply");
                    cancel_pending("routing.start", "Superseded by Apply");
                    cancel_pending("configuration.save", "Superseded by Apply");
                    cancel_pending("beep.start", "Apply cancelled Test Beep");
                }
            } else if constexpr (std::is_same_v<T, DiscoverDevices>) {
                if (!caps.scanning || snapshot().scanning || snapshot().scanner.busy) {
                    response.result.detail = "Scanner unavailable or already busy";
                    return;
                }
                backend_->scan(true, value.advanced, value.scope, value.selected_device,
                               value.options);
                if (snapshot().scanner.available) {
                    target = std::to_string(snapshot().scanner.generation);
                    budget_ms = value.advanced
                                    ? static_cast<int>(value.options.system_timeout_ms + 180000)
                                    : 180000;
                }
                response.result = {snapshot().scanning,
                                   snapshot().scanning ? "Scan accepted" : "Scan rejected"};
                asynchronous = response.result.ok;
            } else if constexpr (std::is_same_v<T, ScannerAction>) {
                response.result = backend_->scanner_action(value.request);
                asynchronous = response.result.ok && snapshot().scanner.busy;
                target = std::to_string(snapshot().scanner.generation);
                budget_ms = value.request.kind == ScannerActionKind::Export ? 600000 : 30000;
            } else if constexpr (std::is_same_v<T, UpdateBuffer>) {
                response.result = backend_->update_buffer(
                    value.profile_id.empty() ? snapshot().active_profile : value.profile_id,
                    value.hole_id, value.reverse, value.milliseconds);
            } else if constexpr (std::is_same_v<T, StartBeep>) {
                if (!caps.beep || !snapshot().beep_destination.empty()) {
                    response.result.detail = "Beep backend unavailable or playback already active";
                    return;
                }
                response.result = backend_->beep(value.destination);
                asynchronous = response.result.ok;
                target = value.destination;
            } else if constexpr (std::is_same_v<T, CancelBeep>) {
                if (!caps.beep) {
                    response.result.detail = "Beep backend unavailable";
                    return;
                }
                backend_->cancel_beep();
                cancel_pending("beep.start", "Test Beep cancelled");
                response.result = {true, "Test Beep cancelled"};
            } else {
                response.result = {
                    true,
                    "Authoritative snapshot available; unavailable measurements remain explicit"};
            }
        },
        command);
    response.revision = snapshot().revision;
    response.outcome = response.result.ok ? (asynchronous ? Outcome::Accepted : Outcome::Completed)
                                          : Outcome::Rejected;
    if (asynchronous)
        pending_.push_back(
            {response, target, budget_ms,
             snapshot().events.empty() ? last_event_ : snapshot().events.back().sequence});
    publish(response);
    return response;
}
void ApplicationController::settle(int elapsed_ms) {
    const auto& current = snapshot();
    const auto previously_published_event = last_event_;
    for (auto iterator = pending_.begin(); iterator != pending_.end();) {
        auto result = iterator->result;
        bool complete = false, failed = false, cancelled = false;
        if (result.command == "routing.start" || result.command == "configuration.save") {
            cancelled = current.active_profile != iterator->target || !current.armed;
            failed = current.state == State::Failed;
            complete = current.state == State::Running || current.state == State::Ready ||
                       current.state == State::Degraded;
        } else if (result.command == "routing.stop")
            complete = current.state == State::Stopped;
        else if (result.command == "beep.start" && current.beep_destination.empty()) {
            complete =
                std::any_of(current.events.begin(), current.events.end(), [&](const auto& event) {
                    return event.sequence > iterator->starting_event_sequence &&
                           event.type == "beep-complete";
                });
            cancelled = !complete;
        } else if (result.command == "devices.discover") {
            complete = !current.scanning;
            if (current.scanner.available) {
                complete = std::to_string(current.scanner.completed_generation) == iterator->target;
                failed = complete && current.scanner.result != "completed" &&
                         current.scanner.result != "partially_completed" &&
                         current.scanner.result != "interrupted";
                cancelled = complete && current.scanner.result == "interrupted";
            }
            for (const auto& event : current.events)
                if (event.sequence > previously_published_event) {
                    failed = failed || event.type == "scan-failed";
                    cancelled = cancelled || event.type == "scan-cancelled";
                }
        } else if (result.command == "scanner.action") {
            complete = std::to_string(current.scanner.completed_generation) == iterator->target;
            failed = complete && current.scanner.result != "completed" &&
                     current.scanner.result != "partially_completed";
            cancelled = complete && current.scanner.result == "interrupted";
        }
        iterator->remaining_ms -= std::clamp(elapsed_ms, 0, 30000);
        if (complete || failed || cancelled || iterator->remaining_ms <= 0) {
            result.outcome = cancelled  ? Outcome::Cancelled
                             : failed   ? Outcome::Failed
                             : complete ? Outcome::Completed
                                        : Outcome::TimedOut;
            result.result = {complete && !failed && !cancelled,
                             current.capabilities.simulated
                                 ? "Simulated operation reached its authoritative terminal state; "
                                   "no hardware result"
                                 : "Operation reached terminal state"};
            if (result.outcome == Outcome::TimedOut)
                result.result = {false, "Command timed out; current state is authoritative. "
                                        "Timeout does not imply cancellation."};
            else if ((result.command == "scanner.action" || result.command == "devices.discover") &&
                     current.scanner.available && !current.scanner.detail.empty())
                result.result.detail = current.scanner.detail;
            result.revision = current.revision;
            iterator = pending_.erase(iterator);
            publish(result);
        } else
            ++iterator;
    }
}
Result ApplicationController::record(const char* name, Result result) {
    CommandResult response{request_id(),
                           name,
                           result.ok ? Outcome::Completed : Outcome::Rejected,
                           result,
                           snapshot().revision,
                           snapshot().capabilities.simulated,
                           {}};
    publish(response);
    return result;
}
Result ApplicationController::save(Profile value, bool apply) {
    return submit(SaveConfiguration{std::move(value), apply}).result;
}
Result ApplicationController::update_buffer(const std::string& profile_id,
                                            const std::string& hole_id, bool reverse,
                                            int milliseconds) {
    return submit(UpdateBuffer{hole_id, reverse, milliseconds, profile_id}).result;
}
Result ApplicationController::activate(const std::string& id, bool arm) {
    return submit(StartRouting{id, arm}).result;
}
void ApplicationController::stop() {
    (void)submit(StopRouting{});
}
Result ApplicationController::beep(const std::string& id) {
    return submit(StartBeep{id}).result;
}
void ApplicationController::cancel_beep() {
    (void)submit(CancelBeep{});
}
void ApplicationController::advance(int ms) {
    require_owner();
    std::deque<Posted> ready;
    {
        std::lock_guard lock(posted_mutex_);
        ready.swap(posted_);
    }
    for (auto& posted : ready)
        execute(std::move(posted.command), posted.id);
    backend_->advance(ms);
    settle(ms);
    publish();
}
std::string ApplicationController::create_profile(const std::string& name,
                                                  const std::string& from) {
    require_owner();
    auto id = backend_->create_profile(name, from);
    record("profile.create",
           {!id.empty(), id.empty() ? backend_->last_edit_error() : "Profile created: " + id});
    return id;
}
Result ApplicationController::remove_profile(const std::string& id) {
    require_owner();
    return record("profile.remove", backend_->remove_profile(id));
}
Hole ApplicationController::make_hole() {
    require_owner();
    return backend_->make_hole();
}
Result ApplicationController::schedule(const std::string& id, int ms) {
    require_owner();
    return record("startup.schedule", backend_->schedule(id, ms));
}
Result ApplicationController::update_endpoint(const Endpoint& value) {
    require_owner();
    if (value.inspection) {
        const auto* original = endpoint(snapshot(), value.id);
        if (!original || value.windows_name != original->windows_name ||
            value.capture != original->capture)
            return record("device.update", {false, "Device identity and direction are read-only."});
        ScannerActionRequest request;
        request.kind = ScannerActionKind::EditMetadata;
        request.logical_id = value.id;
        request.display_name = value.display_name;
        request.notes = value.notes;
        request.expected_path = value.inspection->path;
        request.expected_bytes = value.inspection->edit_bytes;
        return submit(ScannerAction{std::move(request)}).result;
    }
    return record("device.update", backend_->update_endpoint(value));
}
void ApplicationController::fail_path(const std::string& id, bool reverse) {
    require_owner();
    backend_->fail_path(id, reverse);
    publish();
}
void ApplicationController::recover() {
    require_owner();
    backend_->recover();
    publish();
}
void ApplicationController::monitor(bool enabled) {
    require_owner();
    backend_->monitor(enabled);
    publish();
}
void ApplicationController::clear_diagnostics() {
    require_owner();
    backend_->clear_diagnostics();
    record("diagnostics.clear", {true, "Session diagnostics cleared"});
}
void ApplicationController::scan(bool enabled, bool advanced, ScanScope scope,
                                 const std::string& selected, const ScanOptions& options) {
    require_owner();
    if (enabled)
        (void)submit(DiscoverDevices{advanced, scope, selected, options});
    else {
        backend_->scan(false);
        // Real scanner cancellation completes only after the worker cleans up.
        if (!snapshot().scanner.available)
            cancel_pending("devices.discover", "Scan cancelled");
        publish();
    }
}
Result ApplicationController::scanner_action(const ScannerActionRequest& request) {
    return submit(ScannerAction{request}).result;
}
} // namespace audiohole
