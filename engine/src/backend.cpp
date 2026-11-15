#include "audiohole/engine/backend.hpp"
#include "audiohole/engine/channel_mapping.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace audiohole::engine {
Result<BackendId> select_backend(BackendId requested,
                                 const std::vector<BackendDescriptor>& descriptors) {
    std::set<BackendId> registered;
    for (const auto& descriptor : descriptors) {
        if (descriptor.id == BackendId::Automatic ||
            std::string_view(name(descriptor.id)) == "invalid" ||
            !registered.insert(descriptor.id).second)
            return {std::nullopt,
                    {{ErrorCode::InvalidConfiguration, "backends",
                      "Adapter IDs must be concrete and uniquely registered."}}};
    }
    auto available = [&](BackendId id) {
        return std::any_of(descriptors.begin(), descriptors.end(),
                           [&](const auto& d) { return d.id == id && d.available; });
    };
    if (requested == BackendId::Automatic) {
        for (const auto id : {BackendId::Wasapi, BackendId::Miniaudio})
            if (available(id))
                return {id, {}};
    } else if (available(requested)) {
        return {requested, {}};
    }
    return {std::nullopt,
            {{ErrorCode::BackendUnavailable, "routing_engine",
              "Requested backend is unavailable. Automatic excludes Experimental; "
              "explicit requests never substitute another adapter."}}};
}

const char* name(AccessPolicy policy) noexcept {
    switch (policy) {
    case AccessPolicy::AutomaticShared:
        return "automatic_shared";
    case AccessPolicy::SharedRequired:
        return "shared_required";
    case AccessPolicy::ExclusiveRequired:
        return "exclusive_required";
    case AccessPolicy::ExclusivePreferred:
        return "exclusive_preferred";
    }
    return "invalid";
}

AccessPolicy access_policy(const EndpointRequirements& required, bool preferred) noexcept {
    if (required.mode)
        return *required.mode == AccessMode::Exclusive ? AccessPolicy::ExclusiveRequired
                                                       : AccessPolicy::SharedRequired;
    return preferred ? AccessPolicy::ExclusivePreferred : AccessPolicy::AutomaticShared;
}

Result<StreamConfiguration> check_stream(const EndpointRequirements& required,
                                         const StreamConfiguration& stream,
                                         std::string_view location) {
    std::vector<Issue> issues;
    const std::string path(location);
    if (stream.mode != AccessMode::Shared && stream.mode != AccessMode::Exclusive)
        issues.push_back(
            {ErrorCode::ContractViolation, path + "/mode", "Invalid actual access mode."});
    const auto format = check_format(required, stream.client_format, path);
    issues.insert(issues.end(), format.issues.begin(), format.issues.end());
    if (stream.mode != required.mode.value_or(AccessMode::Shared))
        issues.push_back({ErrorCode::ContractViolation, path + "/mode",
                          "Backend changed the concrete attempted access mode."});
    if (required.buffer_ms && stream.applied_buffer_request_ms != required.buffer_ms)
        issues.push_back({ErrorCode::ContractViolation, path + "/buffer_ms",
                          "Explicit buffer request was not confirmed."});
    if (required.buffer_ms) {
        if (!stream.allocated_buffer_frames || !stream.timing_sample_rate_hz ||
            *stream.timing_sample_rate_hz < 1 || *stream.timing_sample_rate_hz > 768000) {
            issues.push_back(
                {ErrorCode::ContractViolation, path + "/buffer_ms",
                 "Cannot verify the explicit buffer without actual allocation and rate."});
        } else if (double(*stream.allocated_buffer_frames) + 1 <
                   double(*stream.timing_sample_rate_hz) * *required.buffer_ms / 1000)
            issues.push_back({ErrorCode::UnsupportedRequirement, path + "/buffer_ms",
                              "Allocated capacity is smaller than the requested capacity."});
        // buffer_ms is the explicit API request, not a promise that a driver
        // allocates an exact frame count. Native periods/alignment and library
        // period counts can change allocation. Keep both values in receipts;
        // missing/ignored requests still fail above, formats/modes remain strict.
    }
    auto timing = [&](const std::optional<double>& requested, const std::optional<double>& applied,
                      const std::optional<std::uint32_t>& frames, const char* field) {
        if (!requested)
            return;
        if (applied != requested || !frames || !stream.timing_sample_rate_hz ||
            *stream.timing_sample_rate_hz <= 0 ||
            std::abs(double(frames.value_or(0)) -
                     *requested * stream.timing_sample_rate_hz.value_or(0) / 1000) > 1.000001)
            issues.push_back(
                {ErrorCode::UnsupportedRequirement, path + "/" + field,
                 "Requested timing is unsupported; no larger target was substituted."});
    };
    timing(required.period_ms, stream.applied_period_request_ms, stream.period_frames, "period_ms");
    timing(required.render_ahead_ms, stream.applied_render_ahead_request_ms,
           stream.render_ahead_target_frames, "render_ahead_ms");
    if ((stream.period_frames && *stream.period_frames == 0) ||
        (stream.allocated_buffer_frames && *stream.allocated_buffer_frames == 0))
        issues.push_back({ErrorCode::ContractViolation, path + "/timing",
                          "Known period and allocation must be positive."});
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    return {stream, {}};
}

Result<NegotiatedRoute> check_negotiated(const RouteRequest& request,
                                         const NegotiatedRoute& actual) {
    std::vector<Issue> issues;
    if (actual.backend == BackendId::Automatic ||
        std::string_view(name(actual.backend)) == "invalid" ||
        (request.backend != BackendId::Automatic && request.backend != actual.backend) ||
        (request.backend == BackendId::Automatic && actual.backend == BackendId::Experimental))
        issues.push_back({ErrorCode::ContractViolation, "routing",
                          "Backend changed the explicit selection or auto-selected Experimental."});
    auto check = [&](const EndpointRequirements& required, const StreamConfiguration& stream,
                     const std::string& path) {
        auto concrete = required;
        const auto policy = access_policy(required, request.exclusive_mode_preferred);
        if (policy == AccessPolicy::ExclusivePreferred) {
            const auto& attempts = stream.access_attempts;
            const bool exclusive = stream.mode == AccessMode::Exclusive && attempts.size() == 1 &&
                                   attempts[0].mode == AccessMode::Exclusive &&
                                   attempts[0].succeeded && attempts[0].native_open_attempted &&
                                   attempts[0].issues.empty() && !stream.access_fell_back;
            const bool fallback = stream.mode == AccessMode::Shared && attempts.size() == 2 &&
                                  attempts[0].mode == AccessMode::Exclusive &&
                                  !attempts[0].succeeded && !attempts[0].issues.empty() &&
                                  attempts[1].mode == AccessMode::Shared && attempts[1].succeeded &&
                                  attempts[1].native_open_attempted && attempts[1].issues.empty() &&
                                  stream.access_fell_back;
            if ((!exclusive && !fallback) || stream.access_policy != policy ||
                stream.requested_mode != required.mode || !stream.exclusive_mode_preferred)
                issues.push_back({ErrorCode::ContractViolation, path + "/mode",
                                  "Preferred Exclusive requires an auditable successful attempt "
                                  "or failed Exclusive followed by successful Shared."});
            concrete.mode = stream.mode;
        } else if (!stream.access_attempts.empty()) {
            const auto& attempts = stream.access_attempts;
            if (stream.access_policy != policy || stream.requested_mode != required.mode ||
                stream.exclusive_mode_preferred != request.exclusive_mode_preferred ||
                stream.access_fell_back || attempts.size() != 1 || !attempts[0].succeeded ||
                !attempts[0].native_open_attempted || !attempts[0].issues.empty() ||
                attempts[0].mode != stream.mode)
                issues.push_back(
                    {ErrorCode::ContractViolation, path + "/mode",
                     "Access receipt contradicts the requested strict/Automatic policy."});
        }
        const auto validated = check_stream(concrete, stream, path);
        issues.insert(issues.end(), validated.issues.begin(), validated.issues.end());
    };
    check(request.source, actual.source, "source");
    check(request.destination, actual.destination, "destination");
    if (request.queue_target_ms &&
        (actual.requested_queue_target_ms != request.queue_target_ms ||
         !actual.fifo_target_source_frames ||
         std::abs(double(actual.fifo_target_source_frames.value_or(0)) -
                  *request.queue_target_ms * actual.source.client_format.sample_rate_hz / 1000) >
             1.000001))
        issues.push_back({ErrorCode::UnsupportedRequirement, "queue_target_ms",
                          "The explicit routing queue target was not applied."});
    if (actual.clock_preference_applied)
        issues.push_back({ErrorCode::ContractViolation, "preferred_clock_device",
                          "Clock preference is metadata only in this engine build."});
    const auto mapping =
        compile_mapping(actual.source.client_format.channel_count,
                        actual.destination.client_format.channel_count, request.channel_map);
    issues.insert(issues.end(), mapping.issues.begin(), mapping.issues.end());
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    return {actual, {}};
}
} // namespace audiohole::engine
