#include "model/controller.h"
#include "model/tutorial.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace audiohole {
namespace {
// Logical fixture timing, chosen for observable UI transitions, not audio latency.
constexpr int transition_duration_ms = 200;
constexpr int tutorial_activation_duration_ms = 8000;
constexpr int stop_duration_ms = 100;
constexpr int beep_duration_ms = 1000;
constexpr int scan_ms_per_percent = 30;
constexpr int simulated_automatic_buffer_ms = 6; // Fixture choice, never a hardware recommendation.
constexpr std::size_t maximum_diagnostic_events = 500;
bool manual_settings(const Direction& direction) {
    const auto& preference = direction.settings.preferences;
    // Reserved clock metadata is saved intent, not an applied audio override.
    return preference.common != EndpointPreferences{} ||
           preference.source != EndpointPreferences{} ||
           preference.destination != EndpointPreferences{} || !preference.channel_map.empty() ||
           preference.queue_target_ms.has_value();
}
bool supports(const Endpoint& device, const Format& format, const std::string& mode) {
    bool supported = false;
    for (const auto& capability : device.capabilities) {
        if (capability.mode != mode || capability.format != format)
            continue;
        if (capability.support == FormatSupport::Unsupported)
            return false; // Conflicting observations need a rescan, not a guess.
        supported = supported || capability.support == FormatSupport::Supported;
    }
    return supported;
}
} // namespace

EndpointPreferences inherit_preferences(const EndpointPreferences& common,
                                        const EndpointPreferences& endpoint) {
    auto result = common;
    if (endpoint.sample_rate_hz)
        result.sample_rate_hz = endpoint.sample_rate_hz;
    if (endpoint.channel_count)
        result.channel_count = endpoint.channel_count;
    if (endpoint.sample_format)
        result.sample_format = endpoint.sample_format;
    if (endpoint.mode)
        result.mode = endpoint.mode;
    if (endpoint.buffer_ms)
        result.buffer_ms = endpoint.buffer_ms;
    if (endpoint.period_ms)
        result.period_ms = endpoint.period_ms;
    if (endpoint.render_ahead_ms)
        result.render_ahead_ms = endpoint.render_ahead_ms;
    return result;
}
void Settings::refresh_projection() {
    buffer_ms = preferences.common.buffer_ms.value_or(0);
    mode = preferences.common.mode.value_or("Automatic");
    const auto explicit_format = derive_pcm_format(preferences.common);
    automatic = !explicit_format;
    format = explicit_format.value_or(Format{});
}
void Settings::set_buffer_preference(int milliseconds) {
    if (milliseconds < 0 || milliseconds > 100)
        throw std::invalid_argument(
            "Buffer must be Automatic (0) or between 1 and 100 milliseconds.");
    preferences.common.buffer_ms = milliseconds ? std::optional<int>(milliseconds) : std::nullopt;
    refresh_projection();
}
std::string unique_name(const std::string& base, const std::vector<std::string>& used) {
    auto free = [&](const std::string& name) {
        return std::none_of(used.begin(), used.end(), [&](const auto& other) {
            if (name.size() != other.size())
                return false;
            return std::equal(name.begin(), name.end(), other.begin(),
                              [](unsigned char a, unsigned char b) {
                                  return std::tolower(a) == std::tolower(b);
                              });
        });
    };
    if (free(base))
        return base;
    for (unsigned i = 1;; ++i) {
        auto next = base + " (" + std::to_string(i) + ")";
        if (free(next))
            return next;
    }
}
std::optional<Format> derive_pcm_format(const EndpointPreferences& p) {
    if (!p.sample_rate_hz || !p.channel_count || !p.sample_format)
        return {};
    const auto rate = *p.sample_rate_hz, channels = *p.channel_count;
    if (rate < 1 || rate > 768000 || channels < 1 || channels > 256)
        return {};
    int container = 32, valid = 32;
    bool floating = *p.sample_format == "float32";
    if (*p.sample_format == "pcm_s16")
        container = valid = 16;
    else if (*p.sample_format == "pcm_s24_packed")
        container = valid = 24;
    else if (*p.sample_format == "pcm_s24_in_32")
        valid = 24;
    else if (*p.sample_format != "pcm_s32" && !floating)
        return {};
    const int alignment = channels * (container / 8);
    return Format{rate, channels, *p.sample_format,
                  WaveFormat{container,
                             valid,
                             65534,
                             {},
                             floating ? "{00000003-0000-0010-8000-00AA00389B71}"
                                      : "{00000001-0000-0010-8000-00AA00389B71}",
                             alignment,
                             rate * alignment}};
}
Result validate_preferences(const Preferences& p) {
    const auto valid_time = [](const std::optional<double>& n) {
        return !n || (std::isfinite(*n) && *n > 0);
    };
    if (!valid_time(p.queue_target_ms) || p.common.render_ahead_ms || p.source.render_ahead_ms)
        return {false, "Invalid routing queue or misplaced playback queue preference."};
    if (p.preferred_clock_device != "automatic" && p.preferred_clock_device != "capture" &&
        p.preferred_clock_device != "playback")
        return {false, "Choose Automatic, Capture or Playback as the preferred clock device."};
    for (const auto* e : {&p.common, &p.source, &p.destination}) {
        if (!valid_time(e->period_ms) || !valid_time(e->render_ahead_ms))
            return {false,
                    "Timing values must be positive finite milliseconds, or Automatic/Inherit."};
        if ((e->sample_rate_hz && (*e->sample_rate_hz < 1 || *e->sample_rate_hz > 768000)) ||
            (e->channel_count && (*e->channel_count < 1 || *e->channel_count > 256)) ||
            (e->buffer_ms && (*e->buffer_ms < 1 || *e->buffer_ms > 100)))
            return {false, "Invalid rate, channel count or buffer preference."};
        if (e->sample_format && *e->sample_format != "float32" && *e->sample_format != "pcm_s16" &&
            *e->sample_format != "pcm_s24_packed" && *e->sample_format != "pcm_s24_in_32" &&
            *e->sample_format != "pcm_s32")
            return {false, "Unknown sample format preference."};
        if (e->mode && *e->mode != "Shared" && *e->mode != "Exclusive")
            return {false, "Unknown access mode preference."};
    }
    if (p.channel_map.size() > 256)
        return {false, "Too many channel routes."};
    std::vector<int> destinations;
    const auto source_count =
        p.source.channel_count ? p.source.channel_count : p.common.channel_count;
    const auto destination_count =
        p.destination.channel_count ? p.destination.channel_count : p.common.channel_count;
    for (const auto& route : p.channel_map) {
        if (route.source < 0 || route.source >= 256 || route.destination < 0 ||
            route.destination >= 256 || (source_count && route.source >= *source_count) ||
            (destination_count && route.destination >= *destination_count) ||
            std::find(destinations.begin(), destinations.end(), route.destination) !=
                destinations.end())
            return {false, "Channel map indexes must fit the selected channel counts; each output "
                           "has one source."};
        destinations.push_back(route.destination);
    }
    return {true, "Preferences are valid."};
}
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
    auto result =
        std::to_string(rate) + " Hz / " + std::to_string(channels) + " ch / " + representation;
    if (wave)
        result += " / " + std::to_string(wave->valid_bits) + " valid bits in " +
                  std::to_string(wave->container_bits) + " / layout " +
                  (wave->channel_mask ? std::to_string(*wave->channel_mask) : "unknown");
    return result;
}
const Endpoint* endpoint(const Snapshot& snapshot, const std::string& id) {
    auto found = std::find_if(snapshot.endpoints.begin(), snapshot.endpoints.end(),
                              [&](const auto& device) { return device.id == id; });
    return found == snapshot.endpoints.end() ? nullptr : &*found;
}
std::vector<const Endpoint*> configuration_devices(const Snapshot& snapshot) {
    std::vector<const Endpoint*> devices;
    for (const auto& device : snapshot.endpoints)
        if (!device.hidden && (snapshot.operating_mode != OperatingMode::Test || device.inspection))
            devices.push_back(&device);
    return devices;
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
        recommendation.explanation = "Select a capture source and playback destination. No format "
                                     "is established until both devices have matching evidence.";
        return recommendation;
    }
    const auto source_requirements = inherit_preferences(direction.settings.preferences.common,
                                                         direction.settings.preferences.source);
    const auto destination_requirements = inherit_preferences(
        direction.settings.preferences.common, direction.settings.preferences.destination);
    if (source->inspection || destination->inspection || !source->capabilities.empty() ||
        !destination->capabilities.empty()) {
        for (const auto& candidate : source->capabilities) {
            if (candidate.support != FormatSupport::Supported ||
                (candidate.mode != "Shared" && candidate.mode != "Exclusive") ||
                (source_requirements.mode && candidate.mode != *source_requirements.mode) ||
                (destination_requirements.mode && candidate.mode != *destination_requirements.mode))
                continue;
            const bool supported = supports(*source, candidate.format, candidate.mode) &&
                                   supports(*destination, candidate.format, candidate.mode);
            if (supported &&
                std::find(recommendation.compatible.begin(), recommendation.compatible.end(),
                          candidate.format) == recommendation.compatible.end())
                recommendation.compatible.push_back(candidate.format);
        }
        recommendation.explanation =
            recommendation.compatible.empty()
                ? "No identical format is recorded for both endpoints. The engine must negotiate "
                  "each endpoint and perform any required conversion. Unknown support is not a "
                  "failed route. Channel maps use frame indexes; hardware bus intent is not "
                  "inferred."
                : "Exact Windows-reported tuples shared by both devices in the selected mode. "
                  "Automatic favors Shared, then 48 kHz float when listed. This is not a verified "
                  "route or measured "
                  "latency; unknown channel layouts remain unknown. Buffer is a requested "
                  "duration, "
                  "not a negotiated engine period.";
    } else {
        for (const auto& format : source->formats)
            if (std::find(destination->formats.begin(), destination->formats.end(), format) !=
                destination->formats.end())
                recommendation.compatible.push_back(format);
        recommendation.explanation =
            recommendation.compatible.empty()
                ? "No common format in the source and destination capability lists. Select another "
                  "pair."
                : "Both endpoints list this format; 48 kHz is preferred when available to avoid "
                  "unnecessary format conversion. A common format does not establish a shared "
                  "clock.";
    }
    auto matches = [](const Format& format, const EndpointPreferences& p) {
        const auto representation =
            format.representation == "PCM16" || format.representation == "int16"
                ? std::string("pcm_s16")
                : format.representation;
        return (!p.sample_rate_hz || format.rate == *p.sample_rate_hz) &&
               (!p.channel_count || format.channels == *p.channel_count) &&
               (!p.sample_format || representation == *p.sample_format);
    };
    const auto& preferences = direction.settings.preferences;
    const auto source_preferences = inherit_preferences(preferences.common, preferences.source);
    const auto destination_preferences =
        inherit_preferences(preferences.common, preferences.destination);
    std::erase_if(recommendation.compatible, [&](const auto& format) {
        return !matches(format, source_preferences) || !matches(format, destination_preferences);
    });
    auto rank = [&](const Format& format) {
        const bool prefer_shared = !preferences.common.mode &&
                                   supports(*source, format, "Shared") &&
                                   supports(*destination, format, "Shared");
        return (prefer_shared ? 0 : 2) +
               (format.rate == 48000 && format.representation == "float32" ? 0 : 1);
    };
    std::stable_sort(recommendation.compatible.begin(), recommendation.compatible.end(),
                     [&](const auto& a, const auto& b) { return rank(a) < rank(b); });
    return recommendation;
}
Direction effective_direction(const Hole& hole, bool reverse) {
    auto direction = reverse ? hole.reverse : hole.forward;
    if (!direction.advanced_tuning)
        direction.settings = Settings{};
    direction.settings.refresh_projection();
    return direction;
}
void describe_profile(const Snapshot& snapshot, Profile& candidate) {
    for (auto& hole : candidate.holes)
        for (auto* direction : {&hole.forward, &hole.reverse}) {
            if (direction->source.empty())
                direction->source_name.clear();
            if (direction->destination.empty())
                direction->destination_name.clear();
            if (const auto* device = endpoint(snapshot, direction->source))
                direction->source_name = device->display_name;
            if (const auto* device = endpoint(snapshot, direction->destination))
                direction->destination_name = device->display_name;
            direction->settings.refresh_projection();
        }
}
Result prepare_profile(const Snapshot& snapshot, const Profile& candidate,
                       PreparedProfile& output) {
    const auto valid = validate(snapshot, candidate);
    if (!valid.ok)
        return valid;
    PreparedProfile prepared;
    prepared.profile_id = candidate.id;
    for (const auto& hole : candidate.holes) {
        if (!hole.enabled)
            continue;
        for (bool reverse : {false, true}) {
            if (reverse && !hole.duplex)
                continue;
            const auto d = effective_direction(hole, reverse);
            const auto* source = endpoint(snapshot, d.source);
            const auto* destination = endpoint(snapshot, d.destination);
            if (!source || !destination || source->availability != Availability::Available ||
                destination->availability != Availability::Available)
                return {false, "A selected device is unavailable or its identity is unresolved."};
            const auto choices = recommend(snapshot, d);
            const auto format =
                choices.compatible.empty()
                    ? derive_pcm_format(d.settings.preferences.common).value_or(Format{})
                    : choices.compatible.front();
            const auto mode = d.settings.preferences.common.mode.value_or(
                d.exclusive_mode_preferred ? "Exclusive" : "Shared");
            RoutingPathRequest path;
            path.hole_id = hole.id;
            path.source = d.source;
            path.destination = d.destination;
            path.mode = mode;
            path.reverse = reverse;
            path.format = format;
            path.buffer_ms = d.settings.buffer_ms;
            prepared.paths.push_back(std::move(path));
            auto& request = prepared.paths.back();
            request.runtime_negotiation = snapshot.operating_mode == OperatingMode::Test;
            request.preferences = d.settings.preferences;
            request.channel_policy = request.preferences.channel_map.empty()
                                         ? "default_index_mapping"
                                         : "explicit_index_mapping";
            request.source_requirements =
                inherit_preferences(request.preferences.common, request.preferences.source);
            request.destination_requirements =
                inherit_preferences(request.preferences.common, request.preferences.destination);
            request.mode = mode;
            request.buffer_ms = request.preferences.common.buffer_ms.value_or(0);
            request.routing_engine = hole.routing_engine;
            request.exclusive_mode_preferred = d.exclusive_mode_preferred;
        }
    }
    output = std::move(prepared);
    return {true, "Routing requests prepared; runtime endpoint initialization is still required."};
}
bool endpoint_used_elsewhere(const Hole& hole, bool reverse, bool source, const std::string& id) {
    if (id.empty())
        return false;
    for (bool other_reverse : {false, true}) {
        if (other_reverse && !hole.duplex)
            continue;
        const auto& direction = other_reverse ? hole.reverse : hole.forward;
        for (bool other_source : {true, false}) {
            if (other_reverse == reverse && other_source == source)
                continue;
            if ((other_source ? direction.source : direction.destination) == id)
                return true;
        }
    }
    return false;
}
Result validate_endpoint_assignments(const Hole& hole) {
    for (bool reverse : {false, true}) {
        if (reverse && !hole.duplex)
            continue;
        const auto& d = reverse ? hole.reverse : hole.forward;
        if (endpoint_used_elsewhere(hole, reverse, true, d.source) ||
            endpoint_used_elsewhere(hole, reverse, false, d.destination))
            return {false, "An endpoint is assigned more than once in this hole. Clear one "
                           "assignment or select another endpoint."};
    }
    return {true, "Endpoint assignments are distinct."};
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
        const auto assignments = validate_endpoint_assignments(hole);
        if (!assignments.ok)
            return assignments;
        if (!hole.enabled)
            continue;
        ++enabled;
        if (hole.routing_engine != "automatic" && hole.routing_engine != "wasapi" &&
            hole.routing_engine != "miniaudio" && hole.routing_engine != "experimental")
            return {false, "The requested Routing Engine is unavailable in this build. Select "
                           "Automatic. Engine selection is independent of audio overrides."};
        for (const bool reverse : {false, true}) {
            if (reverse && !hole.duplex)
                continue;
            const auto effective = effective_direction(hole, reverse);
            const auto* direction = &effective;
            const auto* source = endpoint(snapshot, direction->source);
            const auto* destination = endpoint(snapshot, direction->destination);
            if (snapshot.operating_mode == OperatingMode::Test && !direction->source.empty() &&
                !direction->destination.empty() &&
                ((!source || source->capture) && (!destination || !destination->capture)) &&
                (!source || !destination)) {
                const auto result = validate_preferences(direction->settings.preferences);
                if (!result.ok)
                    return result;
                continue; // Portable references can be valid while this machine cannot resolve
                          // them.
            }
            if (!source || !destination || !source->capture || destination->capture)
                return {false, "Select a saved capture source and playback destination for each "
                               "enabled direction."};
            if (snapshot.operating_mode == OperatingMode::Test &&
                (!source->inspection || !destination->inspection))
                return {
                    false,
                    "Choose devices from the scanned inventory for Test Mode routing requests."};
            if (direction->settings.buffer_ms < 0 || direction->settings.buffer_ms > 100)
                return {false, "Buffer must be Automatic (0) or between 1 and 100 ms."};
            if (direction->settings.mode != "Automatic" && direction->settings.mode != "Shared" &&
                direction->settings.mode != "Exclusive")
                return {false, "Choose Automatic, Shared or Exclusive operating mode."};
            const auto preference_result = validate_preferences(direction->settings.preferences);
            if (!preference_result.ok)
                return preference_result;
            if (snapshot.operating_mode == OperatingMode::Test) {
                // Unknown/absent scan evidence is not evidence of unsupported
                // intent. Runtime initialization owns negotiation and readiness.
                for (const auto* device : {source, destination}) {
                    const auto& specific = device == source
                                               ? direction->settings.preferences.source
                                               : direction->settings.preferences.destination;
                    const auto preference =
                        inherit_preferences(direction->settings.preferences.common, specific);
                    bool shared_rejected = false, exclusive_rejected = false, supported = false;
                    if (preference.sample_rate_hz && preference.channel_count &&
                        preference.sample_format)
                        for (const auto& capability : device->capabilities)
                            if ((capability.mode == preference.mode.value_or("Shared") ||
                                 (!preference.mode && direction->exclusive_mode_preferred &&
                                  capability.mode == "Exclusive")) &&
                                capability.format.rate == *preference.sample_rate_hz &&
                                capability.format.channels == *preference.channel_count &&
                                capability.format.representation == *preference.sample_format) {
                                supported |= capability.support == FormatSupport::Supported;
                                if (capability.support == FormatSupport::Unsupported) {
                                    shared_rejected |= capability.mode == "Shared";
                                    exclusive_rejected |= capability.mode == "Exclusive";
                                }
                            }
                    // A preference cannot turn one access-mode rejection into
                    // a forced requirement. The other mode may still work or
                    // remain untested; initialization owns the final result.
                    const bool rejected =
                        preference.mode ? (*preference.mode == "Exclusive" ? exclusive_rejected
                                                                           : shared_rejected)
                        : direction->exclusive_mode_preferred
                            ? shared_rejected && exclusive_rejected
                            : shared_rejected;
                    if (rejected && !supported)
                        return {false, "An explicit format override was reported unsupported. "
                                       "Change it or reset to Automatic."};
                }
                continue;
            }
            const auto recommendation = recommend(snapshot, *direction);
            if (recommendation.compatible.empty())
                return {false,
                        "No common endpoint format is available. Select another endpoint pair."};
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
    if (milliseconds < 0 || milliseconds > 100)
        return {false, "Buffer must be Automatic (0) or between 1 and 100 milliseconds."};
    const auto* saved = profile(snapshot(), profile_id);
    if (!saved)
        return {false, "Save or select a profile before applying its buffer preference."};
    auto edited = *saved;
    const auto hole = std::find_if(edited.holes.begin(), edited.holes.end(),
                                   [&](const auto& value) { return value.id == hole_id; });
    if (hole == edited.holes.end() || (reverse && !hole->duplex))
        return {false, "Save this direction before applying its buffer preference."};
    (reverse ? hole->reverse : hole->forward).settings.set_buffer_preference(milliseconds);
    const auto result = save(std::move(edited), false);
    return result.ok ? Result{true, "Buffer preference saved. No live engine buffer was applied."}
                     : result;
}
SimulatedController::SimulatedController(bool tutorial) {
    if (tutorial) {
        state_.operating_mode = OperatingMode::Tutorial;
        return;
    }

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
    if (state_.logging_enabled || state_.events.back().type.starts_with("scan-")) {
        state_.diagnostic_events.push_back(state_.events.back());
        if (state_.diagnostic_events.size() > maximum_diagnostic_events)
            state_.diagnostic_events.erase(state_.diagnostic_events.begin());
    }
    if (state_.events.size() > maximum_diagnostic_events)
        state_.events.erase(state_.events.begin());
    ++state_.revision;
}
Hole SimulatedController::make_hole() {
    Hole hole;
    hole.id = "hole-" + std::to_string(next_id_++);
    hole.display_name = "New Hole";
    return hole;
}
std::string SimulatedController::create_profile(const std::string& title, const std::string& from) {
    Profile created;
    if (const auto* original = profile(state_, from))
        created = *original;
    created.id = "profile-" + std::to_string(next_id_++);
    std::vector<std::string> names;
    for (const auto& item : state_.profiles)
        names.push_back(item.display_name);
    created.display_name = unique_name(title.empty() ? "New Profile" : title, names);
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
    for (const auto& hole : draft.holes) {
        const auto checked = validate_endpoint_assignments(hole);
        if (!checked.ok)
            return checked;
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
    if (!available && !wait_for_devices_ && !arm_waiting)
        return {false, "Required devices are unavailable. Automatic waiting is disabled; the "
                       "current route is unchanged."};
    if (!available && state_.armed && !arm_waiting)
        return {false, "Required resources unavailable. Existing route retained. Confirm "
                       "replacement with a waiting profile to proceed."};
    live_ = *candidate;
    pending_manual_ack_ = std::any_of(live_.holes.begin(), live_.holes.end(), [](const auto& hole) {
        return hole.enabled &&
               ((hole.forward.advanced_tuning && manual_settings(hole.forward)) ||
                (hole.duplex && hole.reverse.advanced_tuning && manual_settings(hole.reverse)));
    });
    for (auto& hole : live_.holes) {
        hole.forward = effective_direction(hole);
        hole.reverse = effective_direction(hole, true);
    }
    if (state_.operating_mode == OperatingMode::Tutorial)
        state_.tutorial_activation_percent = 0;
    state_.active_profile = id;
    state_.pending_profile.clear();
    state_.pending_ms = 0;
    failures_.clear();
    cancel_beep();
    state_.monitoring = false;
    state_.monitor_source.clear();
    state_.input_peak.reset();
    state_.armed = true;
    state_.state = State::Starting;
    state_.paths.clear(); // A newly activated plan owns fresh simulated path lifetimes.
    transition_ms_ = state_.operating_mode == OperatingMode::Tutorial
                         ? tutorial_activation_duration_ms
                         : transition_duration_ms;
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
    const auto* saved_profile = profile(state_, profile_id);
    const auto saved_hole = std::find_if(saved_profile->holes.begin(), saved_profile->holes.end(),
                                         [&](const auto& item) { return item.id == hole_id; });
    if (saved_hole == saved_profile->holes.end() ||
        !(reverse ? hole->reverse : hole->forward).advanced_tuning ||
        !(reverse ? saved_hole->reverse : saved_hole->forward).advanced_tuning)
        return {true, "Buffer preference retained. Automatic policy or unapplied policy changes "
                      "keep it inactive."};
    (reverse ? hole->reverse : hole->forward).settings.set_buffer_preference(milliseconds);
    refresh_paths();
    const auto path =
        std::find_if(state_.paths.begin(), state_.paths.end(), [&](const auto& value) {
            return value.hole_id == hole_id && value.reverse == reverse;
        });
    const bool confirmed = path != state_.paths.end() && path->applied_buffer_ms &&
                           (!milliseconds || path->applied_buffer_ms == milliseconds);
    const auto detail =
        confirmed
            ? "Buffer preference saved; active Preview direction confirmed " +
                  (milliseconds ? std::to_string(milliseconds) + " ms"
                                : std::string("Automatic buffer selection")) +
                  " (simulated)."
            : "Buffer preference saved; Preview request updated, awaiting a running direction.";
    event("buffer-updated", detail);
    if (confirmed && milliseconds)
        event("manual-override-applied",
              "Manual buffer override applied to the simulated Preview route.");
    return {true, detail};
}
void SimulatedController::stop() {
    pending_manual_ack_ = false;
    state_.armed = false;
    state_.pending_profile.clear();
    state_.pending_ms = 0;
    scan(false);
    cancel_beep();
    state_.monitoring = false;
    state_.monitor_source.clear();
    state_.input_peak.reset();
    failures_.clear();
    state_.tutorial_activation_percent = -1;
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
                if (direction.settings.buffer_ms)
                    path.requested_buffer_ms = direction.settings.buffer_ms;
                if (path.state == State::Running)
                    path.applied_buffer_ms = direction.settings.buffer_ms
                                                 ? direction.settings.buffer_ms
                                                 : simulated_automatic_buffer_ms;
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
        const auto before = transition_ms_;
        transition_ms_ = std::max(0, transition_ms_ - ms);
        if (state_.operating_mode == OperatingMode::Tutorial &&
            state_.tutorial_activation_percent >= 0 && state_.armed) {
            state_.tutorial_activation_percent =
                100 * (tutorial_activation_duration_ms - transition_ms_) /
                tutorial_activation_duration_ms;
            ++state_.revision;
            if (before / 1000 != transition_ms_ / 1000)
                event("tutorial-routing-progress",
                      "Pretend audio activation: " +
                          std::to_string(state_.tutorial_activation_percent) +
                          "% (no streams opened).");
        }
        if (!transition_ms_) {
            if (!state_.armed) {
                state_.state = State::Stopped;
                state_.paths.clear();
            } else
                refresh_paths();
            event("state", std::string("Routing state: ") + name(state_.state));
            if (state_.operating_mode == OperatingMode::Tutorial && state_.state == State::Running)
                event("simulated_routing_running",
                      "Pretend routing activation completed; no real audio opened.");
            if (pending_manual_ack_) {
                pending_manual_ack_ = false;
                if (std::any_of(state_.paths.begin(), state_.paths.end(), [&](const auto& path) {
                        const auto hole =
                            std::find_if(live_.holes.begin(), live_.holes.end(),
                                         [&](const auto& item) { return item.id == path.hole_id; });
                        return path.state == State::Running && hole != live_.holes.end() &&
                               (path.reverse ? hole->reverse : hole->forward).advanced_tuning &&
                               manual_settings(path.reverse ? hole->reverse : hole->forward);
                    }))
                    event("manual-override-applied",
                          "Manual overrides applied to the simulated Preview route.");
            }
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
        state_.scan_percent =
            std::min(100, scan_ms_ / (state_.operating_mode == OperatingMode::Tutorial
                                          ? 200
                                          : scan_ms_per_percent));
        if (state_.operating_mode == OperatingMode::Tutorial) {
            ++state_.revision;
            if (scan_ms_ / 1000 != (scan_ms_ - ms) / 1000) {
                const auto devices = tutorial_devices();
                const int stage = std::min(19, std::max(0, (scan_ms_ - 1) / 1000));
                static const char* checks[] = {
                    "Reading imaginary identity", "Shared: 48000 Hz / stereo / float32",
                    "Exclusive: 48000 Hz / stereo / PCM16", "Checking fictional buffers",
                    "Fictional capability checks complete"};
                event("scan-progress", "[Pretend] Device " + std::to_string(stage / 5 + 1) +
                                           "/4 | " + devices[stage / 5].display_name + " | Check " +
                                           std::to_string(stage % 5 + 1) + "/5 | " +
                                           checks[stage % 5]);
            }
        }
        if (state_.scan_percent == 100) {
            state_.scanning = false;
            if (state_.operating_mode == OperatingMode::Tutorial) {
                auto discovered = tutorial_devices();
                for (auto& device : discovered)
                    if (const auto* previous = endpoint(state_, device.id)) {
                        device.display_name = previous->display_name;
                        device.notes = previous->notes;
                        device.hidden = previous->hidden;
                    }
                state_.endpoints = std::move(discovered);
                ++state_.scanner.inventory_revision;
                event("scan_completed",
                      "Four fictional endpoints discovered. No real device enumeration performed.");
            } else
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
        if (state_.monitor_source == device.id && device.availability != Availability::Available)
            monitor(false);
        if (state_.armed) {
            transition_ms_ = transition_duration_ms;
            refresh_paths(device.availability == Availability::Available);
        }
        if (!device.hidden)
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
    if (!enabled && !state_.monitoring && state_.monitor_source.empty() && !state_.input_peak)
        return;
    if (!enabled) {
        state_.monitor_source.clear();
        state_.input_peak.reset();
    }
    state_.monitoring = enabled;
    ++state_.revision;
}
Result SimulatedController::check_level(const std::string& id) {
    const auto* source = endpoint(state_, id);
    if (!source || !source->capture || source->availability != Availability::Available)
        return {false, "Choose an available capture endpoint."};
    cancel_beep();
    state_.monitor_source = id;
    state_.monitoring = true;
    state_.input_peak = .35f;
    state_.diagnostic_detail = "Capture level is simulated in Preview.";
    ++state_.revision;
    return {true, state_.diagnostic_detail};
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
    monitor(false);
    state_.beep_destination = id;
    beep_ms_ = beep_duration_ms;
    event("beep-start", "Test Beep target: " + destination->display_name +
                            ". Test signal: approximately 1 kHz, -30 "
                            "dBFS, 1 second with fades.");
    return {true, "Test Beep active for the selected destination."};
}
void SimulatedController::clear_diagnostics() {
    state_.diagnostic_events.clear();
    state_.events.clear();
    ++state_.revision;
    // Do not reset sequence_ or emit a replacement event: clearing must leave an
    // empty log, and future events must remain newer than consumers' cursors.
}
void SimulatedController::set_logging(bool enabled) {
    if (state_.logging_enabled == enabled)
        return;
    if (enabled)
        state_.logging_enabled = true;
    event(enabled ? "logging-started" : "logging-stopped",
          enabled ? "Diagnostic logging started." : "Diagnostic logging stopped.");
    state_.logging_enabled = enabled;
    ++state_.revision;
}
void SimulatedController::cancel_beep() {
    if (!state_.beep_destination.empty())
        ++state_.revision;
    beep_ms_ = 0;
    state_.beep_destination.clear();
}
Result SimulatedController::scanner_action(const ScannerActionRequest& request) {
    if (request.kind != ScannerActionKind::SetHidden)
        return {false, "Real scanner inspection is available in Test Mode."};
    const auto found =
        std::find_if(state_.endpoints.begin(), state_.endpoints.end(),
                     [&](const auto& device) { return device.id == request.logical_id; });
    if (found == state_.endpoints.end())
        return {false, "Device no longer exists."};
    found->hidden = request.hidden;
    event("device-visibility", "Preview device visibility changed; routing is unchanged.");
    return {true, "Device visibility changed for this Preview session."};
}
void SimulatedController::scan(bool enabled, bool advanced, ScanScope scope,
                               const std::string& selected_device, const ScanOptions&) {
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
