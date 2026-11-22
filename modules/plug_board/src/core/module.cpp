#include "module_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace audiohole::plug_board {
namespace {
CommandResult unavailable(const std::string& code, const std::string& message) {
    return {false, false, code, message, {}, {}, 0};
}
} // namespace

Module::Implementation::Implementation(ModuleOptions value) : options(std::move(value)) {
    icon_pixels = std::clamp(options.icon_pixels, 16u, 256u);
    published.store(std::make_shared<const ModuleSnapshot>());
    control = std::jthread([this] { run(); });
}
Module::Implementation::~Implementation() {
    closing = true;
    policy_stop.request_stop();
    wake.notify_all();
    if (control.joinable())
        control.join();
}

bool Module::Implementation::post(Task task) {
    std::unique_lock lock(inbox_mutex);
    if (closing || commands.size() >= 1024) {
        lock.unlock();
        if (task.cancel)
            task.cancel();
        return false;
    }
    commands.push_back(std::move(task));
    if (commands.back().interrupt_policy && in_flight_policy_stop)
        in_flight_policy_stop->request_stop();
    lock.unlock();
    wake.notify_one();
    return true;
}
void Module::Implementation::event(std::function<void()> task) {
    std::lock_guard lock(inbox_mutex);
    if (closing)
        return;
    if (events.size() >= 512) {
        ++dropped;
        resync_needed = true;
    } else
        events.push_back(std::move(task));
    wake.notify_one();
}

void Module::Implementation::observed(std::uint64_t generation, SessionSnapshot snapshot,
                                      std::string utc) {
    // The native monitoring worker invokes this, not its raw COM callback. Only
    // one full value snapshot is retained. Active hints survive replacement by a
    // later Inactive snapshot; none are turned into persistent PID identities.
    auto grouped = group_applications(snapshot);
    std::lock_guard lock(inbox_mutex);
    if (closing)
        return;
    if (!observation_inbox || observation_inbox->generation != generation)
        observation_inbox = ObservationInbox{generation, {}, utc, {}};
    auto& inbox = *observation_inbox;
    const auto milliseconds = utc_milliseconds(utc).value_or(0);
    for (const auto& app : grouped)
        if (!app.active_endpoint_ids.empty()) {
            if (inbox.active.size() >= 20000) {
                ++dropped;
                resync_needed = true;
                break;
            }
            inbox.active[app.observation_key] = {app.identity, app.observation_key,
                                                 app.display_name, milliseconds,
                                                 app.active_endpoint_ids};
        }
    inbox.snapshot = std::move(snapshot);
    inbox.utc = std::move(utc);
    wake.notify_one();
}

void Module::Implementation::run() {
    try {
        initialize();
    } catch (const std::exception& error) {
        phase = ModulePhase::ReadOnlyError;
        storage_result = {false, "initialization_failed", error.what()};
        log(Severity::Error, storage_result.code, storage_result.detail);
        publish();
    }
    while (!closing) {
        try {
            const bool inbox_work = pump_inboxes();
            Task command;
            std::function<void()> notification;
            {
                std::lock_guard lock(inbox_mutex);
                if (!commands.empty()) {
                    command = std::move(commands.front());
                    commands.pop_front();
                } else if (!events.empty()) {
                    notification = std::move(events.front());
                    events.pop_front();
                }
            }
            if (command.run) {
                command.run();
                continue;
            }
            if (notification) {
                notification();
                continue;
            }
            if (inbox_work)
                continue;
            const auto now = Clock::now();
            if (flush_due && now >= *flush_due) {
                flush_due.reset();
                flush();
                publish();
                continue;
            }
            if (recent_due && now >= *recent_due) {
                recent_due.reset();
                publish();
                continue;
            }
            if (!policy_pending.empty()) {
                apply_one();
                continue;
            }
            std::unique_lock lock(inbox_mutex);
            const auto ready = [&] {
                return closing || !commands.empty() || !events.empty() || observation_inbox ||
                       visible_inbox || inventory_inbox || profiles_inbox || profile_inbox ||
                       pixels_inbox || sample_inbox || native_inventory_inbox || resync_needed;
            };
            auto deadline = Clock::time_point::max();
            if (flush_due)
                deadline = std::min(deadline, *flush_due);
            if (recent_due)
                deadline = std::min(deadline, *recent_due);
            if (deadline == Clock::time_point::max())
                wake.wait(lock, ready);
            else
                wake.wait_until(lock, deadline, ready);
        } catch (const std::exception& error) {
            log(Severity::Error, "worker_operation_failed", error.what());
            publish();
        }
    }
    shutdown();
}

bool Module::Implementation::pump_inboxes() {
    std::optional<bool> new_visible;
    std::optional<EndpointInventory> new_inventory;
    std::optional<std::vector<ProfileDescriptor>> new_profiles;
    std::optional<std::pair<std::optional<std::string>, std::uint64_t>> new_profile;
    std::optional<unsigned> new_pixels;
    std::optional<ObservationInbox> observation;
    bool sample = false, gap = false, native_inventory = false;
    {
        std::lock_guard lock(inbox_mutex);
        new_visible = std::exchange(visible_inbox, {});
        new_inventory = std::exchange(inventory_inbox, {});
        new_profiles = std::exchange(profiles_inbox, {});
        new_profile = std::exchange(profile_inbox, {});
        new_pixels = std::exchange(pixels_inbox, {});
        observation = std::exchange(observation_inbox, {});
        sample = std::exchange(sample_inbox, false);
        native_inventory = std::exchange(native_inventory_inbox, false);
        gap = std::exchange(resync_needed, false);
    }
    const bool changed = new_visible || new_inventory || new_profiles || new_profile ||
                         new_pixels || observation || sample || gap || native_inventory;
    if (new_visible)
        set_visibility(*new_visible);
    if (native_inventory && options.standalone_inventory)
        refresh_standalone_inventory();
    else if (native_inventory && options.resolve_system_defaults && !new_inventory)
        // Host endpoint IDs/bindings stay authoritative. A default-role event
        // refreshes only the three System destinations on this control thread.
        update_inventory(inventory);
    if (new_inventory)
        update_inventory(std::move(*new_inventory));
    if (new_profiles)
        profiles = std::move(*new_profiles);
    if (new_profile)
        activate_profile(std::move(new_profile->first), new_profile->second);
    if (new_pixels && *new_pixels >= 16 && *new_pixels <= 256 && icon_pixels != *new_pixels) {
        icon_pixels = *new_pixels;
        image_requests.clear();
        std::vector<std::string> ids;
        for (const auto& [id, app] : applications)
            if (app.observation && ids.size() < 256)
                ids.push_back(id);
        request_images(ids);
    }
    if (observation)
        ingest(std::move(*observation));
    if (gap) {
        log(Severity::Warning, "event_gap",
            "Native notifications exceeded the queue bound; observations need reconciliation.");
        if (visible && backend)
            backend->refresh_observation();
    }
    if (sample && backend)
        static_cast<void>(backend->metrics(true));
    if (changed)
        publish();
    return changed;
}

void Module::Implementation::initialize() {
    profiles = options.profiles;
    active_profile = options.initial_active_profile;
    if (!options.backend)
        throw std::runtime_error("No platform backend was supplied.");
    backend = options.backend();
    if (!backend)
        throw std::runtime_error("The platform backend factory returned no backend.");
    simulation = backend->simulated();
    platform_environment = backend->environment();
    BackendCallbacks callbacks;
    callbacks.observations = [this](auto generation, auto snapshot, auto utc) {
        observed(generation, std::move(snapshot), std::move(utc));
    };
    callbacks.inventory_changed = [this] {
        std::lock_guard lock(inbox_mutex);
        if (!closing) {
            native_inventory_inbox = true;
            wake.notify_one();
        }
    };
    callbacks.process_started = [this](auto evidence) {
        event([this, evidence = std::move(evidence)] { process_started(evidence); });
    };
    callbacks.icon_ready = [this](auto generation, auto key, auto image) {
        if (image.rgba.size() > 256 * 256 * 4) {
            ++dropped;
            return;
        }
        event([this, generation, key = std::move(key), image = std::move(image)]() mutable {
            icon_ready(generation, key, std::move(image));
        });
    };
    callbacks.diagnostic = [this](auto diagnostic) {
        event([this, diagnostic = std::move(diagnostic)] {
            log(diagnostic.severity, diagnostic.code, diagnostic.message, diagnostic.application_id,
                diagnostic.details, diagnostic.operation_id);
        });
    };
    if (options.memory_only && !backend->simulated())
        throw std::invalid_argument("In-memory Plug Board requires a simulated backend.");
    const auto initialized = backend->initialize(std::move(callbacks));
    backend->set_icon_cache_root(options.memory_only ? std::filesystem::path{}
                                                     : options.local_root);
    if (!initialized.ok)
        log(Severity::Warning, initialized.code, initialized.detail);
    capability = backend->capability();
    storage = options.memory_only ? make_memory_storage()
                                  : options.storage(options.data_root / "library.json");
    const auto loaded = storage->read();
    storage_result = loaded.result;
    if (!loaded.result.ok)
        throw std::runtime_error(loaded.result.detail);
    expected_bytes = loaded.bytes;
    if (loaded.bytes) {
        library = std::make_shared<const Library>(parse_library(*loaded.bytes));
        // Apply the durability barrier to existing/manual documents before any
        // startup policy work, including after an uncertain previous shutdown.
        const auto confirmed = commit(*library);
        if (!confirmed.ok)
            throw std::runtime_error(confirmed.detail);
    } else {
        const auto result = commit(make_library(new_id("board"), utc_timestamp()));
        if (!result.ok)
            throw std::runtime_error(result.detail);
    }
    selected = library->default_board_id;
    if (active_profile && library->profile_associations.contains(*active_profile))
        selected = library->profile_associations.at(*active_profile);
    load_local_state();
    rebuild_identity_index();
    phase = ModulePhase::Ready;
    log(Severity::Information, "module_started",
        backend->simulated()
            ? "Simulation backend initialized; no Windows routing is performed."
            : "Plug Board initialized. Discovery waits until the view is visible.");
    if (options.standalone_inventory)
        refresh_standalone_inventory();
    reconcile(false);
    publish();
}

void Module::Implementation::shutdown() {
    phase = ModulePhase::Stopping;
    publish();
    OperationResult result{
        true, "shutdown_complete",
        "Plug Board stopped cleanly; persisted Windows preferences were retained."};
    try {
        if (visible)
            set_visibility(false);
        if (backend) {
            backend->shutdown();
            last_metrics = backend->metrics(false);
        }
        flush();
        if (catalog_dirty || history_dirty || diagnostics_dirty)
            result = {false, "shutdown_storage_incomplete",
                      "Some local changes could not be flushed; diagnostics retain the failure."};
        log(Severity::Information, "module_stopped", result.detail);
        flush();
        backend.reset();
        storage.reset();
        device_storage.reset();
    } catch (const std::exception& error) {
        result = {false, "shutdown_failed", error.what()};
        log(Severity::Error, result.code, result.detail);
        if (backend) {
            try {
                last_metrics = backend->metrics(false);
            } catch (...) {
            }
        }
        try {
            flush();
        } catch (...) {
        }
        backend.reset();
        storage.reset();
        device_storage.reset();
    }
    std::deque<Task> cancelled;
    {
        std::lock_guard lock(inbox_mutex);
        cancelled.swap(commands);
        events.clear();
        observation_inbox.reset();
    }
    for (auto& task : cancelled)
        if (task.cancel)
            task.cancel();
    phase = ModulePhase::Stopped;
    publish();
    shutdown_promise.set_value(result);
}

Module::Module(ModuleOptions options)
    : implementation_(std::make_unique<Implementation>(std::move(options))) {}
Module::~Module() = default;
std::shared_ptr<const ModuleSnapshot> Module::snapshot() const {
    return implementation_->published.load();
}
std::future<CommandResult> Module::submit(Command command, std::optional<std::uint64_t> expected,
                                          std::optional<std::string> expected_board) {
    const bool interrupts_policy = !std::holds_alternative<ClearInactiveApplications>(command) &&
                                   !std::holds_alternative<SetInactiveExemptions>(command) &&
                                   !std::holds_alternative<RenameApplication>(command);
    auto promise = std::make_shared<std::promise<CommandResult>>();
    auto future = promise->get_future();
    auto* state = implementation_.get();
    state->post({[state, promise, command = std::move(command), expected, expected_board] {
                     try {
                         promise->set_value(state->execute(command, expected, expected_board));
                     } catch (const std::exception& error) {
                         promise->set_value(unavailable("command_failed", error.what()));
                     }
                 },
                 [promise] {
                     promise->set_value(unavailable(
                         "unavailable", "Plug Board is stopping or its command queue is full."));
                 },
                 interrupts_policy});
    return future;
}
void Module::set_visible(bool visible) {
    std::lock_guard lock(implementation_->inbox_mutex);
    implementation_->visible_inbox = visible;
    implementation_->wake.notify_one();
}
void Module::set_inventory(EndpointInventory inventory) {
    if (inventory.endpoints.size() > 20000)
        throw std::invalid_argument("Endpoint inventory exceeds its bound.");
    std::lock_guard lock(implementation_->inbox_mutex);
    implementation_->inventory_inbox = std::move(inventory);
    implementation_->wake.notify_one();
}
void Module::set_profiles(std::vector<ProfileDescriptor> profiles) {
    if (profiles.size() > 20000)
        throw std::invalid_argument("Profile inventory exceeds its bound.");
    std::lock_guard lock(implementation_->inbox_mutex);
    implementation_->profiles_inbox = std::move(profiles);
    implementation_->wake.notify_one();
}
void Module::profile_activated(std::optional<std::string> profile, std::uint64_t generation) {
    std::lock_guard lock(implementation_->inbox_mutex);
    if (!implementation_->profile_inbox || generation >= implementation_->profile_inbox->second)
        implementation_->profile_inbox = std::make_pair(std::move(profile), generation);
    implementation_->wake.notify_one();
}
void Module::profile_deleted(const std::string& profile) {
    auto* state = implementation_.get();
    state->event([state, profile] {
        if (!state->library || !state->library->profile_associations.contains(profile))
            return;
        auto candidate = *state->library;
        candidate.profile_associations.erase(profile);
        ++candidate.revision;
        if (state->commit(std::move(candidate)).ok && state->active_profile == profile)
            state->activate_profile({}, state->profile_generation + 1);
        state->publish();
    });
}
void Module::set_icon_pixels(unsigned pixels) {
    std::lock_guard lock(implementation_->inbox_mutex);
    implementation_->pixels_inbox = pixels;
    implementation_->wake.notify_one();
}
void Module::request_images(std::vector<std::string> ids) {
    if (ids.size() > 256)
        ids.resize(256);
    auto* state = implementation_.get();
    state->event([state, ids = std::move(ids)] { state->request_images(ids); });
}
void Module::sample_diagnostics() {
    std::lock_guard lock(implementation_->inbox_mutex);
    implementation_->sample_inbox = true;
    implementation_->wake.notify_one();
}
void Module::record_view_event(std::string code, std::string message, Document details,
                               Severity severity) {
    auto* state = implementation_.get();
    state->event([state, code = std::move(code), message = std::move(message),
                  details = std::move(details), severity]() mutable {
        if (!details.is_object())
            details = {{"value", std::move(details)}};
        details["source"] = "socket_view";
        state->log(severity, std::move(code), std::move(message), {}, std::move(details));
        state->publish();
    });
}
std::uint64_t
Module::subscribe(std::function<void(std::shared_ptr<const ModuleSnapshot>)> observer) {
    std::lock_guard lock(implementation_->observers_mutex);
    const auto id = implementation_->next_observer++;
    implementation_->observers.emplace(id, std::move(observer));
    return id;
}
void Module::unsubscribe(std::uint64_t id) {
    std::lock_guard lock(implementation_->observers_mutex);
    implementation_->observers.erase(id);
}
std::future<CommandResult> Module::reload_library() {
    auto promise = std::make_shared<std::promise<CommandResult>>();
    auto future = promise->get_future();
    auto* state = implementation_.get();
    state->post(
        {[state, promise] { promise->set_value(state->reload()); },
         [promise] { promise->set_value(unavailable("unavailable", "Plug Board is stopping.")); }});
    return future;
}
std::future<CommandResult> Module::restore_library(const std::filesystem::path& path,
                                                   bool confirmed) {
    auto promise = std::make_shared<std::promise<CommandResult>>();
    auto future = promise->get_future();
    auto* state = implementation_.get();
    state->post(
        {[state, promise, path, confirmed] { promise->set_value(state->restore(path, confirmed)); },
         [promise] { promise->set_value(unavailable("unavailable", "Plug Board is stopping.")); }});
    return future;
}
std::future<OperationResult> Module::export_diagnostics(const std::filesystem::path& path,
                                                        bool sensitive, bool overwrite) {
    auto promise = std::make_shared<std::promise<OperationResult>>();
    auto future = promise->get_future();
    auto* state = implementation_.get();
    state->post(
        {[state, promise, path, sensitive, overwrite] {
             promise->set_value(state->export_to(path, sensitive, overwrite));
         },
         [promise] { promise->set_value({false, "unavailable", "Plug Board is stopping."}); }});
    return future;
}
std::shared_future<OperationResult> Module::begin_shutdown() {
    implementation_->closing = true;
    implementation_->policy_stop.request_stop();
    implementation_->wake.notify_all();
    return implementation_->shutdown_future;
}

} // namespace audiohole::plug_board
