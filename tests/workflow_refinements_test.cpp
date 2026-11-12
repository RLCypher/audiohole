#include "model/build_preferences.h"
#include "model/controller.h"
#include "model/first_run.h"
#include "model/persistence.h"
#include "presentation/device_events.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace audiohole;
namespace {
using Json = nlohmann::ordered_json;
struct WorkflowRefinements : testing::Test {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("audiohole-workflow-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path file = root / "configuration.json";
    ~WorkflowRefinements() override {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    static std::string read(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), {}};
    }
    static Profile sample() {
        Profile p;
        p.id = "profile";
        p.display_name = "VR Workflow";
        p.notes = "Keep µ notes";
        Hole h;
        h.id = "hole";
        h.display_name = "Steam Frame";
        h.duplex = true;
        h.forward.advanced_tuning = true;
        h.reverse.advanced_tuning = true;
        h.forward.source = h.reverse.source = "capture";
        h.forward.destination = h.reverse.destination = "render";
        p.holes.push_back(h);
        return p;
    }
    static Snapshot inventory() {
        Snapshot s;
        s.operating_mode = OperatingMode::Test;
        for (bool capture : {true, false}) {
            Endpoint e;
            e.id = capture ? "capture" : "render";
            e.capture = capture;
            e.display_name = capture ? "Microphone" : "Headphones";
            e.inspection = std::make_shared<DeviceInspection>();
            s.endpoints.push_back(e);
        }
        return s;
    }
};
TEST_F(WorkflowRefinements, ForcedModeSupersedesExclusivePreferenceForEveryEngineAndPolicyGate) {
    const auto s = inventory();
    auto p = sample();
    auto& h = p.holes[0];
    for (const auto* engine : {"automatic", "wasapi", "miniaudio", "experimental"})
        for (bool preferred : {false, true})
            for (bool enabled : {false, true}) {
                h.routing_engine = engine;
                h.forward.exclusive_mode_preferred = preferred;
                h.reverse.exclusive_mode_preferred = preferred;
                h.forward.advanced_tuning = enabled;
                h.reverse.advanced_tuning = enabled;
                h.forward.settings.preferences.common.mode = "Shared";
                h.reverse.settings.preferences.common.mode = "Exclusive";
                h.forward.settings.preferences.preferred_clock_device = "capture";
                h.reverse.settings.preferences.preferred_clock_device = "playback";
                PreparedProfile out;
                ASSERT_TRUE(prepare_profile(s, p, out).ok);
                ASSERT_EQ(out.paths.size(), 2u);
                EXPECT_EQ(out.paths[0].routing_engine, engine);
                EXPECT_EQ(out.paths[1].routing_engine, engine);
                EXPECT_EQ(out.paths[0].mode, enabled     ? "Shared"
                                             : preferred ? "Exclusive"
                                                         : "Shared");
                EXPECT_EQ(out.paths[1].mode, enabled     ? "Exclusive"
                                             : preferred ? "Exclusive"
                                                         : "Shared");
                EXPECT_EQ(out.paths[0].exclusive_mode_preferred, preferred);
                EXPECT_EQ(out.paths[0].preferences.preferred_clock_device,
                          enabled ? "capture" : "automatic");
                EXPECT_EQ(out.paths[1].preferences.preferred_clock_device,
                          enabled ? "playback" : "automatic");
                EXPECT_EQ(h.forward.settings.preferences.preferred_clock_device, "capture");
            }
}
TEST_F(WorkflowRefinements,
       RoutingExclusiveAndIndependentClocksRoundTripInNamedProfilesAndTransfer) {
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    auto p = sample();
    auto& h = p.holes[0];
    h.routing_engine = "experimental";
    h.forward.exclusive_mode_preferred = true;
    h.reverse.exclusive_mode_preferred = true;
    h.forward.settings.preferences.preferred_clock_device = "capture";
    h.reverse.settings.preferences.preferred_clock_device = "playback";
    h.forward.settings.preferences.common.mode = "Shared";
    h.reverse.settings.preferences.common.mode = "Exclusive";
    records.profiles.push_back(p);
    ASSERT_TRUE(store.save(records).ok);
    const auto doc = Json::parse(read(root / "profiles" / "VR Workflow.json"));
    EXPECT_EQ(doc["schema_version"], 3);
    EXPECT_EQ(doc["profile"]["holes"][0]["routing"], "experimental");
    EXPECT_TRUE(doc["profile"]["holes"][0]["forward"]["exclusive_mode_preferred"].get<bool>());
    EXPECT_FALSE(doc["profile"]["holes"][0].contains("routing_engine"));
    Profile loaded;
    ASSERT_TRUE(validate_profile_record(doc.dump(), loaded).ok);
    EXPECT_EQ(loaded, p);
    std::string transfer;
    ASSERT_TRUE(read_profile_document(file, transfer).ok);
    EXPECT_EQ(Json::parse(transfer)["schema_version"], 7);
    ConfigurationRecords decoded;
    ASSERT_TRUE(validate_test_document(transfer, decoded).ok);
    EXPECT_EQ(decoded.profiles[0], p);
    auto invalid = doc;
    invalid["profile"]["holes"][0]["forward"]["exclusive_mode_preferred"] = "yes";
    EXPECT_FALSE(validate_profile_record(invalid.dump(), loaded).ok);
    EXPECT_EQ(loaded, p);
    invalid = doc;
    invalid["profile"]["holes"][0]["forward"]["settings"]["preferences"]["preferred_clock_device"] =
        "unknown";
    EXPECT_FALSE(validate_profile_record(invalid.dump(), loaded).ok);
    EXPECT_EQ(loaded, p);
}
TEST_F(WorkflowRefinements, ExplorerPathUsesSavedIdentityAndTracksRenames) {
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    records.profiles.push_back(sample());
    ASSERT_TRUE(store.save(records).ok);
    std::filesystem::path path;
    ASSERT_TRUE(profile_record_path(file, "profile", path).ok);
    EXPECT_EQ(path.filename(), "VR Workflow.json");
    records.profiles[0].display_name = "Renamed headset";
    ASSERT_TRUE(store.save(records).ok);
    ASSERT_TRUE(profile_record_path(file, "profile", path).ok);
    EXPECT_EQ(path.filename(), "Renamed headset.json");
    const auto before = path;
    EXPECT_FALSE(profile_record_path(file, "missing", path).ok);
    EXPECT_EQ(path, before);
    EXPECT_EQ(records.profiles[0].id, "profile");
    EXPECT_EQ(records.profiles[0].notes, "Keep µ notes");
}
TEST_F(WorkflowRefinements, FirstRunMarkerCanBeResetWithoutChangingUserData) {
    const auto marker = root / "bin" / "AudioHole.has-run";
    bool needed = false;
    ASSERT_TRUE(first_run_needed(marker, needed).ok);
    EXPECT_TRUE(needed);
    ASSERT_TRUE(mark_first_run(marker).ok);
    ASSERT_TRUE(first_run_needed(marker, needed).ok);
    EXPECT_FALSE(needed);
    const auto original = read(marker);
    ASSERT_TRUE(mark_first_run(marker).ok);
    EXPECT_EQ(read(marker), original);
    std::ofstream(root / "keep.json") << "user data";
    ASSERT_TRUE(reset_first_run(marker).ok);
    ASSERT_TRUE(first_run_needed(marker, needed).ok);
    EXPECT_TRUE(needed);
    EXPECT_EQ(read(root / "keep.json"), "user data");
    ASSERT_TRUE(reset_first_run(marker).ok);
    std::filesystem::create_directory(marker);
    EXPECT_FALSE(mark_first_run(marker).ok);
    EXPECT_FALSE(reset_first_run(marker).ok);
    EXPECT_TRUE(std::filesystem::is_directory(marker));
}
TEST_F(WorkflowRefinements, DeviceResponsesDeduplicateAndDistinguishAbsenceFromUnavailability) {
    presentation::DeviceEvents observer;
    auto s = inventory();
    EXPECT_TRUE(observer.observe(s.endpoints).empty());
    EXPECT_TRUE(observer.observe(s.endpoints).empty());
    s.endpoints[0].display_name = "Renamed mic";
    EXPECT_TRUE(observer.observe(s.endpoints).empty());
    auto inspection = std::make_shared<DeviceInspection>();
    inspection->observed_state = "not_present";
    s.endpoints[0].inspection = inspection;
    s.endpoints[0].availability = Availability::Unavailable;
    auto notices = observer.observe(s.endpoints);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0].message, presentation::Message::DeviceDisconnected);
    EXPECT_EQ(notices[0].text, "Renamed mic disconnected.");
    EXPECT_TRUE(observer.observe(s.endpoints).empty());
    s.endpoints[0].availability = Availability::Available;
    notices = observer.observe(s.endpoints);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0].message, presentation::Message::DeviceConnected);
    inspection->observed_state = "disabled";
    s.endpoints[0].availability = Availability::Unavailable;
    notices = observer.observe(s.endpoints);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0].message, presentation::Message::DeviceUnavailable);
    inspection->observed_state = "not_present";
    notices = observer.observe(s.endpoints);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0].message, presentation::Message::DeviceDisconnected);
    EXPECT_TRUE(observer.observe(s.endpoints).empty());
    s.endpoints.clear();
    EXPECT_TRUE(observer.observe(s.endpoints).empty());
    s = inventory();
    EXPECT_TRUE(observer.observe(s.endpoints).empty()); // Reimport is a new baseline.
    presentation::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (auto event :
         {presentation::Message::DeviceConnected, presentation::Message::DeviceDisconnected,
          presentation::Message::DeviceUnavailable}) {
        EXPECT_TRUE(messages.show(event, "vanilla", 1).commentary.empty());
        EXPECT_FALSE(messages.show(event, "default", 1).commentary.empty());
        EXPECT_FALSE(messages.show(event, "uwu", 1).commentary.empty());
    }
}
TEST_F(WorkflowRefinements, ExclusivePreferenceDoesNotTurnAnAutomaticModeIntoAForcedRequirement) {
    auto s = inventory();
    auto p = sample();
    auto& hole = p.holes[0];
    hole.forward.exclusive_mode_preferred = true;
    hole.reverse.exclusive_mode_preferred = true;
    hole.forward.settings.preferences.common = {48000, 2, {}, "float32", {}};
    for (auto& device : s.endpoints) {
        device.capabilities.push_back(
            {Format{48000, 2, "float32"}, "Shared", FormatSupport::Supported});
        device.capabilities.push_back(
            {Format{48000, 2, "float32"}, "Exclusive", FormatSupport::Unsupported});
    }
    PreparedProfile prepared;
    ASSERT_TRUE(prepare_profile(s, p, prepared).ok);
    EXPECT_FALSE(prepared.paths[0].source_requirements.mode);
    hole.forward.settings.preferences.common.mode = "Exclusive";
    EXPECT_FALSE(prepare_profile(s, p, prepared).ok);
    hole.forward.settings.preferences.common.mode = "Shared";
    ASSERT_TRUE(prepare_profile(s, p, prepared).ok);
    EXPECT_EQ(prepared.paths[0].mode, "Shared");
}
TEST_F(WorkflowRefinements, ReservedClockChoiceDoesNotClaimAnAppliedAudioOverride) {
    SimulatedController controller;
    auto candidate = *profile(controller.snapshot(), "profile-duplex");
    candidate.holes[0].forward.advanced_tuning = true;
    candidate.holes[0].reverse.advanced_tuning = true;
    candidate.holes[0].forward.settings.preferences.preferred_clock_device = "capture";
    candidate.holes[0].reverse.settings.preferences.preferred_clock_device = "playback";
    ASSERT_TRUE(controller.save(candidate, false).ok);
    ASSERT_TRUE(controller.activate(candidate.id).ok);
    controller.advance(300);
    for (const auto& event : controller.snapshot().events)
        EXPECT_NE(event.type, "manual-override-applied");
}
TEST_F(WorkflowRefinements, DirectionPoliciesAndExclusivePreferencesRemainIndependent) {
    auto candidate = sample();
    auto& hole = candidate.holes[0];
    hole.forward.advanced_tuning = true;
    hole.forward.settings.preferences.common.mode = "Exclusive";
    hole.forward.settings.set_buffer_preference(11);
    hole.reverse.advanced_tuning = false;
    hole.reverse.exclusive_mode_preferred = true;
    hole.reverse.settings.preferences.common.mode = "Shared";
    hole.reverse.settings.set_buffer_preference(41);
    const auto retained_return = hole.reverse;
    PreparedProfile prepared;
    ASSERT_TRUE(prepare_profile(inventory(), candidate, prepared).ok);
    EXPECT_EQ(prepared.paths[0].mode, "Exclusive");
    EXPECT_EQ(prepared.paths[0].buffer_ms, 11);
    EXPECT_EQ(prepared.paths[1].mode, "Exclusive");
    EXPECT_EQ(prepared.paths[1].buffer_ms, 0);
    hole.forward.advanced_tuning = false;
    ASSERT_TRUE(prepare_profile(inventory(), candidate, prepared).ok);
    EXPECT_EQ(prepared.paths[0].mode, "Shared");
    EXPECT_EQ(hole.reverse, retained_return);
    hole.reverse.advanced_tuning = true;
    ASSERT_TRUE(prepare_profile(inventory(), candidate, prepared).ok);
    EXPECT_EQ(prepared.paths[1].mode, "Shared");
    EXPECT_EQ(prepared.paths[1].buffer_ms, 41);
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    records.profiles.push_back(candidate);
    ASSERT_TRUE(store.save(records).ok);
    ASSERT_TRUE(store.load(records).ok);
    EXPECT_EQ(records.profiles[0], candidate);
}
TEST_F(WorkflowRefinements, FreshAndMissingAppearanceFollowSystemWhileSavedDarkIsPreserved) {
    BuildPreferences preferences(root);
    ASSERT_TRUE(preferences.load().ok);
    EXPECT_EQ(preferences.values().appearance, "system");
    auto selected = preferences.values();
    selected.appearance = "dark";
    selected.startup_mode = 2;
    selected.startup_delay_seconds = 23;
    selected.startup_profile_id = "profile";
    ASSERT_TRUE(preferences.update(selected).ok);
    BuildPreferences restarted(root);
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_EQ(restarted.values(), selected);
    auto document = Json::parse(read(root / "preferences.json"));
    document.erase("appearance");
    std::ofstream(root / "preferences.json") << document.dump();
    BuildPreferences legacy(root);
    ASSERT_TRUE(legacy.load().ok);
    EXPECT_EQ(legacy.values().appearance, "system");
    EXPECT_EQ(legacy.values().startup_mode, 2);
    EXPECT_EQ(legacy.values().startup_delay_seconds, 23);
}
} // namespace
