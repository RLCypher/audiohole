#pragma once

#include "audiohole/plug_board/observations.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace audiohole::plug_board {

using Document = nlohmann::ordered_json;

// Portable intent only. Raw Windows endpoint IDs, live PIDs, icon pixels and
// current-policy receipts belong in local/runtime state, never this document.
// Every record retains unrecognized JSON members for lossless forward editing.
struct EndpointTarget {
    std::optional<std::string> logical_device_id;
    Document extra = Document::object();
};
struct SocketAppearance {
    std::string color = "default";
    std::string icon = "none";
    Document extra = Document::object();
};
struct Socket {
    std::string id;
    std::string display_name;
    EndpointTarget endpoint;
    Document extra = Document::object();
    SocketAppearance appearance{};
};
struct BoardSettings {
    bool show_inactive_sources = false;
    std::uint32_t recently_active_seconds = 300;
    Document extra = Document::object();
};
struct BoardConfiguration {
    std::vector<Socket> sockets;
    std::map<std::string, std::string> application_assignments;
    BoardSettings settings;
    Document extra = Document::object();
};
struct RoutingBoard {
    std::string id;
    std::string display_name;
    std::string created_utc;
    std::string modified_utc;
    std::uint64_t working_revision = 0;
    std::uint64_t saved_revision = 0;
    BoardConfiguration working;
    BoardConfiguration saved;
    Document extra = Document::object();
};
struct ApplicationRecord {
    std::string id;
    std::string display_name;
    ApplicationIdentity identity;
    // False for discovery. Explicit moves/endpoint/board actions take ownership;
    // startup and profile activation reconcile only already-managed intentions.
    bool policy_managed = false;
    std::vector<ApplicationIdentity> verified_aliases;
    Document identity_extra = Document::object();
    std::vector<Document> alias_extras;
    Document extra = Document::object();
};
struct Library {
    std::uint64_t revision = 0;
    std::string default_board_id;
    std::map<std::string, std::string> profile_associations;
    std::vector<ApplicationRecord> applications;
    std::vector<RoutingBoard> boards;
    Document extra = Document::object();
};

// Bounded, strict UTF-8 JSON with duplicate-key and semantic-reference checks.
// Parse errors throw and never authorize replacing a bad document with defaults.
// Missing/ambiguous devices remain valid unresolved intent. Unknown versions fail.
Library parse_library(const std::string&);
Document parse_document(const std::string&, std::size_t maximum_bytes = 4 * 1024 * 1024);
std::string serialize_library(const Library&);
void validate_library(const Library&);
Library make_library(const std::string& first_board_id, const std::string& now_utc);
std::string new_id(const std::string& prefix);
std::string utc_timestamp();
std::optional<std::int64_t> utc_milliseconds(const std::string&);

const RoutingBoard* find_board(const Library&, const std::string&);
RoutingBoard* find_board(Library&, const std::string&);
const ApplicationRecord* find_application(const Library&, const std::string&);
std::string assigned_socket(const BoardConfiguration&, const std::string& application_id);

struct CreateBoard {
    std::string name;
};
struct DeleteBoard {
    std::string board_id;
    std::string replacement_default_id;
    bool confirmed = false;
};
struct SaveBoard {
    std::string board_id;
};
struct SaveBoardAs {
    std::string board_id;
    std::string name;
};
struct RenameBoard {
    std::string board_id;
    std::string name;
};
struct SetDefaultBoard {
    std::string board_id;
};
struct LinkProfile {
    std::string profile_id;
    std::string board_id;
    bool enabled = true;
};
struct CreateSocket {
    std::string board_id;
    std::string name;
};
struct DeleteSocket {
    std::string board_id;
    std::string socket_id;
    bool confirmed = false;
};
struct RenameSocket {
    std::string board_id;
    std::string socket_id;
    std::string name;
};
struct ReorderSocket {
    std::string board_id;
    std::string socket_id;
    std::size_t position = 1;
};
struct SetSocketEndpoint {
    std::string board_id;
    std::string socket_id;
    EndpointTarget endpoint;
};
struct SetSocketAppearance {
    std::string board_id;
    std::string socket_id;
    std::optional<std::string> color;
    std::optional<std::string> icon;
};
struct MoveApplication {
    std::string board_id;
    std::string application_id;
    std::string socket_id;
};
struct SetBoardSettings {
    std::string board_id;
    bool show_inactive_sources = false;
    std::uint32_t recently_active_seconds = 300;
};
struct SelectBoard {
    std::string board_id;
    bool explicit_user_action = true;
};
struct RetryRouting {
    std::string application_id;
};
struct RefreshApplications {};

using Command = std::variant<CreateBoard, DeleteBoard, SaveBoard, SaveBoardAs, RenameBoard,
                             SetDefaultBoard, LinkProfile, CreateSocket, DeleteSocket, RenameSocket,
                             ReorderSocket, SetSocketEndpoint, SetSocketAppearance, MoveApplication,
                             SetBoardSettings, SelectBoard, RetryRouting, RefreshApplications>;

struct LibraryEdit {
    Library library;
    std::string selected_board_id;
    std::string created_id;
    bool changed = false;
    bool reconcile = false;
};

// Pure command preparation: validate, copy and edit. The service must durably
// commit this candidate BEFORE adopting it or submitting native policy work.
// Confirmation is revision-bound by the service; these checks also protect direct
// model callers. An ID factory makes copy/remap behavior deterministic in tests.
LibraryEdit prepare_edit(const Library&, const std::string& selected_board_id, const Command&,
                         const std::string& now_utc,
                         const std::function<std::string(const std::string&)>& id_factory = new_id);

} // namespace audiohole::plug_board
