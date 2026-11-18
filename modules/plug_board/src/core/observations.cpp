#include "audiohole/plug_board/observations.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace audiohole::plug_board {
std::string application_display_name(const std::string& value) {
    if (value.size() > 4) {
        auto extension = value.substr(value.size() - 4);
        for (auto& character : extension)
            if (character >= 'A' && character <= 'Z')
                character = static_cast<char>(character + ('a' - 'A'));
        if (extension == ".exe")
            return value.substr(0, value.size() - 4);
    }
    return value;
}
namespace {

// Length-prefix components so legal path/identifier punctuation cannot collide
// with separators in another identity. No hashing hides the identity evidence.
std::string component(const std::string& value) {
    return std::to_string(value.size()) + ":" + value;
}

std::string observation_key(const SessionObservation& session, std::size_t ordinal) {
    if (session.identity.kind != IdentityKind::Unresolved && !session.identity.value.empty()) {
        return std::to_string(static_cast<int>(session.identity.kind)) + ":" +
               component(session.identity.value);
    }
    if (session.process && session.process->pid != 0 && session.process->creation_time != 0) {
        return "unresolved-owner:" + std::to_string(session.process->pid) + ":" +
               std::to_string(session.process->creation_time);
    }
    // An inaccessible owner must not cause unrelated sources to merge by name.
    if (!session.instance_id.empty()) {
        return "unresolved-session:" + component(session.endpoint_id) +
               component(session.instance_id);
    }
    return "unidentified-observation:" + std::to_string(ordinal);
}

template <typename Value> void append_unique(std::vector<Value>& values, const Value& value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

// Use the same instance reconciliation for row grouping AND actual endpoint
// selection. Otherwise a stale Active enumeration could survive a later stop.
std::vector<SessionObservation> reconcile_instances(const SessionSnapshot& snapshot) {
    std::map<std::pair<std::string, std::string>, std::size_t> observed_instances;
    std::vector<SessionObservation> reconciled;
    for (const auto& session : snapshot.sessions) {
        // Missing instance identifiers cannot safely deduplicate notifications.
        // Preserve the observation and mark it uncertain rather than discard it.
        if (!session.instance_id.empty()) {
            const auto [entry, inserted] = observed_instances.emplace(
                std::make_pair(session.endpoint_id, session.instance_id), reconciled.size());
            if (!inserted) {
                // The input is ordered by observation: a later notification can
                // correct state/metadata from enumeration of the same instance.
                reconciled[entry->second] = session;
                continue;
            }
        }
        reconciled.push_back(session);
    }
    return reconciled;
}

} // namespace

bool is_audiohole_application(const ApplicationIdentity& identity) {
    if (identity.kind != IdentityKind::Win32Image &&
        identity.kind != IdentityKind::PackagedApplication)
        return false;
    auto name = identity.value;
    if (identity.kind == IdentityKind::Win32Image) {
        const auto separator = name.find_last_of("/\\");
        if (separator != std::string::npos)
            name.erase(0, separator + 1);
    }
    for (auto& byte : name)
        if (byte >= 'A' && byte <= 'Z')
            byte = static_cast<char>(byte - 'A' + 'a');
    if (identity.kind == IdentityKind::Win32Image && name.starts_with("audiohole") &&
        name.ends_with(".exe"))
        return true;
    if (identity.kind == IdentityKind::PackagedApplication && name.starts_with("audiohole"))
        return true;
    return name == "plug_board_probe.exe" || name == "plug_board_fixture.exe" ||
           name == "plug_board_tests.exe" || name == "plug_board_model_tests.exe" ||
           name == "plug_board_windows_tests.exe";
}

std::vector<ApplicationObservation> group_applications(const SessionSnapshot& snapshot) {
    std::vector<ApplicationObservation> applications;
    std::map<std::string, std::size_t> application_indexes;
    const auto reconciled = reconcile_instances(snapshot);
    for (std::size_t ordinal = 0; ordinal < reconciled.size(); ++ordinal) {
        const auto& session = reconciled[ordinal];
        const auto key = observation_key(session, ordinal);
        auto [entry, inserted] = application_indexes.emplace(key, applications.size());
        if (inserted) {
            ApplicationObservation application;
            application.identity = session.identity;
            application.observation_key = key;
            application.display_name = session.display_name;
            application.complete = snapshot.complete;
            applications.push_back(std::move(application));
        }
        auto& application = applications[entry->second];
        application.sessions.push_back(session);
        if (session.state == SessionState::Active) {
            append_unique(application.active_endpoint_ids, session.endpoint_id);
        }
        if (session.state == SessionState::Unknown || session.instance_id.empty() ||
            session.cross_process || session.identity.kind == IdentityKind::Unresolved) {
            application.complete = false;
        }
        if (session.process && session.process->pid != 0 && session.process->creation_time != 0 &&
            !session.process->executable_path.empty() && !session.cross_process &&
            session.identity.kind != IdentityKind::Unresolved &&
            session.identity.kind != IdentityKind::SystemSounds) {
            append_unique(application.verified_processes, *session.process);
        }
    }
    for (auto& application : applications) {
        std::sort(application.active_endpoint_ids.begin(), application.active_endpoint_ids.end());
    }
    return applications;
}

Activity classify_activity(bool has_active_sessions, std::optional<std::int64_t> last_active_ms,
                           std::int64_t now_ms, std::int64_t recent_window_ms) {
    if (has_active_sessions) {
        return Activity::Active;
    }
    // Reject negative/backward-clock ages instead of making stale data look fresh.
    if (last_active_ms && recent_window_ms > 0 && now_ms >= *last_active_ms &&
        static_cast<std::uint64_t>(now_ms) - static_cast<std::uint64_t>(*last_active_ms) <
            static_cast<std::uint64_t>(recent_window_ms)) {
        return Activity::RecentlyActive;
    }
    return Activity::Inactive;
}

PlaybackObservation observe_playback(const SessionSnapshot& snapshot,
                                     const std::vector<ProcessToken>& processes) {
    PlaybackObservation playback;
    playback.complete = snapshot.complete;
    for (const auto& session : reconcile_instances(snapshot)) {
        if (!session.process ||
            std::find(processes.begin(), processes.end(), *session.process) == processes.end()) {
            continue;
        }
        if (session.cross_process || session.state == SessionState::Unknown) {
            playback.complete = false;
        }
        if (session.state == SessionState::Active) {
            append_unique(playback.active_endpoint_ids, session.endpoint_id);
        }
    }
    std::sort(playback.active_endpoint_ids.begin(), playback.active_endpoint_ids.end());
    return playback;
}

PlaybackMatch compare_playback(const PlaybackObservation& playback,
                               const std::string& desired_endpoint) {
    if (!playback.complete) {
        return PlaybackMatch::Incomplete;
    }
    if (playback.active_endpoint_ids.empty()) {
        return PlaybackMatch::Unobserved;
    }
    if (playback.active_endpoint_ids.size() > 1) {
        return PlaybackMatch::Multiple;
    }
    return playback.active_endpoint_ids.front() == desired_endpoint ? PlaybackMatch::Matches
                                                                    : PlaybackMatch::Differs;
}

} // namespace audiohole::plug_board
