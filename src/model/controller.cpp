#include "model/controller.h"
#include <algorithm>
#include <array>

namespace audiohole {
namespace {
// Logical fixture timing, chosen for observable UI transitions, not audio latency.
constexpr int transition_duration_ms = 200;
constexpr int stop_duration_ms = 100;
constexpr int beep_duration_ms = 1000;
constexpr int scan_ms_per_percent = 30;
constexpr std::size_t maximum_diagnostic_events = 500;
} // namespace

const char* name(State s) {
    constexpr std::array names{"Stopped",  "Ready",      "Starting", "Running",
                               "Degraded", "Recovering", "Failed",   "Stopping"};
    return names.at(static_cast<std::size_t>(s));
}
const char* name(Availability a) {
    constexpr std::array names{"Available", "Unavailable", "Ambiguous identity",
                               "Known, not tested", "Unknown"};
    return names.at(static_cast<std::size_t>(a));
}
std::string Format::label() const {
    return std::to_string(rate) + " Hz / " + std::to_string(channels) + " ch / " + representation;
}
const Endpoint* endpoint(const Snapshot& snapshot, const std::string& id) {
    auto found = std::find_if(snapshot.endpoints.begin(), snapshot.endpoints.end(),
                              [&](const auto& device) { return device.id == id; });
    return found == snapshot.endpoints.end() ? nullptr : &*found;
}
const Profile* profile(const Snapshot& snapshot, const std::string& id) {
    auto found = std::find_if(snapshot.profiles.begin(), snapshot.profiles.end(),
                              [&](const auto& saved_profile) { return saved_profile.id == id; });
    return found == snapshot.profiles.end() ? nullptr : &*found;
}
Recommendation recommend(const Snapshot& snapshot, const Direction& direction) {
    Recommendation recommendation;
    const auto* source = endpoint(snapshot, direction.source);
    const auto* destination = endpoint(snapshot, direction.destination);
    if (!source || !destination) {
        recommendation.explanation =
            "Conservative 48000 Hz / 2 ch / float32 placeholder. Select both "
            "endpoints; compatibility is unverified.";
        return recommendation;
    }
    for (const auto& format : source->formats)
        if (std::find(destination->formats.begin(), destination->formats.end(), format) !=
            destination->formats.end())
            recommendation.compatible.push_back(format);
    recommendation.explanation =
        recommendation.compatible.empty()
            ? "No common format in the source and destination capability lists. Select another "
              "pair."
            : "Both endpoints list this format; 48 kHz is preferred when available to avoid "
              "unnecessary format conversion. A common format does not establish a shared clock.";
    auto preferred =
        std::find(recommendation.compatible.begin(), recommendation.compatible.end(), Format{});
    if (preferred != recommendation.compatible.end())
        std::rotate(recommendation.compatible.begin(), preferred, preferred + 1);
    return recommendation;
}
Result validate(const Snapshot& snapshot, const Profile& candidate) {
    if (candidate.display_name.empty())
        return {false, "Enter a profile display name."};
    std::size_t enabled = 0;
    std::vector<std::string> ids;
    for (const auto& hole : candidate.holes) {
        if (hole.id.empty() || std::find(ids.begin(), ids.end(), hole.id) != ids.end())
            return {false, "Hole IDs must be nonempty and unique."};
        ids.push_back(hole.id);
        if (!hole.enabled)
            continue;
        ++enabled;
        for (const Direction* direction : {&hole.forward, hole.duplex ? &hole.reverse : nullptr}) {
            if (!direction)
                continue;
            const auto* source = endpoint(snapshot, direction->source);
            const auto* destination = endpoint(snapshot, direction->destination);
            if (!source || !destination || !source->capture || destination->capture)
                return {false, "Select a saved capture source and playback destination for each "
                               "enabled direction."};
            if (direction->settings.buffer_ms < 1 || direction->settings.buffer_ms > 100)
                return {false, "Buffer must be between 1 and 100 ms."};
            const auto recommendation = recommend(snapshot, *direction);
            if (recommendation.compatible.empty())
                return {false,
                        "No common endpoint format is available. Select another endpoint pair."};
            if (!direction->settings.automatic &&
                std::find(recommendation.compatible.begin(), recommendation.compatible.end(),
                          direction->settings.format) == recommendation.compatible.end())
                return {false, "Manual format is unsupported by this endpoint pair; "
                               "change the override explicitly."};
        }
    }
    if (enabled > 8)
        return {false, "At most eight enabled holes may be armed, including waiting holes."};
    return {true, enabled > 4 ? "Extended range: five to eight enabled holes; actual performance "
                                "is hardware-dependent."
                              : "Profile is valid."};
}
// Detail strings preview the production UI. This class still advances authored
// fixtures only; the startup notice and exported diagnostics disclose that context.
// Never interpret these strings or transitions as evidence of real stream operation.
Result Controller::update_buffer(const std::string& profile_id, const std::string& hole_id,
                                 bool reverse, int milliseconds) {
    if (milliseconds < 1 || milliseconds > 100)
        return {false, "Buffer must be between 1 and 100 milliseconds."};
    const auto* saved = profile(snapshot(), profile_id);
    if (!saved)
        return {false, "Save or select a profile before applying its buffer preference."};
    auto edited = *saved;
    const auto hole = std::find_if(edited.holes.begin(), edited.holes.end(),
                                   [&](const auto& value) { return value.id == hole_id; });
    if (hole == edited.holes.end() || (reverse && !hole->duplex))
        return {false, "Save this direction before applying its buffer preference."};
    (reverse ? hole->reverse : hole->forward).settings.buffer_ms = milliseconds;
    const auto result = save(std::move(edited), false);
    return result.ok ? Result{true, "Buffer preference saved. No live engine buffer was applied."}
                     : result;
}
SimulatedController::SimulatedController() {
    state_.capabilities.live_buffer = true;
    const std::vector<Format> common{{}, {96000, 2, "float32"}, {48000, 2, "PCM24"}};
    state_.endpoints = {{"mixer-in", "Mixer microphone", "USB Audio Interface (Capture)",
                         "USB mixer", true, Availability::Available, common},
                        {"mixer-out", "Mixer headphones", "USB Audio Interface (Render)",
                         "Independent clock is not ruled out", false, Availability::Available,
                         common},
                        {"vr-in",
                         "VR microphone",
                         "Headset Microphone",
                         "VR headset",
                         true,
                         Availability::Available,
                         {{}}},
                        {"vr-out",
                         "VR headphones",
                         "Headset Speakers",
                         "VR headset",
                         false,
                         Availability::Available,
                         {{}}},
                        {"legacy-out",
                         "Desktop speakers",
                         "Speakers",
                         "Conversion scenario",
                         false,
                         Availability::Available,
                         {{44100, 2, "PCM16"}}},
                        {"missing-in",
                         "Travel microphone",
                         "USB Microphone",
                         "Saved device is disconnected",
                         true,
                         Availability::Unavailable,
                         {{}}},
                        {"twin-in",
                         "USB microphone (identity A)",
                         "USB Microphone",
                         "Serial absent; never auto-bound",
                         true,
                         Availability::Ambiguous,
                         {{}}},
                        {"untested-out",
                         "Untested output",
                         "USB Speakers",
                         "No capability evidence",
                         false,
                         Availability::Untested,
                         {}}};
    auto simple = make_hole();
    simple.display_name = "Microphone to headset";
    simple.forward.source = "mixer-in";
    simple.forward.destination = "vr-out";
    auto duplex = make_hole();
    duplex.display_name = "VR duplex cable";
    duplex.duplex = true;
    duplex.forward = simple.forward;
    duplex.reverse.source = "vr-in";
    duplex.reverse.destination = "mixer-out";
    state_.profiles = {
        {"profile-simplex",
         "Microphone monitoring",
         "Microphone monitoring through the headset.",
         {simple}},
        {"profile-duplex", "VR duplex", "Two independently configured directions.", {duplex}},
        {"profile-empty", "Empty workspace", "Create your own connections.", {}}};
    event("ready", "Ready to configure routing.");
}
void SimulatedController::event(std::string type, std::string detail) {
    const auto severity = type == "error" || type == "scan-failed" ? Severity::Error
                          : type == "recovering"                   ? Severity::Warning
                                                                   : Severity::Info;
    state_.events.push_back({++sequence_, std::move(type), std::move(detail), severity});
    if (state_.events.size() > maximum_diagnostic_events)
        state_.events.erase(state_.events.begin());
    ++state_.revision;
}
Hole SimulatedController::make_hole() {
    Hole hole;
    hole.id = "hole-" + std::to_string(next_id_++);
    hole.display_name = "New hole";
    return hole;
}
std::string SimulatedController::create_profile(const std::string& title, const std::string& from) {
    Profile created;
    if (const auto* original = profile(state_, from))
        created = *original;
    created.id = "profile-" + std::to_string(next_id_++);
    created.display_name = title.empty() ? "New profile" : title;
    for (auto& hole : created.holes)
        hole.id = make_hole().id;
    state_.profiles.push_back(created);
    event("profile-created", "Created in-memory profile: " + created.display_name);
    return created.id;
}
Result SimulatedController::remove_profile(const std::string& id) {
    if ((state_.armed && id == state_.active_profile) || id == state_.pending_profile)
        return {false, "Stop routing and cancel pending activation before deleting this profile."};
    auto found = std::find_if(state_.profiles.begin(), state_.profiles.end(),
                              [&](const auto& saved_profile) { return saved_profile.id == id; });
    if (found == state_.profiles.end())
        return {false, "Profile no longer exists."};
    if (id == state_.active_profile)
        state_.active_profile.clear();
    state_.profiles.erase(found);
    event("profile-deleted", "Profile removed from session.");
    return {true, "Profile deleted."};
}
Result SimulatedController::save(Profile draft, bool apply) {
    auto found = std::find_if(state_.profiles.begin(), state_.profiles.end(),
                              [&](const auto& saved) { return saved.id == draft.id; });
    if (found == state_.profiles.end())
        return {false, "Profile no longer exists."};
    if (draft.display_name.empty())
        return {false, "Enter a profile display name."};
    if (apply) {
        const auto result = validate(state_, draft);
        if (!result.ok)
            return result;
    }
    *found = draft;
    event("saved", "Edits saved in memory only; persistent formats are not implemented.");
    if (apply)
        return activate(draft.id);
    return {
        true,
        "Saved for this session. Active routing settings are unchanged until Apply or Activate."};
}
// Prepare/validate synchronously before changing live_. Real transactional stream
// preparation will need asynchronous generations; this fixture has no queued work.
Result SimulatedController::activate(const std::string& id, bool arm_waiting) {
    const auto* candidate = profile(state_, id);
    if (!candidate)
        return {false, "Profile no longer exists."};
    auto result = validate(state_, *candidate);
    if (!result.ok)
        return result;
    bool available = true;
    for (const auto& hole : candidate->holes)
        if (hole.enabled)
            for (const Direction* direction :
                 {&hole.forward, hole.duplex ? &hole.reverse : nullptr})
                if (direction)
                    available = available &&
                                endpoint(state_, direction->source)->availability ==
                                    Availability::Available &&
                                endpoint(state_, direction->destination)->availability ==
                                    Availability::Available;
    if (!available && state_.armed && !arm_waiting)
        return {false, "Required resources unavailable. Existing route retained. Use Arm "
                       "Ready explicitly to replace it."};
    live_ = *candidate;
    state_.active_profile = id;
    state_.pending_profile.clear();
    state_.pending_ms = 0;
    failures_.clear();
    cancel_beep();
    state_.monitoring = false;
    state_.armed = true;
    state_.state = State::Starting;
    state_.paths.clear(); // A newly activated plan owns fresh simulated path lifetimes.
    transition_ms_ = transition_duration_ms;
    refresh_paths();
    event("activated", "Activated profile: " + candidate->display_name);
    return result;
}
Result SimulatedController::update_buffer(const std::string& profile_id, const std::string& hole_id,
                                          bool reverse, int milliseconds) {
    const auto saved = Controller::update_buffer(profile_id, hole_id, reverse, milliseconds);
    if (!saved.ok)
        return saved;
    if (!state_.armed || live_.id != profile_id)
        return saved;
    const auto hole = std::find_if(live_.holes.begin(), live_.holes.end(),
                                   [&](const auto& value) { return value.id == hole_id; });
    if (hole == live_.holes.end() || (reverse && !hole->duplex))
        return saved;
    (reverse ? hole->reverse : hole->forward).settings.buffer_ms = milliseconds;
    refresh_paths();
    const auto path =
        std::find_if(state_.paths.begin(), state_.paths.end(), [&](const auto& value) {
            return value.hole_id == hole_id && value.reverse == reverse;
        });
    const bool confirmed = path != state_.paths.end() && path->applied_buffer_ms == milliseconds;
    const auto detail =
        confirmed
            ? "Buffer preference saved; active Preview direction confirmed " +
                  std::to_string(milliseconds) + " ms (simulated)."
            : "Buffer preference saved; Preview request updated, awaiting a running direction.";
    event("buffer-updated", detail);
    return {true, detail};
}
void SimulatedController::stop() {
    state_.armed = false;
    state_.pending_profile.clear();
    state_.pending_ms = 0;
    scan(false);
    cancel_beep();
    state_.monitoring = false;
    failures_.clear();
    state_.state = State::Stopping;
    transition_ms_ = stop_duration_ms;
    for (auto& p : state_.paths)
        p.state = State::Stopping;
    event("stopped", "Stop cancels startup, recovery, scan, monitoring, and Test Beep.");
}
Result SimulatedController::schedule(const std::string& id, int ms) {
    if (!profile(state_, id) || ms < 0)
        return {false, "Select an existing startup profile and a nonnegative delay."};
    state_.pending_profile = id;
    state_.pending_ms = ms;
    event("pending", "Startup activation scheduled.");
    return {true, "Startup scheduled."};
}
// Rebuilds each direction independently. Aggregate health never hides a healthy
// duplex leg; unavailable resources wait in Ready, explicit injected errors fail.
void SimulatedController::refresh_paths(bool reconnecting) {
    const auto previous_paths = state_.paths;
    state_.paths.clear();
    if (!state_.armed)
        return;
    int running = 0, missing = 0, failed = 0;
    for (const auto& hole : live_.holes)
        if (hole.enabled) {
            for (int index = 0; index < (hole.duplex ? 2 : 1); ++index) {
                const auto& direction = index == 0 ? hole.forward : hole.reverse;
                const auto* source = endpoint(state_, direction.source);
                const auto* destination = endpoint(state_, direction.destination);
                PathStatus path{hole.id, index == 1, State::Running,
                                "Source and destination connected."};
                const auto key = hole.id + (index == 1 ? "/reverse" : "/forward");
                if (std::find(failures_.begin(), failures_.end(), key) != failures_.end()) {
                    path.state = State::Failed;
                    path.detail = "Stream failed. Use Recover to retry this direction.";
                    ++failed;
                } else if (!source || !destination ||
                           source->availability != Availability::Available ||
                           destination->availability != Availability::Available) {
                    path.state = State::Ready;
                    path.detail = "Waiting for available, unambiguous saved endpoints.";
                    ++missing;
                } else {
                    const auto previous = std::find_if(
                        previous_paths.begin(), previous_paths.end(), [&](const auto& item) {
                            return item.hole_id == path.hole_id && item.reverse == path.reverse;
                        });
                    const bool already_running =
                        previous != previous_paths.end() && previous->state == State::Running;
                    // A sibling direction's recovery must not restart an already healthy leg.
                    path.state = already_running      ? State::Running
                                 : reconnecting       ? State::Recovering
                                 : transition_ms_ > 0 ? State::Starting
                                                      : State::Running;
                    ++running;
                }
                path.requested_buffer_ms = direction.settings.buffer_ms;
                if (path.state == State::Running)
                    path.applied_buffer_ms = direction.settings.buffer_ms;
                state_.paths.push_back(path);
            }
        }
    if (state_.paths.empty())
        state_.state = State::Ready;
    else if (running && (missing || failed))
        state_.state = State::Degraded;
    else if (failed)
        state_.state = State::Failed;
    else if (missing)
        state_.state = State::Ready;
    else
        state_.state = reconnecting         ? State::Recovering
                       : transition_ms_ > 0 ? State::Starting
                                            : State::Running;
}
void SimulatedController::advance(int ms) {
    if (ms <= 0)
        return;
    if (!state_.pending_profile.empty()) {
        state_.pending_ms = std::max(0, state_.pending_ms - ms);
        if (!state_.pending_ms) {
            const auto id = state_.pending_profile;
            state_.pending_profile.clear();
            const auto r = activate(id);
            if (!r.ok)
                event("error", r.detail);
        }
    }
    if (transition_ms_ > 0) {
        transition_ms_ = std::max(0, transition_ms_ - ms);
        if (!transition_ms_) {
            if (!state_.armed) {
                state_.state = State::Stopped;
                state_.paths.clear();
            } else
                refresh_paths();
            event("state", std::string("Routing state: ") + name(state_.state));
        }
    }
    if (beep_ms_ > 0) {
        beep_ms_ = std::max(0, beep_ms_ - ms);
        if (!beep_ms_) {
            state_.beep_destination.clear();
            event("beep-complete", "Test Beep completed for the selected destination.");
        }
    }
    if (state_.scanning) {
        scan_ms_ += ms;
        state_.scan_percent = std::min(100, scan_ms_ / scan_ms_per_percent);
        if (state_.scan_percent == 100) {
            state_.scanning = false;
            event("scan-complete", "Device scan completed.");
        }
    }
}
Result SimulatedController::update_endpoint(const Endpoint& device) {
    auto found = std::find_if(state_.endpoints.begin(), state_.endpoints.end(),
                              [&](const auto& item) { return item.id == device.id; });
    if (found == state_.endpoints.end())
        return {false, "Unknown logical device."};
    if (device.display_name.empty())
        return {false, "Enter a device display name."};
    const bool changed = found->availability != device.availability;
    found->display_name = device.display_name;
    found->notes = device.notes;
    found->availability = device.availability;
    if (changed) {
        cancel_beep();
        if (state_.armed) {
            transition_ms_ = transition_duration_ms;
            refresh_paths(device.availability == Availability::Available);
        }
        event(device.availability == Availability::Available ? "reconnected" : "disconnected",
              device.display_name + ": " + name(device.availability));
    } else
        event("device-edited",
              "Updated display name and notes; Windows name and logical ID preserved.");
    return {true, "Device record updated in memory only."};
}
void SimulatedController::fail_path(const std::string& id, bool reverse) {
    if (!state_.armed)
        return;
    failures_.push_back(id + (reverse ? "/reverse" : "/forward"));
    refresh_paths();
    cancel_beep();
    event("error", "Injected directional failure. Healthy duplex direction is retained.");
}
void SimulatedController::recover() {
    failures_.clear();
    if (state_.armed) {
        transition_ms_ = transition_duration_ms;
        refresh_paths(true);
        event("recovering", "Recovery started.");
    }
}
void SimulatedController::monitor(bool enabled) {
    state_.monitoring = enabled;
    ++state_.revision;
}
Result SimulatedController::beep(const std::string& id) {
    const auto* destination = endpoint(state_, id);
    if (!destination || destination->capture ||
        destination->availability != Availability::Available)
        return {false, "Select an available, unambiguous playback destination. No endpoint "
                       "substitution will occur."};
    for (const auto& hole : live_.holes)
        if (state_.armed && hole.enabled)
            for (const Direction* direction :
                 {&hole.forward, hole.duplex ? &hole.reverse : nullptr})
                if (direction && direction->destination == id &&
                    direction->settings.mode == "Exclusive")
                    return {false, "Selected destination is occupied in exclusive mode. "
                                   "Live routing will not be disturbed."};
    state_.beep_destination = id;
    beep_ms_ = beep_duration_ms;
    event("beep-start", "Test Beep target: " + destination->display_name +
                            ". Test signal: approximately 1 kHz, -30 "
                            "dBFS, 1 second with fades.");
    return {true, "Test Beep active for the selected destination."};
}
void SimulatedController::clear_diagnostics() {
    state_.events.clear();
    ++state_.revision;
    // Do not reset sequence_ or emit a replacement event: clearing must leave an
    // empty log, and future events must remain newer than consumers' cursors.
}
void SimulatedController::cancel_beep() {
    beep_ms_ = 0;
    state_.beep_destination.clear();
}
void SimulatedController::scan(bool enabled, bool advanced, ScanScope scope,
                               const std::string& selected_device) {
    if (enabled && scope == ScanScope::Selected && !endpoint(state_, selected_device)) {
        event("scan-failed", "Select a saved device on the Devices page before a selected-device "
                             "scan. No scan started.");
        return;
    }
    if (!enabled && state_.scanning)
        event("scan-cancelled", "Scan cancelled.");
    state_.scanning = enabled;
    scan_ms_ = 0;
    state_.scan_percent = 0;
    if (enabled) {
        const std::string scope_detail = scope == ScanScope::Full ? "Full device scan. "
                                         : scope == ScanScope::Selected
                                             ? "Selected logical device: " + selected_device + ". "
                                             : "Update saved device records. ";
        event("scan-scope", scope_detail);
        event("scan-start",
              advanced ? "Advanced device scan started." : "Easy device scan started.");
    }
}
} // namespace audiohole
