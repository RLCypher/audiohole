#include "audiohole/plug_board/module.hpp"
#include "audiohole/plug_board/presentation.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace audiohole::plug_board {
namespace {

bool path_or_device(const std::string& value) {
    if (value.find("\\\\") != std::string::npos || value.find("\\Device\\") != std::string::npos ||
        value.find("{0.0.") != std::string::npos || value.find("/home/") != std::string::npos ||
        value.find("/Users/") != std::string::npos ||
        value.find("/workspace/") != std::string::npos)
        return true;
    for (std::size_t index = 0; index + 2 < value.size(); ++index) {
        const auto byte = static_cast<unsigned char>(value[index]);
        if (std::isalpha(byte) && value[index + 1] == ':' &&
            (value[index + 2] == '\\' || value[index + 2] == '/') &&
            (index == 0 || !std::isalnum(static_cast<unsigned char>(value[index - 1]))))
            return true;
    }
    return false;
}

Document redact(Document value, const std::string& key = {}) {
    static const std::set<std::string> sensitive{
        "executable_path", "app_user_model_id", "session_id",   "instance_id",
        "observation_key", "windows_id",        "endpoint_id",  "identity_evidence",
        "data_root",       "local_root",        "library_path", "active_endpoint_ids",
        "shortcut_path"};
    if (!value.is_null() && sensitive.contains(key))
        return "[redacted]";
    if (value.is_object()) {
        for (auto item = value.begin(); item != value.end(); ++item)
            item.value() = redact(std::move(item.value()), item.key());
    } else if (value.is_array()) {
        for (auto& item : value)
            item = redact(std::move(item));
    } else if (value.is_string() && path_or_device(value.get_ref<const std::string&>())) {
        value = "[redacted path or device identifier]";
    }
    return value;
}

Document operation(const OperationResult& result) {
    return {{"succeeded", result.ok}, {"code", result.code}, {"detail", result.detail}};
}
Document token(const ProcessToken& process) {
    return {{"pid", process.pid},
            {"creation_filetime", process.creation_time},
            {"executable_path", process.executable_path},
            {"app_user_model_id", process.app_user_model_id}};
}
std::string endpoint_name(const EndpointInventory& inventory,
                          const std::optional<std::string>& id) {
    if (!id)
        return "Windows Default (no application override)";
    for (const auto& endpoint : inventory.endpoints)
        if (endpoint.windows_id == id)
            return endpoint.display_name;
    return "Unmapped playback output";
}
Document read(const PreferenceRead& result, const EndpointInventory& inventory) {
    return {
        {"result", operation(result.result)},
        {"output_name",
         result.result.ok ? endpoint_name(inventory, result.endpoint_id) : "Read failed"},
        {"endpoint_id", result.endpoint_id ? Document(*result.endpoint_id) : Document(nullptr)}};
}
Document policy(const LivePolicyReport& report, const EndpointInventory& inventory) {
    Document roles = Document::array();
    for (const auto& role : report.roles)
        roles.push_back({{"process", token(role.process)},
                         {"role", role_name(role.role)},
                         {"before", read(role.before, inventory)},
                         {"write_attempted", role.write_attempted},
                         {"write", operation(role.write)},
                         {"readback", read(role.readback, inventory)},
                         {"preference_accepted", role.accepted}});
    return {{"state", policy_state_name(report.state)},
            {"operation_id", report.operation_id},
            {"generation", report.generation},
            {"detail", report.detail},
            {"roles", std::move(roles)},
            {"target_output_name", endpoint_name(inventory, report.target_windows_id)},
            {"endpoint_id",
             report.target_windows_id ? Document(*report.target_windows_id) : Document(nullptr)}};
}
} // namespace

const char* policy_state_name(PolicyState state) {
    switch (state) {
    case PolicyState::NotManaged:
        return "Not managed";
    case PolicyState::PendingStorage:
        return "Pending storage";
    case PolicyState::PendingProcess:
        return "Pending application";
    case PolicyState::PendingDevice:
        return "Pending device";
    case PolicyState::Applying:
        return "Applying preference";
    case PolicyState::Accepted:
        return "Preference accepted";
    case PolicyState::Partial:
        return "Partially accepted";
    case PolicyState::Rejected:
        return "Preference rejected";
    case PolicyState::Unsupported:
        return "Unsupported";
    case PolicyState::Stale:
        return "Historical result";
    }
    return "Unknown";
}
const char* observation_state_name(ObservationState state) {
    switch (state) {
    case ObservationState::Unknown:
        return "Playback unobserved";
    case ObservationState::Matches:
        return "Observed on destination";
    case ObservationState::Differs:
        return "Observed elsewhere";
    case ObservationState::Multiple:
        return "Multiple active outputs";
    case ObservationState::Stale:
        return "Observation is stale";
    case ObservationState::Observed:
        return "Output observed";
    }
    return "Unknown";
}

Document diagnostic_document(const DiagnosticEvent& event, bool include_sensitive) {
    Document result{{"utc", event.utc},
                    {"severity", event.severity == Severity::Error     ? "error"
                                 : event.severity == Severity::Warning ? "warning"
                                                                       : "information"},
                    {"code", event.code},
                    {"message", event.message},
                    {"operation_id", event.operation_id},
                    {"board_id", event.board_id},
                    {"socket_id", event.socket_id},
                    {"application_id", event.application_id},
                    {"details", event.details}};
    return include_sensitive ? result : redact(std::move(result));
}

Document snapshot_document(const ModuleSnapshot& snapshot, bool include_sensitive) {
    Document applications = Document::array(), endpoints = Document::array(),
             events = Document::array();
    for (const auto& app : snapshot.applications) {
        ApplicationPlacement placement;
        if (snapshot.library) {
            if (const auto* board = find_board(*snapshot.library, snapshot.selected_board_id))
                placement = application_placement(board->working, app, snapshot.inventory);
        }
        Document sessions = Document::array(), active_outputs = Document::array();
        if (app.observation)
            for (const auto& id : app.observation->active_endpoint_ids)
                active_outputs.push_back(
                    {{"output_name", endpoint_name(snapshot.inventory, id)}, {"endpoint_id", id}});
        if (app.observation)
            for (const auto& session : app.observation->sessions) {
                sessions.push_back(
                    {{"instance_id", session.instance_id},
                     {"session_id", session.session_id},
                     {"endpoint_id", session.endpoint_id},
                     {"output_name", endpoint_name(snapshot.inventory, session.endpoint_id)},
                     {"state", session.state == SessionState::Active     ? "active"
                               : session.state == SessionState::Inactive ? "inactive"
                               : session.state == SessionState::Expired  ? "expired"
                                                                         : "unknown"},
                     {"cross_process", session.cross_process},
                     {"display_name", session.display_name},
                     {"process", session.process ? token(*session.process) : Document(nullptr)}});
            }
        applications.push_back(
            {{"application_id", app.id},
             {"display_name", app.display_name},
             {"socket_id", app.socket_id},
             {"display_socket_id", placement.socket_id},
             {"placement_basis", placement_basis_name(placement.basis)},
             {"activity", app.activity == Activity::Active           ? "active"
                          : app.activity == Activity::RecentlyActive ? "recently_active"
                                                                     : "inactive"},
             {"observation_fresh", app.observation_fresh},
             {"observation_complete", app.observation && app.observation->complete},
             {"observed_routing", observation_state_name(app.observed_routing)},
             {"active_endpoint_ids",
              app.observation ? Document(app.observation->active_endpoint_ids) : Document::array()},
             {"active_outputs", std::move(active_outputs)},
             {"last_active_utc_ms", app.history.last_active_utc_ms
                                        ? Document(*app.history.last_active_utc_ms)
                                        : Document(nullptr)},
             {"clock_uncertain", app.history.clock_uncertain},
             {"policy", policy(app.policy, snapshot.inventory)},
             {"sessions", std::move(sessions)},
             {"icon", app.image
                          ? Document{{"application_specific", app.image->application_specific},
                                     {"source", app.image->source},
                                     {"evidence", app.image->evidence},
                                     {"result", operation(app.image->result)}}
                          : Document(nullptr)}});
    }
    for (const auto& endpoint : snapshot.inventory.endpoints)
        endpoints.push_back({{"logical_device_id", endpoint.logical_id},
                             {"display_name", endpoint.display_name},
                             {"windows_id", endpoint.windows_id},
                             {"available", endpoint.available},
                             {"ambiguous", endpoint.ambiguous}});
    for (const auto& event : snapshot.diagnostics)
        events.push_back(diagnostic_document(event, include_sensitive));
    const auto& metrics = snapshot.platform_metrics;
    auto optional_number = [](const std::optional<std::uint64_t>& value) {
        return value ? Document(*value) : Document(nullptr);
    };
    Document result{
        {"format", "audiohole-plug-board-diagnostics"},
        {"schema_version", 1},
        {"exported_utc", utc_timestamp()},
        {"sensitive_fields_included", include_sensitive},
        {"simulated", snapshot.simulated},
        {"environment", snapshot.environment},
        {"phase", snapshot.phase == ModulePhase::Loading         ? "loading"
                  : snapshot.phase == ModulePhase::Ready         ? "ready"
                  : snapshot.phase == ModulePhase::ReadOnlyError ? "read_only_error"
                  : snapshot.phase == ModulePhase::Stopping      ? "stopping"
                                                                 : "stopped"},
        {"publication", snapshot.publication},
        {"selected_board_id", snapshot.selected_board_id},
        {"visible", snapshot.visible},
        {"capability", operation(snapshot.capability)},
        {"storage", operation(snapshot.storage)},
        {"diagnostic_storage", operation(snapshot.diagnostic_storage)},
        {"diagnostics_write_pending", snapshot.diagnostics_write_pending},
        {"catalog_write_pending", snapshot.catalog_write_pending},
        {"library", snapshot.library ? Document::parse(serialize_library(*snapshot.library))
                                     : Document(nullptr)},
        {"endpoints", std::move(endpoints)},
        {"applications", std::move(applications)},
        {"events", std::move(events)},
        {"metrics",
         {{"full_session_enumerations", metrics.full_session_enumerations},
          {"session_events", metrics.session_events},
          {"icon_extractions", metrics.icon_extractions},
          {"icon_cache_hits", metrics.icon_cache_hits},
          {"policy_reads", metrics.policy_reads},
          {"policy_writes", metrics.policy_writes},
          {"process_snapshots", metrics.process_snapshots},
          {"dropped_events", metrics.dropped_events + snapshot.callbacks_dropped},
          {"owned_backend_workers", metrics.owned_workers},
          {"discovery_active", metrics.discovery_active},
          {"process_watch_active", metrics.process_watch_active},
          {"library_commits", snapshot.library_commits},
          {"diagnostic_flushes", snapshot.diagnostic_flushes},
          {"process_sample_tick_ms", optional_number(metrics.process_sample_tick_ms)},
          {"process_cpu_100ns", optional_number(metrics.process_cpu_100ns)},
          {"private_bytes", optional_number(metrics.private_bytes)},
          {"working_set_bytes", optional_number(metrics.working_set_bytes)},
          {"handles", optional_number(metrics.handles)},
          {"io_write_operations", optional_number(metrics.io_write_operations)}}},
        {"limitations",
         {"Preference acceptance is independent of observed playback.",
          "Hidden views do not discover full audio sessions; observations then become stale.",
          "Process resource samples exclude WMI provider and other process costs.",
          "No audio samples, peaks, recordings or global-reset operations are collected."}}};
    return include_sensitive ? result : redact(std::move(result));
}

} // namespace audiohole::plug_board
