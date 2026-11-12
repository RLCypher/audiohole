#include "model/persistence.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace audiohole {
namespace {
using Json = nlohmann::ordered_json;
constexpr std::size_t maximum_document = 2 * 1024 * 1024;
std::string profile_filename(std::string name);
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid provisional configuration");
}
void check_storage_path(const std::filesystem::path& file) {
    auto current = std::filesystem::absolute(file).lexically_normal();
    while (!current.empty()) {
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(current.c_str());
        require(attributes == INVALID_FILE_ATTRIBUTES ||
                !(attributes & FILE_ATTRIBUTE_REPARSE_POINT));
#else
        std::error_code error;
        const auto status = std::filesystem::symlink_status(current, error);
        require(!std::filesystem::is_symlink(status));
#endif
        if (current == current.parent_path())
            break;
        current = current.parent_path();
    }
}
std::optional<std::string> read_document(const std::filesystem::path& file) {
    check_storage_path(file);
    if (!std::filesystem::exists(file))
        return std::nullopt;
    require(!std::filesystem::is_symlink(file) && std::filesystem::is_regular_file(file));
    std::ifstream input(file, std::ios::binary);
    std::string bytes(maximum_document + 1, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(input.eof() && !input.bad());
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    require(bytes.size() <= maximum_document);
    return bytes;
}
Json parse(const std::string& bytes) {
    std::vector<std::set<std::string>> keys;
    auto result = Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        require(depth <= 16);
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        else if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        else if (event == Json::parse_event_t::key)
            require(keys.back().insert(value.get<std::string>()).second);
        return true;
    });
    (void)result.dump();
    return result;
}
std::string string(const Json& object, const char* key, std::size_t limit = 65536) {
    const auto value = object.at(key).get<std::string>();
    require(value.size() <= limit && value.find('\0') == std::string::npos);
    return value;
}
int integer(const Json& object, const char* key, int lower, int upper) {
    const auto& value = object.at(key);
    require(value.is_number_integer());
    const auto number = value.get<std::int64_t>();
    require(number >= lower && number <= upper);
    return static_cast<int>(number);
}
constexpr std::array availability_names{"available", "unavailable", "ambiguous", "untested",
                                        "unknown"};
Format format(const Json& value) {
    Format f;
    f.rate = integer(value, "rate", 1, 768000);
    f.channels = integer(value, "channels", 1, 256);
    f.representation = string(value, "representation", 64);
    require(f.rate > 0 && f.rate <= 768000 && f.channels > 0 && f.channels <= 256 &&
            !f.representation.empty());
    if (value.contains("wave") && !value.at("wave").is_null()) {
        const auto& v = value.at("wave");
        WaveFormat wave;
        wave.container_bits = integer(v, "container_bits", 8, 64);
        wave.valid_bits = integer(v, "valid_bits", 1, wave.container_bits);
        wave.format_tag = integer(v, "format_tag", 1, 65535);
        wave.subformat_guid = string(v, "subformat_guid", 64);
        std::transform(wave.subformat_guid.begin(), wave.subformat_guid.end(),
                       wave.subformat_guid.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        wave.block_align_bytes = integer(v, "block_align_bytes", 1, 65535);
        wave.average_bytes_per_second = integer(v, "average_bytes_per_second", 1, 2147483647);
        const auto& mask = v.at("channel_mask");
        if (mask.is_number_integer()) {
            const auto n = mask.get<std::int64_t>();
            require(n >= 0 && n <= 0xffffffffLL);
            wave.channel_mask = static_cast<std::uint32_t>(n);
        } else
            require(mask.is_object() && mask.at("status") == "unknown");
        require(!wave.subformat_guid.empty() && wave.container_bits % 8 == 0 &&
                wave.block_align_bytes == f.channels * wave.container_bits / 8 &&
                static_cast<std::int64_t>(wave.block_align_bytes) * f.rate ==
                    wave.average_bytes_per_second);
        f.wave = std::move(wave);
    }
    return f;
}
Json encode(const Format& f) {
    Json result{{"rate", f.rate},
                {"channels", f.channels},
                {"representation", f.representation},
                {"wave", nullptr}};
    if (f.wave) {
        const auto& w = *f.wave;
        result["wave"] = {
            {"container_bits", w.container_bits},
            {"valid_bits", w.valid_bits},
            {"channel_mask", w.channel_mask ? Json(*w.channel_mask) : Json{{"status", "unknown"}}},
            {"format_tag", w.format_tag},
            {"subformat_guid", w.subformat_guid},
            {"block_align_bytes", w.block_align_bytes},
            {"average_bytes_per_second", w.average_bytes_per_second}};
    }
    return result;
}
Json encode(const EndpointPreferences& p) {
    Json j{{"sample_rate_hz", nullptr},
           {"channel_count", nullptr},
           {"sample_format", nullptr},
           {"mode", nullptr},
           {"buffer_ms", nullptr}};
    if (p.sample_rate_hz)
        j["sample_rate_hz"] = *p.sample_rate_hz;
    if (p.channel_count)
        j["channel_count"] = *p.channel_count;
    if (p.sample_format)
        j["sample_format"] = *p.sample_format;
    if (p.mode)
        j["mode"] = *p.mode;
    if (p.buffer_ms)
        j["buffer_ms"] = *p.buffer_ms;
    return j;
}
EndpointPreferences endpoint_preferences(const Json& j) {
    require(j.is_object());
    EndpointPreferences p;
    auto automatic = [&](const char* key) {
        return !j.contains(key) || j.at(key).is_null() || j.at(key) == "" ||
               j.at(key) == "Automatic";
    };
    if (!automatic("sample_rate_hz"))
        p.sample_rate_hz = integer(j, "sample_rate_hz", 1, 768000);
    if (!automatic("channel_count"))
        p.channel_count = integer(j, "channel_count", 1, 256);
    if (!automatic("buffer_ms")) {
        const auto value = integer(j, "buffer_ms", 0, 100);
        if (value)
            p.buffer_ms = value;
    }
    if (!automatic("sample_format"))
        p.sample_format = string(j, "sample_format", 32);
    if (!automatic("mode"))
        p.mode = string(j, "mode", 32);
    return p;
}
Json encode(const Preferences& p) {
    Json map = Json::array();
    for (const auto& r : p.channel_map)
        map.push_back({{"source", r.source}, {"destination", r.destination}});
    return {{"common", encode(p.common)},
            {"source", encode(p.source)},
            {"destination", encode(p.destination)},
            {"channel_map", map},
            {"preferred_clock_device", p.preferred_clock_device}};
}
Direction direction(const Json& value, int version) {
    Direction d;
    if (version >= 7) {
        d.advanced_tuning = value.at("advanced_tuning").get<bool>();
        d.exclusive_mode_preferred = value.value("exclusive_mode_preferred", false);
    }
    d.source = string(value, "source", 128);
    d.destination = string(value, "destination", 128);
    if (value.contains("source_name"))
        d.source_name = string(value, "source_name");
    if (value.contains("destination_name"))
        d.destination_name = string(value, "destination_name");
    if (version >= 4 && !value.contains("settings"))
        return d;
    const auto& settings = value.at("settings");
    require(settings.is_object());
    const auto p = version >= 4 ? settings.value("preferences", Json::object()) : Json::object();
    require(p.is_object());
    d.settings.preferences.common = endpoint_preferences(p.value("common", Json::object()));
    d.settings.preferences.source = endpoint_preferences(p.value("source", Json::object()));
    d.settings.preferences.destination =
        endpoint_preferences(p.value("destination", Json::object()));
    d.settings.preferences.preferred_clock_device = p.contains("preferred_clock_device")
                                                        ? string(p, "preferred_clock_device", 32)
                                                        : "automatic";
    if (p.contains("channel_map")) {
        require(p.at("channel_map").is_array() && p.at("channel_map").size() <= 256);
        for (const auto& route : p.at("channel_map"))
            d.settings.preferences.channel_map.push_back(
                {integer(route, "source", 0, 255), integer(route, "destination", 0, 255)});
    }
    if (version < 5) {
        // Translate legacy intent once. In particular, Automatic's cached format
        // is never an override, even if its descriptor looks complete.
        const bool automatic =
            version < 4 ? settings.at("automatic").get<bool>() : settings.value("automatic", true);
        auto& common = d.settings.preferences.common;
        if (!automatic) {
            const auto legacy = format(settings.at("format"));
            if (!common.sample_rate_hz)
                common.sample_rate_hz = legacy.rate;
            if (!common.channel_count)
                common.channel_count = legacy.channels;
            if (!common.sample_format) {
                auto representation = legacy.representation;
                if (representation == "int16" || representation == "PCM16")
                    representation = "pcm_s16";
                if (representation == "int24" || representation == "PCM24")
                    representation = legacy.wave && legacy.wave->container_bits == 32
                                         ? "pcm_s24_in_32"
                                         : "pcm_s24_packed";
                if (representation == "int32" || representation == "PCM32")
                    representation = "pcm_s32";
                common.sample_format = representation;
            }
        }
        if (!common.mode && settings.contains("mode") && settings.at("mode") != "" &&
            !settings.at("mode").is_null()) {
            const auto mode = string(settings, "mode", 64);
            if (mode != "Automatic")
                common.mode = mode;
        }
        if (!common.buffer_ms && settings.contains("buffer_ms") && settings.at("buffer_ms") != "" &&
            !settings.at("buffer_ms").is_null()) {
            const auto buffer = integer(settings, "buffer_ms", 0, 100);
            if (buffer)
                common.buffer_ms = buffer;
        }
    }
    require(validate_preferences(d.settings.preferences).ok);
    d.settings.refresh_projection();
    return d;
}

Json encode(const Direction& d) {
    return {{"advanced_tuning", d.advanced_tuning},
            {"exclusive_mode_preferred", d.exclusive_mode_preferred},
            {"source_name", d.source_name},
            {"source", d.source},
            {"destination_name", d.destination_name},
            {"destination", d.destination},
            {"settings", {{"preferences", encode(d.settings.preferences)}}}};
}
Json encode(const ConfigurationRecords& records) {
    Json result = {{"format", "audiohole-test-configuration"},
                   {"schema_version", 7},
                   {"profiles", Json::array()},
                   {"devices", Json::array()}};
    for (const auto& d : records.devices) {
        Json formats = Json::array();
        for (const auto& f : d.formats)
            formats.push_back(encode(f));
        result["devices"].push_back(
            {{"id", d.id},
             {"display_name", d.display_name},
             {"hidden", d.hidden},
             {"windows_name", d.windows_name},
             {"notes", d.notes},
             {"capture", d.capture},
             {"availability", availability_names.at(static_cast<std::size_t>(d.availability))},
             {"formats", formats}});
    }
    for (const auto& p : records.profiles) {
        Json holes = Json::array();
        for (const auto& h : p.holes)
            holes.push_back({{"display_name", h.display_name},
                             {"notes", h.notes},
                             {"id", h.id},
                             {"routing", h.routing_engine},
                             {"enabled", h.enabled},
                             {"duplex", h.duplex},
                             {"forward", encode(h.forward)},
                             {"reverse", encode(h.reverse)}});
        result["profiles"].push_back(
            {{"display_name", p.display_name}, {"notes", p.notes}, {"id", p.id}, {"holes", holes}});
    }
    return result;
}
ConfigurationRecords decode(const Json& root) {
    require(root.at("format") == "audiohole-test-configuration" &&
            integer(root, "schema_version", 1, 7) >= 1);
    ConfigurationRecords records;
    const auto& devices = root.at("devices");
    const auto& profiles = root.at("profiles");
    require(devices.is_array() && devices.size() <= 512 && profiles.is_array() &&
            profiles.size() <= 256);
    std::set<std::string> device_ids, profile_ids, hole_ids;
    auto id = [&](const Json& v, std::set<std::string>& ids) {
        auto value = string(v, "id", 128);
        require(!value.empty() && ids.insert(value).second);
        return value;
    };
    for (const auto& v : devices) {
        Endpoint d;
        d.id = id(v, device_ids);
        d.display_name = string(v, "display_name");
        d.hidden = v.value("hidden", false);
        d.windows_name = string(v, "windows_name");
        d.notes = string(v, "notes");
        d.capture = v.at("capture").get<bool>();
        const auto availability = string(v, "availability", 32);
        const auto found =
            std::find(availability_names.begin(), availability_names.end(), availability);
        require(found != availability_names.end());
        d.availability = static_cast<Availability>(found - availability_names.begin());
        require(v.at("formats").is_array() && v.at("formats").size() <= 256);
        for (const auto& f : v.at("formats")) {
            const auto decoded = format(f);
            require(std::find(d.formats.begin(), d.formats.end(), decoded) == d.formats.end());
            d.formats.push_back(decoded);
        }
        records.devices.push_back(std::move(d));
    }
    for (const auto& v : profiles) {
        Profile p;
        p.id = id(v, profile_ids);
        p.display_name = string(v, "display_name");
        p.notes = string(v, "notes");
        require(!p.display_name.empty() && v.at("holes").is_array() && v.at("holes").size() <= 128);
        for (const auto& x : v.at("holes")) {
            Hole h;
            h.id = id(x, hole_ids);
            h.display_name = string(x, "display_name");
            h.notes = string(x, "notes");
            h.enabled = x.at("enabled").get<bool>();
            h.duplex = x.at("duplex").get<bool>();
            h.forward = direction(x.at("forward"), root.at("schema_version").get<int>());
            if (root.at("schema_version") >= 4 && !x.contains("reverse"))
                h.reverse = Direction{};
            else
                h.reverse = direction(x.at("reverse"), root.at("schema_version").get<int>());
            if (root.at("schema_version") < 7) {
                require(x.contains("advanced_tuning") || root.at("schema_version") < 3);
                const bool tuning =
                    x.contains("advanced_tuning") ? x.at("advanced_tuning").get<bool>() : true;
                for (auto* direction : {&h.forward, &h.reverse}) {
                    direction->advanced_tuning = tuning;
                    direction->exclusive_mode_preferred =
                        x.value("exclusive_mode_preferred", false);
                }
            }
            const auto* routing_key = root.at("schema_version") >= 6 ? "routing" : "routing_engine";
            h.routing_engine = x.contains(routing_key) ? string(x, routing_key, 64) : "automatic";
            require(!h.routing_engine.empty());
            p.holes.push_back(std::move(h));
        }
        records.profiles.push_back(std::move(p));
    }
    return records;
}
// Legacy transport descriptors no longer belong to profile intent. Preserve
// only opaque annotations under an explicitly non-operational extension key.
Json legacy_format_extensions(const Json& settings) {
    if (!settings.contains("format") || !settings.at("format").is_object())
        return Json::object();
    auto extension = settings.at("format");
    for (const auto* key : {"rate", "channels", "representation"})
        extension.erase(key);
    if (extension.contains("wave") && extension.at("wave").is_object()) {
        auto& wave = extension["wave"];
        for (const auto* key : {"container_bits", "valid_bits", "channel_mask", "format_tag",
                                "subformat_guid", "block_align_bytes", "average_bytes_per_second"})
            wave.erase(key);
        if (wave.empty())
            extension.erase("wave");
    } else
        extension.erase("wave");
    return extension;
}
// Preserve unrecognized fields recursively. Record arrays match by stable ID;
// explicit removals stay removed. New known values take precedence.
Json merge(const Json& old, const Json& current) {
    if (old.is_object() && current.is_object()) {
        auto result = Json::object();
        for (const auto& [key, value] : current.items())
            result[key] = old.contains(key) ? merge(old.at(key), value) : value;
        for (const auto& [key, value] : old.items())
            if (!current.contains(key)) {
                if (current.contains("routing") &&
                    (key == "routing_engine" || key == "advanced_tuning" ||
                     key == "exclusive_mode_preferred"))
                    continue;
                if (current.contains("preferences") &&
                    (key == "automatic" || key == "format" || key == "mode" || key == "buffer_ms"))
                    continue;
                result[key] = value;
            }
        if (current.contains("preferences")) {
            const auto extension = legacy_format_extensions(old);
            if (!extension.empty()) {
                if (result.contains("legacy_format_extensions"))
                    require(result.at("legacy_format_extensions") == extension);
                else
                    result["legacy_format_extensions"] = extension;
            }
        }
        return result;
    }
    if (old.is_array() && current.is_array()) {
        auto result = Json::array();
        for (const auto& item : current) {
            auto previous = old.end();
            if (item.is_object() && item.contains("id"))
                previous = std::find_if(old.begin(), old.end(), [&](const auto& x) {
                    return x.is_object() && x.contains("id") && x.at("id") == item.at("id");
                });
            else if (item.is_object())
                previous = std::find_if(old.begin(), old.end(), [&](const auto& original) {
                    if (item.contains("rate") && item.contains("channels") &&
                        item.contains("representation") && original.is_object() &&
                        original.contains("rate") && original.contains("channels") &&
                        original.contains("representation"))
                        return format(original) == format(item);
                    return original.is_object() &&
                           std::all_of(item.items().begin(), item.items().end(),
                                       [&](const auto& field) {
                                           return original.contains(field.key()) &&
                                                  original.at(field.key()) == field.value();
                                       });
                });
            result.push_back(previous == old.end() ? item : merge(*previous, item));
        }
        return result;
    }
    return current;
}
} // namespace
Result replace_document(const std::filesystem::path& file, const std::string& bytes,
                        const std::optional<std::string>& expected) {
    static std::atomic<unsigned> serial{0};
    std::filesystem::path temporary;
    try {
        require(bytes.size() <= maximum_document);
        if (read_document(file) != expected)
            return {false,
                    "File changed externally. Reload before saving; no data was overwritten."};
        std::filesystem::create_directories(file.parent_path());
        require(!std::filesystem::is_symlink(file.parent_path()));
        temporary = file;
        temporary += ".tmp-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "-" + std::to_string(serial++);
#ifdef _WIN32
        const auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return {false, "Could not create an isolated temporary configuration file."};
        DWORD written = 0;
        bool ok =
            WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size();
        ok = FlushFileBuffers(handle) && ok;
        ok = CloseHandle(handle) && ok;
#else
        auto* stream = std::fopen(temporary.c_str(), "wbx");
        if (!stream)
            return {false, "Could not create an isolated temporary configuration file."};
        bool ok = std::fwrite(bytes.data(), 1, bytes.size(), stream) == bytes.size();
        ok = std::fflush(stream) == 0 && ok;
        ok = ::fsync(fileno(stream)) == 0 && ok;
        ok = std::fclose(stream) == 0 && ok;
#endif
        if (!ok || read_document(file) != expected) {
            std::filesystem::remove(temporary);
            return {
                false,
                "Configuration write failed or the file changed externally; original retained."};
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Atomic replacement failed");
#else
        std::filesystem::rename(temporary, file);
#endif
        return {true, "Configuration saved."};
    } catch (const std::exception&) {
        std::error_code ignored;
        if (!temporary.empty())
            std::filesystem::remove(temporary, ignored);
        return {false, "Configuration unavailable or invalid; existing data was preserved."};
    }
}
struct TestJsonPersistence::Impl {
    std::filesystem::path file;
    std::optional<std::string> expected;
    Json original = Json::object();
    bool ready = false;
};
TestJsonPersistence::TestJsonPersistence(std::filesystem::path file)
    : impl_(std::make_unique<Impl>()) {
    impl_->file = std::move(file);
}
TestJsonPersistence::~TestJsonPersistence() = default;
Result validate_test_document(const std::string& bytes, ConfigurationRecords& output) {
    try {
        require(bytes.size() <= maximum_document);
        output = decode(parse(bytes));
        return {true, "Configuration validated."};
    } catch (const std::exception& e) {
        return {false, std::string("Invalid Test configuration: ") + e.what()};
    }
}
Result merge_test_profiles(const std::optional<std::string>& existing, const std::string& incoming,
                           std::string& output) {
    try {
        require(incoming.size() <= maximum_document);
        auto added = parse(incoming);
        auto current = existing ? parse(*existing) : encode(ConfigurationRecords{});
        added = merge(added, encode(decode(added)));
        current = merge(current, encode(decode(current)));
        require(added.at("devices").empty());
        for (const auto& item : added.at("profiles")) {
            auto found =
                std::find_if(current["profiles"].begin(), current["profiles"].end(),
                             [&](const auto& old) { return old.at("id") == item.at("id"); });
            if (found != current["profiles"].end()) {
                if (nlohmann::json(*found) != nlohmann::json(item))
                    return {false, "Conflicting profile: " + string(item, "display_name") +
                                       ". Existing profile was not overwritten."};
            } else
                current["profiles"].push_back(item);
        }
        if (added.contains("profile_record_extensions")) {
            require(added.at("profile_record_extensions").is_object());
            if (!current.contains("profile_record_extensions"))
                current["profile_record_extensions"] = Json::object();
            for (const auto& [id, extension] : added.at("profile_record_extensions").items()) {
                require(extension.is_object());
                if (current["profile_record_extensions"].contains(id))
                    require(current["profile_record_extensions"].at(id) == extension);
                current["profile_record_extensions"][id] = extension;
            }
        }
        current["schema_version"] = 7;
        std::set<std::string> filenames;
        for (const auto& profile : current.at("profiles")) {
            auto filename = profile_filename(string(profile, "display_name"));
            std::transform(filename.begin(), filename.end(), filename.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (!filenames.insert(filename).second)
                throw std::runtime_error("Profile filename conflict: " + filename +
                                         ". Rename a profile before importing.");
        }
        (void)decode(current); // Also detects hole IDs colliding across profiles.
        output = current.dump(2) + "\n";
        require(output.size() <= maximum_document);
        return {true, "Profiles validated; existing identities retained."};
    } catch (const std::exception& e) {
        return {false, std::string("Profile import validation failed: ") + e.what()};
    }
}
Result TestJsonPersistence::load(ConfigurationRecords& output) {
    try {
        impl_->ready = false;
        auto bytes = read_document(impl_->file);
        auto document = bytes ? parse(*bytes) : encode(ConfigurationRecords{});
        auto records = decode(document);
        output = std::move(records);
        impl_->original = std::move(document);
        impl_->expected = std::move(bytes);
        impl_->ready = true;
        return {true, "Test configuration loaded."};
    } catch (const std::exception&) {
        return {false, "Test configuration is invalid or unavailable. Fix the file and restart; "
                       "existing data is retained."};
    }
}
Result TestJsonPersistence::save(const ConfigurationRecords& records) {
    if (!impl_->ready)
        return {false, "Configuration was not loaded successfully; saving is blocked to preserve "
                       "existing data."};
    try {
        auto document = merge(impl_->original, encode(records));
        (void)decode(document);
        const auto bytes = document.dump(2) + "\n";
        if (impl_->expected && impl_->original.at("schema_version") != 7) {
            // No load-time migration. The first explicit save keeps the exact
            // legacy original; never overwrite a different earlier backup.
            if (read_document(impl_->file) != impl_->expected)
                return {false,
                        "File changed externally. Reload before saving; no data was overwritten."};
            auto backup = impl_->file;
            const auto version = impl_->original.at("schema_version").get<int>();
            backup += ".v" + std::to_string(version) + ".bak";
            const auto existing = read_document(backup);
            if (existing && existing != impl_->expected)
                return {false,
                        "A different legacy backup already exists. Preserve it and choose a new "
                        "configuration destination before upgrading."};
            if (!existing) {
                const auto result = replace_document(backup, *impl_->expected, std::nullopt);
                if (!result.ok)
                    return result;
            }
        }
        auto result = replace_document(impl_->file, bytes, impl_->expected);
        if (result.ok) {
            impl_->expected = bytes;
            impl_->original = std::move(document);
        }
        return result;
    } catch (const std::exception&) {
        return {false, "Invalid test configuration; no data written."};
    }
}
namespace {
using ProfileFiles = std::map<std::filesystem::path, std::string>;
// Whole-directory staging keeps a rename/delete from exposing a partially updated
// set. Previous generations are retained under local/profile-backups. A crash
// between the two directory renames is recovered by restoring .profiles-previous.
void recover_profiles(const std::filesystem::path& root) {
    check_storage_path(root / "profiles");
    check_storage_path(root / ".profiles-previous");
    const auto folder = root / "profiles", previous = root / ".profiles-previous";
    require(!std::filesystem::is_symlink(folder) && !std::filesystem::is_symlink(previous));
    if (!std::filesystem::exists(folder) && std::filesystem::exists(previous))
        std::filesystem::rename(previous, folder);
}
ProfileFiles profile_files(const std::filesystem::path& root) {
    ProfileFiles files;
    const auto folder = root / "profiles";
    require(!std::filesystem::is_symlink(folder));
    if (!std::filesystem::exists(folder))
        return files;
    std::size_t total_bytes = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        require(files.size() < 258 && entry.path().extension() == ".json");
        const auto bytes = read_document(entry.path());
        require(bytes.has_value());
        total_bytes += bytes->size();
        require(total_bytes <= maximum_document + 128 * 1024);
        files.emplace(entry.path().filename(), *bytes);
    }
    return files;
}
// Individual records and aggregate transfer wrappers have independent versions.
// Envelope extensions travel through an explicitly identified transport map.
Json current_document(const Json& document) {
    return merge(document, encode(decode(document)));
}
Json record_document(const Json& record) {
    require(record.at("format") == "audiohole-profile" &&
            integer(record, "schema_version", 1, 3) >= 1);
    auto wrapper = encode(ConfigurationRecords{});
    wrapper["schema_version"] = record.at("schema_version") == 1   ? 5
                                : record.at("schema_version") == 2 ? 6
                                                                   : 7;
    wrapper["profiles"].push_back(record.at("profile"));
    return current_document(wrapper);
}
Json assembled_profiles(const ProfileFiles& files) {
    auto root = encode(ConfigurationRecords{});
    if (const auto state = files.find(".state.json"); state != files.end()) {
        root = current_document(parse(state->second));
        require(root.at("profiles").empty());
    }
    auto envelopes = root.value("profile_record_extensions", Json::object());
    require(envelopes.is_object());
    for (const auto& [path, bytes] : files) {
        if (path == ".state.json")
            continue;
        auto document = parse(bytes);
        Json current;
        if (document.value("format", "") == "audiohole-profile") {
            current = record_document(document);
            auto extensions = document;
            extensions.erase("format");
            extensions.erase("schema_version");
            extensions.erase("profile");
            if (!extensions.empty())
                envelopes[string(document.at("profile"), "id", 128)] = extensions;
        } else {
            current = current_document(document);
            auto extension = document;
            for (const auto* key : {"format", "schema_version", "profiles", "devices"})
                extension.erase(key);
            if (!extension.empty()) {
                const auto id = string(current.at("profiles").at(0), "id", 128);
                auto envelope = envelopes.value(id, Json::object());
                require(envelope.is_object());
                if (envelope.contains("legacy_wrapper_extensions"))
                    require(envelope.at("legacy_wrapper_extensions") == extension);
                else
                    envelope["legacy_wrapper_extensions"] = extension;
                envelopes[id] = std::move(envelope);
            }
        }
        const auto records = decode(current);
        require(records.profiles.size() == 1 && records.devices.empty());
        root["profiles"].push_back(current.at("profiles").at(0));
    }
    if (!envelopes.empty())
        root["profile_record_extensions"] = std::move(envelopes);
    require(root.dump().size() <= maximum_document);
    (void)decode(root);
    return root;
}
std::string profile_filename(std::string name) {
    for (auto& c : name)
        if (static_cast<unsigned char>(c) < 32 ||
            std::string("<>:\"/\\|?*").find(c) != std::string::npos)
            c = '_';
    while (!name.empty() && (name.back() == '.' || name.back() == ' '))
        name.pop_back();
    require(!name.empty() && name.size() <= 180 && name != ".state");
    auto upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    auto stem = upper.substr(0, upper.find('.'));
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
        (stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
         stem[3] >= '1' && stem[3] <= '9'))
        name = "_" + name;
    return name + ".json";
}
} // namespace
Result validate_profile_record(const std::string& bytes, Profile& output) {
    try {
        require(bytes.size() <= maximum_document);
        const auto records = decode(record_document(parse(bytes)));
        output = records.profiles.front();
        return {true, "Profile record validated."};
    } catch (const std::exception& error) {
        return {false, std::string("Invalid profile record: ") + error.what()};
    }
}
struct ProfileDirectoryPersistence::Impl {
    std::filesystem::path file;
    ProfileFiles observed;
    std::optional<std::string> legacy;
    Json original;
    bool directory = false, ready = false;
    std::function<Result(const std::filesystem::path&, const std::string&,
                         const std::optional<std::string>&)>
        writer = replace_document;
};
ProfileDirectoryPersistence::ProfileDirectoryPersistence(std::filesystem::path file)
    : impl_(std::make_unique<Impl>()) {
    impl_->file = std::move(file);
}
ProfileDirectoryPersistence::~ProfileDirectoryPersistence() = default;
Result ProfileDirectoryPersistence::load(ConfigurationRecords& output) {
    try {
        impl_->ready = false;
        const auto root = impl_->file.parent_path();
        recover_profiles(root);
        impl_->directory = std::filesystem::exists(root / "profiles" / ".state.json");
        impl_->observed = profile_files(root);
        impl_->legacy = read_document(impl_->file);
        auto document = impl_->directory || !impl_->observed.empty()
                            ? assembled_profiles(impl_->observed)
                        : impl_->legacy ? parse(*impl_->legacy)
                                        : encode(ConfigurationRecords{});
        output = decode(document);
        impl_->original = std::move(document);
        impl_->ready = true;
        return {true, "Profiles loaded."};
    } catch (const std::exception& e) {
        return {false,
                std::string("Profile folder is invalid; existing files retained: ") + e.what()};
    }
}
Result ProfileDirectoryPersistence::save(const ConfigurationRecords& records) {
    if (!impl_->ready)
        return {false, "Load valid profiles before saving."};
    const auto root = impl_->file.parent_path();
    const auto folder = root / "profiles", next = root / ".profiles-next",
               previous = root / ".profiles-previous";
    bool created_next = false;
    try {
        auto document = merge(impl_->original, encode(records));
        require(document.dump().size() <= maximum_document);
        (void)decode(document);
        require(profile_files(root) == impl_->observed &&
                read_document(impl_->file) == impl_->legacy);
        if (impl_->legacy) {
            const auto backup = root / "local" / "legacy-profiles" / impl_->file.filename();
            const auto existing = read_document(backup);
            require(!existing || existing == impl_->legacy);
            if (!existing) {
                const auto saved = replace_document(backup, *impl_->legacy, {});
                if (!saved.ok)
                    throw std::runtime_error(saved.detail);
            }
        }
        if (std::filesystem::exists(next)) {
            require(!std::filesystem::is_symlink(next));
            const auto abandoned =
                root / "local" / "profile-backups" /
                ("uncommitted-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            check_storage_path(abandoned);
            std::filesystem::create_directories(abandoned.parent_path());
            std::filesystem::rename(next, abandoned);
        }
        if (std::filesystem::exists(previous)) {
            const auto backup =
                root / "local" / "profile-backups" /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            check_storage_path(backup);
            std::filesystem::create_directories(backup.parent_path());
            std::filesystem::rename(previous, backup);
        }
        std::filesystem::create_directories(next);
        created_next = true;
        std::set<std::string> names;
        Json active_envelopes = Json::object();
        for (const auto& profile : document.at("profiles")) {
            auto name = profile_filename(string(profile, "display_name"));
            auto key = name;
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (!names.insert(key).second)
                throw std::runtime_error("Another profile uses filename " + name +
                                         ". Choose a different display name.");
            const auto id = string(profile, "id", 128);
            const auto envelopes = document.value("profile_record_extensions", Json::object());
            require(envelopes.is_object());
            const auto extension = envelopes.value(id, Json::object());
            require(extension.is_object());
            if (!extension.empty())
                active_envelopes[id] = extension;
            Json single{
                {"format", "audiohole-profile"}, {"schema_version", 3}, {"profile", profile}};
            for (const auto& [key, value] : extension.items())
                if (!single.contains(key))
                    single[key] = value;
            const auto result = impl_->writer(
                next / std::filesystem::path(std::u8string(
                           reinterpret_cast<const char8_t*>(name.data()), name.size())),
                single.dump(2) + "\n", {});
            if (!result.ok)
                throw std::runtime_error(result.detail);
        }
        auto state = document;
        state["profiles"] = Json::array();
        state.erase("profile_record_extensions");
        if (!active_envelopes.empty())
            state["profile_record_extensions"] = active_envelopes;
        const auto stored = impl_->writer(next / ".state.json", state.dump(2) + "\n", {});
        if (!stored.ok)
            throw std::runtime_error(stored.detail);
        require(profile_files(root) == impl_->observed &&
                read_document(impl_->file) == impl_->legacy);
        if (std::filesystem::exists(folder))
            std::filesystem::rename(folder, previous);
        try {
            std::filesystem::rename(next, folder);
        } catch (...) {
            if (std::filesystem::exists(previous))
                std::filesystem::rename(previous, folder);
            throw;
        }
        // A complete named generation is durable before legacy input is archived.
        if (impl_->legacy) {
            auto backup = root / "local" / "legacy-profiles" / impl_->file.filename();
            const auto existing = read_document(backup);
            if (!existing)
                (void)replace_document(backup, *impl_->legacy, {});
            if (read_document(backup) == impl_->legacy &&
                read_document(impl_->file) == impl_->legacy)
                std::filesystem::remove(impl_->file);
        }
        for (int version : {1, 2, 3, 4, 5, 6}) {
            auto old = impl_->file;
            old += ".v" + std::to_string(version) + ".bak";
            if (const auto bytes = read_document(old)) {
                const auto backup = root / "local" / "legacy-profiles" / old.filename();
                if (!read_document(backup))
                    (void)replace_document(backup, *bytes, {});
                if (read_document(backup) == bytes && read_document(old) == bytes)
                    std::filesystem::remove(old);
            }
        }
        ConfigurationRecords loaded;
        const auto refreshed = load(loaded);
        return refreshed.ok ? Result{true, "Profiles saved in named files."} : refreshed;
    } catch (const std::exception& e) {
        std::error_code ignored;
        if (created_next)
            std::filesystem::remove_all(next, ignored);
        return {false,
                std::string("Profile save blocked; check external edits or filename conflicts: ") +
                    e.what()};
    }
}
Result profile_record_path(const std::filesystem::path& file, const std::string& id,
                           std::filesystem::path& output) {
    try {
        std::filesystem::path selected;
        for (const auto& [name, bytes] : profile_files(file.parent_path())) {
            if (name == ".state.json")
                continue;
            const auto document = parse(bytes);
            const auto records = decode(document.value("format", "") == "audiohole-profile"
                                            ? record_document(document)
                                            : current_document(document));
            if (records.profiles.size() == 1 && records.profiles[0].id == id) {
                require(selected.empty());
                selected = file.parent_path() / "profiles" / name;
            }
        }
        if (selected.empty())
            return {false, "Save this profile before showing its JSON in Explorer."};
        output = selected;
        return {true, "Saved profile file located."};
    } catch (const std::exception&) {
        return {
            false,
            "Cannot identify a unique saved profile file. Reload and check the profiles folder."};
    }
}
Result read_profile_document(const std::filesystem::path& file, std::string& output) {
    ProfileDirectoryPersistence provider(file);
    ConfigurationRecords records;
    const auto result = provider.load(records);
    if (!result.ok)
        return result;
    // Assembly retains unknown fields, rather than re-encoding typed records.
    const auto files = profile_files(file.parent_path());
    output = !files.empty() ? assembled_profiles(files).dump(2) + "\n"
                            : read_document(file).value_or(encode(records).dump(2) + "\n");
    return {true, "Profiles read."};
}
Result write_profile_document(const std::filesystem::path& file, const std::string& bytes,
                              const std::string& expected,
                              std::function<Result(const std::filesystem::path&, const std::string&,
                                                   const std::optional<std::string>&)>
                                  writer) {
    ProfileDirectoryPersistence provider(file);
    ConfigurationRecords records;
    auto result = provider.load(records);
    if (!result.ok)
        return result;
    std::string actual;
    result = read_profile_document(file, actual);
    if (!result.ok || parse(actual) != parse(expected))
        return {false, "Profiles changed after preview."};
    result = validate_test_document(bytes, records);
    if (!result.ok)
        return result;
    // Keep imported extension data when saving the merged typed records.
    // Merge through each profile's full document is implemented by loading an
    // isolated aggregate; the directory provider below receives that original.
    provider.impl_->original = parse(bytes);
    if (writer)
        provider.impl_->writer = std::move(writer);
    return provider.save(records);
}
} // namespace audiohole
