#include "audiohole/engine/profile.hpp"

#include <algorithm>
#include <set>

namespace audiohole::engine {
namespace {
void validate(const EndpointRequirements& r, const std::string& location,
              std::vector<Issue>& issues) {
    auto range = [&](const std::optional<int>& value, int maximum, const char* key) {
        if (value && (*value < 1 || *value > maximum))
            issues.push_back({ErrorCode::InvalidConfiguration, location + "/" + key,
                              "Preference is outside the supported serialization range."});
    };
    range(r.sample_rate_hz, 768000, "sample_rate_hz");
    range(r.channel_count, 256, "channel_count");
    range(r.buffer_ms, 100, "buffer_ms");
    if (r.sample_format && std::string_view(name(*r.sample_format)) == "invalid")
        issues.push_back({ErrorCode::InvalidConfiguration, location + "/sample_format",
                          "Invalid typed sample format."});
    if (r.mode && *r.mode != AccessMode::Shared && *r.mode != AccessMode::Exclusive)
        issues.push_back(
            {ErrorCode::InvalidConfiguration, location + "/mode", "Invalid typed access mode."});
}

Result<LocalBinding> binding(const std::string& logical_id, EndpointDirection expected,
                             const BindingSnapshot& snapshot, const std::string& location) {
    const LocalBinding* selected = nullptr;
    for (const auto& candidate : snapshot.endpoints) {
        if (candidate.logical_id != logical_id)
            continue;
        if (selected)
            return {std::nullopt,
                    {{ErrorCode::AmbiguousEndpoint, location,
                      "Multiple bindings reference this logical endpoint."}}};
        selected = &candidate;
    }
    if (!selected)
        return {std::nullopt,
                {{ErrorCode::UnresolvedEndpoint, location,
                  "Logical endpoint has no current local binding."}}};
    if (selected->state == BindingState::Ambiguous)
        return {
            std::nullopt,
            {{ErrorCode::AmbiguousEndpoint, location, "Endpoint identity requires confirmation."}}};
    if (selected->state != BindingState::Available || selected->endpoint_id.empty())
        return {std::nullopt,
                {{ErrorCode::UnavailableEndpoint, location,
                  "Endpoint is unavailable or its current state is unknown."}}};
    if (selected->endpoint_id.size() > 32768 ||
        selected->endpoint_id.find('\0') != std::string::npos)
        return {std::nullopt,
                {{ErrorCode::InvalidConfiguration, location,
                  "Native endpoint ID is too long or contains a NUL character."}}};
    if (selected->direction != expected)
        return {std::nullopt,
                {{ErrorCode::DirectionMismatch, location,
                  "Route requires capture source and render destination."}}};
    for (const auto& other : snapshot.endpoints)
        if (other.logical_id != logical_id && other.endpoint_id == selected->endpoint_id)
            return {std::nullopt,
                    {{ErrorCode::AmbiguousEndpoint, location,
                      "Different logical devices claim the same native endpoint."}}};
    return {*selected, {}};
}
} // namespace

Result<PreparedProfile> prepare_profile(const Profile& profile) {
    PreparedProfile prepared;
    prepared.profile_id = profile.id;
    std::vector<Issue> issues;
    if (profile.id.empty() || profile.id.size() > 128 || profile.display_name.empty() ||
        profile.holes.size() > 128)
        issues.push_back({ErrorCode::InvalidConfiguration, "/profile",
                          "Profile needs a bounded stable ID and a display name."});
    std::set<std::string> ids;
    for (std::size_t i = 0; i < profile.holes.size(); ++i) {
        const auto& hole = profile.holes[i];
        const auto base = "/profile/holes/" + std::to_string(i);
        if (hole.id.empty() || hole.id.size() > 128 || !ids.insert(hole.id).second)
            issues.push_back({ErrorCode::InvalidConfiguration, base + "/id",
                              "Hole ID must be bounded, nonempty and unique."});
        if (!hole.enabled)
            continue;
        if (++prepared.enabled_holes > 8) {
            issues.push_back({ErrorCode::InvalidConfiguration, base + "/enabled",
                              "At most eight enabled logical holes may start."});
            break;
        }
        const auto backend = backend_id(hole.routing_engine);
        if (!backend) {
            issues.push_back({ErrorCode::UnsupportedBackend, base + "/routing",
                              "Requested backend is unknown; it will not be substituted."});
            continue;
        }
        for (const auto direction : {DirectionId::Forward, DirectionId::Reverse}) {
            if (direction == DirectionId::Reverse && !hole.duplex)
                continue;
            const bool reverse = direction == DirectionId::Reverse;
            const auto& saved = reverse ? hole.reverse : hole.forward;
            const auto path = base + (reverse ? "/reverse" : "/forward");
            if (saved.source.empty() || saved.destination.empty() || saved.source.size() > 128 ||
                saved.destination.size() > 128 || saved.source.find('\0') != std::string::npos ||
                saved.destination.find('\0') != std::string::npos)
                issues.push_back({ErrorCode::InvalidConfiguration, path,
                                  "Assign bounded logical source and destination IDs."});
            RouteRequest request;
            request.profile_id = profile.id;
            request.hole_id = hole.id;
            request.direction = direction;
            request.source_id = saved.source;
            request.destination_id = saved.destination;
            request.backend = *backend;
            request.exclusive_mode_preferred = saved.exclusive_mode_preferred;
            const auto& clock = saved.preferences.preferred_clock_device;
            if (clock.size() > 128 || clock.find('\0') != std::string::npos)
                issues.push_back({ErrorCode::InvalidConfiguration,
                                  path + "/settings/preferences/preferred_clock_device",
                                  "Saved clock metadata exceeds its bound or contains NUL."});
            else
                request.preferred_clock_device = clock;
            if (saved.advanced_tuning) {
                // Validate the saved groups independently, then merge fields.
                // Inactive retained preferences are deliberately not activated.
                const auto& p = saved.preferences;
                const auto prefs = path + "/settings/preferences";
                validate(p.common, prefs + "/common", issues);
                validate(p.source, prefs + "/source", issues);
                validate(p.destination, prefs + "/destination", issues);
                request.source = inherit(p.common, p.source);
                request.destination = inherit(p.common, p.destination);
                request.channel_map = p.channel_map;
                if (p.channel_map.size() > 256)
                    issues.push_back({ErrorCode::InvalidConfiguration, prefs + "/channel_map",
                                      "At most 256 channel routes are supported."});
                std::set<int> outputs;
                for (const auto& map : p.channel_map) {
                    if (map.source < 0 || map.source > 255 || map.destination < 0 ||
                        map.destination > 255 || !outputs.insert(map.destination).second ||
                        (request.source.channel_count &&
                         map.source >= *request.source.channel_count) ||
                        (request.destination.channel_count &&
                         map.destination >= *request.destination.channel_count))
                        issues.push_back({ErrorCode::InvalidConfiguration, prefs + "/channel_map",
                                          "Map indexes must fit stream widths; each output has "
                                          "one source."});
                }
            }
            prepared.routes.push_back(std::move(request));
        }
    }
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    return {std::move(prepared), {}};
}

Result<BoundRoute> bind_route(const RouteRequest& request, const BindingSnapshot& snapshot) {
    auto source = binding(request.source_id, EndpointDirection::Capture, snapshot, "source");
    auto destination =
        binding(request.destination_id, EndpointDirection::Render, snapshot, "destination");
    std::vector<Issue> issues = std::move(source.issues);
    issues.insert(issues.end(), destination.issues.begin(), destination.issues.end());
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    return {BoundRoute{request, std::move(*source.value), std::move(*destination.value),
                       snapshot.revision},
            {}};
}

Result<PcmGeometry> pcm_geometry(const StreamFormat& format) {
    EndpointRequirements values;
    values.sample_rate_hz = format.sample_rate_hz;
    values.channel_count = format.channel_count;
    values.sample_format = format.sample_format;
    std::vector<Issue> issues;
    validate(values, "format", issues);
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    PcmGeometry result;
    switch (format.sample_format) {
    case SampleFormat::PcmS16:
        result.container_bits = result.valid_bits = 16;
        break;
    case SampleFormat::PcmS24Packed:
        result.container_bits = result.valid_bits = 24;
        break;
    case SampleFormat::PcmS24In32:
        result.container_bits = 32;
        result.valid_bits = 24;
        break;
    case SampleFormat::Float32:
    case SampleFormat::PcmS32:
        result.container_bits = result.valid_bits = 32;
        break;
    }
    result.block_align_bytes = static_cast<std::uint32_t>(format.channel_count) *
                               static_cast<std::uint32_t>(result.container_bits / 8);
    result.average_bytes_per_second =
        static_cast<std::uint64_t>(format.sample_rate_hz) * result.block_align_bytes;
    return {result, {}};
}

Result<StreamFormat> check_format(const EndpointRequirements& required, const StreamFormat& actual,
                                  std::string_view location) {
    const auto geometry = pcm_geometry(actual);
    if (!geometry.ok())
        return {std::nullopt, geometry.issues};
    std::vector<Issue> issues;
    validate(required, std::string(location), issues);
    auto check = [&](bool mismatch, const char* key) {
        if (mismatch)
            issues.push_back({ErrorCode::ContractViolation, std::string(location) + "/" + key,
                              "Backend did not satisfy the explicit format requirement."});
    };
    check(required.sample_rate_hz && *required.sample_rate_hz != actual.sample_rate_hz,
          "sample_rate_hz");
    check(required.channel_count && *required.channel_count != actual.channel_count,
          "channel_count");
    check(required.sample_format && *required.sample_format != actual.sample_format,
          "sample_format");
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    return {actual, {}};
}
} // namespace audiohole::engine
