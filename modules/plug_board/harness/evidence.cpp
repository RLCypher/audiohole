#include "evidence.hpp"
#include "support.hpp"

#include <fstream>
#include <map>
#include <set>

namespace audiohole::plug_board::harness {
using namespace windows_detail;
namespace {

// UTC records observation/reporting time. It does not invent application activity
// while discovery was off or establish any measured playback latency.
std::string utc_now() {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream text;
    text << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2) << time.wMonth
         << '-' << std::setw(2) << time.wDay << 'T' << std::setw(2) << time.wHour << ':'
         << std::setw(2) << time.wMinute << ':' << std::setw(2) << time.wSecond << '.'
         << std::setw(3) << time.wMilliseconds << 'Z';
    return text.str();
}

Json nullable(const std::optional<std::string>& value) {
    return value ? Json(*value) : Json(nullptr);
}
Json process_json(const ProcessToken& process) {
    return {{"pid", process.pid},
            {"creation_filetime", process.creation_time},
            {"executable_path", process.executable_path},
            {"app_user_model_id", process.app_user_model_id}};
}
Json identity_json(const ApplicationIdentity& identity) {
    const char* kind = identity.kind == IdentityKind::Win32Image            ? "win32_image"
                       : identity.kind == IdentityKind::PackagedApplication ? "packaged_application"
                       : identity.kind == IdentityKind::SystemSounds        ? "system_sounds"
                                                                            : "unresolved";
    return {{"kind", kind}, {"evidence", identity.value}};
}
const char* session_state(SessionState state) {
    switch (state) {
    case SessionState::Active:
        return "active";
    case SessionState::Inactive:
        return "inactive";
    case SessionState::Expired:
        return "expired";
    case SessionState::Unknown:
        return "unknown";
    }
    return "unknown";
}
Json session_json(const SessionObservation& session) {
    return {{"instance_id", session.instance_id},
            {"session_id", session.session_id},
            {"endpoint_id", session.endpoint_id},
            {"state", session_state(session.state)},
            {"cross_process", session.cross_process},
            {"identity", identity_json(session.identity)},
            {"process", session.process ? process_json(*session.process) : Json(nullptr)},
            {"display_name", session.display_name},
            {"icon_path", session.icon_path}};
}
Json playback_json(const PlaybackObservation& playback) {
    return {{"complete", playback.complete}, {"active_endpoint_ids", playback.active_endpoint_ids}};
}
const char* restore_state(RestoreState state) {
    switch (state) {
    case RestoreState::NotAttempted:
        return "not_attempted";
    case RestoreState::AlreadyOriginal:
        return "already_original";
    case RestoreState::Restored:
        return "restored";
    case RestoreState::Conflict:
        return "external_change_preserved";
    case RestoreState::Failed:
        return "failed";
    }
    return "unknown";
}

} // namespace

EvidenceDirectory::EvidenceDirectory(std::filesystem::path root)
    : root_(std::filesystem::absolute(std::move(root))) {
    if (!std::filesystem::create_directories(root_)) {
        throw std::runtime_error(
            "Use a new evidence directory; existing paths are never overwritten.");
    }
}
const std::filesystem::path& EvidenceDirectory::root() const {
    return root_;
}
void EvidenceDirectory::write(const std::wstring& filename, const Json& document) {
    if (std::filesystem::path(filename).filename() != filename) {
        throw std::runtime_error("Evidence filename must not escape its owned directory.");
    }
    const auto bytes = document.dump(2) + "\n";
    if (bytes.size() > 16 * 1024 * 1024) {
        throw std::runtime_error("Evidence document exceeds the 16 MiB bound.");
    }
    Handle file(CreateFileW((root_ / filename).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        throw std::runtime_error("Could not exclusively create evidence file.");
    }
    DWORD written = 0;
    if (!WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
        written != bytes.size() || !FlushFileBuffers(file.get())) {
        throw std::runtime_error(
            "Evidence flush failed; no subsequent preference mutation is permitted.");
    }
}
OperationResult EvidenceDirectory::prepare(const RecoveryPlan& plan) {
    try {
        write(L"recovery.json", recovery_json(plan));
        return {true, "durable_recovery_prepared", {}};
    } catch (const std::exception& error) {
        return {false, "recovery_write_failed", error.what()};
    }
}
OperationResult EvidenceDirectory::finish(const ExperimentResult& result) {
    try {
        write(L"policy-result.json", result_json(result));
        return {true, "final_evidence_flushed", {}};
    } catch (const std::exception& error) {
        return {false, "final_evidence_failed", error.what()};
    }
}

Json operation_json(const OperationResult& result) {
    return {{"succeeded", result.ok}, {"code", result.code}, {"detail", result.detail}};
}
Json inspection_json(const WindowsInspection& inspection) {
    Json endpoints = Json::array(), applications = Json::array();
    for (const auto& endpoint : inspection.endpoints) {
        endpoints.push_back({{"endpoint_id", endpoint.endpoint_id},
                             {"display_name", endpoint.display_name},
                             {"available", endpoint.available},
                             {"native_state", endpoint.native_state}});
    }
    for (const auto& application : group_applications(inspection.snapshot)) {
        Json sessions = Json::array(), processes = Json::array();
        for (const auto& session : application.sessions) {
            sessions.push_back(session_json(session));
        }
        for (const auto& process : application.verified_processes) {
            processes.push_back(process_json(process));
        }
        applications.push_back({{"observation_key", application.observation_key},
                                {"identity", identity_json(application.identity)},
                                {"display_name", application.display_name},
                                {"complete", application.complete},
                                {"active_endpoint_ids", application.active_endpoint_ids},
                                {"verified_processes", processes},
                                {"sessions", sessions}});
    }
    Json defaults = Json::object();
    for (std::size_t index = 0; index < render_roles.size(); ++index) {
        defaults[role_name(render_roles[index])] = nullable(inspection.default_endpoints[index]);
    }
    const auto& environment = inspection.environment;
    return {
        {"format", "audiohole-plug-board-inspection"},
        {"schema_version", 1},
        {"observed_utc", utc_now()},
        {"source", environment.wine ? "wine_supplemental_only" : "windows_core_audio_observation"},
        {"environment",
         {{"major", environment.major},
          {"minor", environment.minor},
          {"build", environment.build},
          {"update_build_revision", environment.update_build_revision
                                        ? Json(*environment.update_build_revision)
                                        : Json(nullptr)},
          {"workstation", environment.workstation},
          {"wine", environment.wine},
          {"elevated", environment.privilege_known ? Json(environment.elevated) : Json(nullptr)},
          {"detail", environment.detail}}},
        {"snapshot_complete", inspection.snapshot.complete},
        {"diagnostics", inspection.snapshot.diagnostics},
        {"render_endpoints", endpoints},
        {"default_render_endpoints", defaults},
        {"applications", applications}};
}
Json result_json(const ExperimentResult& result) {
    Json roles = Json::array();
    for (const auto& role : result.roles) {
        roles.push_back({{"process", process_json(role.process)},
                         {"role", role_name(role.role)},
                         {"original_endpoint_id", nullable(role.original_endpoint_id)},
                         {"temporary_write_attempted", role.write_attempted},
                         {"write", operation_json(role.write_result)},
                         {"readback",
                          {{"result", operation_json(role.readback.result)},
                           {"endpoint_id", nullable(role.readback.endpoint_id)}}},
                         {"preference_accepted", role.preference_accepted},
                         {"restore_state", restore_state(role.restore)},
                         {"restore_result", operation_json(role.restore_result)}});
    }
    return {{"format", "audiohole-plug-board-policy-experiment"},
            {"schema_version", 1},
            {"reported_utc", utc_now()},
            {"simulated", result.simulated},
            {"recovery_operation", result.recovery_operation},
            {"outcome", result.outcome},
            {"detail", result.detail},
            {"journal_prepared", result.journal_prepared},
            {"temporary_preferences_accepted", result.temporary_preferences_accepted},
            {"restoration_complete", result.restoration_complete},
            {"roles", roles},
            {"before", playback_json(result.before)},
            {"after_apply", playback_json(result.after_apply)},
            {"after_fixture_stream_recreation", playback_json(result.after_recreation)}};
}
Json recovery_json(const RecoveryPlan& plan) {
    Json originals = Json::array();
    for (const auto& original : plan.originals) {
        Json roles = Json::object();
        for (std::size_t index = 0; index < render_roles.size(); ++index) {
            roles[role_name(render_roles[index])] = nullable(original.endpoints[index]);
        }
        originals.push_back({{"process", process_json(original.process)}, {"roles", roles}});
    }
    return {{"format", "audiohole-plug-board-feasibility-recovery"},
            {"schema_version", 1},
            {"recorded_utc", utc_now()},
            {"operation_id", plan.operation_id},
            {"temporary_endpoint_id", plan.temporary_endpoint_id},
            {"warning", "Historical process tokens are not persistent application identities. "
                        "Recovery may target only the exact disposable fixture."},
            {"originals", originals}};
}
Json icons_json(const std::vector<IconExport>& icons) {
    Json results = Json::array();
    for (const auto& icon : icons) {
        results.push_back({{"observation_key", icon.observation_key},
                           {"source", icon.source},
                           {"window_icon_kind", icon.window_icon_kind},
                           {"app_user_model_id", icon.taskbar_app_user_model_id},
                           {"shortcut_match", icon.shortcut_match},
                           {"shortcut_path", icon.shortcut_path},
                           {"file", utf8(icon.file.wstring())},
                           {"application_specific", icon.application_specific},
                           {"result", operation_json(icon.result)}});
    }
    return results;
}
Json watcher_json(const ProcessStartProbe& probe) {
    return {{"subscription", operation_json(probe.subscription)},
            {"target_observed", probe.target_observed},
            {"identity_verified", probe.identity_verified},
            {"unrelated_events", probe.unrelated_events},
            {"detail", probe.detail}};
}

RecoveryPlan read_recovery_plan(const std::filesystem::path& file) {
    if (!std::filesystem::is_regular_file(file) ||
        std::filesystem::file_size(file) > 16 * 1024 * 1024) {
        throw std::runtime_error(
            "Recovery input must be a regular JSON file no larger than 16 MiB.");
    }
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Could not open recovery evidence.");
    }
    // Reject duplicate keys and excessive nesting rather than silently accepting
    // the final copy of an original preference supplied by a damaged document.
    std::map<int, std::set<std::string>> keys;
    const auto document =
        Json::parse(stream, [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > 64) {
                throw std::runtime_error("Recovery JSON exceeds the nesting bound.");
            }
            if (event == Json::parse_event_t::object_start) {
                keys[depth + 1].clear();
            }
            if (event == Json::parse_event_t::key &&
                !keys[depth].insert(value.get<std::string>()).second) {
                throw std::runtime_error("Recovery JSON contains a duplicate key.");
            }
            return true;
        });
    if (document.at("format") != "audiohole-plug-board-feasibility-recovery" ||
        !document.at("schema_version").is_number_integer() || document.at("schema_version") != 1 ||
        !document.at("originals").is_array() || document.at("originals").size() != 1) {
        throw std::runtime_error("Recovery format/version or disposable-fixture scope is invalid.");
    }
    RecoveryPlan plan;
    plan.operation_id = document.at("operation_id").get<std::string>();
    plan.temporary_endpoint_id = document.at("temporary_endpoint_id").get<std::string>();
    const auto& entry = document.at("originals").at(0);
    OriginalPreferences original;
    original.process.executable_path = entry.at("process").at("executable_path").get<std::string>();
    if (plan.operation_id.empty() || plan.operation_id.size() > 128 ||
        plan.temporary_endpoint_id.empty() || plan.temporary_endpoint_id.size() > 32768 ||
        original.process.executable_path.empty() ||
        original.process.executable_path.size() > 131072) {
        throw std::runtime_error("Recovery operation or identity exceeds its documented bounds.");
    }
    // A new disposable fixture gets a freshly verified token. Never trust the
    // historical PID to identify whatever process happens to own that PID now.
    for (std::size_t index = 0; index < render_roles.size(); ++index) {
        const auto& endpoint = entry.at("roles").at(role_name(render_roles[index]));
        if (!endpoint.is_null()) {
            original.endpoints[index] = endpoint.get<std::string>();
            if (original.endpoints[index]->empty() || original.endpoints[index]->size() > 32768) {
                throw std::runtime_error(
                    "An original endpoint must be nonempty or null for Windows default.");
            }
        }
    }
    plan.originals.push_back(std::move(original));
    return plan;
}

} // namespace audiohole::plug_board::harness
