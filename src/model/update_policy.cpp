#include "model/update_policy.h"
#include "model/persistence.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <nlohmann/json.hpp>

namespace audiohole::updates {
namespace {
using Json = nlohmann::json;
constexpr std::string_view download_prefix =
    "https://github.com/RLCypher/audiohole/releases/download/";
bool safe_installer(const Installer& asset) {
    return asset.bytes > 0 && asset.bytes <= maximum_installer_bytes &&
           asset.url.starts_with(download_prefix) && asset.url.size() <= 4096 &&
           std::all_of(asset.url.begin(), asset.url.end(),
                       [](unsigned char c) { return c > 32 && c < 127; }) &&
           asset.sha256.size() == 64 &&
           std::all_of(asset.sha256.begin(), asset.sha256.end(),
                       [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
std::string brief_notes(std::string text) {
    constexpr std::size_t limit = 2000;
    if (text.size() > limit) {
        auto end = limit;
        while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
            --end;
        text.resize(end);
        text += "\n…";
    }
    return text;
}
Json release_json(const Release& release) {
    Json result{{"version", release.version}, {"notes", release.notes}};
    if (release.installer)
        result["installer"] = {{"url", release.installer->url},
                               {"sha256", release.installer->sha256},
                               {"bytes", release.installer->bytes}};
    return result;
}
Release cached_release(const Json& json) {
    Release release{json.at("version").get<std::string>(), json.value("notes", std::string{}), {}};
    if (json.contains("installer")) {
        const auto& asset = json.at("installer");
        release.installer =
            Installer{asset.at("url").get<std::string>(), asset.at("sha256").get<std::string>(),
                      asset.at("bytes").get<std::uint64_t>()};
    }
    if (!valid(release))
        throw std::runtime_error("Invalid cached release.");
    return release;
}
} // namespace
std::optional<Version> parse_version(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
        text.remove_prefix(1);
    if (text.empty() || text.size() > 20)
        return {};
    Version version;
    unsigned* parts[]{&version.major, &version.minor, &version.patch};
    for (unsigned i = 0; i < 3; ++i) {
        const auto dot = text.find('.');
        const auto token = text.substr(0, dot);
        if (token.empty())
            return {};
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), *parts[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            *parts[i] > 65535)
            return {};
        if (dot == std::string_view::npos)
            return i >= 1 ? std::optional{version} : std::nullopt;
        text.remove_prefix(dot + 1);
    }
    return {};
}
std::string Version::normalized() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}
std::string Version::display() const {
    return "v" + std::to_string(major) + "." + std::to_string(minor) +
           (patch ? "." + std::to_string(patch) : "");
}
std::string Release::page_url() const {
    return "https://github.com/RLCypher/audiohole/releases";
}
bool valid(const Release& release) {
    return parse_version(release.version).has_value() && release.notes.size() <= 4096 &&
           release.notes.find('\0') == std::string::npos &&
           (!release.installer || safe_installer(*release.installer));
}
bool check_due(const State& state, bool enabled, std::int64_t now) {
    return enabled && now >= 0 &&
           (state.last_checked_unix == 0 || now < state.last_checked_unix ||
            now - state.last_checked_unix >= check_interval_seconds);
}
bool should_notify(const State& state, std::string_view current) {
    const auto running = parse_version(current);
    if (!running || !state.cached || !valid(*state.cached))
        return false;
    const auto offered = parse_version(state.cached->version);
    const auto skipped = parse_version(state.skipped_version);
    return offered && *offered > *running && (!skipped || *offered != *skipped);
}
Release parse_release(std::string_view bytes) {
    if (bytes.size() > 512 * 1024)
        throw std::runtime_error("Release response is too large.");
    const auto json = Json::parse(bytes, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 32)
            throw std::runtime_error("Release response is too deeply nested.");
        return true;
    });
    if (json.value("draft", false) || json.value("prerelease", false))
        throw std::runtime_error("The offered release is not a stable published version.");
    const auto version = parse_version(json.at("tag_name").get<std::string>());
    if (!version)
        throw std::runtime_error("Release has no supported version number.");
    Release release{version->normalized(),
                    brief_notes(json.contains("body") && !json.at("body").is_null()
                                    ? json.at("body").get<std::string>()
                                    : std::string{}),
                    {}};
    const std::string expected = "Install AudioHole " + version->display() + ".exe";
    auto dotted = expected;
    std::replace(dotted.begin(), dotted.end(), ' ', '.');
    if (json.contains("assets")) {
        const auto& assets = json.at("assets");
        if (!assets.is_array() || assets.size() > 128)
            throw std::runtime_error("Invalid release assets.");
        for (const auto& asset : assets) {
            const auto name = asset.value("name", std::string{});
            if (name != expected && name != dotted)
                continue;
            if (!asset.contains("digest") || !asset.at("digest").is_string())
                continue;
            const auto digest = asset.at("digest").get<std::string>();
            if (!digest.starts_with("sha256:"))
                continue;
            Installer candidate{asset.at("browser_download_url").get<std::string>(),
                                digest.substr(7), asset.at("size").get<std::uint64_t>()};
            if (safe_installer(candidate))
                release.installer = std::move(candidate);
        }
    }
    if (!valid(release))
        throw std::runtime_error("Invalid release information.");
    return release;
}
struct Store::Impl {
    std::filesystem::path file;
    bool memory = false, ready = false;
    State state;
    Json document = Json::object();
    std::optional<std::string> expected;
};
Store::Store(std::filesystem::path file, bool memory) : impl_(std::make_unique<Impl>()) {
    impl_->file = std::move(file);
    impl_->memory = memory;
}
Store::~Store() = default;
const State& Store::values() const {
    return impl_->state;
}
Result Store::load() {
    try {
        impl_->ready = false;
        if (!impl_->memory) {
            impl_->state = {};
            impl_->document = Json::object();
            impl_->expected.reset();
        }
        if (!impl_->memory && std::filesystem::exists(impl_->file)) {
            if (std::filesystem::is_symlink(impl_->file) ||
                !std::filesystem::is_regular_file(impl_->file) ||
                std::filesystem::file_size(impl_->file) > 65536)
                throw std::runtime_error("Update state is not a bounded regular file.");
            std::ifstream stream(impl_->file, std::ios::binary);
            std::string bytes(65537, '\0');
            stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            bytes.resize(static_cast<std::size_t>(stream.gcount()));
            if (stream.bad() || !stream.eof() || bytes.size() > 65536)
                throw std::runtime_error("Cannot read bounded update state.");
            auto json = Json::parse(bytes, [](int depth, Json::parse_event_t, Json&) {
                if (depth > 16)
                    throw std::runtime_error("Update state nesting is too deep.");
                return true;
            });
            if (json.at("format") != "audiohole-update-state" || json.at("schema_version") != 1)
                throw std::runtime_error("Unsupported update state.");
            State state;
            const auto last = json.value("last_checked_unix", Json(0));
            if (!last.is_number_integer() || last < 0 || last > INT64_MAX)
                throw std::runtime_error("Invalid update check time.");
            state.last_checked_unix = last.get<std::int64_t>();
            state.skipped_version = json.value("skipped_version", std::string{});
            if (!state.skipped_version.empty() && !parse_version(state.skipped_version))
                throw std::runtime_error("Invalid skipped version.");
            if (json.contains("cached_release"))
                state.cached = cached_release(json.at("cached_release"));
            impl_->document = std::move(json);
            impl_->state = std::move(state);
            impl_->expected = bytes;
        }
        impl_->ready = true;
        return {true, "Update state loaded."};
    } catch (const std::exception& e) {
        return {false, std::string("Cannot load update state: ") + e.what()};
    }
}
Result Store::save(const State& state) {
    if (!impl_->ready || state.last_checked_unix < 0 ||
        (!state.skipped_version.empty() && !parse_version(state.skipped_version)) ||
        (state.cached && !valid(*state.cached)))
        return {false, "Invalid update state; nothing saved."};
    if (impl_->memory) {
        impl_->state = state;
        return {true, "In-memory update state saved."};
    }
    try {
        auto json = impl_->document;
        json["format"] = "audiohole-update-state";
        json["schema_version"] = 1;
        json["last_checked_unix"] = state.last_checked_unix;
        json["skipped_version"] = state.skipped_version;
        if (state.cached)
            json["cached_release"] = release_json(*state.cached);
        else
            json.erase("cached_release");
        const auto bytes = json.dump(2) + "\n";
        const auto result = replace_document(impl_->file, bytes, impl_->expected);
        if (result.ok) {
            impl_->document = std::move(json);
            impl_->expected = bytes;
            impl_->state = state;
        }
        return result;
    } catch (const std::exception& e) {
        return {false, std::string("Cannot save update state: ") + e.what()};
    }
}
} // namespace audiohole::updates
