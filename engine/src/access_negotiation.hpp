#pragma once

#include "audiohole/engine/backend.hpp"
#include <functional>

namespace audiohole::engine::detail {

// Non-real-time endpoint acquisition shared by both real Windows adapters.
// open() must leave owned resources stopped. close() is idempotent and releases
// them on the caller's owning lifecycle/COM thread, including a partially failed
// open. No active stream is retried or reconfigured by this helper.
using StreamOpenAttempt = std::function<Result<StreamConfiguration>(const EndpointRequirements&)>;
using StreamCloseAttempt = std::function<void()>;
Result<StreamConfiguration>
negotiate_access(const EndpointRequirements&, bool prefer_exclusive, std::stop_token,
                 std::string_view location, const StreamOpenAttempt&, const StreamCloseAttempt&,
                 const std::optional<Issue>& known_exclusive_conflict = std::nullopt);

// One supervisor-owned retry is allowed before the FIRST transferred frame.
// Reopen the whole stopped route, changing only unforced preferred endpoints to
// Shared. Endpoints already Shared stay Shared; forced modes and every other
// explicit requirement are retained. No eligible Exclusive endpoint means no
// retry. This helper neither touches devices nor changes saved profile intent.
std::optional<RouteRequest> startup_shared_retry(const RouteRequest&, const NegotiatedRoute&);

// After validating the retry against its concrete requirements, restore the
// original intent in its receipt. A previously accepted Exclusive acquisition
// becomes a failed startup attempt with its diagnostic; Shared success follows.
// Existing open-time fallback reasons survive. Missing receipts are rejected,
// and the result is checked against original intent. The caller must stop old
// resources BEFORE reopening; this helper cannot establish native ownership.
Result<NegotiatedRoute> record_startup_fallback(const RouteRequest&,
                                                const NegotiatedRoute& previous,
                                                NegotiatedRoute& current,
                                                const Issue& startup_failure);

} // namespace audiohole::engine::detail
