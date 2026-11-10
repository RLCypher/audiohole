#include "model/configuration_controller.h"
#include "model/scanner_service.h"
#include <algorithm>
#include <stdexcept>

namespace audiohole {
ConfigurationController::ConfigurationController(std::unique_ptr<ConfigurationPersistence> provider,
                                                 std::unique_ptr<ScannerService> scanner)
    : persistence_(std::move(provider)), scanner_(std::move(scanner)) {
    if (!persistence_)
        throw std::invalid_argument("Test configuration provider is required");
    state_.operating_mode = OperatingMode::Test;
    state_.capabilities = {false, false, false, false, false, true, false};
    ConfigurationRecords records;
    const auto result = persistence_->load(records);
    loaded_ = result.ok;
    if (loaded_) {
        state_.endpoints = std::move(records.devices);
        state_.profiles = std::move(records.profiles);
        for (const auto& device : state_.endpoints)
            used_ids_.insert(device.id);
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
}
ConfigurationController::~ConfigurationController() = default;
void ConfigurationController::event(std::string type, std::string detail) {
    const auto severity = type == "error" || type == "scan-failed" ? Severity::Error
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
    const auto detail =
        operation + " is unavailable: Test Mode has no routing, monitoring or playback engine.";
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
    if (!profile(state_, id))
        return {false, "Profile no longer exists."};
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    std::erase_if(records.profiles, [&](const auto& item) { return item.id == id; });
    return commit(std::move(records), "Profile deleted from Test configuration.");
}
Result ConfigurationController::save(Profile edited, bool apply) {
    if (apply)
        return unavailable("Applying a live routing plan; use Save for configuration-only changes");
    describe_profile(state_, edited);
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    const auto found = std::find_if(records.profiles.begin(), records.profiles.end(),
                                    [&](const auto& record) { return record.id == edited.id; });
    if (found == records.profiles.end())
        return {false, "Profile no longer exists."};
    *found = std::move(edited);
    return commit(std::move(records),
                  "Profile saved to Test configuration. No engine values were applied.");
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
    (reverse ? hole->reverse : hole->forward).settings.buffer_ms = milliseconds;
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
Result ConfigurationController::activate(const std::string&, bool) {
    return unavailable("Routing");
}
Result ConfigurationController::schedule(const std::string&, int) {
    return unavailable("Scheduled routing");
}
Result ConfigurationController::beep(const std::string&) {
    return unavailable("Test Beep playback");
}
void ConfigurationController::monitor(bool enabled) {
    if (enabled)
        unavailable("Source monitoring");
}
void ConfigurationController::fail_path(const std::string&, bool) {
    unavailable("Preview failure injection");
}
void ConfigurationController::recover() {
    unavailable("Recovery");
}
void ConfigurationController::scan(bool enabled, bool deep, ScanScope scope, const std::string& id,
                                   const ScanOptions& options) {
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
    if (scanner_)
        scan(false);
}
Result ConfigurationController::scanner_action(const ScannerActionRequest& request) {
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
void ConfigurationController::advance(int) {
    if (!scanner_)
        return;
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
}
void ConfigurationController::clear_diagnostics() {
    state_.events.clear();
    ++state_.revision;
}
} // namespace audiohole
