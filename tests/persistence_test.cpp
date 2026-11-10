#include "model/application_controller.h"
#include "model/build_preferences.h"
#include "model/configuration_controller.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {
using namespace audiohole;
class PersistenceTest : public testing::Test {
  protected:
    std::filesystem::path root, file;
    void SetUp() override {
        static std::atomic<unsigned> serial{0};
        root = std::filesystem::temp_directory_path() /
               ("AudioHole-test-storage-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(serial++));
        file = root / "test" / "configuration.json";
        std::filesystem::create_directories(file.parent_path());
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    std::string read() {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), {}};
    }
    void write(const std::string& text) {
        std::ofstream stream(file, std::ios::binary);
        stream << text;
        ASSERT_TRUE(stream.good());
    }
    ConfigurationRecords sample() {
        ConfigurationRecords data;
        Endpoint device;
        device.id = "stable-device";
        device.display_name = "Coffee input";
        device.windows_name = "Windows name";
        device.notes = "Unicode (≧▽≦)\nSecond line";
        device.capture = true;
        device.availability = Availability::Unknown;
        device.formats.push_back({44100, 1, "int16"});
        data.devices.push_back(device);
        Profile record;
        record.id = "profile-1";
        record.display_name = "Saved test";
        record.notes = "Keep this note.";
        Hole hole;
        hole.id = "hole-1";
        hole.display_name = "Duplex";
        hole.duplex = true;
        hole.forward.source = device.id;
        hole.forward.settings.buffer_ms = 17;
        hole.reverse.settings.buffer_ms = 39;
        hole.reverse.settings.format = {96000, 4, "float32"};
        record.holes.push_back(hole);
        data.profiles.push_back(record);
        return data;
    }
    void seed() {
        TestJsonPersistence store(file);
        ConfigurationRecords data;
        ASSERT_TRUE(store.load(data).ok);
        ASSERT_TRUE(store.save(sample()).ok);
    }
};
TEST_F(PersistenceTest, RoundTripPreservesIdsUnicodeIndependentDirectionsAndUnknownAvailability) {
    seed();
    TestJsonPersistence restarted(file);
    ConfigurationRecords data;
    ASSERT_TRUE(restarted.load(data).ok);
    ASSERT_EQ(data.devices.size(), 1u);
    ASSERT_EQ(data.profiles.size(), 1u);
    EXPECT_EQ(data.devices[0].id, "stable-device");
    EXPECT_EQ(data.devices[0].notes, sample().devices[0].notes);
    EXPECT_EQ(data.devices[0].availability, Availability::Unknown);
    const auto& hole = data.profiles[0].holes[0];
    EXPECT_EQ(hole.id, "hole-1");
    EXPECT_TRUE(hole.duplex);
    EXPECT_EQ(hole.forward.settings.buffer_ms, 17);
    EXPECT_EQ(hole.reverse.settings.buffer_ms, 39);
    EXPECT_EQ(hole.reverse.settings.format.rate, 96000);
}
TEST_F(PersistenceTest, LegacyPolicyMigratesExplicitlyAndInactiveManualValuesSurviveRestart) {
    seed();
    auto legacy = nlohmann::ordered_json::parse(read());
    legacy["schema_version"] = 2;
    auto& hole = legacy["profiles"][0]["holes"][0];
    hole.erase("advanced_tuning");
    hole["extension"] = {{"keep", "user data"}};
    hole["forward"]["settings"]["mode"] = "Exclusive";
    hole["forward"]["settings"]["automatic"] = false;
    hole["forward"]["settings"]["custom"] = {{"keep", true}};
    const auto original = legacy.dump(2) + "\n";
    write(original);
    TestJsonPersistence provider(file);
    ConfigurationRecords loaded;
    ASSERT_TRUE(provider.load(loaded).ok);
    EXPECT_EQ(read(), original);
    ASSERT_TRUE(loaded.profiles[0].holes[0].advanced_tuning);
    const auto retained = loaded.profiles[0].holes[0].forward;
    loaded.profiles[0].holes[0].advanced_tuning = false;
    ASSERT_TRUE(provider.save(loaded).ok);
    std::ifstream backup(file.string() + ".v2.bak", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(backup), {}), original);
    TestJsonPersistence restarted(file);
    ASSERT_TRUE(restarted.load(loaded).ok);
    EXPECT_FALSE(loaded.profiles[0].holes[0].advanced_tuning);
    EXPECT_EQ(loaded.profiles[0].holes[0].forward, retained);
    const auto json = nlohmann::ordered_json::parse(read());
    EXPECT_EQ(json.at("schema_version"), 4);
    EXPECT_EQ(json["profiles"][0]["holes"][0]["extension"], hole["extension"]);
    EXPECT_TRUE(json["profiles"][0]["holes"][0]["forward"]["settings"]["custom"]["keep"]);
}
TEST_F(PersistenceTest, V3RequiresAnExplicitBooleanTuningPolicyAndPreservesInvalidInput) {
    seed();
    const auto valid = nlohmann::ordered_json::parse(read());
    for (const auto& value : {nlohmann::ordered_json("yes"), nlohmann::ordered_json(1),
                              nlohmann::ordered_json(nullptr)}) {
        auto invalid = valid;
        invalid["profiles"][0]["holes"][0]["advanced_tuning"] = value;
        const auto bytes = invalid.dump();
        write(bytes);
        TestJsonPersistence provider(file);
        ConfigurationRecords loaded;
        EXPECT_FALSE(provider.load(loaded).ok);
        EXPECT_EQ(read(), bytes);
    }
    auto missing = valid;
    missing["profiles"][0]["holes"][0].erase("advanced_tuning");
    write(missing.dump());
    TestJsonPersistence provider(file);
    ConfigurationRecords loaded;
    EXPECT_FALSE(provider.load(loaded).ok);
}
TEST_F(PersistenceTest, RoutingEngineDefaultsRoundTripsAndRejectsMalformedTypes) {
    seed();
    auto json = nlohmann::ordered_json::parse(read());
    auto& hole = json["profiles"][0]["holes"][0];
    EXPECT_EQ(hole.at("routing_engine"), "automatic");
    hole.erase("routing_engine");
    write(json.dump());
    TestJsonPersistence provider(file);
    ConfigurationRecords records;
    ASSERT_TRUE(provider.load(records).ok);
    EXPECT_EQ(records.profiles[0].holes[0].routing_engine, "automatic");
    records.profiles[0].holes[0].routing_engine = "future-test-engine";
    records.profiles[0].holes[0].advanced_tuning = false;
    ASSERT_TRUE(provider.save(records).ok);
    TestJsonPersistence restarted(file);
    ASSERT_TRUE(restarted.load(records).ok);
    EXPECT_EQ(records.profiles[0].holes[0].routing_engine, "future-test-engine");
    EXPECT_FALSE(records.profiles[0].holes[0].advanced_tuning);
    const auto saved = read();
    for (const auto& invalid :
         {nlohmann::ordered_json(2), nlohmann::ordered_json(""), nlohmann::ordered_json(nullptr)}) {
        auto broken = nlohmann::ordered_json::parse(saved);
        broken["profiles"][0]["holes"][0]["routing_engine"] = invalid;
        const auto bytes = broken.dump();
        write(bytes);
        TestJsonPersistence rejected(file);
        EXPECT_FALSE(rejected.load(records).ok);
        EXPECT_EQ(read(), bytes);
    }
}
TEST_F(PersistenceTest, ControllerActuallyPersistsCreateEditDeleteAndDeviceChangesAcrossInstances) {
    seed();
    std::string created;
    {
        ConfigurationController controller(std::make_unique<TestJsonPersistence>(file));
        EXPECT_EQ(controller.snapshot().operating_mode, OperatingMode::Test);
        created = controller.create_profile("New persistent profile");
        ASSERT_FALSE(created.empty());
        EXPECT_NE(created, "profile-1");
        auto edited = *profile(controller.snapshot(), created);
        edited.notes = "Survives restart";
        ASSERT_TRUE(controller.save(edited, false).ok);
        auto device = *endpoint(controller.snapshot(), "stable-device");
        device.notes = "Persistent device note";
        ASSERT_TRUE(controller.update_endpoint(device).ok);
        EXPECT_FALSE(controller.activate(created).ok);
        EXPECT_FALSE(controller.beep("stable-device").ok);
        EXPECT_FALSE(controller.save(edited, true).ok);
        EXPECT_EQ(controller.snapshot().state, State::Stopped);
    }
    {
        ConfigurationController restarted(std::make_unique<TestJsonPersistence>(file));
        ASSERT_NE(profile(restarted.snapshot(), created), nullptr);
        EXPECT_EQ(profile(restarted.snapshot(), created)->notes, "Survives restart");
        EXPECT_EQ(endpoint(restarted.snapshot(), "stable-device")->notes, "Persistent device note");
        ASSERT_TRUE(restarted.remove_profile(created).ok);
    }
    ConfigurationController again(std::make_unique<TestJsonPersistence>(file));
    EXPECT_EQ(profile(again.snapshot(), created), nullptr);
    EXPECT_NE(profile(again.snapshot(), "profile-1"), nullptr);
}
TEST_F(PersistenceTest, MalformedNewerDuplicateAndFractionalInputNeverGetsOverwritten) {
    seed();
    const auto valid = read();
    for (auto text : {std::string("{broken"), std::string("{}"),
                      std::string(R"({"schema_version":1,"schema_version":2})")}) {
        write(text);
        TestJsonPersistence store(file);
        ConfigurationRecords data;
        EXPECT_FALSE(store.load(data).ok);
        EXPECT_FALSE(store.save(sample()).ok);
        EXPECT_EQ(read(), text);
    }
    auto newer = valid;
    const auto version = newer.find("\"schema_version\": 4");
    ASSERT_NE(version, std::string::npos);
    newer.replace(version, 19, "\"schema_version\": 5");
    write(newer);
    TestJsonPersistence store(file);
    ConfigurationRecords data;
    EXPECT_FALSE(store.load(data).ok);
    EXPECT_FALSE(store.save(sample()).ok);
    EXPECT_EQ(read(), newer);
}
TEST_F(PersistenceTest, ExternalEditsCauseConflictAndLeaveVisibleSavedRecordsUnchanged) {
    seed();
    ConfigurationController controller(std::make_unique<TestJsonPersistence>(file));
    auto external = read();
    external.insert(1, "\"external_extension\":true,");
    write(external);
    auto edited = *profile(controller.snapshot(), "profile-1");
    edited.notes = "Should not commit";
    EXPECT_FALSE(controller.save(edited, false).ok);
    EXPECT_EQ(read(), external);
    EXPECT_EQ(profile(controller.snapshot(), "profile-1")->notes, "Keep this note.");
    EXPECT_TRUE(controller.create_profile("Should fail").empty());
}
TEST_F(PersistenceTest, UnknownFieldsSurviveRecognizedEditsAndInvalidSavesPreserveOriginal) {
    seed();
    auto text = read();
    text.insert(1, "\"future_extension\":{\"marker\":42},");
    const auto location = text.find("\"id\": \"stable-device\"");
    ASSERT_NE(location, std::string::npos);
    text.insert(location, "\"future_device_field\":\"keep me\",");
    write(text);
    TestJsonPersistence store(file);
    ConfigurationRecords data;
    ASSERT_TRUE(store.load(data).ok);
    data.devices[0].display_name = "Edited name";
    ASSERT_TRUE(store.save(data).ok);
    auto saved = read();
    EXPECT_NE(saved.find("future_extension"), std::string::npos);
    EXPECT_NE(saved.find("future_device_field"), std::string::npos);
    data.profiles[0].id.clear();
    EXPECT_FALSE(store.save(data).ok);
    EXPECT_EQ(read(), saved);
}
TEST_F(PersistenceTest, PreviewAndTestRootsStayIndependentAndNoFixtureRecordsLeakIntoTest) {
    const auto production = root / "production.json";
    {
        std::ofstream stream(production);
        stream << "production sentinel";
    }
    SimulatedController preview;
    ConfigurationController test(std::make_unique<TestJsonPersistence>(file));
    EXPECT_TRUE(test.snapshot().profiles.empty());
    EXPECT_TRUE(test.snapshot().endpoints.empty());
    const auto preview_count = preview.snapshot().profiles.size();
    ASSERT_FALSE(test.create_profile("Only Test").empty());
    EXPECT_EQ(preview.snapshot().profiles.size(), preview_count);
    std::ifstream stream(production);
    std::string sentinel;
    std::getline(stream, sentinel);
    EXPECT_EQ(sentinel, "production sentinel");
    EXPECT_FALSE(std::filesystem::exists(root / "preview" / "configuration.json"));
}
TEST_F(PersistenceTest, PreviewPreferencesDefaultEnabledPersistAcrossRestartAndKeepRootsSeparate) {
    BuildPreferences settings(root / "PreviewBuild");
    ASSERT_TRUE(settings.load().ok);
    EXPECT_TRUE(settings.values().preview_mode);
    EXPECT_TRUE(settings.values().show_preview_dialog);
    EXPECT_FALSE(settings.values().hold_expressions);
    EXPECT_EQ(settings.values().animation_loops, 1);
    EXPECT_FALSE(settings.values().loop_until_event);
    auto next = settings.values();
    next.preview_mode = false;
    next.show_preview_dialog = false;
    next.personality_id = "uwu";
    next.appearance = "dark";
    next.hold_expressions = true;
    next.animation_loops = 3;
    next.loop_until_event = true;
    ASSERT_TRUE(settings.update(next).ok);
    auto invalid = next;
    invalid.animation_loops = 0;
    EXPECT_FALSE(settings.update(invalid).ok);
    invalid = next;
    invalid.animation_loops = 21;
    EXPECT_FALSE(settings.update(invalid).ok);
    invalid = next;
    invalid.personality_id = "../invalid";
    EXPECT_FALSE(settings.update(invalid).ok);
    invalid = next;
    invalid.startup_profile_id = std::string(1, static_cast<char>(0xff));
    EXPECT_FALSE(settings.update(invalid).ok);
    BuildPreferences restarted(root / "PreviewBuild");
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_EQ(restarted.values(), next);
    EXPECT_NE(restarted.data_root(OperatingMode::Preview),
              restarted.data_root(OperatingMode::Test));
    EXPECT_EQ(restarted.test_configuration(),
              root / "PreviewBuild" / "test" / "configuration.json");
    EXPECT_FALSE(std::filesystem::exists(root / "configuration.json"));
}
TEST_F(PersistenceTest, PreferenceExternalEditsAndMalformedFilesAreNeverOverwritten) {
    BuildPreferences settings(root);
    ASSERT_TRUE(settings.load().ok);
    auto values = settings.values();
    ASSERT_TRUE(settings.update(values).ok);
    const auto preferences_file = root / "preferences.json";
    {
        std::ofstream stream(preferences_file);
        stream << "{broken";
    }
    values.show_preview_dialog = false;
    EXPECT_FALSE(settings.update(values).ok);
    BuildPreferences restarted(root);
    EXPECT_FALSE(restarted.load().ok);
    EXPECT_FALSE(restarted.update(values).ok);
    std::ifstream stream(preferences_file);
    std::string contents;
    std::getline(stream, contents);
    EXPECT_EQ(contents, "{broken");
}
TEST_F(PersistenceTest, ExpressionHoldPreferenceIsOptionalAndRejectsInvalidTypes) {
    const auto preferences_file = root / "preferences.json";
    const std::string legacy = R"({"format":"audiohole-preview-preferences","schema_version":1,
        "preview_mode":true,"show_preview_dialog":true})";
    {
        std::ofstream stream(preferences_file);
        stream << legacy;
    }
    BuildPreferences compatible(root);
    ASSERT_TRUE(compatible.load().ok);
    EXPECT_FALSE(compatible.values().hold_expressions);
    const auto malformed = legacy.substr(0, legacy.size() - 1) + R"(,"hold_expressions":"yes"})";
    {
        std::ofstream stream(preferences_file);
        stream << malformed;
    }
    BuildPreferences invalid(root);
    EXPECT_FALSE(invalid.load().ok);
    EXPECT_FALSE(invalid.update({}).ok);
    std::ifstream stream(preferences_file);
    const std::string bytes{std::istreambuf_iterator<char>(stream), {}};
    EXPECT_EQ(bytes, malformed);
}
TEST_F(PersistenceTest, TypedTestCommandsRejectUnavailableBackendsAndOnlyCompleteRealSaves) {
    seed();
    ApplicationController application(
        std::make_unique<ConfigurationController>(std::make_unique<TestJsonPersistence>(file)));
    const auto before = read();
    for (Command command :
         {Command(StartRouting{"profile-1"}), Command(DiscoverDevices{}),
          Command(StartBeep{"stable-device"}), Command(UpdateBuffer{"hole-1", false, 5})}) {
        const auto result = application.submit(std::move(command));
        EXPECT_EQ(result.outcome, Outcome::Rejected);
        EXPECT_FALSE(result.simulated);
    }
    EXPECT_EQ(read(), before);
    auto edited = *profile(application.snapshot(), "profile-1");
    edited.notes = "Saved through the shared typed command";
    const auto result = application.submit(SaveConfiguration{edited, false});
    EXPECT_EQ(result.outcome, Outcome::Completed);
    TestJsonPersistence restarted(file);
    ConfigurationRecords records;
    ASSERT_TRUE(restarted.load(records).ok);
    EXPECT_EQ(records.profiles.front().notes, edited.notes);
}
TEST_F(PersistenceTest, FractionalAndOverflowNumericFieldsPreserveOriginalOnFailure) {
    seed();
    const auto valid = read();
    for (const auto& invalid : {"1.5", "18446744073709551615"}) {
        auto broken = valid;
        const auto position = broken.find("\"schema_version\": 4");
        ASSERT_NE(position, std::string::npos);
        broken.replace(position, std::string("\"schema_version\": 4").size(),
                       std::string("\"schema_version\": ") + invalid);
        write(broken);
        TestJsonPersistence provider(file);
        ConfigurationRecords records;
        EXPECT_FALSE(provider.load(records).ok);
        EXPECT_FALSE(provider.save(sample()).ok);
        EXPECT_EQ(read(), broken);
    }
}
TEST_F(PersistenceTest, DirectionBufferApplyPersistsAloneWithoutActivatingAnEngine) {
    seed();
    ApplicationController application(
        std::make_unique<ConfigurationController>(std::make_unique<TestJsonPersistence>(file)));
    const auto original = *profile(application.snapshot(), "profile-1");
    const auto result = application.submit(UpdateBuffer{"hole-1", true, 19, "profile-1"});
    ASSERT_TRUE(result.result.ok);
    EXPECT_EQ(result.outcome, Outcome::Completed);
    EXPECT_FALSE(result.simulated);
    EXPECT_TRUE(application.snapshot().paths.empty());
    EXPECT_FALSE(application.snapshot().armed);
    TestJsonPersistence restarted(file);
    ConfigurationRecords records;
    ASSERT_TRUE(restarted.load(records).ok);
    auto expected = original;
    expected.holes.front().reverse.settings.buffer_ms = 19;
    expected.holes.front().reverse.settings.preferences.common.buffer_ms = 19;
    EXPECT_EQ(records.profiles.front(), expected);
    const auto prior = read();
    write(prior + " "); // External edit must remain protected on this command path too.
    EXPECT_FALSE(application.submit(UpdateBuffer{"hole-1", false, 12, "profile-1"}).result.ok);
    EXPECT_EQ(read(), prior + " ");
    EXPECT_EQ(*profile(application.snapshot(), "profile-1"), expected);
}
TEST_F(PersistenceTest, V1LoadsWithoutWritesAndExplicitSavePreservesOriginalBeforeUpgrade) {
    seed();
    auto legacy = read();
    const auto version = legacy.find("\"schema_version\": 4");
    ASSERT_NE(version, std::string::npos);
    legacy.replace(version, 19, "\"schema_version\": 1");
    write(legacy);
    TestJsonPersistence provider(file);
    ConfigurationRecords records;
    ASSERT_TRUE(provider.load(records).ok);
    EXPECT_EQ(read(), legacy);
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".v1.bak"));
    ASSERT_TRUE(provider.save(records).ok);
    std::ifstream backup(file.string() + ".v1.bak", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(backup), {}), legacy);
    EXPECT_NE(read().find("\"schema_version\": 4"), std::string::npos);
    const auto bytes = read();
    EXPECT_LT(bytes.find("\"profiles\""), bytes.find("\"devices\""));
    EXPECT_LT(bytes.find("\"display_name\""), bytes.find("\"notes\""));
    EXPECT_LT(bytes.find("\"notes\""), bytes.find("\"id\""));
}
TEST_F(PersistenceTest, ExactDescriptorRoundTripsAndInvalidEditsPreserveOriginal) {
    seed();
    TestJsonPersistence provider(file);
    ConfigurationRecords records;
    ASSERT_TRUE(provider.load(records).ok);
    auto& d = records.profiles[0].holes[0].forward;
    d.source_name = "Microphone";
    d.destination_name = "Four channel output";
    d.settings.automatic = false;
    d.settings.format = {
        48000, 4, "pcm_s32",
        WaveFormat{32, 24, 65534, 51, "{00000001-0000-0010-8000-00AA00389B71}", 16, 768000}};
    ASSERT_TRUE(provider.save(records).ok);
    TestJsonPersistence restarted(file);
    ConfigurationRecords loaded;
    ASSERT_TRUE(restarted.load(loaded).ok);
    EXPECT_EQ(records.profiles, loaded.profiles);
    auto original = read();
    d.settings.format.wave->block_align_bytes = 8;
    EXPECT_FALSE(provider.save(records).ok);
    EXPECT_EQ(read(), original);
    d.settings.format.wave->block_align_bytes = 16;
    d.settings.format.wave->channel_mask.reset();
    ASSERT_TRUE(provider.save(records).ok);
    TestJsonPersistence unknown(file);
    ASSERT_TRUE(unknown.load(loaded).ok);
    EXPECT_FALSE(loaded.profiles[0].holes[0].forward.settings.format.wave->channel_mask);
}
TEST_F(PersistenceTest, ConflictingV1BackupBlocksUpgradeWithoutChangingEitherFile) {
    seed();
    auto legacy = read();
    legacy.replace(legacy.find("\"schema_version\": 4"), 19, "\"schema_version\": 1");
    write(legacy);
    const auto backup = file.string() + ".v1.bak";
    std::ofstream(backup, std::ios::binary) << "Earlier user backup";
    TestJsonPersistence provider(file);
    ConfigurationRecords records;
    ASSERT_TRUE(provider.load(records).ok);
    EXPECT_FALSE(provider.save(records).ok);
    EXPECT_EQ(read(), legacy);
    std::ifstream input(backup, std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(input), {}), "Earlier user backup");
}
TEST_F(PersistenceTest, V1FormatExtensionsSurviveExplicitUpgrade) {
    seed();
    auto old = nlohmann::ordered_json::parse(read());
    old["schema_version"] = 1;
    auto& f = old["devices"][0]["formats"][0];
    f.erase("wave");
    f["user_annotation"] = "Keep my external format notes";
    write(old.dump(2));
    TestJsonPersistence provider(file);
    ConfigurationRecords records;
    ASSERT_TRUE(provider.load(records).ok);
    ASSERT_TRUE(provider.save(records).ok);
    const auto saved = nlohmann::ordered_json::parse(read());
    EXPECT_EQ(saved["devices"][0]["formats"][0]["user_annotation"],
              "Keep my external format notes");
    EXPECT_TRUE(saved["devices"][0]["formats"][0]["wave"].is_null());
}
} // namespace
