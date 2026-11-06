#include "model/persistence.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <fstream>
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
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid provisional configuration");
}
std::optional<std::string> read_document(const std::filesystem::path& file) {
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
Direction direction(const Json& value) {
    Direction d;
    d.source = string(value, "source", 128);
    d.destination = string(value, "destination", 128);
    if (value.contains("source_name"))
        d.source_name = string(value, "source_name");
    if (value.contains("destination_name"))
        d.destination_name = string(value, "destination_name");
    const auto& settings = value.at("settings");
    d.settings.automatic = settings.at("automatic").get<bool>();
    d.settings.buffer_ms = integer(settings, "buffer_ms", 1, 100);
    require(d.settings.buffer_ms >= 1 && d.settings.buffer_ms <= 100);
    d.settings.mode = string(settings, "mode", 64);
    d.settings.format = format(settings.at("format"));
    return d;
}
Json encode(const Direction& d) {
    return {{"source_name", d.source_name},
            {"source", d.source},
            {"destination_name", d.destination_name},
            {"destination", d.destination},
            {"settings",
             {{"automatic", d.settings.automatic},
              {"format", encode(d.settings.format)},
              {"mode", d.settings.mode},
              {"buffer_ms", d.settings.buffer_ms}}}};
}
Json encode(const ConfigurationRecords& records) {
    Json result = {{"format", "audiohole-test-configuration"},
                   {"schema_version", 2},
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
            integer(root, "schema_version", 1, 2) >= 1);
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
            h.forward = direction(x.at("forward"));
            h.reverse = direction(x.at("reverse"));
            p.holes.push_back(std::move(h));
        }
        records.profiles.push_back(std::move(p));
    }
    return records;
}
// Preserve unrecognized fields recursively. Record arrays match by stable ID;
// explicit removals stay removed. New known values take precedence.
Json merge(const Json& old, const Json& current) {
    if (old.is_object() && current.is_object()) {
        auto result = Json::object();
        for (const auto& [key, value] : current.items())
            result[key] = old.contains(key) ? merge(old.at(key), value) : value;
        for (const auto& [key, value] : old.items())
            if (!current.contains(key))
                result[key] = value;
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
        (void)decode(added);
        (void)decode(current);
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
        current["schema_version"] = 2;
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
        if (impl_->expected && impl_->original.at("schema_version") == 1) {
            // No load-time migration. The first explicit save keeps the exact
            // v1 original; never overwrite a different earlier backup.
            if (read_document(impl_->file) != impl_->expected)
                return {false,
                        "File changed externally. Reload before saving; no data was overwritten."};
            auto backup = impl_->file;
            backup += ".v1.bak";
            const auto existing = read_document(backup);
            if (existing && existing != impl_->expected)
                return {false, "A different v1 backup already exists. Preserve it and choose a new "
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
} // namespace audiohole
