#include "access_negotiation.hpp"

#include <array>

namespace audiohole::engine::detail {

std::optional<RouteRequest> startup_shared_retry(const RouteRequest& original,
                                                 const NegotiatedRoute& previous) {
    auto retry = original;
    bool changed = false;
    auto endpoint = [&](EndpointRequirements& required, const StreamConfiguration& actual) {
        if (access_policy(required, original.exclusive_mode_preferred) !=
            AccessPolicy::ExclusivePreferred)
            return;
        changed |= actual.mode == AccessMode::Exclusive;
        required.mode = AccessMode::Shared;
    };
    endpoint(retry.source, previous.source);
    endpoint(retry.destination, previous.destination);
    return changed ? std::optional<RouteRequest>(std::move(retry)) : std::nullopt;
}

Result<NegotiatedRoute> record_startup_fallback(const RouteRequest& original,
                                                const NegotiatedRoute& previous,
                                                NegotiatedRoute& current,
                                                const Issue& startup_failure) {
    std::vector<Issue> issues;
    auto endpoint = [&](const EndpointRequirements& required, const StreamConfiguration& before,
                        StreamConfiguration& after, const char* location) {
        if (access_policy(required, original.exclusive_mode_preferred) !=
            AccessPolicy::ExclusivePreferred)
            return;
        if (before.access_attempts.empty() || after.access_attempts.empty()) {
            issues.push_back({ErrorCode::ContractViolation, std::string(location) + "/mode",
                              "Startup fallback requires auditable endpoint open receipts."});
            return;
        }
        auto exclusive = before.access_attempts.front(); // Previous receipt was validated.
        if (before.mode == AccessMode::Exclusive) {
            exclusive.succeeded = false;
            auto issue = startup_failure;
            issue.location = std::string(location) + "/" + issue.location;
            exclusive.issues.push_back(std::move(issue));
        }
        const auto shared = after.access_attempts.back(); // Concrete retry was validated.
        after.access_policy = AccessPolicy::ExclusivePreferred;
        after.requested_mode = required.mode;
        after.exclusive_mode_preferred = original.exclusive_mode_preferred;
        after.access_fell_back = true;
        after.access_attempts = {std::move(exclusive), shared};
    };
    endpoint(original.source, previous.source, current.source, "source");
    endpoint(original.destination, previous.destination, current.destination, "destination");
    if (!issues.empty())
        return {std::nullopt, std::move(issues)};
    return check_negotiated(original, current);
}

Result<StreamConfiguration> negotiate_access(const EndpointRequirements& required,
                                             bool prefer_exclusive, std::stop_token stop,
                                             std::string_view location,
                                             const StreamOpenAttempt& open,
                                             const StreamCloseAttempt& close,
                                             const std::optional<Issue>& known_exclusive_conflict) {
    if (required.mode && *required.mode != AccessMode::Shared &&
        *required.mode != AccessMode::Exclusive)
        return {std::nullopt,
                {{ErrorCode::InvalidConfiguration, std::string(location) + "/mode",
                  "Invalid requested access mode."}}};
    const auto policy = access_policy(required, prefer_exclusive);
    const bool may_fallback = policy == AccessPolicy::ExclusivePreferred;
    const AccessMode first = policy == AccessPolicy::ExclusiveRequired || may_fallback
                                 ? AccessMode::Exclusive
                                 : AccessMode::Shared;
    const std::array<AccessMode, 2> modes{first, AccessMode::Shared};
    std::vector<AccessAttempt> attempts;
    std::vector<Issue> failures;
    for (std::size_t i = 0; i < (may_fallback ? 2u : 1u); ++i) {
        if (stop.stop_requested()) {
            close();
            return {std::nullopt,
                    {{ErrorCode::Cancelled, std::string(location),
                      "Endpoint acquisition cancelled before another mode attempt."}}};
        }
        auto concrete = required;
        concrete.mode = modes[i]; // Every other explicit requirement is unchanged.
        const bool blocked = modes[i] == AccessMode::Exclusive && known_exclusive_conflict;
        auto opened = blocked
                          ? Result<StreamConfiguration>{std::nullopt, {*known_exclusive_conflict}}
                          : open(concrete);
        if (opened.ok())
            opened = check_stream(concrete, *opened.value, location);
        if (stop.stop_requested()) {
            close();
            return {std::nullopt,
                    {{ErrorCode::Cancelled, std::string(location),
                      "Endpoint acquisition cancelled; opened resources released."}}};
        }
        if (opened.ok()) {
            attempts.push_back({modes[i], true, !blocked, {}});
            auto result = std::move(*opened.value);
            result.access_policy = policy;
            result.requested_mode = required.mode;
            result.exclusive_mode_preferred = prefer_exclusive;
            result.access_fell_back = i != 0;
            result.access_attempts = std::move(attempts);
            return {std::move(result), {}};
        }
        if (opened.issues.empty())
            opened.issues.push_back({ErrorCode::ContractViolation, std::string(location),
                                     "Backend failed acquisition without an explanation."});
        attempts.push_back({modes[i], false, !blocked, opened.issues});
        close(); // Failed Exclusive must release all ownership BEFORE Shared.
        bool cancelled = false;
        for (auto issue : opened.issues) {
            cancelled |= issue.code == ErrorCode::Cancelled;
            issue.location =
                std::string(location) + "/access/" + name(modes[i]) + "/" + issue.location;
            issue.detail = std::string(name(modes[i])) + " acquisition failed: " + issue.detail;
            failures.push_back(std::move(issue));
        }
        if (cancelled)
            break; // Cancellation never becomes a fallback request.
    }
    return {std::nullopt, std::move(failures)};
}

} // namespace audiohole::engine::detail
