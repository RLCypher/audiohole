#include "presentation/messages.h"
#include <algorithm>
#include <array>
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
    case Mode::Normal:
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
Message dfa_event(int value) {
    switch (value) {
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
                              root.at("schema_version") == 1);
        Personality candidate;
        candidate.id = root.at("id").get<std::string>();
        candidate.display_name = root.at("display_name").get<std::string>();
        require_valid_catalog(
            identifier(candidate.id) && candidate.id != "vanilla" &&
            presentation_text(candidate.display_name, maximum_display_name_bytes));
        const auto& events = root.at("events");
        require_valid_catalog(events.is_object() && events.size() <= maximum_event_pools);
        for (const auto& [key, pool] : events.items()) {
            require_valid_catalog(identifier(key) && pool.is_array() && !pool.empty() &&
                                  pool.size() <= maximum_variants_per_pool);
            for (const auto& entry : pool) {
                Variant variant;
                if (entry.is_string())
                    variant.text = entry.get<std::string>();
                else {
                    require_valid_catalog(entry.is_object() && entry.contains("text") &&
                                          entry.size() <= 2);
                    variant.text = entry.at("text").get<std::string>();
                    if (entry.contains("weight")) {
                        require_valid_catalog(entry.at("weight").is_number_integer());
                        const auto weight = entry.at("weight").get<std::int64_t>();
                        require_valid_catalog(weight >= 1 && weight <= maximum_variant_weight);
                        variant.weight = static_cast<unsigned>(weight);
                    } else
                        require_valid_catalog(entry.size() == 1);
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
        return {false,
                {},
                "Personality file unavailable or invalid. Use UTF-8 JSON schema version 1 with "
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
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (iterator->path().extension() == ".json")
            files.push_back(iterator->path());
        if (files.size() > maximum_catalogs) {
            warnings.push_back("Too many personality files; keep at most 32. Vanilla is available "
                               "until the directory is corrected.");
            return warnings;
        }
    }
    if (error) {
        warnings.push_back("Personality directory unavailable. Vanilla is the safe fallback.");
        return warnings;
    }
    std::sort(files.begin(), files.end());
    if (files.size() > maximum_catalogs)
        files.resize(maximum_catalogs);
    std::set<std::string> seen_ids;
    for (const auto& file : files) {
        auto result = load_file(file);
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
    return result;
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
    auto& history = history_[{event, id}];
    if (history.valid && history.occurrence == occurrence)
        return history.copy;
    Copy result{technical(event), {}, {}};
    const auto pool = variants(event, id);
    if (id != "vanilla" && pool.empty())
        result.fallback_detail =
            available(id) ? "No commentary for this event; Vanilla information shown."
                          : "Selected personality unavailable; Vanilla information shown.";
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
