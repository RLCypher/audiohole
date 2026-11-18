#include "presentation/messages.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace audiohole::presentation {
namespace {
struct NeutralEntry {
    const char* id;
    const char* text;
};
// Only stable Vanilla information is compiled in. Optional voice files cannot
// replace these facts, warning text, endpoint names, or accessibility labels.
constexpr std::array<NeutralEntry, static_cast<std::size_t>(Message::Count)> neutral{{
    {"ready", "Ready to configure routing."},
    {"created", "Hole created in the editor."},
    {"enabled", "Hole enabled in the editor."},
    {"disabled", "Hole disabled in the editor."},
    {"source", "Source selected in the editor."},
    {"destination", "Destination selected in the editor."},
    {"started", "Routing is Running."},
    {"stopped", "Routing stopped."},
    {"duplex", "Duplex selected. Each direction has independent settings."},
    {"activated", "Profile activated."},
    {"pending", "Startup activation is pending."},
    {"reconnected", "Device reconnected."},
    {"disconnected", "Device disconnected."},
    {"signal", "Source activity detected. Destination audibility is untested."},
    {"no_signal", "No source activity. Silence does not establish disconnection."},
    {"beep_ready", "Select a destination for Test Beep."},
    {"beep_complete", "Test Beep completed."},
    {"beep_rejected", "Select an available playback destination before testing."},
    {"questionable", "This setup may introduce latency or feedback. Review the settings."},
    {"invalid", "Enter a valid value; review the detailed validation message."},
    {"recovering", "Recovery is in progress."},
    {"dfa", "DFA Acknowledged"},
    {"cancelled", "Pending operation cancelled."},
    {"personality", "Presentation mode changed. Operational behavior is unchanged."},
    {"scan_started", "Scan started."},
    {"scan_completed", "Scan completed."},
    {"scan_cancelled", "Scan cancelled."},
    {"scan_failed", "Scan failed. Review the technical diagnostics."},
    {"dfa_minimum", "DFA Acknowledged"},
    {"dfa_nice", "DFA Acknowledged"},
    {"dfa_dank", "DFA Acknowledged"},
    {"dfa_metal", "DFA Acknowledged"},
    {"dfa_six_seven", "DFA Acknowledged"},
    {"dfa_maximum", "DFA Acknowledged"},
    {"buffer_changed", "Buffer changed in the editor. Save / Apply is required."},
    {"simplex", "Simplex selected. Save / Apply is required to change active routing."},
    {"device_saved", "Device saved. Windows name and logical identity are unchanged."},
    {"scan_level_changed", "Testing level changed. Applies to the next scan."},
    {"scan_scope_changed", "Scan scope changed. Applies to the next scan."},
    {"diagnostics_copied", "Visible diagnostics copied to the clipboard."},
    {"diagnostics_exported", "Visible diagnostics exported."},
    {"diagnostics_cleared", "Diagnostics log cleared. Routing and settings are unchanged."},
    {"dfa_101", "DFA Acknowledged"},
    {"dfa_121", "DFA Acknowledged"},
    {"dfa_134", "DFA Acknowledged"},
    {"dfa_222", "DFA Acknowledged"},
    {"dfa_256", "DFA Acknowledged"},
    {"dfa_300", "DFA Acknowledged"},
    {"dfa_314", "DFA Acknowledged"},
    {"dfa_333", "DFA Acknowledged"},
    {"dfa_401", "DFA Acknowledged"},
    {"dfa_403", "DFA Acknowledged"},
    {"dfa_404", "DFA Acknowledged"},
    {"dfa_409", "DFA Acknowledged"},
    {"dfa_410", "DFA Acknowledged"},
    {"dfa_413", "DFA Acknowledged"},
    {"dfa_418", "DFA Acknowledged"},
    {"dfa_429", "DFA Acknowledged"},
    {"dfa_451", "DFA Acknowledged"},
    {"dfa_500", "DFA Acknowledged"},
    {"dfa_503", "DFA Acknowledged"},
    {"dfa_621", "DFA Acknowledged"},
    {"dfa_707", "DFA Acknowledged"},
    {"dfa_777", "DFA Acknowledged"},
    {"dfa_808", "DFA Acknowledged"},
    {"dfa_909", "DFA Acknowledged"},
    {"dfa_911", "DFA Acknowledged"},
    {"dfa_1024", "DFA Acknowledged"},
    {"dfa_negative", "DFA must be at least 0.00. Previous value retained."},
    {"dfa_above_maximum", "DFA must be at most 11.00. Previous value retained."},
    {"bother", "Personality interaction. Routing settings are unchanged."},
    {"dream", ""}, // Cosmetic sleep text has no technical/operational claim.
    {"advanced_tuning_enabled", "Overrides enabled. Save / Apply to use them."},
    {"manual_override_applied", "Overrides applied to Preview (simulated)."},
    {"automatic_configuration_restored", "Automatic selected. Saved overrides are inactive."},
    {"device_connected", "Device connected."},
    {"device_disconnected", "Device disconnected."},
    {"device_unavailable", "Device became unavailable."},
    {"level_check_started", "Checking the selected capture device's level."},
    {"beep_started", "Test beep requested on the selected playback device."},
    {"error_minor", "The action could not be completed. Review the detailed error."},
    {"error_major",
     "AudioHole could not continue the requested operation. Review the detailed error."},
    {"plug_board_socket_created", "Socket created."},
    {"plug_board_socket_moved", "Socket order saved."},
    {"plug_board_socket_deleted", "Socket deleted; its applications are assigned to System."},
    {"plug_board_app_moved", "Application socket assignment saved. Windows policy and observed "
                             "playback are reported separately."},
    {"plug_board_socket_renamed", "Socket renamed."},
    {"plug_board_socket_icon_assigned", "Socket icon saved."},
    {"plug_board_socket_color_assigned", "Socket color saved."},
    {"plug_board_socket_endpoint_changed", "Socket endpoint preference saved."},
    {"plug_board_board_profile_associated", "Board association with the profile saved."},
    {"plug_board_windows_mixer_opened", "Windows Volume Mixer opened."},
}};
constexpr std::uintmax_t maximum_file_bytes = 256 * 1024;
constexpr std::size_t maximum_catalogs = 32;
constexpr std::size_t maximum_identifier_bytes = 64;
constexpr std::size_t maximum_display_name_bytes = 128;
constexpr std::size_t maximum_event_pools = 128;
constexpr std::size_t maximum_variants_per_pool = 32;
constexpr std::size_t maximum_variant_bytes = 2048;
constexpr std::int64_t maximum_variant_weight = 100;
constexpr int maximum_json_depth = 12;
bool identifier(const std::string& value) {
    return !value.empty() && value.size() <= maximum_identifier_bytes &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
           });
}
bool presentation_text(const std::string& value, std::size_t limit) {
    // No embedded terminal/control sequences. UTF-8 validity is checked by dump()
    // after parsing, before publication; text is displayed literally, never executed.
    return !value.empty() && value.size() <= limit &&
           std::none_of(value.begin(), value.end(),
                        [](unsigned char c) { return c < 32 || c == 127; });
}
void require_valid_catalog(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid personality structure");
}
} // namespace
std::string mode_id(Mode mode) {
    switch (mode) {
    case Mode::Norm:
        return "default";
    case Mode::UwU:
        return "uwu";
    default:
        return "vanilla";
    }
}
std::string technical(Message event) {
    return neutral.at(static_cast<std::size_t>(event)).text;
}
std::string event_id(Message event) {
    return neutral.at(static_cast<std::size_t>(event)).id;
}
DfaInput parse_dfa(std::string_view input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())))
        input.remove_prefix(1);
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back())))
        input.remove_suffix(1);
    if (input.empty())
        return {};
    std::size_t position = 0;
    const bool negative = input.front() == '-';
    if (input.front() == '-' || input.front() == '+')
        ++position;
    auto digit = [](char value) { return value >= '0' && value <= '9'; };
    std::string digits;
    while (position < input.size() && digit(input[position]))
        digits += input[position++];
    const auto integer_digits = static_cast<std::int64_t>(digits.size());
    if (position < input.size() && input[position] == '.') {
        ++position;
        while (position < input.size() && digit(input[position]))
            digits += input[position++];
    }
    if (digits.empty())
        return {};
    std::int64_t exponent = 0;
    if (position < input.size() && (input[position] == 'e' || input[position] == 'E')) {
        ++position;
        bool exponent_negative = false;
        if (position < input.size() && (input[position] == '-' || input[position] == '+')) {
            exponent_negative = input[position] == '-';
            ++position;
        }
        const auto exponent_start = position;
        // Exponents beyond the entire text length cannot bring any nonzero
        // mantissa near 0..11. Saturating that magnitude avoids integer overflow
        // without changing the range/rounding result or limiting decimal precision.
        const auto limit = static_cast<std::int64_t>(input.size()) + 4;
        while (position < input.size() && digit(input[position])) {
            const int next = input[position++] - '0';
            exponent = exponent > limit / 10 || (exponent == limit / 10 && next > limit % 10)
                           ? limit
                           : exponent * 10 + next;
        }
        if (position == exponent_start)
            return {};
        if (exponent_negative)
            exponent = -exponent;
    }
    if (position != input.size())
        return {};
    const auto first = digits.find_first_not_of('0');
    if (first == std::string::npos)
        return {DfaInputStatus::Valid, 0};
    if (negative)
        return {DfaInputStatus::Negative};
    // Decimal point position relative to the first significant digit. No binary
    // float conversion occurs, including when comparing 11.000...001 with 11.
    const auto point = integer_digits - static_cast<std::int64_t>(first) + exponent;
    const std::string_view significant(digits.data() + first, digits.size() - first);
    auto at = [&](std::int64_t index) -> int {
        return index >= 0 && static_cast<std::uint64_t>(index) < significant.size()
                   ? significant[static_cast<std::size_t>(index)] - '0'
                   : 0;
    };
    if (point > 2)
        return {DfaInputStatus::AboveMaximum};
    if (point == 2) {
        const int integral = at(0) * 10 + at(1);
        const bool fraction = significant.size() > 2 && significant.substr(2).find_first_not_of(
                                                            '0') != std::string_view::npos;
        if (integral > 11 || (integral == 11 && fraction))
            return {DfaInputStatus::AboveMaximum};
    }
    int hundredths = at(point - 2) * 1000 + at(point - 1) * 100 + at(point) * 10 + at(point + 1);
    if (at(point + 2) >= 5)
        ++hundredths;
    return {DfaInputStatus::Valid, hundredths};
}
std::string dfa_text(int hundredths) {
    if (hundredths < 0 || hundredths > 1100)
        throw std::invalid_argument("DFA value must be between 0 and 1100 hundredths");
    const int fraction = hundredths % 100;
    return std::to_string(hundredths / 100) + "." + (fraction < 10 ? "0" : "") +
           std::to_string(fraction);
}
Message dfa_event(int value) {
    switch (value) {
    case 101:
        return Message::Dfa101;
    case 121:
        return Message::Dfa121;
    case 134:
        return Message::Dfa134;
    case 222:
        return Message::Dfa222;
    case 256:
        return Message::Dfa256;
    case 300:
        return Message::Dfa300;
    case 314:
        return Message::Dfa314;
    case 333:
        return Message::Dfa333;
    case 401:
        return Message::Dfa401;
    case 403:
        return Message::Dfa403;
    case 404:
        return Message::Dfa404;
    case 409:
        return Message::Dfa409;
    case 410:
        return Message::Dfa410;
    case 413:
        return Message::Dfa413;
    case 418:
        return Message::Dfa418;
    case 429:
        return Message::Dfa429;
    case 451:
        return Message::Dfa451;
    case 500:
        return Message::Dfa500;
    case 503:
        return Message::Dfa503;
    case 621:
        return Message::Dfa621;
    case 707:
        return Message::Dfa707;
    case 777:
        return Message::Dfa777;
    case 808:
        return Message::Dfa808;
    case 909:
        return Message::Dfa909;
    case 911:
        return Message::Dfa911;
    case 1024:
        return Message::Dfa1024;
    case 0:
        return Message::DfaMinimum;
    case 69:
    case 690:
        return Message::DfaNice;
    case 420:
        return Message::DfaDank;
    case 666:
        return Message::DfaMetal;
    case 670:
        return Message::DfaSixSeven;
    case 1100:
        return Message::DfaMaximum;
    default:
        return Message::Dfa;
    }
}
Messages::Messages(std::uint32_t seed) : random_(seed) {
    choose_ = [this](unsigned total) {
        return std::uniform_int_distribution<unsigned>(0, total - 1)(random_);
    };
}
Messages::Messages(std::function<unsigned(unsigned)> choose) : choose_(std::move(choose)) {}
LoadResult Messages::load_file(const std::filesystem::path& path) {
    try {
        require_valid_catalog(std::filesystem::is_regular_file(path) &&
                              std::filesystem::file_size(path) <= maximum_file_bytes);
        // Bound the actual read too: a file can grow after the size check.
        std::ifstream input(path, std::ios::binary);
        std::string bytes(static_cast<std::size_t>(maximum_file_bytes) + 1, '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        require_valid_catalog(input.eof() && !input.bad());
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        require_valid_catalog(bytes.size() <= maximum_file_bytes);
        // Reject excessive nesting before JSON parsing can exhaust the stack.
        // Duplicate object keys are rejected rather than silently taking the last.
        std::vector<std::set<std::string>> object_keys;
        const auto root = nlohmann::json::parse(bytes, [&](int depth,
                                                           nlohmann::json::parse_event_t event,
                                                           nlohmann::json& value) {
            require_valid_catalog(depth <= maximum_json_depth);
            if (event == nlohmann::json::parse_event_t::object_start)
                object_keys.emplace_back();
            else if (event == nlohmann::json::parse_event_t::object_end)
                object_keys.pop_back();
            else if (event == nlohmann::json::parse_event_t::key)
                require_valid_catalog(object_keys.back().insert(value.get<std::string>()).second);
            return true;
        });
        (void)root.dump(); // Reject invalid UTF-8, including inside otherwise unused future events.
        require_valid_catalog(root.is_object() && root.size() == 4 &&
                              (root.at("schema_version") == 1 || root.at("schema_version") == 2));
        const bool metadata_required = root.at("schema_version") == 2;
        std::set<std::string> response_ids;
        const std::set<std::string> expressions{
            "idle",        "happy",     "thinking", "working",     "talking",
            "confused",    "concerned", "error",    "angry",       "surprised",
            "celebrating", "sleeping",  "smug",     "embarrassed", "laughing",
            "suspicious",  "panicking", "bored",    "mischievous", "deadpan"};
        Personality candidate;
        candidate.id = root.at("id").get<std::string>();
        candidate.display_name = root.at("display_name").get<std::string>();
        require_valid_catalog(
            identifier(candidate.id) && candidate.id != "vanilla" &&
            presentation_text(candidate.display_name, maximum_display_name_bytes));
        const auto& events = root.at("events");
        require_valid_catalog(events.is_object() && events.size() <= maximum_event_pools);
        require_valid_catalog(!metadata_required || !events.empty());
        for (const auto& [key, pool] : events.items()) {
            require_valid_catalog(identifier(key) && pool.is_array() && !pool.empty() &&
                                  pool.size() <= maximum_variants_per_pool);
            for (const auto& entry : pool) {
                Variant variant;
                if (entry.is_string()) {
                    require_valid_catalog(!metadata_required);
                    variant.text = entry.get<std::string>();
                } else {
                    require_valid_catalog(entry.is_object() && entry.contains("text"));
                    for (const auto& [field, value] : entry.items()) {
                        (void)value;
                        require_valid_catalog(
                            field == "text" || field == "weight" ||
                            (metadata_required &&
                             (field == "id" || field == "category" || field == "subcategory" ||
                              field == "expression" || field == "fallback_expression")));
                    }
                    variant.text = entry.at("text").get<std::string>();
                    if (entry.contains("weight")) {
                        require_valid_catalog(entry.at("weight").is_number_integer());
                        const auto weight = entry.at("weight").get<std::int64_t>();
                        require_valid_catalog(weight >= 1 && weight <= maximum_variant_weight);
                        variant.weight = static_cast<unsigned>(weight);
                    }
                    if (metadata_required) {
                        variant.id = entry.at("id").get<std::string>();
                        variant.category = entry.at("category").get<std::string>();
                        variant.subcategory = entry.value("subcategory", std::string{});
                        variant.expression = entry.at("expression").get<std::string>();
                        variant.fallback_expression =
                            entry.value("fallback_expression", std::string{});
                        require_valid_catalog(
                            identifier(variant.id) && response_ids.insert(variant.id).second &&
                            identifier(variant.category) &&
                            (variant.subcategory.empty() || identifier(variant.subcategory)) &&
                            expressions.contains(variant.expression) &&
                            (variant.fallback_expression.empty() ||
                             expressions.contains(variant.fallback_expression)));
                    }
                }
                require_valid_catalog(presentation_text(variant.text, maximum_variant_bytes));
                auto& variants = candidate.events[key];
                require_valid_catalog(
                    std::none_of(variants.begin(), variants.end(), [&](const auto& previous) {
                        return previous.text == variant.text;
                    }));
                variants.push_back(std::move(variant));
            }
        }
        require_valid_catalog(catalogs_.contains(candidate.id) ||
                              catalogs_.size() < maximum_catalogs);
        const auto id = candidate.id;
        // A file whose ID was edited must not leave its previous identity active.
        for (auto previous = catalog_sources_.begin(); previous != catalog_sources_.end();) {
            if (previous->second == path.lexically_normal() && previous->first != id) {
                catalogs_.erase(previous->first);
                previous = catalog_sources_.erase(previous);
            } else
                ++previous;
        }
        catalogs_[id] = std::move(candidate);
        catalog_sources_[id] = path.lexically_normal();
        history_.clear();
        return {true, id,
                "Personality loaded. Technical messages remain built-in Vanilla information."};
    } catch (const std::exception&) {
        for (auto previous = catalog_sources_.begin(); previous != catalog_sources_.end();) {
            if (previous->second == path.lexically_normal()) {
                catalogs_.erase(previous->first);
                previous = catalog_sources_.erase(previous);
            } else
                ++previous;
        }
        history_.clear();
        return {
            false,
            {},
            "Personality file unavailable or invalid. Use UTF-8 JSON schema version 1 or 2 with "
            "valid commentary pools; Vanilla remains available."};
    }
}
std::vector<std::string> Messages::load_directory(const std::filesystem::path& directory) {
    catalogs_.clear();
    catalog_sources_.clear();
    history_.clear();
    std::vector<std::string> warnings;
    std::error_code error;
    std::vector<std::filesystem::path> files;
    std::size_t visited = 0;
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (++visited > 512) {
            warnings.push_back(
                "Personality directory has too many entries; presentation disabled.");
            return warnings;
        }
        const auto path = iterator->path();
        if ((iterator->is_directory(error) && std::filesystem::exists(path / "personality.json")) ||
            path.extension() == ".json")
            files.push_back(path);
        if (files.size() > maximum_catalogs) {
            catalogs_.clear();
            warnings.push_back("Too many personalities; keep at most 32. Presentation disabled.");
            return warnings;
        }
    }
    if (error) {
        warnings.push_back("Personality directory unavailable. Presentation disabled.");
        return warnings;
    }
    std::sort(files.begin(), files.end());
    if (files.size() > maximum_catalogs)
        files.resize(maximum_catalogs);
    std::set<std::string> seen_ids;
    for (const auto& file : files) {
        auto result = std::filesystem::is_directory(file) ? load_package(file) : load_file(file);
        if (!result.ok)
            warnings.push_back(result.detail);
        else if (!seen_ids.insert(result.id).second) {
            catalogs_.erase(result.id);
            catalog_sources_.erase(result.id);
            warnings.push_back("Duplicate personality ID: " + result.id +
                               ". Use unique IDs; that personality is unavailable.");
        }
    }
    return warnings;
}
std::vector<Personality> Messages::personalities() const {
    std::vector<Personality> result;
    for (const auto& [id, catalog] : catalogs_)
        result.push_back(catalog);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return std::tie(a.order, a.id) < std::tie(b.order, b.id);
    });
    return result;
}
const Personality* Messages::package(const std::string& id) const {
    const auto found = catalogs_.find(id);
    return found == catalogs_.end() ? nullptr : &found->second;
}
std::string Messages::resolve_selection(const std::string& requested) const {
    if (catalogs_.contains(requested))
        return requested;
    for (const auto& id : {"default", "vanilla", "uwu"})
        if (catalogs_.contains(id))
            return id;
    const auto items = personalities();
    return items.empty() ? std::string{} : items.front().id;
}
bool Messages::available(const std::string& id) const {
    return id == "vanilla" || catalogs_.contains(id);
}
std::vector<Variant> Messages::variants(Message event, const std::string& id) const {
    auto catalog = catalogs_.find(id);
    if (catalog == catalogs_.end())
        return {};
    auto pool = catalog->second.events.find(event_id(event));
    return pool == catalog->second.events.end() ? std::vector<Variant>{} : pool->second;
}
Copy Messages::show(Message event, const std::string& id, std::uint64_t occurrence) {
    if (!history_.contains({event, id}) && history_.size() >= 512)
        history_.clear();
    auto& history = history_[{event, id}];
    if (history.valid && history.occurrence == occurrence)
        return history.copy;
    Copy result{technical(event), {}, {}};
    result.personality_id = id.empty() ? "" : available(id) ? id : "vanilla";
    if (result.personality_id == "vanilla") {
        result.response_id = "vanilla_" + event_id(event);
        result.category = "technical";
        result.expression = "idle";
        switch (event) {
        case Message::ErrorMajor:
        case Message::Invalid:
        case Message::ScanFailed:
            result.expression = "error";
            break;
        case Message::Disconnected:
        case Message::Questionable:
        case Message::Recovering:
            result.expression = "concerned";
            break;
        case Message::Created:
        case Message::Reconnected:
        case Message::ScanCompleted:
            result.expression = "happy";
            break;
        case Message::Source:
        case Message::Destination:
        case Message::BufferChanged:
            result.expression = "thinking";
            break;
        default:
            break;
        }
        result.fallback_expression = "idle";
    }
    const auto pool = variants(event, id);
    if (id != "vanilla" && pool.empty())
        result.fallback_detail =
            available(id) ? "No optional commentary for this event; technical information shown."
                          : "Selected personality unavailable; technical information shown.";
    unsigned total = 0;
    for (std::size_t index = 0; index < pool.size(); ++index)
        if (pool.size() == 1 || static_cast<int>(index) != history.last)
            total += pool[index].weight;
    if (total) {
        unsigned choice = choose_(total);
        if (choice >= total)
            throw std::out_of_range("Injected presentation selector exceeded its bound");
        for (std::size_t index = 0; index < pool.size(); ++index) {
            if (pool.size() != 1 && static_cast<int>(index) == history.last)
                continue;
            if (choice < pool[index].weight) {
                result.commentary = pool[index].text;
                result.response_id = pool[index].id;
                result.category = pool[index].category;
                result.subcategory = pool[index].subcategory;
                result.expression = pool[index].expression;
                result.fallback_expression = pool[index].fallback_expression;
                history.last = static_cast<int>(index);
                break;
            }
            choice -= pool[index].weight;
        }
    }
    history.copy = result;
    history.occurrence = occurrence;
    history.valid = true;
    return result;
}
} // namespace audiohole::presentation
