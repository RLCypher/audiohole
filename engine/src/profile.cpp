#include "audiohole/engine/profile.hpp"

#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <utility>

namespace audiohole::engine {
namespace {
using Json = nlohmann::json;

// Exceptions are confined to the non-real-time parser and translated into
// structured results. Do not catch/throw from a sample-processing callback.
struct ParseFailure {
    Issue issue;
};

[[noreturn]] void fail(std::string location, std::string detail,
                       ErrorCode code = ErrorCode::InvalidDocument) {
    throw ParseFailure{{code, std::move(location), std::move(detail)}};
}

void require_object(const Json& value, const std::string& path) {
    if (!value.is_object())
        fail(path, "Expected an object.");
}

const Json& object_member(const Json& parent, const char* key, const std::string& path) {
    static const Json empty = Json::object();
    if (!parent.contains(key))
        return empty;
    const auto& value = parent.at(key);
    require_object(value, path + "/" + key);
    return value;
}

std::string text(const Json& parent, const char* key, const std::string& path,
                 std::size_t limit = 65536, bool required = true) {
    if (!parent.contains(key)) {
        if (required)
            fail(path + "/" + key, "Required string is missing.");
        return {};
    }
    const auto& value = parent.at(key);
    if (!value.is_string())
        fail(path + "/" + key, "Expected a string.");
    auto result = value.get<std::string>();
    if (result.size() > limit || result.find('\0') != std::string::npos)
        fail(path + "/" + key, "String exceeds its bound or contains a NUL character.");
    return result;
}

bool boolean(const Json& parent, const char* key, bool fallback, const std::string& path) {
    if (!parent.contains(key))
        return fallback;
    if (!parent.at(key).is_boolean())
        fail(path + "/" + key, "Expected a boolean.");
    return parent.at(key).get<bool>();
}

int integer(const Json& value, int lower, int upper, const std::string& path) {
    // Check JSON magnitude before conversion so huge unsigned integers cannot
    // wrap into an apparently valid signed preference.
    if (!value.is_number_integer() || value < lower || value > upper)
        fail(path, "Expected an integer in " + std::to_string(lower) + ".." +
                       std::to_string(upper) + ".");
    return value.get<int>();
}

bool inherited(const Json& object, const char* key) {
    if (!object.contains(key))
        return true;
    const auto& value = object.at(key);
    return value.is_null() || value == "" || value == "Automatic";
}

std::optional<double> timing(const Json& object, const char* key, const std::string& path) {
    if (!object.contains(key) || object.at(key).is_null())
        return {};
    const auto& v = object.at(key);
    if (!v.is_number())
        fail(path + "/" + key, "Expected positive finite milliseconds or null.");
    const double n = v.get<double>();
    if (!std::isfinite(n) || n <= 0)
        fail(path + "/" + key, "Expected positive finite milliseconds or null.");
    return n;
}
EndpointRequirements requirements(const Json& value, const std::string& path) {
    require_object(value, path);
    EndpointRequirements result;
    result.period_ms = timing(value, "period_ms", path);
    result.render_ahead_ms = timing(value, "render_ahead_ms", path);
    if (!inherited(value, "sample_rate_hz"))
        result.sample_rate_hz =
            integer(value.at("sample_rate_hz"), 1, 768000, path + "/sample_rate_hz");
    if (!inherited(value, "channel_count"))
        result.channel_count = integer(value.at("channel_count"), 1, 256, path + "/channel_count");
    if (!inherited(value, "buffer_ms")) {
        const int duration = integer(value.at("buffer_ms"), 0, 100, path + "/buffer_ms");
        if (duration != 0)
            result.buffer_ms = duration;
    }
    if (!inherited(value, "sample_format")) {
        const auto format = text(value, "sample_format", path, 32);
        if (format == "float32")
            result.sample_format = SampleFormat::Float32;
        else if (format == "pcm_s16")
            result.sample_format = SampleFormat::PcmS16;
        else if (format == "pcm_s24_packed")
            result.sample_format = SampleFormat::PcmS24Packed;
        else if (format == "pcm_s24_in_32")
            result.sample_format = SampleFormat::PcmS24In32;
        else if (format == "pcm_s32")
            result.sample_format = SampleFormat::PcmS32;
        else
            fail(path + "/sample_format", "Unknown sample format.");
    }
    if (!inherited(value, "mode")) {
        const auto mode = text(value, "mode", path, 32);
        if (mode == "Shared")
            result.mode = AccessMode::Shared;
        else if (mode == "Exclusive")
            result.mode = AccessMode::Exclusive;
        else
            fail(path + "/mode", "Expected Shared, Exclusive or Automatic.");
    }
    return result;
}

Direction direction(const Json& value, const std::string& path, int version) {
    require_object(value, path);
    Direction result;
    if (version >= 3) {
        result.advanced_tuning = boolean(value, "advanced_tuning", false, path);
        result.exclusive_mode_preferred = boolean(value, "exclusive_mode_preferred", false, path);
    }
    result.source = text(value, "source", path, 128);
    result.destination = text(value, "destination", path, 128);
    result.source_name = text(value, "source_name", path, 65536, false);
    result.destination_name = text(value, "destination_name", path, 65536, false);
    const auto& settings = object_member(value, "settings", path);
    const auto& preferences = object_member(settings, "preferences", path + "/settings");
    const std::string base = path + "/settings/preferences";
    result.preferences.queue_target_ms = timing(preferences, "queue_target_ms", base);
    if (version >= 2 && preferences.contains("preferred_clock_device"))
        result.preferences.preferred_clock_device =
            text(preferences, "preferred_clock_device", base, 128);
    result.preferences.common =
        requirements(object_member(preferences, "common", base), base + "/common");
    result.preferences.source =
        requirements(object_member(preferences, "source", base), base + "/source");
    result.preferences.destination =
        requirements(object_member(preferences, "destination", base), base + "/destination");
    if (preferences.contains("channel_map")) {
        const auto& routes = preferences.at("channel_map");
        if (!routes.is_array() || routes.size() > 256)
            fail(base + "/channel_map", "Expected at most 256 channel routes.");
        for (std::size_t i = 0; i < routes.size(); ++i) {
            const auto location = base + "/channel_map/" + std::to_string(i);
            const auto& route = routes.at(i);
            require_object(route, location);
            if (!route.contains("source") || !route.contains("destination"))
                fail(location, "Channel route needs source and destination indexes.");
            result.preferences.channel_map.push_back(
                {integer(route.at("source"), 0, 255, location + "/source"),
                 integer(route.at("destination"), 0, 255, location + "/destination")});
        }
    }
    return result;
}
} // namespace

const char* name(AccessMode mode) noexcept {
    return mode == AccessMode::Shared ? "Shared" : "Exclusive";
}
const char* name(SampleFormat format) noexcept {
    switch (format) {
    case SampleFormat::Float32:
        return "float32";
    case SampleFormat::PcmS16:
        return "pcm_s16";
    case SampleFormat::PcmS24Packed:
        return "pcm_s24_packed";
    case SampleFormat::PcmS24In32:
        return "pcm_s24_in_32";
    case SampleFormat::PcmS32:
        return "pcm_s32";
    }
    return "invalid";
}
const char* name(BackendId backend) noexcept {
    switch (backend) {
    case BackendId::Automatic:
        return "automatic";
    case BackendId::Miniaudio:
        return "miniaudio";
    case BackendId::Wasapi:
        return "wasapi";
    case BackendId::Experimental:
        return "experimental";
    }
    return "invalid";
}
std::optional<BackendId> backend_id(std::string_view value) noexcept {
    for (const auto backend :
         {BackendId::Automatic, BackendId::Miniaudio, BackendId::Wasapi, BackendId::Experimental})
        if (value == name(backend))
            return backend;
    return std::nullopt;
}

Result<Profile> parse_profile(std::string_view bytes) {
    try {
        if (bytes.size() > 2 * 1024 * 1024)
            fail("", "Profile document exceeds 2 MiB.");
        std::vector<std::set<std::string>> keys;
        const Json document = Json::parse(
            bytes.begin(), bytes.end(), [&](int depth, Json::parse_event_t event, Json& value) {
                if (depth > 16)
                    fail("", "Profile nesting exceeds depth 16.");
                if (event == Json::parse_event_t::object_start)
                    keys.emplace_back();
                else if (event == Json::parse_event_t::object_end)
                    keys.pop_back();
                else if (event == Json::parse_event_t::key &&
                         !keys.back().insert(value.get<std::string>()).second)
                    fail("", "Duplicate JSON object key.");
                return true;
            });
        require_object(document, "");
        if (text(document, "format", "", 64) != "audiohole-profile")
            fail("/format", "Expected an individual audiohole-profile record.");
        if (!document.contains("schema_version") ||
            !document.at("schema_version").is_number_integer() ||
            (document.at("schema_version") != 1 && document.at("schema_version") != 2 &&
             document.at("schema_version") != 3 && document.at("schema_version") != 4))
            fail("/schema_version", "Supported individual profile versions are 1 through 4.",
                 ErrorCode::UnsupportedVersion);
        const int version = document.at("schema_version").get<int>();
        if (!document.contains("profile"))
            fail("/profile", "Profile is missing.");
        const auto& value = document.at("profile");
        require_object(value, "/profile");
        Profile result;
        result.source_schema_version = version;
        result.id = text(value, "id", "/profile", 128);
        result.display_name = text(value, "display_name", "/profile");
        result.notes = text(value, "notes", "/profile", 65536, false);
        if (result.id.empty() || result.display_name.empty())
            fail("/profile", "Profile ID and display name must be nonempty.");
        if (!value.contains("holes") || !value.at("holes").is_array() ||
            value.at("holes").size() > 128)
            fail("/profile/holes", "Expected an array of at most 128 saved holes.");
        std::set<std::string> hole_ids;
        for (std::size_t i = 0; i < value.at("holes").size(); ++i) {
            const auto base = "/profile/holes/" + std::to_string(i);
            const auto& record = value.at("holes").at(i);
            require_object(record, base);
            Hole hole;
            hole.id = text(record, "id", base, 128);
            hole.display_name = text(record, "display_name", base);
            hole.notes = text(record, "notes", base, 65536, false);
            if (hole.id.empty() || !hole_ids.insert(hole.id).second)
                fail(base + "/id", "Hole IDs must be nonempty and unique in a profile.");
            hole.enabled = boolean(record, "enabled", true, base);
            hole.duplex = boolean(record, "duplex", false, base);
            const char* routing_key = version == 1 ? "routing_engine" : "routing";
            if (record.contains(routing_key))
                hole.routing_engine = text(record, routing_key, base, 64);
            else if (version == 2 && record.contains("routing_engine"))
                hole.routing_engine = text(record, "routing_engine", base, 64);
            else if (version == 3 && record.contains("routing_engine"))
                fail(base + "/routing", "Schema 3 uses hole.routing, not routing_engine.");
            if (hole.routing_engine.empty())
                fail(base + "/" + routing_key, "Backend selector must be nonempty.");
            if (record.contains("forward"))
                hole.forward = direction(record.at("forward"), base + "/forward", version);
            if (record.contains("reverse"))
                hole.reverse = direction(record.at("reverse"), base + "/reverse", version);
            if (version < 3) {
                // Loading compatibility is an in-memory projection only; never
                // rewrite the original document, notes or retained preferences.
                const bool advanced = boolean(record, "advanced_tuning", false, base);
                const bool prefer =
                    version == 2 && boolean(record, "exclusive_mode_preferred", false, base);
                hole.forward.advanced_tuning = hole.reverse.advanced_tuning = advanced;
                hole.forward.exclusive_mode_preferred = hole.reverse.exclusive_mode_preferred =
                    prefer;
            }
            result.holes.push_back(std::move(hole));
        }
        return {std::move(result), {}};
    } catch (const ParseFailure& error) {
        return {std::nullopt, {error.issue}};
    } catch (const Json::exception&) {
        // Parser exception text may quote user contents. Keep diagnostics bounded
        // and stable; callers still have the untouched source for inspection.
        return {std::nullopt, {{ErrorCode::InvalidDocument, "", "Malformed UTF-8 JSON profile."}}};
    }
}

EndpointRequirements inherit(const EndpointRequirements& common,
                             const EndpointRequirements& endpoint) {
    EndpointRequirements result = endpoint;
    if (!result.sample_rate_hz)
        result.sample_rate_hz = common.sample_rate_hz;
    if (!result.channel_count)
        result.channel_count = common.channel_count;
    if (!result.sample_format)
        result.sample_format = common.sample_format;
    if (!result.mode)
        result.mode = common.mode;
    if (!result.buffer_ms)
        result.buffer_ms = common.buffer_ms;
    if (!result.period_ms)
        result.period_ms = common.period_ms;
    if (!result.render_ahead_ms)
        result.render_ahead_ms = common.render_ahead_ms;
    return result;
}
} // namespace audiohole::engine
