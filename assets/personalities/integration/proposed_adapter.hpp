#pragma once
// Proposed personality-presentation mapping only. This header uses the real
// inspected application types; it never calls a controller or generates copy.
// The primary application agent must review/apply it at the existing GUI seam.
#include "model/controller.h"
#include "presentation/messages.h"
#include <optional>
#include <string>

namespace audiohole::avatar::integration {
struct Baseline {
    std::string state;
    int priority;
};
inline Baseline baseline(audiohole::State state, bool scanning) {
    using State = audiohole::State;
    if (state == State::Failed)
        return {"error", 100};
    if (state == State::Degraded)
        return {"concerned", 70};
    if (state == State::Recovering)
        return {"working", 60};
    if (state == State::Starting)
        return {"working", 30};
    if (state == State::Stopping)
        return {"working", 25};
    if (scanning)
        return {"working", 25};
    // Stopped, Ready and healthy Running are ambient operation.
    return {"idle", 0};
}
struct VisualReaction {
    std::string state;
    int priority;
};
inline std::optional<VisualReaction> reaction(presentation::Message event,
                                              const std::string& personality) {
    using Message = presentation::Message;
    switch (event) {
    case Message::Disconnected:
        return VisualReaction{personality == "uwu"       ? "panicking"
                              : personality == "default" ? "angry"
                                                         : "error",
                              80};
    case Message::Invalid:
    case Message::ScanFailed:
        return VisualReaction{"error", 90};
    case Message::ScanCompleted:
        return VisualReaction{personality == "uwu" ? "celebrating" : "happy", 35};
    case Message::Created:
    case Message::Activated:
    case Message::Reconnected:
        return VisualReaction{"happy", 30};
    default:
        return std::nullopt;
    }
}
// A GUI poll can observe a disconnection and its recovery in one retained event
// batch. Do not replay panic after the current authoritative state is healthy,
// or celebrate a completed scan over a newer scan that is already running.
inline std::optional<VisualReaction> reaction_for_snapshot(presentation::Message event,
                                                           const std::string& personality,
                                                           audiohole::State state, bool scanning) {
    if (event == presentation::Message::Disconnected && state != audiohole::State::Degraded &&
        state != audiohole::State::Failed)
        return std::nullopt;
    if (event == presentation::Message::ScanCompleted && scanning)
        return std::nullopt;
    return reaction(event, personality);
}
} // namespace audiohole::avatar::integration
