#include "audiohole/plug_board/library.hpp"
#include "audiohole/plug_board/socket_styles.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <locale>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace audiohole::plug_board {
namespace {
constexpr std::uint64_t maximum_revision = 9007199254740991ULL;
constexpr std::size_t maximum_document_bytes = 4 * 1024 * 1024;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

void valid_text(const std::string& value, std::size_t maximum, const char* field) {
    const auto characters = std::count_if(value.begin(), value.end(),
                                          [](unsigned char byte) { return (byte & 0xC0) != 0x80; });
    require(
        !value.empty() && value.find_first_not_of(' ') != std::string::npos &&
            value.size() <= maximum * 4 && static_cast<std::size_t>(characters) <= maximum &&
            std::none_of(value.begin(), value.end(), [](unsigned char byte) { return byte < 32; }),
        std::string(field) + " is empty, too long or contains control characters.");
    static_cast<void>(Document(value).dump()); // Strict UTF-8 validation, including API callers.
}

std::uint64_t revision(const Document& value) {
    require(value.is_number_integer(), "Revision must be a nonnegative integer.");
    require(value.is_number_unsigned() || value.get<std::int64_t>() >= 0,
            "Revision must not be negative.");
    const auto number = value.get<std::uint64_t>();
    require(number <= maximum_revision, "Revision exceeds the exact JSON integer bound.");
    return number;
}

Document extra(Document value, std::initializer_list<const char*> known) {
    require(value.is_object(), "A configuration record must be a JSON object.");
    for (const auto* key : known)
        value.erase(key);
    return value;
}

ApplicationIdentity read_identity(const Document& value) {
    const auto kind = value.at("kind").get<std::string>();
    if (kind == "win32_image")
        return {IdentityKind::Win32Image, value.at("executable_path").get<std::string>()};
    if (kind == "packaged_application")
        return {IdentityKind::PackagedApplication,
                value.at("app_user_model_id").get<std::string>()};
    if (kind == "system_sounds")
        return {IdentityKind::SystemSounds, "windows-system-sounds"};
    require(kind == "unresolved", "Unknown application identity kind.");
    return {};
}

Document identity_extra(const Document& value) {
    const auto kind = value.at("kind").get<std::string>();
    if (kind == "win32_image")
        return extra(value, {"kind", "executable_path"});
    if (kind == "packaged_application")
        return extra(value, {"kind", "app_user_model_id"});
    return extra(value, {"kind"});
}

Document write_identity(const ApplicationIdentity& identity, Document value) {
    switch (identity.kind) {
    case IdentityKind::Win32Image:
        value["kind"] = "win32_image";
        value["executable_path"] = identity.value;
        break;
    case IdentityKind::PackagedApplication:
        value["kind"] = "packaged_application";
        value["app_user_model_id"] = identity.value;
        break;
    case IdentityKind::SystemSounds:
        value["kind"] = "system_sounds";
        break;
    case IdentityKind::Unresolved:
        value["kind"] = "unresolved";
        break;
    }
    return value;
}

void valid_identity(const ApplicationIdentity& identity) {
    if (identity.kind == IdentityKind::Win32Image)
        valid_text(identity.value, 32768, "Executable path");
    if (identity.kind == IdentityKind::PackagedApplication)
        valid_text(identity.value, 1024, "Application ID");
    require(identity.kind != IdentityKind::SystemSounds ||
                identity.value == "windows-system-sounds",
            "System Sounds requires its canonical identity.");
    require(identity.kind != IdentityKind::Unresolved || identity.value.empty(),
            "An unresolved identity cannot carry unverified routing evidence.");
}

BoardConfiguration read_configuration(const Document& value) {
    BoardConfiguration result;
    result.extra = extra(value, {"sockets", "application_assignments", "settings"});
    require(value.at("sockets").is_array(), "Sockets must be an ordered array.");
    for (const auto& item : value.at("sockets")) {
        Socket socket;
        socket.id = item.at("id").get<std::string>();
        socket.display_name = item.at("display_name").get<std::string>();
        socket.extra = extra(item, {"id", "display_name", "endpoint", "appearance"});
        if (item.contains("appearance")) {
            const auto& appearance = item.at("appearance");
            require(appearance.is_object(), "Socket appearance must be an object.");
            socket.appearance.color = appearance.value("color", "default");
            socket.appearance.icon = appearance.value("icon", "none");
            socket.appearance.extra = extra(appearance, {"color", "icon"});
        }
        const auto& target = item.at("endpoint");
        const auto kind = target.at("kind").get<std::string>();
        require(kind == "system_default" || kind == "logical_device",
                "Unknown endpoint target kind.");
        if (kind == "logical_device")
            socket.endpoint.logical_device_id = target.at("logical_device_id").get<std::string>();
        socket.endpoint.extra = kind == "logical_device"
                                    ? extra(target, {"kind", "logical_device_id"})
                                    : extra(target, {"kind"});
        result.sockets.push_back(std::move(socket));
    }
    require(value.at("application_assignments").is_object(),
            "Application assignments must be an object.");
    for (auto entry = value.at("application_assignments").begin();
         entry != value.at("application_assignments").end(); ++entry)
        result.application_assignments.emplace(entry.key(), entry.value().get<std::string>());
    const auto& settings = value.at("settings");
    result.settings.show_inactive_sources = settings.at("show_inactive_sources").get<bool>();
    const auto seconds = revision(settings.at("recently_active_seconds"));
    require(seconds >= 1 && seconds <= 86400,
            "Recent activity interval must be 1 to 86400 seconds.");
    result.settings.recently_active_seconds = static_cast<std::uint32_t>(seconds);
    result.settings.extra = extra(settings, {"show_inactive_sources", "recently_active_seconds"});
    return result;
}

Document write_configuration(const BoardConfiguration& configuration) {
    auto result = configuration.extra;
    result["sockets"] = Document::array();
    for (const auto& socket : configuration.sockets) {
        auto item = socket.extra;
        item["id"] = socket.id;
        item["display_name"] = socket.display_name;
        if (socket.appearance.color != "default" || socket.appearance.icon != "none" ||
            !socket.appearance.extra.empty()) {
            auto appearance = socket.appearance.extra;
            appearance["color"] = socket.appearance.color;
            appearance["icon"] = socket.appearance.icon;
            item["appearance"] = std::move(appearance);
        }
        auto target = socket.endpoint.extra;
        target["kind"] = socket.endpoint.logical_device_id ? "logical_device" : "system_default";
        if (socket.endpoint.logical_device_id)
            target["logical_device_id"] = *socket.endpoint.logical_device_id;
        item["endpoint"] = std::move(target);
        result["sockets"].push_back(std::move(item));
    }
    result["application_assignments"] = configuration.application_assignments;
    auto settings = configuration.settings.extra;
    settings["show_inactive_sources"] = configuration.settings.show_inactive_sources;
    settings["recently_active_seconds"] = configuration.settings.recently_active_seconds;
    result["settings"] = std::move(settings);
    return result;
}

RoutingBoard& board_or_throw(Library& library, const std::string& id) {
    auto* board = find_board(library, id);
    require(board != nullptr, "The Routing Board no longer exists.");
    return *board;
}
Socket& socket_or_throw(RoutingBoard& board, const std::string& id) {
    const auto found = std::find_if(board.working.sockets.begin(), board.working.sockets.end(),
                                    [&](const auto& socket) { return socket.id == id; });
    require(found != board.working.sockets.end(), "The socket no longer exists.");
    return *found;
}
void changed(RoutingBoard& board, const std::string& now) {
    require(board.working_revision < maximum_revision, "Board revision is exhausted.");
    ++board.working_revision;
    board.modified_utc = now;
}
void manage(Library& library, const BoardConfiguration& configuration, const std::string& socket) {
    for (auto& app : library.applications)
        if (assigned_socket(configuration, app.id) == socket)
            app.policy_managed = true;
}
} // namespace

std::optional<std::int64_t> utc_milliseconds(const std::string& value) {
    if (value.size() < 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value.back() != 'Z')
        return std::nullopt;
    auto number = [&](std::size_t offset, std::size_t size) {
        if (!std::all_of(value.begin() + static_cast<std::ptrdiff_t>(offset),
                         value.begin() + static_cast<std::ptrdiff_t>(offset + size),
                         [](char digit) { return digit >= '0' && digit <= '9'; }))
            return -1;
        int result = -1;
        const auto parsed =
            std::from_chars(value.data() + offset, value.data() + offset + size, result);
        return parsed.ec == std::errc{} && parsed.ptr == value.data() + offset + size ? result : -1;
    };
    const int year = number(0, 4), month = number(5, 2), day = number(8, 2);
    const int hour = number(11, 2), minute = number(14, 2), second = number(17, 2);
    const std::chrono::year_month_day date{std::chrono::year(year), std::chrono::month(month),
                                           std::chrono::day(day)};
    if (year < 1601 || !date.ok() || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 59)
        return std::nullopt;
    int fraction = 0;
    if (value.size() != 20) {
        if (value[19] != '.' || value.size() < 22 || value.size() > 24)
            return std::nullopt;
        const auto digits = value.size() - 21;
        fraction = number(20, digits);
        if (fraction < 0)
            return std::nullopt;
        if (digits == 1)
            fraction *= 100;
        if (digits == 2)
            fraction *= 10;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::sys_days(date).time_since_epoch())
               .count() +
           ((hour * 60LL + minute) * 60 + second) * 1000 + fraction;
}

std::string utc_timestamp() {
    using namespace std::chrono;
    const auto now = floor<milliseconds>(system_clock::now());
    const auto day = floor<days>(now);
    const year_month_day date(day);
    const hh_mm_ss time(now - day);
    std::ostringstream result;
    result.imbue(std::locale::classic());
    result << std::setfill('0') << std::setw(4) << int(date.year()) << '-' << std::setw(2)
           << unsigned(date.month()) << '-' << std::setw(2) << unsigned(date.day()) << 'T'
           << std::setw(2) << time.hours().count() << ':' << std::setw(2) << time.minutes().count()
           << ':' << std::setw(2) << time.seconds().count() << '.' << std::setw(3)
           << time.subseconds().count() << 'Z';
    return result.str();
}

std::string new_id(const std::string& prefix) {
    std::random_device random;
    std::ostringstream result;
    result.imbue(std::locale::classic());
    result << prefix << '-';
    for (int byte = 0; byte < 16; ++byte)
        result << std::hex << std::setfill('0') << std::setw(2) << (random() & 255u);
    return result.str();
}

const RoutingBoard* find_board(const Library& library, const std::string& id) {
    const auto found = std::find_if(library.boards.begin(), library.boards.end(),
                                    [&](const auto& board) { return board.id == id; });
    return found == library.boards.end() ? nullptr : &*found;
}
RoutingBoard* find_board(Library& library, const std::string& id) {
    const auto found = std::find_if(library.boards.begin(), library.boards.end(),
                                    [&](const auto& board) { return board.id == id; });
    return found == library.boards.end() ? nullptr : &*found;
}
const ApplicationRecord* find_application(const Library& library, const std::string& id) {
    const auto found = std::find_if(library.applications.begin(), library.applications.end(),
                                    [&](const auto& app) { return app.id == id; });
    return found == library.applications.end() ? nullptr : &*found;
}
std::string assigned_socket(const BoardConfiguration& configuration, const std::string& app) {
    const auto found = configuration.application_assignments.find(app);
    return found == configuration.application_assignments.end() ? "system" : found->second;
}

void validate_library(const Library& library) {
    require(library.revision <= maximum_revision, "Library revision exceeds its bound.");
    require(!library.boards.empty() && library.boards.size() <= 128,
            "A library requires 1 to 128 Routing Boards.");
    require(library.applications.size() <= 20000, "The application catalog exceeds its bound.");
    require(library.profile_associations.size() <= 20000,
            "Profile association count exceeds its bound.");
    std::set<std::string> applications, boards, identities;
    for (const auto& app : library.applications) {
        valid_text(app.id, 128, "Application record ID");
        valid_text(app.display_name, 256, "Application name");
        require(applications.insert(app.id).second, "Duplicate application record ID.");
        require(app.verified_aliases.size() <= 64, "Too many verified executable aliases.");
        auto unique_identity = [&](const ApplicationIdentity& identity) {
            valid_identity(identity);
            if (identity.kind != IdentityKind::Unresolved)
                require(identities
                            .insert(std::to_string(static_cast<int>(identity.kind)) + ':' +
                                    identity.value)
                            .second,
                        "An application identity or alias belongs to multiple records.");
        };
        unique_identity(app.identity);
        for (const auto& alias : app.verified_aliases)
            unique_identity(alias);
    }
    for (const auto& board : library.boards) {
        valid_text(board.id, 128, "Routing Board ID");
        valid_text(board.display_name, 256, "Routing Board name");
        require(boards.insert(board.id).second, "Duplicate Routing Board ID.");
        require(utc_milliseconds(board.created_utc).has_value() &&
                    utc_milliseconds(board.modified_utc).has_value(),
                "Invalid Routing Board UTC timestamp.");
        require(board.saved_revision <= board.working_revision &&
                    board.working_revision <= maximum_revision,
                "Invalid working/saved revision ordering.");
        for (const auto* configuration : {&board.working, &board.saved}) {
            require(!configuration->sockets.empty() && configuration->sockets.size() <= 256 &&
                        configuration->sockets.front().id == "system",
                    "System must be the first socket; 1 to 256 sockets are supported.");
            std::set<std::string> sockets;
            for (const auto& socket : configuration->sockets) {
                valid_text(socket.id, 128, "Socket ID");
                valid_text(socket.display_name, 256, "Socket name");
                // Unknown future style IDs are preserved for forward-compatible
                // readers; the view uses an automatic/empty fallback for them.
                valid_text(socket.appearance.color, 64, "Socket color");
                valid_text(socket.appearance.icon, 64, "Socket icon");
                require(sockets.insert(socket.id).second, "Duplicate socket ID within a board.");
                if (socket.endpoint.logical_device_id)
                    valid_text(*socket.endpoint.logical_device_id, 128,
                               "Logical playback device ID");
            }
            for (const auto& [app, socket] : configuration->application_assignments)
                require(applications.contains(app) && sockets.contains(socket),
                        "An assignment refers to a missing application or socket.");
            require(configuration->settings.recently_active_seconds >= 1 &&
                        configuration->settings.recently_active_seconds <= 86400,
                    "Recent activity interval must be 1 to 86400 seconds.");
        }
    }
    require(boards.contains(library.default_board_id), "The default Routing Board is missing.");
    for (const auto& [profile, board] : library.profile_associations) {
        valid_text(profile, 128, "Profile ID");
        require(boards.contains(board), "A profile refers to a missing Routing Board.");
    }
}

Document parse_document(const std::string& bytes, std::size_t maximum_bytes) {
    require(bytes.size() <= maximum_bytes, "Plug Board JSON exceeds its size bound.");
    std::map<int, std::set<std::string>> keys;
    return Document::parse(bytes, [&](int depth, Document::parse_event_t event, Document& value) {
        require(depth <= 64, "Plug Board JSON exceeds the nesting bound.");
        if (event == Document::parse_event_t::object_start)
            keys[depth + 1].clear();
        if (event == Document::parse_event_t::key)
            require(keys[depth].insert(value.get<std::string>()).second,
                    "Plug Board JSON contains a duplicate key.");
        return true;
    });
}

Library parse_library(const std::string& bytes) {
    const auto document = parse_document(bytes, maximum_document_bytes);
    require(document.at("format") == "audiohole-plug-board" &&
                revision(document.at("schema_version")) == 1,
            "Unsupported Plug Board library format or version.");
    Library result;
    result.revision = revision(document.at("revision"));
    result.default_board_id = document.at("default_board_id").get<std::string>();
    result.extra = extra(document, {"format", "schema_version", "revision", "default_board_id",
                                    "profile_associations", "applications", "boards"});
    require(document.at("profile_associations").is_object(),
            "Profile associations must be an object.");
    for (auto entry = document.at("profile_associations").begin();
         entry != document.at("profile_associations").end(); ++entry)
        result.profile_associations.emplace(entry.key(), entry.value().get<std::string>());
    require(document.at("applications").is_array() && document.at("boards").is_array(),
            "Applications and Routing Boards must be arrays.");
    for (const auto& item : document.at("applications")) {
        ApplicationRecord app;
        app.id = item.at("id").get<std::string>();
        app.display_name = item.at("display_name").get<std::string>();
        app.identity = read_identity(item.at("identity"));
        app.identity_extra = identity_extra(item.at("identity"));
        app.policy_managed = item.at("policy_managed").get<bool>();
        app.extra =
            extra(item, {"id", "display_name", "identity", "policy_managed", "verified_aliases"});
        if (item.contains("verified_aliases")) {
            require(item.at("verified_aliases").is_array(), "Verified aliases must be an array.");
            for (const auto& alias : item.at("verified_aliases")) {
                app.verified_aliases.push_back(read_identity(alias));
                app.alias_extras.push_back(identity_extra(alias));
            }
        }
        result.applications.push_back(std::move(app));
    }
    for (const auto& item : document.at("boards")) {
        RoutingBoard board;
        board.id = item.at("id").get<std::string>();
        board.display_name = item.at("display_name").get<std::string>();
        board.created_utc = item.at("created_utc").get<std::string>();
        board.modified_utc = item.at("modified_utc").get<std::string>();
        board.working_revision = revision(item.at("working_revision"));
        board.saved_revision = revision(item.at("saved_revision"));
        board.working = read_configuration(item.at("working"));
        board.saved = read_configuration(item.at("saved"));
        board.extra = extra(item, {"id", "display_name", "created_utc", "modified_utc",
                                   "working_revision", "saved_revision", "working", "saved"});
        result.boards.push_back(std::move(board));
    }
    validate_library(result);
    return result;
}

std::string serialize_library(const Library& library) {
    validate_library(library);
    auto result = library.extra;
    result["format"] = "audiohole-plug-board";
    result["schema_version"] = 1;
    result["revision"] = library.revision;
    result["default_board_id"] = library.default_board_id;
    result["profile_associations"] = library.profile_associations;
    result["applications"] = Document::array();
    for (const auto& app : library.applications) {
        auto item = app.extra;
        item["id"] = app.id;
        item["display_name"] = app.display_name;
        item["identity"] = write_identity(app.identity, app.identity_extra);
        item["policy_managed"] = app.policy_managed;
        item["verified_aliases"] = Document::array();
        for (std::size_t index = 0; index < app.verified_aliases.size(); ++index)
            item["verified_aliases"].push_back(write_identity(
                app.verified_aliases[index],
                index < app.alias_extras.size() ? app.alias_extras[index] : Document::object()));
        result["applications"].push_back(std::move(item));
    }
    result["boards"] = Document::array();
    for (const auto& board : library.boards) {
        auto item = board.extra;
        item["id"] = board.id;
        item["display_name"] = board.display_name;
        item["created_utc"] = board.created_utc;
        item["modified_utc"] = board.modified_utc;
        item["working_revision"] = board.working_revision;
        item["saved_revision"] = board.saved_revision;
        item["working"] = write_configuration(board.working);
        item["saved"] = write_configuration(board.saved);
        result["boards"].push_back(std::move(item));
    }
    auto bytes = result.dump(2) + '\n';
    require(bytes.size() <= maximum_document_bytes, "Plug Board library exceeds 4 MiB.");
    return bytes;
}

Library make_library(const std::string& id, const std::string& now) {
    Library result;
    result.default_board_id = id;
    RoutingBoard board;
    board.id = id;
    board.display_name = "Default";
    board.created_utc = now;
    board.modified_utc = now;
    board.working.sockets.push_back({"system", "System (Default)", {}});
    board.saved = board.working;
    result.boards.push_back(std::move(board));
    validate_library(result);
    return result;
}

LibraryEdit prepare_edit(const Library& original, const std::string& selected,
                         const Command& command, const std::string& now,
                         const std::function<std::string(const std::string&)>& id_factory) {
    validate_library(original);
    require(utc_milliseconds(now).has_value(), "The command timestamp must be valid UTC.");
    LibraryEdit edit{original, selected, {}, true, false};
    std::visit(
        [&](const auto& operation) {
            using T = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<T, CreateBoard>) {
                auto fresh = make_library(id_factory("board"), now).boards.front();
                fresh.display_name =
                    operation.name.empty()
                        ? "Routing Board " + std::to_string(edit.library.boards.size() + 1)
                        : operation.name;
                edit.created_id = fresh.id;
                edit.selected_board_id = fresh.id;
                edit.library.boards.push_back(std::move(fresh));
                edit.reconcile = true;
                for (auto& app : edit.library.applications)
                    app.policy_managed = true;
            } else if constexpr (std::is_same_v<T, DeleteBoard>) {
                require(operation.confirmed, "Deleting a Routing Board requires confirmation.");
                board_or_throw(edit.library, operation.board_id);
                require(edit.library.boards.size() > 1,
                        "The final Routing Board cannot be deleted.");
                if (operation.board_id == edit.library.default_board_id) {
                    require(operation.replacement_default_id != operation.board_id,
                            "Choose a surviving default Routing Board.");
                    board_or_throw(edit.library, operation.replacement_default_id);
                    edit.library.default_board_id = operation.replacement_default_id;
                }
                for (auto& [profile, board] : edit.library.profile_associations)
                    if (board == operation.board_id)
                        board = edit.library.default_board_id;
                std::erase_if(edit.library.boards,
                              [&](const auto& board) { return board.id == operation.board_id; });
                if (selected == operation.board_id) {
                    edit.selected_board_id = edit.library.default_board_id;
                    edit.reconcile = true;
                }
            } else if constexpr (std::is_same_v<T, SaveBoard>) {
                auto& board = board_or_throw(edit.library, operation.board_id);
                board.saved = board.working;
                board.saved_revision = board.working_revision;
                board.modified_utc = now;
            } else if constexpr (std::is_same_v<T, SaveBoardAs>) {
                auto copy = board_or_throw(edit.library, operation.board_id);
                copy.id = id_factory("board");
                copy.display_name = operation.name;
                copy.created_utc = now;
                copy.modified_utc = now;
                copy.working_revision = 0;
                copy.saved_revision = 0;
                std::map<std::string, std::string> sockets{{"system", "system"}};
                for (auto& socket : copy.working.sockets)
                    if (socket.id != "system") {
                        const auto old = socket.id;
                        socket.id = id_factory("socket");
                        sockets.emplace(old, socket.id);
                    }
                for (auto& [app, socket] : copy.working.application_assignments)
                    socket = sockets.at(socket);
                copy.saved = copy.working;
                edit.created_id = copy.id;
                edit.selected_board_id = copy.id;
                edit.library.boards.push_back(std::move(copy));
                edit.reconcile = true;
            } else if constexpr (std::is_same_v<T, RenameBoard>) {
                auto& board = board_or_throw(edit.library, operation.board_id);
                board.display_name = operation.name;
                board.modified_utc = now;
            } else if constexpr (std::is_same_v<T, SetDefaultBoard>) {
                board_or_throw(edit.library, operation.board_id);
                edit.library.default_board_id = operation.board_id;
            } else if constexpr (std::is_same_v<T, LinkProfile>) {
                valid_text(operation.profile_id, 128, "Profile ID");
                board_or_throw(edit.library, operation.board_id);
                if (operation.enabled)
                    edit.library.profile_associations[operation.profile_id] = operation.board_id;
                else if (edit.library.profile_associations.contains(operation.profile_id) &&
                         edit.library.profile_associations.at(operation.profile_id) ==
                             operation.board_id)
                    edit.library.profile_associations.erase(operation.profile_id);
            } else if constexpr (std::is_same_v<T, SelectBoard>) {
                board_or_throw(edit.library, operation.board_id);
                edit.selected_board_id = operation.board_id;
                edit.reconcile = true;
                edit.changed = false;
                if (operation.explicit_user_action)
                    for (auto& app : edit.library.applications) {
                        if (!app.policy_managed) {
                            app.policy_managed = true;
                            edit.changed = true;
                        }
                    }
            } else if constexpr (std::is_same_v<T, RetryRouting> ||
                                 std::is_same_v<T, RefreshApplications>) {
                edit.changed = false;
                edit.reconcile = std::is_same_v<T, RetryRouting>;
            } else {
                auto& board = board_or_throw(edit.library, operation.board_id);
                if constexpr (std::is_same_v<T, CreateSocket>) {
                    const auto id = id_factory("socket");
                    edit.created_id = id;
                    board.working.sockets.push_back(
                        {id, operation.name.empty() ? "New Socket" : operation.name, {}});
                } else if constexpr (std::is_same_v<T, DeleteSocket>) {
                    require(operation.confirmed, "Deleting a socket requires confirmation.");
                    require(operation.socket_id != "system", "System cannot be deleted.");
                    socket_or_throw(board, operation.socket_id);
                    manage(edit.library, board.working, operation.socket_id);
                    for (auto& [app, socket] : board.working.application_assignments)
                        if (socket == operation.socket_id)
                            socket = "system";
                    std::erase_if(board.working.sockets, [&](const auto& socket) {
                        return socket.id == operation.socket_id;
                    });
                    edit.reconcile = selected == board.id;
                } else if constexpr (std::is_same_v<T, RenameSocket>) {
                    socket_or_throw(board, operation.socket_id).display_name = operation.name;
                } else if constexpr (std::is_same_v<T, ReorderSocket>) {
                    require(operation.socket_id != "system" && operation.position > 0 &&
                                operation.position < board.working.sockets.size(),
                            "System must remain first; choose a valid socket position.");
                    auto socket = socket_or_throw(board, operation.socket_id);
                    std::erase_if(board.working.sockets,
                                  [&](const auto& item) { return item.id == socket.id; });
                    board.working.sockets.insert(
                        board.working.sockets.begin() +
                            static_cast<std::ptrdiff_t>(operation.position),
                        std::move(socket));
                } else if constexpr (std::is_same_v<T, SetSocketEndpoint>) {
                    auto& endpoint = socket_or_throw(board, operation.socket_id).endpoint;
                    endpoint.logical_device_id = operation.endpoint.logical_device_id;
                    // A UI edit changes known intent without deleting fields
                    // introduced by a later compatible producer.
                    endpoint.extra.update(operation.endpoint.extra);
                    manage(edit.library, board.working, operation.socket_id);
                    edit.reconcile = selected == board.id;
                } else if constexpr (std::is_same_v<T, SetSocketAppearance>) {
                    auto& appearance = socket_or_throw(board, operation.socket_id).appearance;
                    if (operation.color) {
                        require(*operation.color == "default" ||
                                    std::any_of(socket_colors.begin(), socket_colors.end(),
                                                [&](const auto& color) {
                                                    return color.id == *operation.color;
                                                }),
                                "Choose an available socket color.");
                        appearance.color = *operation.color;
                    }
                    if (operation.icon) {
                        require(*operation.icon == "none" ||
                                    std::any_of(socket_icons.begin(), socket_icons.end(),
                                                [&](const auto& icon) {
                                                    return icon.id == *operation.icon;
                                                }),
                                "Choose an available socket icon.");
                        appearance.icon = *operation.icon;
                    }
                    // Appearance edits persist immediately, but never enroll
                    // an application or schedule a native preference operation.
                } else if constexpr (std::is_same_v<T, MoveApplication>) {
                    // Assignment is durable working-board intent, independent
                    // of session lifetime, recent-activity age and icon caches.
                    // Only a later user edit/removal changes this mapping.
                    socket_or_throw(board, operation.socket_id);
                    require(find_application(edit.library, operation.application_id),
                            "The application is no longer in the catalog.");
                    for (auto& app : edit.library.applications)
                        if (app.id == operation.application_id)
                            app.policy_managed = true;
                    board.working.application_assignments[operation.application_id] =
                        operation.socket_id;
                    edit.reconcile = selected == board.id;
                } else if constexpr (std::is_same_v<T, SetBoardSettings>) {
                    board.working.settings.show_inactive_sources = operation.show_inactive_sources;
                    board.working.settings.recently_active_seconds =
                        operation.recently_active_seconds;
                }
                changed(board, now);
            }
        },
        command);
    if (edit.changed) {
        require(edit.library.revision < maximum_revision, "Library revision is exhausted.");
        ++edit.library.revision;
    }
    validate_library(edit.library);
    return edit;
}

} // namespace audiohole::plug_board
