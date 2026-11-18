#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace audiohole::plug_board {
// Presentation only: hide a trailing Windows executable extension without
// changing any canonical image identity, alias, process token or saved routing.
std::string application_display_name(const std::string&);

// These value types contain observations, not routing configuration. They may be
// copied across non-audio threads. They own no COM interfaces or Windows handles.
enum class SessionState { Inactive, Active, Expired, Unknown };
enum class IdentityKind { Win32Image, PackagedApplication, SystemSounds, Unresolved };
enum class Activity { Active, RecentlyActive, Inactive };

// A token protects an operation against PID reuse. It is NEVER a saved application
// identity. A recovery journal may record it as historical operation evidence.
// creation_time is the Windows process creation FILETIME, not Unix milliseconds.
struct ProcessToken {
    std::uint32_t pid = 0;
    std::uint64_t creation_time = 0;
    std::string executable_path;
    // Empty for ordinary Win32 processes; distinguishes packaged apps sharing a host.
    std::string app_user_model_id{};
    bool operator==(const ProcessToken&) const = default;
};

struct ApplicationIdentity {
    IdentityKind kind = IdentityKind::Unresolved;
    // Exact canonical image path or verified AppUserModelID. The platform resolver
    // supplies path semantics: the portable reducer must not guess or lowercase it.
    std::string value;
    bool operator==(const ApplicationIdentity&) const = default;
};

// AudioHole and its standalone helpers are implementation processes, never
// user audio sources. Match executable/package identity, not display labels.
bool is_audiohole_application(const ApplicationIdentity&);

struct SessionObservation {
    std::string endpoint_id;
    std::string instance_id;
    std::string session_id;
    SessionState state = SessionState::Unknown;
    ApplicationIdentity identity;
    std::optional<ProcessToken> process;
    std::string display_name;
    std::string icon_path;
    // A cross-process session's creator PID does not identify all stream owners.
    bool cross_process = false;
};

struct SessionSnapshot {
    // Enumeration records precede newer notifications for the same instance.
    std::vector<SessionObservation> sessions;
    bool complete = true;
    std::vector<std::string> diagnostics{};
};

struct ApplicationObservation {
    ApplicationIdentity identity;
    // Runtime grouping key, not the future persistent application UUID. Provisional
    // keys can contain live-owner tokens and must never be saved as assignments.
    std::string observation_key;
    std::string display_name;
    std::vector<SessionObservation> sessions;
    std::vector<std::string> active_endpoint_ids;
    std::vector<ProcessToken> verified_processes;
    bool complete = true;
};

// Collapse the complete cross-endpoint observation set before building any view.
// Repeated creation/enumeration notifications for one session are idempotent.
// Inactive and expired sessions remain diagnostic data, never extra app rows.
std::vector<ApplicationObservation> group_applications(const SessionSnapshot&);

// Only the caller's monotonic observation clock is accepted here. Unknown history
// is Inactive, including an app first discovered with only inactive sessions.
Activity classify_activity(bool has_active_sessions, std::optional<std::int64_t> last_active_ms,
                           std::int64_t now_ms, std::int64_t recent_window_ms);

struct PlaybackObservation {
    std::vector<std::string> active_endpoint_ids;
    bool complete = true;
};

// Select only Active sessions belonging to the exact verified process tokens.
// Incomplete enumeration or ambiguous cross-process ownership stays explicit.
PlaybackObservation observe_playback(const SessionSnapshot&, const std::vector<ProcessToken>&);

enum class PlaybackMatch { Unobserved, Matches, Differs, Multiple, Incomplete };
PlaybackMatch compare_playback(const PlaybackObservation&, const std::string& desired_endpoint);

} // namespace audiohole::plug_board
