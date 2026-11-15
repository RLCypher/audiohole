#include "audiohole/engine/backend.hpp"
#include "audiohole/engine/channel_mapping.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

using namespace audiohole::engine;
using Json = nlohmann::json;

namespace {
int checks = 0;
void expect(bool condition, const char* description) {
    ++checks;
    if (!condition)
        throw std::runtime_error(description);
}

Json document() {
    // Necessary portable fixtures: these identifiers never enter a production
    // device provider and no test claims real endpoint or audio success.
    const Json direction{{"source", "capture-test"},
                         {"destination", "render-test"},
                         {"settings",
                          {{"preferences",
                            {{"common", Json::object()},
                             {"source", Json::object()},
                             {"destination", Json::object()},
                             {"channel_map", Json::array()}}}}}};
    return {{"format", "audiohole-profile"},
            {"schema_version", 1},
            {"profile",
             {{"id", "profile-test"},
              {"display_name", "Control fixture"},
              {"notes", "Unicode notes: RØDE\nUser text is preserved."},
              {"holes", Json::array({{{"id", "hole-test"},
                                      {"display_name", "Test route"},
                                      {"notes", ""},
                                      {"enabled", true},
                                      {"duplex", true},
                                      {"advanced_tuning", true},
                                      {"routing_engine", "automatic"},
                                      {"forward", direction},
                                      {"reverse", direction}}})}}}};
}

Profile parsed(const Json& j) {
    auto p = parse_profile(j.dump());
    if (!p.ok())
        throw std::runtime_error("Unexpected parse failure");
    return std::move(*p.value);
}

void profile_contracts() {
    auto j = document();
    auto& prefs = j["profile"]["holes"][0]["forward"]["settings"]["preferences"];
    prefs["common"] = {{"sample_rate_hz", 48000},
                       {"channel_count", 2},
                       {"sample_format", "float32"},
                       {"mode", "Shared"},
                       {"buffer_ms", 10}};
    prefs["source"] = {{"sample_rate_hz", 44100}, {"buffer_ms", nullptr}};
    prefs["destination"] = {{"channel_count", 4}, {"mode", "Exclusive"}};
    prefs["channel_map"] =
        Json::array({{{"source", 0}, {"destination", 2}}, {{"source", 1}, {"destination", 3}}});
    auto profile = parsed(j);
    const auto original = profile;
    auto prepared = prepare_profile(profile);
    expect(prepared.ok(), "asymmetric duplex prepares");
    expect(prepared.value->routes.size() == 2, "duplex counts as two paths");
    expect(prepared.value->enabled_holes == 1, "duplex counts as one hole");
    const auto& forward = prepared.value->routes.front();
    expect(forward.source.sample_rate_hz == 44100, "source overrides common rate");
    expect(forward.destination.sample_rate_hz == 48000, "destination inherits common rate");
    expect(forward.source.channel_count == 2 && forward.destination.channel_count == 4,
           "independent stream widths");
    expect(forward.source.buffer_ms == 10, "null inherits rather than clearing common");
    expect(forward.destination.mode == AccessMode::Exclusive, "endpoint access override");
    expect(!prepared.value->routes.back().source.sample_rate_hz, "reverse remains automatic");
    expect(profile == original, "preparation never mutates saved intent or notes");
    profile.holes[0].forward.advanced_tuning = profile.holes[0].reverse.advanced_tuning = false;
    prepared = prepare_profile(profile);
    expect(prepared.ok() && !prepared.value->routes.front().source.sample_rate_hz &&
               prepared.value->routes.front().channel_map.empty(),
           "policy gates all overrides");
    profile.holes[0].forward.advanced_tuning = profile.holes[0].reverse.advanced_tuning = true;
    expect(profile == original, "disable/re-enable retains exact preferences");
    profile.holes[0].routing_engine = "not-installed";
    profile.holes[0].forward.advanced_tuning = profile.holes[0].reverse.advanced_tuning = false;
    expect(!prepare_profile(profile).ok(), "backend selection is independent of tuning");
    profile.holes[0].enabled = false;
    expect(prepare_profile(profile).ok(), "unknown inactive backend remains a saved draft");

    j = document();
    auto& settings = j["profile"]["holes"][0]["forward"]["settings"];
    settings.erase("preferences");
    settings["format"] = {{"rate", 192000}};
    settings["mode"] = "Exclusive";
    settings["buffer_ms"] = 25;
    settings["automatic"] = false;
    prepared = prepare_profile(parsed(j));
    expect(prepared.ok() && !prepared.value->routes[0].source.sample_rate_hz &&
               !prepared.value->routes[0].source.mode &&
               !prepared.value->routes[0].source.buffer_ms,
           "missing current preferences never consult legacy projections");
    for (const auto value : {Json(nullptr), Json(""), Json("Automatic")}) {
        j["profile"]["holes"][0]["forward"]["settings"]["preferences"]["common"] = {
            {"sample_rate_hz", value}, {"buffer_ms", value}};
        expect(prepare_profile(parsed(j)).ok(), "documented Automatic values normalize");
    }
    j["profile"]["holes"][0]["forward"]["settings"]["preferences"]["common"] = {{"buffer_ms", 0}};
    expect(!prepare_profile(parsed(j)).value->routes[0].source.buffer_ms,
           "zero buffer is inheritance, never a zero allocation");
    for (const auto value : {Json(-1), Json(768001), Json(48000.5), Json(true), Json("48000"),
                             Json(std::numeric_limits<std::uint64_t>::max())}) {
        auto bad = document();
        bad["profile"]["holes"][0]["forward"]["settings"]["preferences"]["common"] = {
            {"sample_rate_hz", value}};
        expect(!parse_profile(bad.dump()).ok(), "malformed numeric requirement rejects");
    }
    expect(!parse_profile("{\"format\":1,\"format\":2}").ok(), "duplicate keys reject");
    auto bad = document();
    bad["schema_version"] = 5;
    expect(!parse_profile(bad.dump()).ok(), "future profile versions reject");
    bad = document();
    bad["format"] = "audiohole-test-configuration";
    expect(!parse_profile(bad.dump()).ok(), "aggregate wrapper is a separate format");
    bad = document();
    bad["profile"]["holes"][0]["enabled"] = 1;
    expect(!parse_profile(bad.dump()).ok(), "numeric boolean rejects");
    bad = document();
    bad["profile"]["holes"].push_back(bad["profile"]["holes"][0]);
    expect(!parse_profile(bad.dump()).ok(), "duplicate hole IDs reject");
    auto p = parsed(document());
    for (int i = 1; i < 9; ++i) {
        auto copy = p.holes.front();
        copy.id = "hole-" + std::to_string(i);
        p.holes.push_back(std::move(copy));
    }
    expect(!prepare_profile(p).ok(), "ninth enabled hole rejects");
    p.holes.back().enabled = false;
    expect(prepare_profile(p).ok(), "disabled saved holes do not count");
    p.holes[0].forward.preferences.source.channel_count = 0;
    expect(!prepare_profile(p).ok(), "typed input is validated too");
}

void profile_v3_contracts() {
    auto j = document();
    j["schema_version"] = 3;
    auto& h = j["profile"]["holes"][0];
    h["routing"] = "wasapi";
    h.erase("routing_engine");
    h["advanced_tuning"] = true; // Stale hole flags have no v3 authority.
    h["exclusive_mode_preferred"] = false;
    h["forward"]["advanced_tuning"] = true;
    h["forward"]["exclusive_mode_preferred"] = true;
    h["reverse"]["advanced_tuning"] = false;
    h["reverse"]["exclusive_mode_preferred"] = true;
    auto& f = h["forward"]["settings"]["preferences"];
    auto& r = h["reverse"]["settings"]["preferences"];
    f["common"] = {{"sample_rate_hz", 48000}, {"mode", "Exclusive"}};
    f["source"] = {{"mode", "Shared"}};
    f["preferred_clock_device"] = "capture";
    r["common"] = {{"sample_rate_hz", 44100}, {"mode", "Exclusive"}};
    r["channel_map"] = Json::array({{{"source", 0}, {"destination", 1}}});
    const auto bytes = j.dump();
    const auto p = parsed(j);
    const auto before = p;
    const auto prepared = prepare_profile(p);
    expect(p.source_schema_version == 3 && prepared.ok() && prepared.value->routes.size() == 2,
           "schema-v3 asymmetric directions prepare independently");
    const auto& forward = prepared.value->routes[0];
    const auto& reverse = prepared.value->routes[1];
    expect(forward.backend == BackendId::Wasapi && reverse.backend == BackendId::Wasapi,
           "hole.routing selects the backend for both directions");
    expect(forward.source.mode == AccessMode::Shared &&
               forward.destination.mode == AccessMode::Exclusive &&
               forward.exclusive_mode_preferred && forward.preferred_clock_device == "capture",
           "effective endpoint modes override common while clock preference stays metadata");
    expect(reverse.exclusive_mode_preferred && !reverse.source.mode &&
               !reverse.source.sample_rate_hz && reverse.channel_map.empty(),
           "preference remains active when only reverse advanced overrides are disabled");
    expect(p == before && j.dump() == bytes && p.notes.find("User text") != std::string::npos,
           "loading/planning does not rewrite notes, flags, metadata or original JSON");
    auto oversized_clock = p;
    oversized_clock.holes[0].reverse.preferences.preferred_clock_device = std::string(129, 'x');
    expect(!prepare_profile(oversized_clock).ok(),
           "typed saved clock metadata is bounded even when advanced tuning is disabled");
    for (const auto key : {"advanced_tuning", "exclusive_mode_preferred"}) {
        auto bad = j;
        bad["profile"]["holes"][0]["forward"][key] = "true";
        expect(!parse_profile(bad.dump()).ok(), "direction policy flags require actual booleans");
    }
    auto bad = j;
    bad["profile"]["holes"][0].erase("routing");
    bad["profile"]["holes"][0]["routing_engine"] = "wasapi";
    expect(!parse_profile(bad.dump()).ok(), "v3 old backend key cannot silently select Automatic");
    bad = j;
    bad["format"] = "audiohole-test-configuration";
    bad["schema_version"] = 7;
    expect(!parse_profile(bad.dump()).ok(), "v7 aggregate is not individual routing input");
    auto old = document();
    old["profile"]["holes"][0]["forward"]["advanced_tuning"] = "unused legacy extension";
    old["profile"]["holes"][0]["forward"]["settings"]["preferences"]["preferred_clock_device"] = 42;
    expect(parse_profile(old.dump()).ok(),
           "v1 does not activate or validate formerly unknown direction extensions");
    old["schema_version"] = 2;
    old["profile"]["holes"][0]["forward"]["settings"]["preferences"]["preferred_clock_device"] =
        "capture";
    auto& legacy = old["profile"]["holes"][0];
    legacy["routing"] = "miniaudio";
    legacy["exclusive_mode_preferred"] = true;
    const auto migrated = parsed(old);
    expect(migrated.holes[0].forward.advanced_tuning && migrated.holes[0].reverse.advanced_tuning &&
               migrated.holes[0].forward.exclusive_mode_preferred &&
               migrated.holes[0].reverse.exclusive_mode_preferred &&
               migrated.holes[0].routing_engine == "miniaudio",
           "v2 hole policy is projected in memory onto both directions");
}

void bindings_and_backends() {
    auto prepared = prepare_profile(parsed(document()));
    const auto& route = prepared.value->routes.front();
    BindingSnapshot bindings{
        42,
        {{"capture-test", "opaque-input", EndpointDirection::Capture, BindingState::Available},
         {"render-test", "opaque-output", EndpointDirection::Render, BindingState::Available}}};
    auto bound = bind_route(route, bindings);
    expect(bound.ok() && bound.value->binding_revision == 42, "exact immutable binding revision");
    bindings.endpoints[0].state = BindingState::Unknown;
    expect(!bind_route(route, bindings).ok(), "unknown availability is not usable");
    bindings.endpoints[0].state = BindingState::Ambiguous;
    expect(bind_route(route, bindings).issues.front().code == ErrorCode::AmbiguousEndpoint,
           "ambiguous identity is explicit");
    bindings.endpoints[0].state = BindingState::Available;
    bindings.endpoints[0].direction = EndpointDirection::Render;
    expect(!bind_route(route, bindings).ok(), "wrong direction rejects");
    bindings.endpoints[0].direction = EndpointDirection::Capture;
    bindings.endpoints.push_back(bindings.endpoints.front());
    expect(!bind_route(route, bindings).ok(), "duplicate logical claims reject");
    bindings.endpoints.clear();
    expect(!bind_route(route, bindings).ok(), "names cannot substitute missing logical bindings");
    const std::vector<BackendDescriptor> adapters{{BackendId::Miniaudio, true, "fixture only"},
                                                  {BackendId::Wasapi, true, "fixture only"},
                                                  {BackendId::Experimental, true, "fixture only"}};
    expect(select_backend(BackendId::Automatic, adapters).value == BackendId::Wasapi,
           "Automatic prefers native comparison baseline");
    expect(select_backend(BackendId::Miniaudio, adapters).value == BackendId::Miniaudio,
           "explicit miniaudio remains explicit");
    expect(!select_backend(BackendId::Miniaudio, {{BackendId::Wasapi, true, ""}}).ok(),
           "explicit unavailable backend never substitutes");
    expect(!select_backend(BackendId::Automatic, {{BackendId::Experimental, true, ""}}).ok(),
           "Automatic excludes Experimental");
    expect(!select_backend(BackendId::Automatic,
                           {{BackendId::Wasapi, true, ""}, {BackendId::Wasapi, true, ""}})
                .ok(),
           "duplicate adapter registration rejects");
}

void formats_and_maps() {
    const auto pcm = pcm_geometry({48000, 10, SampleFormat::PcmS24In32});
    expect(pcm.ok() && pcm.value->container_bits == 32 && pcm.value->valid_bits == 24 &&
               pcm.value->block_align_bytes == 40 && pcm.value->average_bytes_per_second == 1920000,
           "derive multichannel PCM storage without inventing a layout");
    expect(!pcm_geometry({0, 2, SampleFormat::Float32}).ok(), "invalid negotiated format rejects");
    EndpointRequirements required;
    required.channel_count = 2;
    expect(!check_format(required, {48000, 10, SampleFormat::Float32}, "destination").ok(),
           "backend cannot ignore explicit width");
    auto mapping = compile_mapping(2, 4, {});
    float input[]{1, 2, 3, 4};
    float output[]{9, 9, 9, 9, 9, 9, 9, 9};
    expect(mapping.ok() && mapping.value->process(input, output, 2), "default mapping executes");
    expect(output[0] == 1 && output[1] == 2 && output[2] == 0 && output[3] == 0 && output[4] == 3 &&
               output[5] == 4 && output[6] == 0 && output[7] == 0,
           "unmapped outputs are silent in every frame");
    mapping = compile_mapping(2, 4, {{0, 3}, {1, 1}, {0, 2}});
    expect(mapping.value->process(input, output, 2) && output[0] == 0 && output[1] == 2 &&
               output[2] == 1 && output[3] == 1,
           "explicit indexes permit source duplication");
    expect(!compile_mapping(2, 2, {{0, 0}, {1, 0}}).ok(), "implicit summing rejects");
    expect(!compile_mapping(2, 2, {{2, 0}}).ok(), "map rechecks negotiated width");
    expect(!mapping.value->process(input, input, 1), "overlap rejects before writing");
    expect(!mapping.value->process(input, output, std::numeric_limits<std::size_t>::max()),
           "frame count overflow rejects");
    mapping = compile_mapping(2, 1, {});
    expect(mapping.value->process(input, output, 2) && output[0] == 1 && output[1] == 3,
           "default mapping discards unmatched inputs without a downmix");
    // Automatic mono monitoring must reach both ears without a saved manual
    // map. Multichannel destinations use their first pair; extras stay silent.
    mapping = compile_mapping(1, 4, {});
    float mono[]{0.25f, -0.5f};
    expect(mapping.value->process(mono, output, 2) && output[0] == mono[0] &&
               output[1] == mono[0] && output[2] == 0 && output[3] == 0 &&
               output[4] == mono[1] && output[5] == mono[1] && output[6] == 0 && output[7] == 0,
           "automatic mono duplicates into the first playback pair only");
    mapping = compile_mapping(1, 2, {{0, 1}});
    expect(mapping.value->process(mono, output, 2) && output[0] == 0 && output[1] == mono[0] &&
               output[2] == 0 && output[3] == mono[1],
           "an explicit mono map overrides automatic duplication");
    mapping = compile_mapping(2, 2, {});
    expect(mapping.value->process(input, output, 2) &&
               std::equal(std::begin(input), std::end(input), std::begin(output)),
           "automatic stereo preserves separate left and right samples");
    RouteRequest request;
    NegotiatedRoute negotiated;
    negotiated.source.client_format = negotiated.destination.client_format =
        StreamFormat{48000, 2, SampleFormat::Float32};
    expect(check_negotiated(request, negotiated).ok(), "Automatic admits concrete shared formats");
    negotiated.destination.mode = AccessMode::Exclusive;
    expect(!check_negotiated(request, negotiated).ok(), "Automatic cannot escalate exclusive");
    negotiated.destination.mode = AccessMode::Shared;
    request.destination.buffer_ms = 10;
    expect(!check_negotiated(request, negotiated).ok(), "unconfirmed manual buffer rejects");
    negotiated.destination.applied_buffer_request_ms = 10;
    negotiated.destination.allocated_buffer_frames = 480;
    negotiated.destination.timing_sample_rate_hz = 48000;
    expect(check_negotiated(request, negotiated).ok(), "manual buffer confirmation admits");
    negotiated.destination.allocated_buffer_frames = 1056;
    expect(check_negotiated(request, negotiated).ok() &&
               negotiated.destination.applied_buffer_request_ms == 10 &&
               negotiated.destination.allocated_buffer_frames == 1056,
           "driver-aligned allocation retains both actual frames and the supplied request");
    negotiated.destination.allocated_buffer_frames = 481;
    expect(check_negotiated(request, negotiated).ok(),
           "one frame of allocation rounding is permitted");
    negotiated.destination.allocated_buffer_frames = 482;
    expect(check_negotiated(request, negotiated).ok(),
           "driver allocation need not equal the request");
    negotiated.destination.applied_buffer_request_ms = 11;
    expect(!check_negotiated(request, negotiated).ok(),
           "an ignored or substituted request rejects");
    negotiated.destination.applied_buffer_request_ms = 10;
    negotiated.destination.allocated_buffer_frames = 0;
    expect(!check_negotiated(request, negotiated).ok(), "zero actual allocation still rejects");
}
} // namespace

int main() {
    try {
        profile_contracts();
        profile_v3_contracts();
        bindings_and_backends();
        formats_and_maps();
        std::ifstream file(AUDIOHOLE_ENGINE_EXAMPLE);
        const std::string bytes((std::istreambuf_iterator<char>(file)), {});
        expect(parse_profile(bytes).ok(), "actual profile builder example is accepted");
        std::ifstream current(AUDIOHOLE_ENGINE_V3_EXAMPLE);
        const std::string v3_bytes((std::istreambuf_iterator<char>(current)), {});
        const auto supplied = parse_profile(v3_bytes);
        expect(supplied.ok() && supplied.value->source_schema_version == 3,
               "corrected owner-supplied schema-v3 fixture parses without device I/O");
        const auto v3_plan = prepare_profile(*supplied.value);
        expect(v3_plan.ok() && v3_plan.value->routes.size() == 2 &&
                   v3_plan.value->routes[0].source_id != v3_plan.value->routes[1].source_id &&
                   v3_plan.value->routes[0].exclusive_mode_preferred &&
                   v3_plan.value->routes[1].exclusive_mode_preferred &&
                   v3_plan.value->routes[0].preferred_clock_device == "capture",
               "corrected capture assignment and independent direction preference are honored");
        std::cout << checks << " portable contract checks passed; no hardware I/O performed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
