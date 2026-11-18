#include "module_state.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace audiohole::plug_board {
namespace {
std::string identity_key(const ApplicationIdentity& identity) {
    return std::to_string(static_cast<int>(identity.kind)) + ':' + identity.value;
}

std::optional<std::string> resolved_target(const EndpointInventory& inventory,
                                           const EndpointTarget& target) {
    if (!target.logical_device_id)
        return {};
    const auto found = std::find_if(
        inventory.endpoints.begin(), inventory.endpoints.end(), [&](const auto& endpoint) {
            return endpoint.logical_id == *target.logical_device_id && endpoint.playback &&
                   endpoint.available && !endpoint.ambiguous && !endpoint.windows_id.empty();
        });
    return found == inventory.endpoints.end() ? std::nullopt : std::optional(found->windows_id);
}
} // namespace

void Module::Implementation::publish() {
    auto snapshot = std::make_shared<ModuleSnapshot>();
    snapshot->publication = ++publication;
    snapshot->phase = phase;
    snapshot->library = library;
    snapshot->selected_board_id = selected;
    snapshot->inventory = inventory;
    snapshot->profiles = profiles;
    snapshot->active_profile_id = active_profile;
    snapshot->visible = visible;
    snapshot->capability = capability;
    snapshot->storage = storage_result;
    snapshot->diagnostic_storage = diagnostic_storage_result;
    snapshot->diagnostics_write_pending = diagnostics_dirty;
    snapshot->last_command = last_command;
    snapshot->catalog_write_pending = catalog_dirty;
    snapshot->observed_utc = utc_timestamp();
    snapshot->library_commits = library_commits;
    snapshot->diagnostic_flushes = diagnostic_flushes;
    snapshot->callbacks_dropped = dropped.load();
    snapshot->diagnostics.assign(diagnostics.begin(), diagnostics.end());
    snapshot->simulated = simulation;
    snapshot->environment = platform_environment;
    if (backend)
        last_metrics = backend->metrics(false);
    snapshot->platform_metrics = last_metrics;
    recent_due.reset();
    const auto now = Clock::now();
    const auto* board = library ? find_board(*library, selected) : nullptr;
    if (board)
        for (const auto& record : library->applications) {
            if (backend && backend->internal_application(record.identity))
                continue;
            auto& runtime = applications[record.id];
            ApplicationView view;
            view.id = record.id;
            view.display_name = application_display_name(record.display_name);
            view.socket_id = assigned_socket(board->working, record.id);
            view.observation = runtime.observation;
            view.observation_fresh = runtime.fresh && visible;
            view.history = runtime.history;
            view.policy = runtime.policy;
            const bool active = view.observation_fresh && runtime.observation &&
                                !runtime.observation->active_endpoint_ids.empty();
            view.activity = active ? Activity::Active : Activity::Inactive;
            if (!active && runtime.last_active_monotonic) {
                const auto age = std::max(std::chrono::milliseconds(0),
                                          std::chrono::duration_cast<std::chrono::milliseconds>(
                                              now - *runtime.last_active_monotonic));
                view.inactive_seconds = age.count() / 1000;
                const auto window =
                    std::chrono::seconds(board->working.settings.recently_active_seconds);
                if (age < window) {
                    view.activity = Activity::RecentlyActive;
                    const auto due = *runtime.last_active_monotonic + window;
                    if (!recent_due || due < *recent_due)
                        recent_due = due;
                }
            }
            // Hidden history is never promoted to a live playback observation.
            if (!view.observation_fresh)
                view.observed_routing =
                    runtime.observation ? ObservationState::Stale : ObservationState::Unknown;
            else if (runtime.observation) {
                const auto& observed = *runtime.observation;
                // An observed Active session establishes known playback even
                // if discovery elsewhere was partial or role defaults differ.
                // Only complete evidence may establish a destination match.
                if (!observed.active_endpoint_ids.empty())
                    view.observed_routing = ObservationState::Observed;
                if (observed.active_endpoint_ids.size() > 1)
                    view.observed_routing = ObservationState::Multiple;
                else if (observed.complete && observed.active_endpoint_ids.size() == 1) {
                    const auto* socket = socket_for(record.id);
                    std::optional<std::string> expected;
                    if (socket) {
                        expected = resolved_target(inventory, socket->endpoint);
                        // Different Windows role defaults cannot identify one
                        // expected physical output for an untyped audio session.
                        if (!socket->endpoint.logical_device_id &&
                            inventory.default_windows_endpoints[0] &&
                            inventory.default_windows_endpoints[0] ==
                                inventory.default_windows_endpoints[1] &&
                            inventory.default_windows_endpoints[1] ==
                                inventory.default_windows_endpoints[2])
                            expected = inventory.default_windows_endpoints[0];
                    }
                    if (expected)
                        view.observed_routing = *expected == observed.active_endpoint_ids.front()
                                                    ? ObservationState::Matches
                                                    : ObservationState::Differs;
                }
            }
            if (const auto image = images.find(record.id); image != images.end())
                view.image = image->second.image;
            snapshot->applications.push_back(std::move(view));
        }
    // Recent-age deadlines have no purpose when no application list is visible.
    if (!visible)
        recent_due.reset();
    published.store(snapshot);
    std::vector<std::function<void(std::shared_ptr<const ModuleSnapshot>)>> listeners;
    {
        std::lock_guard lock(observers_mutex);
        for (const auto& [id, listener] : observers)
            listeners.push_back(listener);
    }
    for (auto& listener : listeners) {
        try {
            listener(snapshot);
        } catch (...) { /* A view must not terminate the service. */
        }
    }
}

const Socket* Module::Implementation::socket_for(const std::string& app) const {
    const auto* board = library ? find_board(*library, selected) : nullptr;
    if (!board)
        return nullptr;
    const auto id = assigned_socket(board->working, app);
    const auto socket = std::find_if(board->working.sockets.begin(), board->working.sockets.end(),
                                     [&](const auto& value) { return value.id == id; });
    return socket == board->working.sockets.end() ? nullptr : &*socket;
}
bool Module::Implementation::managed(const ApplicationRecord& app) const {
    if (backend && backend->internal_application(app.identity))
        return false;
    const auto* board = library ? find_board(*library, selected) : nullptr;
    return app.policy_managed || (board && board->working.application_assignments.contains(app.id));
}

void Module::Implementation::set_visibility(bool value) {
    if (visible == value)
        return;
    if (!value) {
        const auto now = utc_milliseconds(utc_timestamp()).value();
        for (auto& [id, app] : applications) {
            if (app.fresh && app.was_active) {
                app.history.last_active_utc_ms = now;
                app.last_active_monotonic = Clock::now();
                history_dirty = true;
            }
            app.fresh = false;
        }
    }
    visible = value;
    // Recheck native sources without blanking an icon learned before the app
    // closed. The native worker also retains its best taskbar pixels on disk.
    if (value)
        image_requests.clear();
    ++discovery_generation;
    if (backend)
        backend->observe(value && phase == ModulePhase::Ready, inventory, discovery_generation);
    if (value && phase == ModulePhase::Ready)
        reconcile(false);
    log(Severity::Information, value ? "view_opened" : "view_hidden",
        value ? "Visible session monitoring requested."
              : "Session monitoring stopped; desired routing and history were retained.");
}

void Module::Implementation::update_inventory(EndpointInventory value) {
    if (backend && options.resolve_system_defaults)
        value.default_windows_endpoints = backend->default_endpoints();
    if (value.revision < inventory.revision) {
        log(Severity::Warning, "stale_inventory",
            "An older endpoint inventory update was ignored.");
        return;
    }
    std::set<std::string> ids;
    for (const auto& endpoint : value.endpoints) {
        if (endpoint.logical_id.empty() || !ids.insert(endpoint.logical_id).second) {
            log(Severity::Error, "invalid_inventory",
                "Endpoint inventory contains an empty or duplicate logical ID.");
            return;
        }
    }
    // Keep names for unavailable references; a missing entry never becomes an
    // arbitrary replacement binding and never deletes a socket's desired ID.
    for (const auto& old : inventory.endpoints)
        if (!ids.contains(old.logical_id)) {
            auto missing = old;
            missing.available = false;
            value.endpoints.push_back(std::move(missing));
        }
    auto bindings = [](const EndpointInventory& list) {
        std::map<std::string, std::tuple<std::string, bool, bool, bool>> result;
        for (const auto& endpoint : list.endpoints)
            result.emplace(endpoint.logical_id,
                           std::make_tuple(endpoint.windows_id, endpoint.available,
                                           endpoint.ambiguous, endpoint.playback));
        return result;
    };
    auto watched_ids = [](const EndpointInventory& list) {
        std::set<std::string> result;
        for (const auto& endpoint : list.endpoints)
            if (endpoint.available && !endpoint.ambiguous && endpoint.playback &&
                !endpoint.windows_id.empty())
                result.insert(endpoint.windows_id);
        return result;
    };
    const bool changed_bindings = bindings(value) != bindings(inventory);
    const bool changed_sessions = watched_ids(value) != watched_ids(inventory);
    inventory = std::move(value);
    if (changed_bindings)
        ++routing_generation;
    if (visible && backend && changed_sessions)
        backend->observe(true, inventory, ++discovery_generation);
    reconcile(false);
    log(Severity::Information, "inventory_updated",
        "Playback inventory reconciled; missing device associations were retained.");
}

void Module::Implementation::refresh_standalone_inventory() {
    if (!backend || !device_storage)
        return;
    const auto raw = backend->inventory();
    auto bindings = standalone_devices;
    for (const auto& endpoint : raw.endpoints)
        if (!bindings.contains(endpoint.windows_id))
            bindings[endpoint.windows_id] = new_id("device");
    bool durable = true;
    if (bindings != standalone_devices) {
        auto document = standalone_device_extra;
        document["format"] = "audiohole-plug-board-local-devices";
        document["schema_version"] = 1;
        document["bindings"] = bindings;
        const auto bytes = document.dump(2) + '\n';
        const auto result = device_storage->replace(device_bytes, bytes);
        durable = result.ok;
        if (durable) {
            device_bytes = bytes;
            standalone_devices = bindings;
            standalone_device_extra = std::move(document);
        } else
            log(Severity::Error, "device_binding_write_failed",
                "New output bindings could not be saved; their routing remains unavailable.");
    }
    EndpointInventory value;
    value.revision = inventory.revision + 1;
    value.default_windows_endpoints = raw.defaults;
    for (const auto& endpoint : raw.endpoints) {
        const bool known = standalone_devices.contains(endpoint.windows_id);
        value.endpoints.push_back({bindings.at(endpoint.windows_id), endpoint.display_name,
                                   endpoint.windows_id, endpoint.available && (durable || known),
                                   false, true});
    }
    update_inventory(std::move(value));
}

void Module::Implementation::activate_profile(std::optional<std::string> profile,
                                              std::uint64_t generation) {
    if (!library || generation <= profile_generation)
        return;
    profile_generation = generation;
    active_profile = std::move(profile);
    selected = active_profile && library->profile_associations.contains(*active_profile)
                   ? library->profile_associations.at(*active_profile)
                   : library->default_board_id;
    // A committed host activation chooses intent without enrolling newly
    // discovered, unassigned applications into policy management.
    reconcile(true);
    log(Severity::Information, "profile_activated",
        "A committed profile activation selected its associated or default Routing Board.");
}

std::string Module::Implementation::register_application(const ApplicationIdentity& identity,
                                                         const std::string& name,
                                                         const std::string& provisional_key) {
    if (!library)
        return {};
    if (backend && backend->internal_application(identity))
        return {};
    const auto key = identity_key(identity);
    bool provisional = identity.kind == IdentityKind::Unresolved;
    if (identity.kind != IdentityKind::Unresolved) {
        const auto found = identity_index.find(key);
        if (found != identity_index.end() && !found->second.empty())
            return found->second;
        provisional = found != identity_index.end();
    }
    if (provisional) {
        const auto found = provisional_ids.find(provisional_key);
        if (found != provisional_ids.end())
            return found->second;
    }
    if (library->applications.size() >= 20000) {
        log(Severity::Warning, "catalog_capacity",
            "The application catalog reached its bound; new unidentified sources were not merged "
            "into existing applications.");
        return {};
    }
    ApplicationRecord record;
    record.id = new_id("application");
    record.identity = provisional ? ApplicationIdentity{} : identity;
    record.display_name = name.empty() ? "Unidentified application" : name;
    const auto characters = std::count_if(record.display_name.begin(), record.display_name.end(),
                                          [](unsigned char byte) { return (byte & 0xC0) != 0x80; });
    if (record.display_name.size() > 1024 || characters > 256 ||
        record.display_name.find_first_not_of(' ') == std::string::npos ||
        std::any_of(record.display_name.begin(), record.display_name.end(),
                    [](unsigned char byte) { return byte < 32; }))
        record.display_name = "Application name unavailable";
    auto candidate = *library;
    candidate.applications.push_back(record);
    ++candidate.revision;
    validate_library(candidate);
    library = std::make_shared<const Library>(std::move(candidate));
    applications[record.id].identity_signature = catalog_signature(record);
    if (provisional)
        provisional_ids[provisional_key] = record.id;
    else
        identity_index[key] = record.id;
    catalog_dirty = true;
    mark_flush();
    log(Severity::Information, "application_discovered",
        "Application discovered in System without changing its Windows preference.", record.id);
    return record.id;
}

void Module::Implementation::ingest(ObservationInbox inbox) {
    if (!visible || phase != ModulePhase::Ready || inbox.generation != discovery_generation)
        return;
    const auto now = Clock::now();
    const auto wall = utc_milliseconds(utc_timestamp()).value();
    for (auto& [key, hint] : inbox.active) {
        const auto id =
            register_application(hint.identity, hint.display_name, hint.observation_key);
        if (id.empty())
            continue;
        auto& runtime = applications[id];
        if (!runtime.was_active || !runtime.last_active_monotonic) {
            runtime.history.last_active_utc_ms = hint.utc_ms;
            runtime.history.last_active_windows_endpoints = hint.endpoints;
            const auto age =
                hint.utc_ms <= wall ? std::min(wall - hint.utc_ms, std::int64_t(86400000)) : 0;
            runtime.last_active_monotonic = now - std::chrono::milliseconds(age);
            if (hint.utc_ms > wall)
                runtime.history.clock_uncertain = true;
            history_dirty = true;
            mark_flush();
        }
    }
    std::set<std::string> seen;
    std::map<std::string, ApplicationObservation> combined;
    for (auto observation : group_applications(inbox.snapshot)) {
        if (backend && backend->internal_application(observation.identity))
            continue;
        const auto id = register_application(observation.identity, observation.display_name,
                                             observation.observation_key);
        if (id.empty())
            continue;
        if (!combined.contains(id)) {
            observation.observation_key = "application:" + id;
            combined.emplace(id, std::move(observation));
        } else {
            auto& group = combined.at(id);
            group.sessions.insert(group.sessions.end(), observation.sessions.begin(),
                                  observation.sessions.end());
            for (const auto& endpoint : observation.active_endpoint_ids)
                if (std::find(group.active_endpoint_ids.begin(), group.active_endpoint_ids.end(),
                              endpoint) == group.active_endpoint_ids.end())
                    group.active_endpoint_ids.push_back(endpoint);
            for (const auto& token : observation.verified_processes)
                if (std::find(group.verified_processes.begin(), group.verified_processes.end(),
                              token) == group.verified_processes.end())
                    group.verified_processes.push_back(token);
            group.complete = group.complete && observation.complete;
            if (const auto* record = find_application(*library, id);
                record && observation.identity == record->identity)
                group.identity = observation.identity;
        }
    }
    for (auto& [id, observation] : combined) {
        std::sort(observation.active_endpoint_ids.begin(), observation.active_endpoint_ids.end());
        auto& runtime = applications[id];
        Document icon_sources = Document::array();
        for (const auto& session : observation.sessions)
            icon_sources.push_back(
                {session.icon_path,
                 session.process ? session.process->executable_path : std::string{},
                 session.process ? session.process->creation_time : 0});
        const auto icon_signature = icon_sources.dump();
        if (runtime.icon_signature != icon_signature) {
            image_requests.erase(id);
            runtime.icon_signature = icon_signature;
        }
        const bool active = !observation.active_endpoint_ids.empty();
        if (runtime.was_active != active || (!runtime.observation && active)) {
            if (active || runtime.was_active) {
                runtime.history.last_active_utc_ms = wall;
                runtime.last_active_monotonic = now;
                if (active)
                    runtime.history.last_active_windows_endpoints = observation.active_endpoint_ids;
                history_dirty = true;
                mark_flush();
            }
        }
        runtime.was_active = active;
        if (active &&
            runtime.history.last_active_windows_endpoints != observation.active_endpoint_ids) {
            runtime.history.last_active_windows_endpoints = observation.active_endpoint_ids;
            history_dirty = true;
            mark_flush();
        }
        runtime.fresh = true;
        runtime.processes = observation.verified_processes;
        runtime.observation = observation;
        seen.insert(id);
    }
    for (auto& [id, runtime] : applications)
        if (!seen.contains(id)) {
            if (runtime.was_active && inbox.snapshot.complete) {
                runtime.history.last_active_utc_ms = wall;
                runtime.last_active_monotonic = now;
                history_dirty = true;
                mark_flush();
            }
            runtime.was_active = false;
            runtime.fresh = inbox.snapshot.complete;
            if (inbox.snapshot.complete)
                runtime.observation.reset();
            // A stopped audio session is not proof the process terminated. Retain
            // verified tokens and let the policy adapter validate their live handle.
        }
    for (const auto& diagnostic : inbox.snapshot.diagnostics)
        log(Severity::Warning, "session_observation", diagnostic);
    reconcile(false);
}

void Module::Implementation::process_started(ProcessEvidence evidence) {
    if (!library || phase != ModulePhase::Ready)
        return;
    if (backend && backend->internal_application(evidence.identity))
        return;
    const auto found = identity_index.find(identity_key(evidence.identity));
    if (found == identity_index.end() || found->second.empty())
        return;
    const auto* record = find_application(*library, found->second);
    if (!record || !managed(*record))
        return;
    auto& runtime = applications[record->id];
    // A successful preference receipt does not end an assignment's lifetime.
    // Recheck the live instances on each managed start so an exited instance's
    // stale PID token cannot make the new instance's result look partial. This
    // is one event-driven snapshot, never a timer or a session-enumeration loop.
    std::vector<ApplicationIdentity> identities{record->identity};
    for (const auto& alias : record->verified_aliases)
        if (!backend->internal_application(alias))
            identities.push_back(alias);
    try {
        const auto live = backend->find_processes(identities);
        runtime.processes.clear();
        for (const auto& process : live) {
            const auto owner = identity_index.find(identity_key(process.identity));
            if (owner != identity_index.end() && owner->second == record->id)
                runtime.processes.push_back(process.token);
        }
    } catch (const std::exception& error) {
        log(Severity::Warning, "process_discovery_failed", error.what(), record->id);
    }
    std::erase_if(runtime.processes, [&](const auto& token) {
        return token.pid == evidence.token.pid && token != evidence.token;
    });
    if (std::find(runtime.processes.begin(), runtime.processes.end(), evidence.token) ==
        runtime.processes.end())
        runtime.processes.push_back(std::move(evidence.token));
    reconcile(false, record->id);
    log(Severity::Information, "managed_process_available",
        "A verified managed application process became available.", record->id);
    publish();
}

void Module::Implementation::icon_ready(std::uint64_t generation, const std::string& key,
                                        ApplicationImage image) {
    if (!visible || generation != discovery_generation)
        return;
    // An audio session can end before a delayed window-icon lookup completes.
    // Resolve its stable catalog request instead of requiring a live session.
    if (!key.starts_with("image:") || !library)
        return;
    const auto id = key.substr(6);
    if (!find_application(*library, id) || !image_requests.contains(id))
        return;
    const bool failed = !image.result.ok || !image.application_specific;
    const auto detail = image.result.detail;
    images[id] = {std::make_shared<const ApplicationImage>(std::move(image)), ++image_sequence};
    auto bytes = [&] {
        std::size_t total = 0;
        for (const auto& [app, cached] : images)
            total += cached.image->rgba.size();
        return total;
    };
    while (images.size() > 512 || bytes() > 32 * 1024 * 1024) {
        const auto oldest = std::min_element(images.begin(), images.end(),
                                             [](const auto& first, const auto& second) {
                                                 return first.second.used < second.second.used;
                                             });
        image_requests.erase(oldest->first);
        images.erase(oldest);
    }
    if (failed)
        log(Severity::Warning, "application_icon_fallback",
            detail.empty()
                ? "The real application icon was unavailable; a labeled generic fallback is used."
                : detail,
            id);
    publish();
}

void Module::Implementation::request_images(const std::vector<std::string>& ids) {
    if (!visible || !backend)
        return;
    std::vector<ApplicationObservation> requests;
    for (const auto& id : ids) {
        if (auto image = images.find(id); image != images.end())
            image->second.used = ++image_sequence;
        const auto found = applications.find(id);
        const auto signature =
            std::to_string(discovery_generation) + ':' + std::to_string(icon_pixels) + ':' +
            (found == applications.end() ? std::string{} : found->second.icon_signature);
        if (image_requests.contains(id) && image_requests.at(id) == signature)
            continue;
        const auto before = requests.size();
        if (found != applications.end() && found->second.observation)
            requests.push_back(*found->second.observation);
        else if (library) {
            const auto* record = find_application(*library, id);
            if (record) {
                ApplicationObservation app;
                app.identity = record->identity;
                app.display_name = record->display_name;
                requests.push_back(std::move(app));
            }
        }
        if (requests.size() != before) {
            requests.back().observation_key = "image:" + id;
            image_requests[id] = signature;
        }
    }
    if (!requests.empty())
        backend->request_icons(requests, icon_pixels, discovery_generation);
}

std::string Module::Implementation::fingerprint(const std::string& id, const EndpointTarget& target,
                                                const std::optional<std::string>& windows) const {
    Document tokens = Document::array();
    const auto found = applications.find(id);
    if (found != applications.end()) {
        auto ordered = found->second.processes;
        std::sort(ordered.begin(), ordered.end(), [](const auto& first, const auto& second) {
            return std::tie(first.pid, first.creation_time, first.executable_path,
                            first.app_user_model_id) < std::tie(second.pid, second.creation_time,
                                                                second.executable_path,
                                                                second.app_user_model_id);
        });
        for (const auto& token : ordered)
            tokens.push_back(
                {token.pid, token.creation_time, token.executable_path, token.app_user_model_id});
    }
    return Document{{"logical", target.logical_device_id ? Document(*target.logical_device_id)
                                                         : Document(nullptr)},
                    {"windows", windows ? Document(*windows) : Document(nullptr)},
                    {"tokens", std::move(tokens)}}
        .dump();
}

void Module::Implementation::seek_pending() {
    if (!library || !backend || !capability.ok)
        return;
    std::vector<ApplicationIdentity> identities;
    for (const auto& record : library->applications) {
        auto& runtime = applications[record.id];
        if (managed(record) && runtime.processes.empty() &&
            record.identity.kind != IdentityKind::Unresolved &&
            record.identity.kind != IdentityKind::SystemSounds &&
            runtime.last_seek_generation != routing_generation) {
            runtime.last_seek_generation = routing_generation;
            identities.push_back(record.identity);
            for (const auto& alias : record.verified_aliases)
                if (!backend->internal_application(alias))
                    identities.push_back(alias);
        }
    }
    if (identities.empty())
        return;
    // Subscribe to every managed identity before the initial snapshot. Keeping
    // accepted assignments subscribed closes the restart gap while hidden.
    update_watcher();
    std::vector<ProcessEvidence> processes;
    try {
        processes = backend->find_processes(identities);
    } catch (const std::exception& error) {
        log(Severity::Warning, "process_discovery_failed", error.what());
    }
    for (auto evidence : processes) {
        const auto found = identity_index.find(identity_key(evidence.identity));
        if (found == identity_index.end() || found->second.empty())
            continue;
        auto& tokens = applications[found->second].processes;
        if (std::find(tokens.begin(), tokens.end(), evidence.token) == tokens.end())
            tokens.push_back(std::move(evidence.token));
    }
}

void Module::Implementation::reconcile(bool force, const std::string& only) {
    if (!library || !backend || phase != ModulePhase::Ready)
        return;
    if (force) {
        ++routing_generation;
        watcher_dirty = true;
    }
    if (storage_result.ok && !catalog_dirty)
        seek_pending();
    for (const auto& record : library->applications) {
        if (!only.empty() && record.id != only)
            continue;
        auto& runtime = applications[record.id];
        if (!managed(record)) {
            runtime.policy.state = PolicyState::NotManaged;
            continue;
        }
        if (!storage_result.ok || catalog_dirty) {
            runtime.attempted_fingerprint.clear();
            runtime.policy.state = PolicyState::PendingStorage;
            runtime.policy.detail =
                "Routing is withheld until the working configuration is durably recorded.";
            continue;
        }
        const auto* socket = socket_for(record.id);
        if (!socket)
            continue;
        const auto target = resolved_target(inventory, socket->endpoint);
        if (record.identity.kind == IdentityKind::SystemSounds ||
            record.identity.kind == IdentityKind::Unresolved) {
            runtime.policy.state = PolicyState::Unsupported;
            runtime.policy.detail = "This source has no unambiguous application process target.";
            continue;
        }
        if (runtime.observation &&
            std::any_of(runtime.observation->sessions.begin(), runtime.observation->sessions.end(),
                        [](const auto& session) { return session.cross_process; })) {
            runtime.policy.state = PolicyState::Unsupported;
            runtime.policy.detail =
                "A shared-process audio session does not identify every stream owner.";
            continue;
        }
        if (socket->endpoint.logical_device_id && !target) {
            runtime.attempted_fingerprint.clear();
            runtime.policy.state = PolicyState::PendingDevice;
            runtime.policy.detail = "The assigned playback device is unavailable or ambiguous; no "
                                    "substitute was selected.";
            continue;
        }
        if (!capability.ok) {
            runtime.policy.state = PolicyState::Unsupported;
            runtime.policy.detail = capability.detail;
            continue;
        }
        if (runtime.processes.empty()) {
            runtime.policy.state = PolicyState::PendingProcess;
            runtime.policy.detail =
                "Desired routing is saved and awaits a verified application process.";
            continue;
        }
        const auto key = fingerprint(record.id, socket->endpoint, target);
        if (force || runtime.attempted_fingerprint != key)
            policy_pending.insert(record.id);
    }
    update_watcher();
}

void Module::Implementation::update_watcher() {
    if (!backend || !library)
        return;
    std::vector<ApplicationIdentity> identities;
    if (capability.ok && storage_result.ok && !catalog_dirty)
        for (const auto& record : library->applications) {
            if (managed(record) && record.identity.kind != IdentityKind::Unresolved &&
                record.identity.kind != IdentityKind::SystemSounds) {
                identities.push_back(record.identity);
                for (const auto& alias : record.verified_aliases)
                    if (!backend->internal_application(alias))
                        identities.push_back(alias);
            }
        }
    if (identities != watched || watcher_dirty) {
        watched = identities;
        backend->watch_processes(identities);
        watcher_dirty = false;
    }
}

void Module::Implementation::apply_one() {
    const auto id = *policy_pending.begin();
    policy_pending.erase(policy_pending.begin());
    if (!library || !backend || phase != ModulePhase::Ready || !storage_result.ok || catalog_dirty)
        return;
    const auto* socket = socket_for(id);
    const auto* record = find_application(*library, id);
    if (!socket || !record || !managed(*record))
        return;
    auto& runtime = applications[id];
    const auto target = resolved_target(inventory, socket->endpoint);
    if ((socket->endpoint.logical_device_id && !target) || runtime.processes.empty()) {
        reconcile(false, id);
        return;
    }
    const auto key = fingerprint(id, socket->endpoint, target);
    runtime.policy.state = PolicyState::Applying;
    publish();
    std::stop_source cancellation;
    std::stop_callback shutdown_cancel(policy_stop.get_token(),
                                       [&] { cancellation.request_stop(); });
    {
        std::lock_guard lock(inbox_mutex);
        in_flight_policy_stop = cancellation;
    }
    LivePolicyReport result;
    try {
        result = backend->apply_policy(runtime.processes, target, routing_generation,
                                       cancellation.get_token());
    } catch (const std::exception& error) {
        result.state = PolicyState::Rejected;
        result.detail = error.what();
    }
    {
        std::lock_guard lock(inbox_mutex);
        in_flight_policy_stop.reset();
    }
    result.target_windows_id = target;
    result.generation = routing_generation;
    if (result.operation_id.empty())
        result.operation_id = new_id("policy");
    runtime.policy = std::move(result);
    runtime.attempted_fingerprint = cancellation.stop_requested() ? std::string{} : key;
    if (runtime.observation && !runtime.observation->complete &&
        runtime.policy.state == PolicyState::Accepted) {
        runtime.policy.state = PolicyState::Partial;
        runtime.policy.detail = "Verified process preferences were accepted; incomplete discovery "
                                "prevents an application-wide acceptance claim.";
    }
    if (runtime.policy.state == PolicyState::PendingProcess) {
        runtime.processes.clear();
        seek_pending();
        reconcile(false, id);
    }
    history_dirty = true;
    mark_flush();
    update_watcher();
    log(runtime.policy.state == PolicyState::Accepted ? Severity::Information : Severity::Warning,
        "policy_result", runtime.policy.detail, id,
        {{"policy_state", policy_state_name(runtime.policy.state)},
         {"role_results", runtime.policy.roles.size()},
         {"generation", routing_generation},
         {"cancelled", cancellation.stop_requested()}},
        runtime.policy.operation_id);
    publish();
}

} // namespace audiohole::plug_board
