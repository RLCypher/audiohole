#include "audiohole/plug_board/presentation.hpp"
#include <algorithm>

namespace audiohole::plug_board {

std::string last_seen_text(std::int64_t seconds, std::int64_t elapsed_seconds) {
    constexpr std::int64_t minute = 60, hour = 60 * minute, day = 24 * hour, week = 7 * day;
    constexpr auto ceiling = 4 * week + 1;
    const auto age = std::min(ceiling, std::clamp(seconds, std::int64_t(0), ceiling) +
                                           std::clamp(elapsed_seconds, std::int64_t(0), ceiling));
    std::string relative;
    if (age > 4 * week)
        relative = "a while ago";
    else if (age >= week)
        relative = std::to_string(age / week) + "w ago";
    else if (age >= day)
        relative = std::to_string(age / day) + "d ago";
    else if (age >= hour) {
        relative = std::to_string(age / hour) + "h";
        if (const auto minutes = age % hour / minute; minutes != 0)
            relative += " " + std::to_string(minutes) + "m";
        relative += " ago";
    } else if (age >= minute)
        relative = std::to_string(age / minute) + "m ago";
    else
        relative = "just now";
    return "Last seen: " + relative;
}

ApplicationPlacement application_placement(const BoardConfiguration& board,
                                           const ApplicationView& app,
                                           const EndpointInventory& inventory) {
    if (const auto saved = board.application_assignments.find(app.id);
        saved != board.application_assignments.end())
        return {saved->second, PlacementBasis::SavedAssignment};

    if (app.activity != Activity::Active || !app.observation_fresh || !app.observation ||
        app.observation->active_endpoint_ids.size() != 1)
        return {};

    const auto& actual = app.observation->active_endpoint_ids.front();
    std::string matching_socket;
    for (const auto& socket : board.sockets) {
        // System is the fallback category, not an additional competing match
        // whenever Windows Default happens to equal a custom socket's output.
        if (socket.id == "system" || !socket.endpoint.logical_device_id)
            continue;
        bool matches = false;
        for (const auto& output : inventory.endpoints)
            if (output.logical_id == *socket.endpoint.logical_device_id && output.playback &&
                output.available && !output.ambiguous && !output.windows_id.empty() &&
                output.windows_id == actual)
                matches = true;
        if (!matches)
            continue;
        if (!matching_socket.empty())
            return {}; // Two categories for one output have no inferred priority.
        matching_socket = socket.id;
    }
    return matching_socket.empty()
               ? ApplicationPlacement{}
               : ApplicationPlacement{matching_socket, PlacementBasis::ObservedOutput};
}

const char* placement_basis_name(PlacementBasis basis) {
    switch (basis) {
    case PlacementBasis::SavedAssignment:
        return "saved_assignment";
    case PlacementBasis::ObservedOutput:
        return "observed_output";
    case PlacementBasis::SystemFallback:
        return "system_fallback";
    }
    return "unknown";
}

} // namespace audiohole::plug_board
