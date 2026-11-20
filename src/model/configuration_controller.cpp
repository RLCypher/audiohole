#include "model/configuration_controller.h"
#include "model/scanner_service.h"
#include <algorithm>
#include <stdexcept>

namespace audiohole {
namespace {
std::string routing_failure_detail(const Snapshot& state) {
    if (!has_routing_failure(state))
        return {};
    if (!state.routing_admission_issues.empty())
        return routing_issues(state.routing_admission_issues);
    std::string issues;
    for (const auto& path : state.paths)
        if (path.state == State::Failed && !path.detail.empty())
            issues +=
                path.hole_id + (path.reverse ? " / Return: " : " / Forward: ") + path.detail + "\n";
    return issues.empty() ? "Routing failed. Stop, inspect the device status, then retry." : issues;
}
// Names/notes can be applied without releasing otherwise unchanged streams.
// Retained tuning still participates: Apply must accept the complete intent.
bool same_routing_settings(const Profile& a, const Profile& b) {
    if (a.id != b.id || a.holes.size() != b.holes.size())
        return false;
    const auto same_direction = [](const Direction& x, const Direction& y) {
        return x.source == y.source && x.destination == y.destination && x.settings == y.settings &&
               x.advanced_tuning == y.advanced_tuning &&
               x.exclusive_mode_preferred == y.exclusive_mode_preferred;
    };
    for (std::size_t i = 0; i < a.holes.size(); ++i) {
        const auto& x = a.holes[i];
        const auto& y = b.holes[i];
        if (x.id != y.id || x.enabled != y.enabled || x.duplex != y.duplex ||
            x.routing_engine != y.routing_engine || !same_direction(x.forward, y.forward) ||
            !same_direction(x.reverse, y.reverse))
            return false;
    }
    return true;
}
} // namespace
ConfigurationController::ConfigurationController(std::unique_ptr<ConfigurationPersistence> provider,
                                                 std::unique_ptr<ScannerService> scanner,
                                                 std::unique_ptr<DiagnosticService> checks,
                                                 std::unique_ptr<RoutingService> routing)
    : persistence_(std::move(provider)), scanner_(std::move(scanner)),
      device_checks_(std::move(checks)), routing_(std::move(routing)) {
    if (!persistence_)
        throw std::invalid_argument("Test configuration provider is required");
    state_.operating_mode = OperatingMode::Test;
    state_.capabilities = {false, false, false, false, false, true, false};
    state_.capabilities.beep = state_.capabilities.monitoring = bool(device_checks_);
    state_.capabilities.real_engine = bool(routing_);
    ConfigurationRecords records;
    const auto result = persistence_->load(records);
    loaded_ = result.ok;
    if (loaded_) {
        state_.endpoints = std::move(records.devices);
        state_.profiles = std::move(records.profiles);
        for (const auto& device : state_.endpoints) {
            used_ids_.insert(device.id);
            if (device.inspection)
                scanner_devices_.push_back(device);
        }
        for (const auto& saved : state_.profiles) {
            used_ids_.insert(saved.id);
            for (const auto& hole : saved.holes)
                used_ids_.insert(hole.id);
        }
    } else {
        edit_error_ = result.detail;
        event("error", result.detail);
    }
    if (scanner_) {
        state_.capabilities.scanning = true;
        state_.scanner.available = true;
        (void)scanner_action(ScannerActionRequest{});
    }
    poll_routing();
}
ConfigurationController::~ConfigurationController() = default;
void ConfigurationController::event(std::string type, std::string detail) {
    const auto severity = type == "error" || type == "routing-failed" || type == "scan-failed"
                              ? Severity::Error
                          : type == "recovering" || type == "scanner-action-failed" ||
                                  type == "configuration-save-failed" || type == "action-rejected"
                              ? Severity::Warning
                              : Severity::Info;
    state_.events.push_back({++event_sequence_, std::move(type), std::move(detail), severity});
    if (state_.events.size() > 500)
        state_.events.erase(state_.events.begin());
    ++state_.revision;
}
Result ConfigurationController::unavailable(const std::string& operation) {
    const auto detail = operation + " is unavailable in this environment.";
    event("action-rejected", detail);
    return {false, detail};
}
Result ConfigurationController::commit(ConfigurationRecords records, const std::string& detail) {
    if (bundle_job_ && state_.scanner.busy)
        return {false, "Wait for the configuration bundle operation before saving."};
    if (!loaded_)
        return {false, edit_error_};
    // Scanner device-v2 records remain authoritative in their own files. Never
    // flatten rich observations into the provisional profile/configuration file.
    std::erase_if(records.devices, [](const auto& device) { return bool(device.inspection); });
    const auto result = persistence_->save(records);
    if (!result.ok) {
        edit_error_ = result.detail;
        event("configuration-save-failed", edit_error_);
        return result;
    }
    state_.endpoints = std::move(records.devices);
    state_.profiles = std::move(records.profiles);
    merge_scanner_devices();
    for (const auto& device : state_.endpoints)
        used_ids_.insert(device.id);
    for (const auto& saved : state_.profiles) {
        used_ids_.insert(saved.id);
        for (const auto& hole : saved.holes)
            used_ids_.insert(hole.id);
    }
    edit_error_.clear();
    event("configuration-saved", detail);
    return {true, detail};
}
std::string ConfigurationController::new_id(const std::string& prefix) {
    for (;;) {
        auto candidate = prefix + std::to_string(next_id_++);
        if (used_ids_.insert(candidate).second)
            return candidate;
    }
}
Hole ConfigurationController::make_hole() {
    Hole hole;
    hole.id = new_id("hole-");
    hole.display_name = "New Hole";
    return hole;
}
std::string ConfigurationController::create_profile(const std::string& title,
                                                    const std::string& copy_from) {
    if (!loaded_)
        return {};
    Profile created;
    if (!copy_from.empty()) {
        const auto* source = profile(state_, copy_from);
        if (!source) {
            edit_error_ = "The profile to duplicate no longer exists.";
            return {};
        }
        created = *source;
    }
    created.id = new_id("profile-");
    std::vector<std::string> names;
    for (const auto& item : state_.profiles)
        names.push_back(item.display_name);
    created.display_name = unique_name(title.empty() ? "New Profile" : title, names);
    for (auto& hole : created.holes)
        hole.id = make_hole().id;
    const auto id = created.id;
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    records.profiles.push_back(std::move(created));
    return commit(std::move(records), "Profile created and saved to the isolated Test data root.")
                   .ok
               ? id
               : std::string{};
}
Result ConfigurationController::remove_profile(const std::string& id) {
    if ((routing_active() && state_.active_profile == id) || state_.pending_profile == id)
        return {false, "Stop routing before deleting the active or pending profile."};
    if (!profile(state_, id))
        return {false, "Profile no longer exists."};
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    std::erase_if(records.profiles, [&](const auto& item) { return item.id == id; });
    return commit(std::move(records), "Profile deleted from Test configuration.");
}
Result ConfigurationController::save(Profile edited, bool apply) {
    if (apply && !routing_)
        return unavailable("Applying a live routing plan; use Save for configuration-only changes");
    if (apply && (state_.routing_pending || state_.state == State::Stopping))
        return {false, "Wait for the current routing operation to finish before applying edits."};
    if (apply && state_.armed && state_.active_profile != edited.id)
        return {false, "Apply edits to the active profile, or stop routing before applying another "
                       "profile."};
    if (apply && (state_.scanner.busy || (device_checks_ && device_checks_->reading().active)))
        return {false, "Finish the scan or device check before applying routing changes."};
    for (const auto& hole : edited.holes) {
        const auto checked = validate_endpoint_assignments(hole);
        if (!checked.ok)
            return checked;
    }
    describe_profile(state_, edited);
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    const auto found = std::find_if(records.profiles.begin(), records.profiles.end(),
                                    [&](const auto& record) { return record.id == edited.id; });
    if (found == records.profiles.end())
        return {false, "Profile no longer exists."};
    const auto id = edited.id;
    bool has_routes = true;
    if (apply) {
        auto projected = routing_profile(edited);
        if (!projected.ok())
            return {false, routing_issues(projected.issues)};
        const auto prepared = engine::prepare_profile(*projected.value);
        if (!prepared.ok())
            return {false, routing_issues(prepared.issues)};
        has_routes = !prepared.value->routes.empty();
    }
    *found = std::move(edited);
    const auto saved = commit(
        std::move(records), "Profile saved to Test configuration. No engine values were applied.");
    if (!saved.ok || !apply)
        return saved;
    const auto accepted = *profile(state_, id);
    if (!has_routes) {
        if (state_.armed)
            stop();
        state_.active_profile = id;
        state_.routing_intent = std::make_shared<const Profile>(accepted);
        return {true, "Profile saved. All holes are disabled; routing will remain stopped."};
    }
    if (state_.armed) {
        if (state_.state == State::Running && state_.routing_intent &&
            same_routing_settings(accepted, *state_.routing_intent)) {
            state_.routing_intent = std::make_shared<const Profile>(accepted);
            return {true, "Profile saved and applied; routing settings are unchanged."};
        }
        stop();
        pending_apply_ = accepted;
        return {true, "Profile saved. Restarting routing with the applied settings."};
    }
    return activate(id);
}
Result ConfigurationController::update_buffer(const std::string& profile_id,
                                              const std::string& hole_id, bool reverse,
                                              int milliseconds) {
    if (milliseconds < 0 || milliseconds > 100)
        return {false, "Buffer must be Automatic (0) or between 1 and 100 milliseconds."};
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    const auto selected = std::find_if(records.profiles.begin(), records.profiles.end(),
                                       [&](const auto& p) { return p.id == profile_id; });
    if (selected == records.profiles.end())
        return {false, "Save or select a profile before applying its buffer preference."};
    const auto hole = std::find_if(selected->holes.begin(), selected->holes.end(),
                                   [&](const auto& h) { return h.id == hole_id; });
    if (hole == selected->holes.end() || (reverse && !hole->duplex))
        return {false, "Save this hole and direction before applying its buffer preference."};
    // An individual Apply must not refresh names or resolve formats elsewhere in
    // the profile. Those changes belong to an explicit full-profile Save.
    (reverse ? hole->reverse : hole->forward).settings.set_buffer_preference(milliseconds);
    return commit(std::move(records),
                  "Direction buffer preference saved; no engine values were applied.");
}
Result ConfigurationController::update_endpoint(const Endpoint& edited) {
    const auto* original = endpoint(state_, edited.id);
    if (!original)
        return {false, "Saved device no longer exists."};
    if (original->inspection) {
        if (!edited.inspection || edited.windows_name != original->windows_name ||
            edited.capture != original->capture)
            return {false, "Device identity and direction are read-only."};
        ScannerActionRequest request;
        request.kind = ScannerActionKind::EditMetadata;
        request.logical_id = edited.id;
        request.display_name = edited.display_name;
        request.notes = edited.notes;
        request.expected_path = edited.inspection->path;
        request.expected_bytes = edited.inspection->edit_bytes;
        return scanner_action(request);
    }
    if (edited.windows_name != original->windows_name || edited.capture != original->capture)
        return {false, "Device identity and direction are read-only; edit their saved record "
                       "explicitly outside the GUI."};
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    const auto found = std::find_if(records.devices.begin(), records.devices.end(),
                                    [&](const auto& record) { return record.id == edited.id; });
    *found = edited;
    return commit(std::move(records),
                  "Device configuration saved. Hardware availability remains unverified.");
}
bool ConfigurationController::routing_active() const {
    return state_.armed || state_.routing_pending || state_.state == State::Stopping;
}
Result ConfigurationController::activate(const std::string& id, bool) {
    if (!routing_)
        return unavailable("Routing");
    if (routing_active())
        return {false, "Stop the current routing session before starting another profile."};
    if (state_.scanner.busy || (device_checks_ && device_checks_->reading().active))
        return {false, "Finish the scan or device check before starting routing."};
    const auto* selected = profile(state_, id);
    if (!selected)
        return {false, "Select a saved profile before starting routing."};
    return start_profile(*selected);
}
Result ConfigurationController::start_profile(const Profile& selected) {
    auto projected = routing_profile(selected);
    if (!projected.ok())
        return {false, routing_issues(projected.issues)};
    auto prepared = engine::prepare_profile(*projected.value);
    if (!prepared.ok())
        return {false, routing_issues(prepared.issues)};
    if (prepared.value->routes.empty())
        return {false, "Enable at least one configured hole before starting routing."};
    const auto generation = routing_->start(std::move(*projected.value), routing_bindings(state_));
    if (!generation)
        return {
            false,
            "The engine is initializing or still cleaning up. Wait for Stopped, then try again."};
    routing_generation_ = generation;
    routed_inventory_revision_ = state_.scanner.inventory_revision;
    state_.routing_intent = std::make_shared<const Profile>(selected);
    state_.routing_status.reset();
    state_.routing_admission_issues.clear();
    state_.paths.clear();
    state_.active_profile = selected.id;
    state_.pending_profile.clear();
    state_.pending_ms = 0;
    state_.routing_pending = true;
    state_.state = State::Starting;
    state_.armed = false;
    event("routing-requested", "Start requested for " + selected.display_name +
                                   ". Waiting for engine admission and frame progress.");
    return {true, "Starting routing. Waiting for confirmed frame progress."};
}
Result ConfigurationController::schedule(const std::string& id, int milliseconds) {
    if (!routing_)
        return unavailable("Scheduled routing");
    if (routing_active())
        return {false, "Stop routing before scheduling another profile."};
    if (!profile(state_, id) || milliseconds < 0 || milliseconds > 300000)
        return {false, "Choose a saved profile and a delay between 0 and 300 seconds."};
    state_.pending_profile = id;
    state_.pending_ms = milliseconds;
    ++state_.revision;
    return {true, "Routing activation scheduled."};
}
Result ConfigurationController::start_check(const std::string& id, bool capture) {
    if (routing_active())
        return {false, "Stop routing before opening a temporary device check."};
    const auto* target = endpoint(state_, id);
    if (!device_checks_)
        return unavailable("Device checking");
    if (state_.scanner.busy || !target || target->capture != capture ||
        target->availability != Availability::Available || !target->inspection ||
        target->inspection->diagnostic_endpoint_id.empty())
        return {false, "Choose an available, unambiguous scanned endpoint and finish any scan "
                       "before checking it."};
    if (!device_checks_->start(id, target->inspection->diagnostic_endpoint_id, capture))
        return {false, "The previous device check is still stopping. Try again after cleanup."};
    diagnostic_binding_ = target->inspection->diagnostic_endpoint_id;
    state_.monitoring = capture;
    state_.monitor_source = capture ? id : "";
    state_.beep_destination = capture ? "" : id;
    state_.input_peak.reset();
    state_.diagnostic_failed = false;
    ++state_.revision;
    return {true, capture ? "Opening the selected capture endpoint."
                          : "Opening the selected playback endpoint."};
}
Result ConfigurationController::beep(const std::string& id) {
    return start_check(id, false);
}
Result ConfigurationController::check_level(const std::string& id) {
    return start_check(id, true);
}
void ConfigurationController::cancel_beep() {
    if (device_checks_ && !device_checks_->reading().capture)
        device_checks_->cancel();
}
void ConfigurationController::monitor(bool enabled) {
    if (enabled)
        unavailable("Select an explicit capture endpoint");
    else if (device_checks_ && device_checks_->reading().capture)
        device_checks_->cancel();
}
void ConfigurationController::fail_path(const std::string&, bool) {
    unavailable("Preview failure injection");
}
void ConfigurationController::recover() {
    unavailable("Recovery");
}
void ConfigurationController::scan(bool enabled, bool deep, ScanScope scope, const std::string& id,
                                   const ScanOptions& options) {
    if (enabled && routing_active()) {
        event("scan-failed", "Stop routing before scanning device capabilities.");
        return;
    }
    if (enabled && device_checks_ && device_checks_->reading().active) {
        event("scan-failed", "Cancel the active device check before scanning.");
        return;
    }

    if (!scanner_) {
        if (enabled)
            unavailable("Device scanning");
        return;
    }
    if (!enabled) {
        scanner_->cancel();
        if (state_.scanner.busy) {
            state_.scanner.phase = "cancelling";
            state_.scanner.detail = "Cancellation requested; waiting for scanner/worker cleanup.";
            ++state_.revision;
        }
        return;
    }
    if (state_.scanner.busy) {
        event("scan-failed", "Another scanner operation is still running or cleaning up.");
        return;
    }
    try {
        const auto generation = scanner_->start_scan(deep, scope, id, options);
        if (!generation)
            throw std::runtime_error("The scanner is still finishing its previous operation.");
        state_.scanner.generation = generation;
        state_.scanner.busy = state_.scanning = scanner_job_is_scan_ = true;
        state_.scanner.result = "running";
        state_.scanner.import_plan.reset();
        state_.scanner.phase = "starting";
        state_.scan_percent = -1;
        event("scan-start", deep ? "Deep scan started with explicit consent and bounded limits."
                                 : "Basic device discovery and capability queries started.");
    } catch (const std::exception& error) {
        event("scanner-action-failed", error.what());
    }
}
void ConfigurationController::stop() {
    pending_apply_.reset();
    state_.pending_profile.clear();
    state_.pending_ms = 0;
    if (routing_) {
        routing_generation_ = routing_->stop();
        state_.routing_pending = true;
        state_.armed = false;
        state_.state = State::Stopping;
        event("stop-requested", "Stop requested; waiting for native stream cleanup.");
    }
    if (device_checks_)
        device_checks_->cancel();
    if (scanner_)
        scan(false);
}
Result ConfigurationController::scanner_action(const ScannerActionRequest& request) {
    if (routing_active() && (request.kind == ScannerActionKind::ClearInventory ||
                             request.kind == ScannerActionKind::ImportBundle))
        return {false, "Stop routing before replacing device or profile inventory."};
    if (state_.scanner.busy)
        return {false, "Wait for the current scanner operation or cancel it first."};
    const auto* selected = endpoint(state_, request.logical_id);
    if (request.kind == ScannerActionKind::SetHidden && selected && !selected->inspection) {
        auto edited = *selected;
        edited.hidden = request.hidden;
        return update_endpoint(edited);
    }
    if (!scanner_)
        return {false, "The scanner service is unavailable."};
    try {
        const auto generation = scanner_->action(request);
        if (!generation)
            return {false, "The scanner is still finishing its previous operation."};
        state_.scanner.generation = generation;
        state_.scanner.busy = true;
        scanner_job_is_scan_ = false;
        bundle_job_ = request.kind == ScannerActionKind::ExportDevices ||
                      request.kind == ScannerActionKind::ExportProfiles ||
                      request.kind == ScannerActionKind::InspectDevices ||
                      request.kind == ScannerActionKind::InspectProfiles ||
                      request.kind == ScannerActionKind::ImportBundle;
        state_.scanner.import_plan.reset();
        state_.scanner.result = "running";
        state_.scanner.phase =
            bundle_job_                                         ? "configuration bundle"
            : request.kind == ScannerActionKind::ClearInventory ? "clearing device scan information"
            : request.kind == ScannerActionKind::EditMetadata   ? "saving device metadata"
            : request.kind == ScannerActionKind::Export         ? "exporting"
            : request.kind == ScannerActionKind::SetHidden      ? "saving visibility"
            : request.kind == ScannerActionKind::RefreshAvailability ? "refreshing availability"
                                                                     : "loading inventory";
        ++state_.revision;
        return {true, "Scanner operation accepted."};
    } catch (const std::exception& error) {
        return {false, error.what()};
    }
}
void ConfigurationController::merge_scanner_devices() {
    std::erase_if(state_.endpoints, [](const auto& device) { return bool(device.inspection); });
    for (const auto& device : scanner_devices_) {
        const auto existing = std::find_if(state_.endpoints.begin(), state_.endpoints.end(),
                                           [&](const auto& item) { return item.id == device.id; });
        if (existing != state_.endpoints.end()) {
            existing->availability = Availability::Ambiguous;
            event("error",
                  "A legacy Test device and scanner record share logical ID " + device.id +
                      ". Resolve the conflicting records explicitly; neither was overwritten.");
        } else
            state_.endpoints.push_back(device);
    }
    for (const auto& device : state_.endpoints)
        used_ids_.insert(device.id);
}
void ConfigurationController::poll_routing() {
    if (!routing_)
        return;
    const auto update = routing_->reading();
    if (update.revision == routing_revision_)
        return;
    routing_revision_ = update.revision;
    state_.routing_backends = update.backends;
    state_.capabilities.routing =
        update.ready && std::any_of(update.backends.begin(), update.backends.end(),
                                    [](const auto& b) { return b.available; });
    if (update.generation != routing_generation_ || update.pending)
        return;
    const auto previous = state_.state;
    const auto previous_failure = routing_failure_detail(state_);
    state_.routing_pending = false;
    state_.routing_admission_issues = update.admission_issues;
    if (!update.admission_issues.empty()) {
        state_.state = State::Failed;
        state_.armed = false;
        state_.active_profile.clear();
        state_.routing_intent.reset();
    } else if (update.state) {
        const auto& actual = *update.state;
        state_.routing_status = update.state;
        state_.state = routing_state(actual.state);
        state_.armed = actual.armed && state_.state != State::Stopping;
        state_.paths.clear();
        for (const auto& p : actual.paths) {
            PathStatus path;
            path.hole_id = p.hole_id;
            path.reverse = p.direction == engine::DirectionId::Reverse;
            path.state = routing_state(p.state);
            path.detail = routing_issues(p.issues);
            if (p.negotiated)
                path.applied_buffer_ms = p.negotiated->destination.applied_buffer_request_ms;
            state_.paths.push_back(std::move(path));
        }
        if (previous != state_.state) {
            if (state_.state == State::Stopped)
                event("stopped", "All routing streams stopped and released.");
            else if (state_.state == State::Running)
                event("state", "Routing is running; frame progress confirmed by the engine.");
            else if (state_.state == State::Degraded &&
                     std::all_of(state_.paths.begin(), state_.paths.end(), [](const auto& p) {
                         return p.state == State::Running || p.state == State::Ready;
                     }))
                event("state", "Available holes are running; other holes are waiting for devices.");
            else if (state_.state == State::Degraded && !has_routing_failure(state_))
                event("state",
                      "Routing has reduced progress; check the direction states for details.");
            else if (state_.state == State::Ready)
                event("routing-waiting",
                      "Waiting for this profile's required devices to become available.");
        }
    }
    // Inspect path failures even when aggregate Degraded is unchanged: a sibling
    // can move from Starting to Failed while another direction keeps running.
    const auto failure = routing_failure_detail(state_);
    if (!failure.empty() && failure != previous_failure)
        event("routing-failed", failure);
    if (update.state && update.admission_issues.empty()) {
        const auto now = std::chrono::steady_clock::now();
        if (!update.state->paths.empty() &&
            (previous != state_.state ||
             (update.state->armed && now - last_engine_log_ >= std::chrono::seconds(2)))) {
            last_engine_log_ = now;
            event("engine-status", routing_diagnostics(*update.state, state_.routing_intent.get()));
        }
    }
    ++state_.revision;
}
void ConfigurationController::advance(int milliseconds) {
    poll_routing();
    if (pending_apply_ && !state_.routing_pending && state_.state != State::Stopping) {
        auto accepted = std::move(*pending_apply_);
        pending_apply_.reset();
        if (state_.state == State::Stopped) {
            const auto applied = start_profile(accepted);
            if (!applied.ok) {
                state_.state = State::Failed;
                event("error",
                      "Profile was saved, but restarting routing failed: " + applied.detail);
            }
        } else {
            event("error", "Profile was saved, but engine cleanup failed; Apply was cancelled.");
        }
    }
    if (!state_.pending_profile.empty() && milliseconds > 0) {
        ++state_.revision;
        state_.pending_ms = std::max(0, state_.pending_ms - milliseconds);
        const auto readiness = routing_->reading();
        if (!readiness.ready && !readiness.admission_issues.empty()) {
            state_.pending_profile.clear();
            state_.pending_ms = 0;
        } else if (!state_.pending_ms && readiness.ready && !state_.scanner.busy) {
            const auto id = state_.pending_profile;
            state_.pending_profile.clear();
            const auto result = activate(id);
            if (!result.ok)
                event("error", result.detail);
        }
    }
    if (device_checks_) {
        const auto value = device_checks_->reading();
        const bool changed =
            state_.monitoring != (value.active && value.capture) ||
            state_.monitor_source != (value.active && value.capture ? value.target : "") ||
            state_.input_peak != (value.active && value.capture ? value.peak : std::nullopt) ||
            state_.beep_destination != (value.active && !value.capture ? value.target : "") ||
            state_.diagnostic_detail != value.detail || state_.diagnostic_failed != value.failed;
        if (changed)
            ++state_.revision;
        state_.monitoring = value.active && value.capture;
        state_.monitor_source = state_.monitoring ? value.target : "";
        state_.input_peak = state_.monitoring ? value.peak : std::nullopt;
        state_.beep_destination = value.active && !value.capture ? value.target : "";
        state_.diagnostic_detail = value.detail;
        state_.diagnostic_failed = value.failed;
        if (value.active) {
            const auto* target = endpoint(state_, value.target);
            if (!target || target->availability != Availability::Available || !target->inspection ||
                target->inspection->diagnostic_endpoint_id != diagnostic_binding_)
                device_checks_->cancel();
        }
    }

    if (!scanner_) {
        refresh_waiting_routes();
        return;
    }
    for (auto& update : scanner_->drain()) {
        if (update.generation != state_.scanner.generation)
            continue; // A superseded operation cannot publish into a newer view.
        state_.scanner.phase = update.phase;
        state_.scanner.detail = update.detail;
        state_.scan_percent = update.percent;
        if (!update.terminal) {
            event("scan-progress", update.detail);
            continue;
        }
        state_.scanner.busy = state_.scanning = false;
        state_.scanner.completed_generation = update.generation;
        state_.scanner.result = update.result;
        state_.scanner.session_path = update.session_path;
        state_.scanner.import_plan = std::move(update.import_plan);
        if (update.profiles_imported) {
            ConfigurationRecords loaded;
            const auto result = persistence_->load(loaded);
            if (result.ok) {
                state_.profiles = std::move(loaded.profiles);
                state_.endpoints = std::move(loaded.devices);
                for (const auto& saved : state_.profiles) {
                    used_ids_.insert(saved.id);
                    for (const auto& hole : saved.holes)
                        used_ids_.insert(hole.id);
                }
            } else {
                loaded_ = false;
                edit_error_ = result.detail;
                state_.scanner.result = "failed";
                event("configuration-save-failed", result.detail);
            }
        }
        if (update.inventory_valid) {
            scanner_devices_ = std::move(update.devices);
            merge_scanner_devices();
            ++state_.scanner.inventory_revision;
        }
        if (scanner_job_is_scan_) {
            state_.scan_percent = update.result == "completed" ? 100 : update.percent;
            event(update.result == "interrupted" ? "scan-cancelled"
                  : update.result == "completed" || update.result == "partially_completed"
                      ? "scan-complete"
                      : "scan-failed",
                  update.detail);
        } else
            event(update.result == "completed" || update.result == "partially_completed"
                      ? "scanner-updated"
                      : "scanner-action-failed",
                  update.detail);
    }
    refresh_waiting_routes();
}
void ConfigurationController::audio_worker_policy(const std::string& task,
                                                  const std::string& priority) {
    if (routing_)
        routing_->configure_workers({task == "audio"       ? engine::SchedulingTask::Audio
                                     : task == "pro_audio" ? engine::SchedulingTask::ProAudio
                                                           : engine::SchedulingTask::Automatic,
                                     priority == "high"     ? engine::AudioPriority::High
                                     : priority == "normal" ? engine::AudioPriority::Normal
                                                            : engine::AudioPriority::Automatic});
}
void ConfigurationController::refresh_waiting_routes() {
    if (!routing_ || !state_.armed || state_.routing_pending || state_.state == State::Stopping ||
        !state_.routing_intent || routed_inventory_revision_ == state_.scanner.inventory_revision)
        return;
    if (routing_->update_bindings(routing_bindings(state_), wait_for_devices_))
        routed_inventory_revision_ = state_.scanner.inventory_revision;
}
void ConfigurationController::clear_diagnostics() {
    state_.events.clear();
    ++state_.revision;
}
} // namespace audiohole
