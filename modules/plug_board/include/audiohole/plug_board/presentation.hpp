#pragma once

#include "audiohole/plug_board/module.hpp"

namespace audiohole::plug_board {

// Compact, complete row status. Negative ages/elapsed times clamp to zero;
// addition saturates at the four-week display ceiling before it can overflow.
// Whole days/weeks deliberately omit smaller units. No timestamps are changed.
std::string last_seen_text(std::int64_t seconds, std::int64_t elapsed_seconds = 0);

enum class PlacementBasis { SavedAssignment, ObservedOutput, SystemFallback };
struct ApplicationPlacement {
    std::string socket_id = "system";
    PlacementBasis basis = PlacementBasis::SystemFallback;
};

// Placement is a read-only projection, never a policy command or saved edit.
// Explicit assignments win. An unassigned app with one fresh observed active
// output appears in the unique custom socket bound to that output. Missing,
// ambiguous, stale and multi-output observations fall back to System once.
// Partial discovery may place known playback, but never proves route success.
ApplicationPlacement application_placement(const BoardConfiguration&, const ApplicationView&,
                                           const EndpointInventory&);
const char* placement_basis_name(PlacementBasis);

} // namespace audiohole::plug_board
