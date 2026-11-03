#include "model/configuration_controller.h"
#include <algorithm>
#include <stdexcept>

namespace audiohole {
ConfigurationController::ConfigurationController(std::unique_ptr<ConfigurationPersistence> provider)
    : persistence_(std::move(provider)) {
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
}
void ConfigurationController::event(std::string type, std::string detail) {
    const auto severity = type == "error" || type == "scan-failed" ? Severity::Error
                          : type == "recovering"                   ? Severity::Warning
                                                                   : Severity::Info;
    state_.events.push_back({++event_sequence_, std::move(type), std::move(detail), severity});
    if (state_.events.size() > 500)
        state_.events.erase(state_.events.begin());
    ++state_.revision;
}
Result ConfigurationController::unavailable(const std::string& operation) {
    const auto detail =
        operation +
        " is unavailable: Test Mode has persistent configuration but no audio/scanner backend.";
    event("error", detail);
    return {false, detail};
}
Result ConfigurationController::commit(ConfigurationRecords records, const std::string& detail) {
    if (!loaded_)
        return {false, edit_error_};
    const auto result = persistence_->save(records);
    if (!result.ok) {
        edit_error_ = result.detail;
        event("error", edit_error_);
        return result;
    }
    state_.endpoints = std::move(records.devices);
    state_.profiles = std::move(records.profiles);
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
    hole.display_name = "New hole";
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
    created.display_name = title.empty() ? "New profile" : title;
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
    ConfigurationRecords records{state_.endpoints, state_.profiles};
    const auto found = std::find_if(records.profiles.begin(), records.profiles.end(),
                                    [&](const auto& record) { return record.id == edited.id; });
    if (found == records.profiles.end())
        return {false, "Profile no longer exists."};
    *found = std::move(edited);
    return commit(std::move(records),
                  "Profile saved to Test configuration. No engine values were applied.");
}
Result ConfigurationController::update_endpoint(const Endpoint& edited) {
    const auto* original = endpoint(state_, edited.id);
    if (!original)
        return {false, "Saved device no longer exists."};
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
void ConfigurationController::scan(bool enabled, bool, ScanScope, const std::string&) {
    if (enabled)
        unavailable("Device scanning");
}
void ConfigurationController::clear_diagnostics() {
    state_.events.clear();
    ++state_.revision;
}
} // namespace audiohole
