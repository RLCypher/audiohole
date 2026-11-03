#include "presentation/messages.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace audiohole::presentation {
namespace {
using Json = nlohmann::json;
void require(bool condition, const char* detail) {
    if (!condition)
        throw std::runtime_error(detail);
}
bool identifier(const std::string& value) {
    return !value.empty() && value.size() <= 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
           });
}
std::string bounded_text(const Json& root, const char* key, std::size_t limit,
                         bool optional = false) {
    auto value = optional ? root.value(key, std::string{}) : root.at(key).get<std::string>();
    require((optional || !value.empty()) && value.size() <= limit &&
                std::none_of(value.begin(), value.end(),
                             [](unsigned char c) { return c < 32 || c == 127; }),
            "Invalid package text");
    return value;
}
bool linked(const std::filesystem::path& file) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(file.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return true;
#endif
    return std::filesystem::is_symlink(file);
}
Json document(const std::filesystem::path& file, std::size_t limit = 65536) {
    std::ifstream input(file, std::ios::binary);
    std::string bytes(limit + 1, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(input.eof() && !input.bad(), "Manifest missing or too large");
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    std::vector<std::set<std::string>> keys;
    auto result = Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        require(depth <= 12, "Manifest nested too deeply");
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        else if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        else if (event == Json::parse_event_t::key)
            require(keys.back().insert(value.get<std::string>()).second, "Duplicate manifest key");
        return true;
    });
    (void)result.dump();
    require(result.is_object(), "Manifest must be an object");
    return result;
}
// Reject links even when they point inside the package. No filesystem escape,
// Windows drive/UNC/ADS syntax or platform-dependent backslash interpretation.
std::filesystem::path owned(const std::filesystem::path& root, const std::string& relative) {
    require(!relative.empty() && relative.size() <= 240 && relative.front() != '/' &&
                relative.find_first_of("\\:") == std::string::npos,
            "Resource path must be package-relative");
    const std::filesystem::path path(std::u8string(relative.begin(), relative.end()));
    require(!path.is_absolute() && !path.has_root_path(), "Absolute resource path");
    auto current = root;
    for (const auto& part : path) {
        require(part != ".." && part != "." && !part.empty(), "Resource traversal");
        const auto name = part.u8string();
        require(name.back() != u8' ' && name.back() != u8'.', "Ambiguous Windows path");
        current /= part;
        require(!linked(current), "Linked resource");
    }
    return current;
}
bool dfa_event_name(const std::string& value) {
    return value == "dfa" || value.starts_with("dfa_");
}
} // namespace
LoadResult Messages::load_package(const std::filesystem::path& directory) {
    try {
        require(!linked(directory), "Linked package directory");
        const auto manifest = owned(directory, "personality.json");
        const auto root = document(manifest);
        require(root.at("schema_version").is_number_integer() && root.at("schema_version") == 1 &&
                    root.at("api_version").is_number_integer() && root.at("api_version") == 1,
                "Unsupported package/API version");
        Personality candidate;
        candidate.id = root.at("id").get<std::string>();
        require(identifier(candidate.id), "Invalid personality identity");
        candidate.display_name = bounded_text(root, "display_name", 128);
        candidate.character_name = bounded_text(root, "character_name", 128, true);
        candidate.description = bounded_text(root, "description", 2048);
        candidate.packaged = true;
        candidate.package_root = std::filesystem::absolute(directory).lexically_normal();
        if (root.contains("order")) {
            require(root.at("order").is_number_integer() && root.at("order") >= -1000 &&
                        root.at("order") <= 1000,
                    "Invalid package order");
            candidate.order = root.at("order").get<int>();
        }
        const std::set<std::string> known{"idle",      "happy",       "thinking",    "working",
                                          "talking",   "confused",    "concerned",   "error",
                                          "angry",     "surprised",   "celebrating", "sleeping",
                                          "smug",      "embarrassed", "laughing",    "suspicious",
                                          "panicking", "bored",       "mischievous", "deadpan"};
        const auto& expressions = root.at("expressions");
        require(expressions.is_array() && !expressions.empty() && expressions.size() <= 20,
                "Invalid expression capabilities");
        std::set<std::string> supported;
        for (const auto& expression : expressions) {
            const auto value = expression.get<std::string>();
            require(known.contains(value) && supported.insert(value).second,
                    "Unknown or duplicate expression");
            candidate.expressions.push_back(value);
        }
        const auto& dialogue = root.at("dialogue");
        require(dialogue.is_object(), "Invalid dialogue references");
        const bool builtin = dialogue.value("builtin_technical", false);
        require(!builtin || candidate.id == "vanilla",
                "Built-in technical provider belongs to Vanilla");
        require(candidate.id != "vanilla" || builtin, "Vanilla cannot override technical facts");
        require(!builtin || dialogue.size() == 1,
                "Built-in technical provider cannot declare external commentary");
        std::map<std::filesystem::path, Personality> loaded;
        auto catalog = [&](const std::string& relative) -> const Personality& {
            const auto path = owned(directory, relative);
            require(path.extension() == ".json", "Dialogue must be JSON");
            if (!loaded.contains(path)) {
                Messages staging;
                const auto result = staging.load_file(path);
                require(result.ok && result.id == candidate.id,
                        "Invalid or mismatched dialogue catalog");
                const auto* value = staging.package(result.id);
                require(value != nullptr, "Missing dialogue catalog");
                for (const auto& [event, pool] : value->events)
                    for (const auto& variant : pool)
                        require(
                            !variant.id.empty() && supported.contains(variant.expression) &&
                                (variant.fallback_expression.empty() ||
                                 supported.contains(variant.fallback_expression)),
                            "Packaged responses require owned expression metadata (catalog v2)");
                loaded.emplace(path, *value);
            }
            return loaded.at(path);
        };
        if (!builtin) {
            candidate.events = catalog(dialogue.at("responses").get<std::string>()).events;
            // Optional resources govern their feature even if the main catalog has
            // those events. A missing Bother/DFA never borrows another identity.
            std::erase_if(candidate.events, [](const auto& item) {
                return item.first == "bother" || dfa_event_name(item.first);
            });
            for (const auto* feature : {"bother", "dfa"}) {
                if (!dialogue.contains(feature) || dialogue.at(feature).is_null())
                    continue;
                const auto relative = dialogue.at(feature).get<std::string>();
                const auto path = owned(directory, relative);
                if (!std::filesystem::exists(path))
                    continue;
                for (const auto& [event, pool] : catalog(relative).events)
                    if ((std::string_view(feature) == "bother" && event == "bother") ||
                        (std::string_view(feature) == "dfa" && dfa_event_name(event)))
                        candidate.events[event] = pool;
            }
            std::set<std::string> ids;
            for (const auto& [event, pool] : candidate.events)
                for (const auto& variant : pool)
                    require(ids.insert(variant.id).second,
                            "Duplicate response identity across resources");
        }
        if (root.contains("avatar")) {
            const auto& avatar = root.at("avatar");
            require(avatar.is_object(), "Invalid avatar references");
            if (avatar.contains("registry") && !avatar.at("registry").is_null())
                candidate.avatar_registry =
                    owned(directory, avatar.at("registry").get<std::string>());
            if (avatar.contains("manifest") && !avatar.at("manifest").is_null())
                candidate.avatar_manifest =
                    owned(directory, avatar.at("manifest").get<std::string>());
            const bool manifest_exists = !candidate.avatar_manifest.empty() &&
                                         std::filesystem::exists(candidate.avatar_manifest);
            const bool registry_exists = !candidate.avatar_registry.empty() &&
                                         std::filesystem::exists(candidate.avatar_registry);
            if (manifest_exists) {
                const auto data = document(candidate.avatar_manifest, 2 * 1024 * 1024);
                require(data.at("schema_version").is_number_integer() &&
                            data.at("schema_version") == 1 &&
                            data.at("personality_id") == candidate.id &&
                            data.at("styles").is_object(),
                        "Invalid avatar manifest identity/version");
                candidate.avatar_style = data.value("default_style", std::string{});
                if (!candidate.avatar_style.empty() &&
                    data.at("styles").contains(candidate.avatar_style)) {
                    const auto& style = data.at("styles").at(candidate.avatar_style);
                    if (style.value("enabled", false) && style.contains("states"))
                        for (const auto& [state, info] : style.at("states").items())
                            if ((info.contains("static") &&
                                 info.at("static").value("status", std::string{}) == "approved") ||
                                (info.contains("animation") &&
                                 info.at("animation").value("status", std::string{}) == "approved"))
                                candidate.avatar_states.push_back(state);
                }
            }
            if (registry_exists) {
                const auto data = document(candidate.avatar_registry);
                require(data.at("schema_version").is_number_integer() &&
                            data.at("schema_version") == 1 && data.at("packs").is_object() &&
                            data.at("packs").size() == 1 && data.at("packs").contains(candidate.id),
                        "Invalid avatar registry");
                require(owned(candidate.avatar_registry.parent_path(),
                              data.at("packs").at(candidate.id).get<std::string>()) ==
                            candidate.avatar_manifest,
                        "Avatar registry must reference this package's manifest");
            }
            // Availability is structural only. The external engine must still
            // validate integrity, styles, approval flags and decoded images.
            candidate.avatar_available = manifest_exists && registry_exists;
        }
        if (root.contains("preview_image") && !root.at("preview_image").is_null()) {
            const auto path = owned(directory, root.at("preview_image").get<std::string>());
            if (std::filesystem::exists(path)) {
                require(path.extension() == ".png" &&
                            std::filesystem::file_size(path) <= 4 * 1024 * 1024,
                        "Invalid preview image");
                std::ifstream png(path, std::ios::binary);
                std::array<unsigned char, 24> header{};
                png.read(reinterpret_cast<char*>(header.data()), header.size());
                const std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
                require(png.gcount() == 24 &&
                            std::equal(signature.begin(), signature.end(), header.begin()),
                        "Invalid PNG header");
                auto dimension = [&](int start) {
                    return (unsigned(header[start]) << 24) | (unsigned(header[start + 1]) << 16) |
                           (unsigned(header[start + 2]) << 8) | unsigned(header[start + 3]);
                };
                require(dimension(16) > 0 && dimension(20) > 0 && dimension(16) <= 4096 &&
                            dimension(20) <= 4096,
                        "Excessive PNG dimensions");
                candidate.preview_image = path;
            }
        }
        const auto id = candidate.id;
        require(catalogs_.contains(id) || catalogs_.size() < 32, "Too many packages");
        for (auto previous = catalog_sources_.begin(); previous != catalog_sources_.end();) {
            if (previous->second == directory.lexically_normal() && previous->first != id) {
                catalogs_.erase(previous->first);
                previous = catalog_sources_.erase(previous);
            } else
                ++previous;
        }
        catalogs_[id] = std::move(candidate);
        catalog_sources_[id] = directory.lexically_normal();
        history_.clear();
        return {true, id,
                "Personality package validated. Missing optional resources are disabled."};
    } catch (const std::exception& error) {
        for (auto previous = catalog_sources_.begin(); previous != catalog_sources_.end();) {
            if (previous->second == directory.lexically_normal()) {
                catalogs_.erase(previous->first);
                previous = catalog_sources_.erase(previous);
            } else
                ++previous;
        }
        history_.clear();
        return {false,
                {},
                std::string("Personality package rejected: ") + directory.filename().string() +
                    ". " + error.what()};
    }
}
} // namespace audiohole::presentation
