#include "model/build_preferences.h"
#include "model/controller.h"
#include "model/persistence.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace audiohole;
namespace {
using Json = nlohmann::ordered_json;
struct ProfileStorage : testing::Test {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("profile-builder-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path file = root / "configuration.json";
    ~ProfileStorage() override {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    ConfigurationRecords sample() {
        Hole h;
        h.id = "hole";
        h.display_name = "New Hole";
        h.forward.source = "capture-id";
        h.forward.destination = "render-id";
        Profile p;
        p.id = "profile";
        p.display_name = "VR Streaming";
        p.notes = "Keep µ notes";
        p.holes.push_back(h);
        return {{}, {p}};
    }
    std::string read(const std::filesystem::path& p) {
        std::ifstream stream(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), {}};
    }
    void write(const std::filesystem::path& p, const std::string& bytes) {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << bytes;
    }
};
TEST_F(ProfileStorage, NamedFilesRenamePreservingIdentityUnknownFieldsAndExternalEditProtection) {
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(sample()).ok);
    const auto first = root / "profiles" / "VR Streaming.json";
    ASSERT_TRUE(std::filesystem::exists(first));
    EXPECT_FALSE(std::filesystem::exists(file));
    auto json = Json::parse(read(first));
    json["profile"]["extension"] = {{"keep", 42}};
    write(first, json.dump(2));
    ASSERT_FALSE(store.save(sample()).ok);
    EXPECT_EQ(Json::parse(read(first))["profile"]["extension"]["keep"], 42);
    ASSERT_TRUE(store.load(records).ok);
    records.profiles[0].display_name = "Headset";
    ASSERT_TRUE(store.save(records).ok);
    const auto renamed = root / "profiles" / "Headset.json";
    EXPECT_FALSE(std::filesystem::exists(first));
    ASSERT_TRUE(std::filesystem::exists(renamed));
    EXPECT_EQ(Json::parse(read(renamed))["profile"]["extension"]["keep"], 42);
    ProfileDirectoryPersistence restarted(file);
    ConfigurationRecords loaded;
    ASSERT_TRUE(restarted.load(loaded).ok);
    EXPECT_EQ(loaded.profiles, records.profiles);
}
TEST_F(ProfileStorage,
       V3MigrationIsReadOnlyUntilSaveAndPreservesInactiveManualPolicyAndOriginalBytes) {
    TestJsonPersistence legacy(file);
    ConfigurationRecords records;
    ASSERT_TRUE(legacy.load(records).ok);
    auto original = sample();
    auto& h = original.profiles[0].holes[0];
    h.advanced_tuning = false;
    h.forward.settings.preferences.common = {44100, 6, {}, "pcm_s16", {}};
    h.forward.settings.refresh_projection();
    ASSERT_TRUE(legacy.save(original).ok);
    auto json = Json::parse(read(file));
    json["schema_version"] = 3;
    for (const auto* which : {"forward", "reverse"}) {
        auto& settings = json["profiles"][0]["holes"][0][which]["settings"];
        settings["automatic"] = which == std::string("reverse");
        settings["format"] = {
            {"rate", 44100}, {"channels", 6}, {"representation", "pcm_s16"}, {"wave", nullptr}};
        settings["mode"] = "Automatic";
        settings["buffer_ms"] = 0;
        settings.erase("preferences");
    }
    const auto bytes = json.dump(2);
    write(file, bytes);
    ProfileDirectoryPersistence migration(file);
    ASSERT_TRUE(migration.load(records).ok);
    EXPECT_EQ(read(file), bytes);
    EXPECT_FALSE(std::filesystem::exists(root / "profiles"));
    ASSERT_TRUE(migration.save(records).ok);
    EXPECT_EQ(read(root / "local" / "legacy-profiles" / "configuration.json"), bytes);
    EXPECT_FALSE(std::filesystem::exists(file));
    EXPECT_FALSE(records.profiles[0].holes[0].advanced_tuning);
    EXPECT_EQ(records.profiles[0].holes[0].forward.settings.preferences.common.channel_count, 6);
}
TEST_F(ProfileStorage, CollisionOrFailedStageRetainsCompleteOriginalGeneration) {
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    records = sample();
    ASSERT_TRUE(store.save(records).ok);
    std::string before;
    ASSERT_TRUE(read_profile_document(file, before).ok);
    auto duplicate = records.profiles[0];
    duplicate.id = "other";
    duplicate.holes.clear();
    duplicate.display_name = "vr streaming";
    records.profiles.push_back(duplicate);
    EXPECT_FALSE(store.save(records).ok);
    std::string after;
    ASSERT_TRUE(read_profile_document(file, after).ok);
    EXPECT_EQ(before, after);
    auto changed = Json::parse(before);
    changed["profiles"][0]["notes"] = "new";
    auto failed = write_profile_document(
        file, changed.dump(), before,
        [](const auto&, const auto&, const auto&) { return Result{false, "injected failure"}; });
    EXPECT_FALSE(failed.ok);
    ASSERT_TRUE(read_profile_document(file, after).ok);
    EXPECT_EQ(before, after);
}
TEST_F(ProfileStorage, InterruptedDirectoryRenameRecoversPreviousGeneration) {
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(sample()).ok);
    std::filesystem::rename(root / "profiles", root / ".profiles-previous");
    write(root / ".profiles-next" / "unfinished.json", "partial staging");
    ProfileDirectoryPersistence recovered(file);
    ASSERT_TRUE(recovered.load(records).ok);
    EXPECT_EQ(records.profiles[0].id, "profile");
    ASSERT_TRUE(recovered.save(records).ok);
    EXPECT_FALSE(std::filesystem::exists(root / ".profiles-next"));
}
TEST_F(ProfileStorage, CurrentDirectionsNeedOnlyEndpointAssignments) {
    TestJsonPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(sample()).ok);
    auto json = Json::parse(read(file));
    auto& hole = json["profiles"][0]["holes"][0];
    hole["forward"].erase("settings");
    hole["reverse"]["settings"] = Json::object();
    ASSERT_TRUE(validate_test_document(json.dump(), records).ok);
    EXPECT_TRUE(records.profiles[0].holes[0].forward.settings.automatic);
    EXPECT_EQ(records.profiles[0].holes[0].reverse.settings.buffer_ms, 0);
}
TEST_F(ProfileStorage, CurrentNullPreferencesNeverReviveLegacyFields) {
    TestJsonPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(sample()).ok);
    auto document = Json::parse(read(file));
    auto& settings = document["profiles"][0]["holes"][0]["forward"]["settings"];
    settings["automatic"] = false;
    settings["format"] = {{"rate", 96000}, {"channels", 8}, {"representation", "float32"}};
    settings["mode"] = "Exclusive";
    settings["buffer_ms"] = 3;
    ASSERT_TRUE(validate_test_document(document.dump(), records).ok);
    EXPECT_EQ(records.profiles[0].holes[0].forward.settings.preferences, Preferences{});
    settings.erase("preferences");
    ASSERT_TRUE(validate_test_document(document.dump(), records).ok);
    EXPECT_EQ(records.profiles[0].holes[0].forward.settings.preferences, Preferences{});
    // Obsolete fields cannot affect current validity either.
    settings["buffer_ms"] = "malformed obsolete value";
    ASSERT_TRUE(validate_test_document(document.dump(), records).ok);
    write(file, document.dump());
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(records).ok);
    const auto current = Json::parse(read(file))["profiles"][0]["holes"][0]["forward"]["settings"];
    ASSERT_EQ(current.size(), 1u);
    EXPECT_TRUE(current.contains("preferences"));
}
TEST_F(ProfileStorage, DocumentedIndividualRecordExpressesAutomaticIntent) {
    const auto example = std::filesystem::path(AUDIOHOLE_TEST_PERSONALITIES_DIR).parent_path() /
                         "docs" / "test-profile-example.json";
    Profile profile;
    ASSERT_TRUE(validate_profile_record(read(example), profile).ok);
    ASSERT_EQ(profile.holes.size(), 1u);
    EXPECT_FALSE(profile.holes[0].advanced_tuning);
    EXPECT_EQ(profile.holes[0].forward.settings.preferences, Preferences{});
    EXPECT_EQ(profile.holes[0].routing_engine, "automatic");
    EXPECT_FALSE(profile.holes[0].forward.source.empty());
    EXPECT_FALSE(profile.holes[0].forward.destination.empty());
}
TEST_F(ProfileStorage, V4MigrationTranslatesIntentOnceWithoutPromotingAutomaticCache) {
    TestJsonPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(sample()).ok);
    auto document = Json::parse(read(file));
    document["schema_version"] = 4;
    auto& settings = document["profiles"][0]["holes"][0]["forward"]["settings"];
    settings["automatic"] = true;
    settings["format"] = {{"rate", 48000},
                          {"channels", 2},
                          {"representation", "float32"},
                          {"vendor_note", "preserve me"},
                          {"wave", {{"owner_note", "µ"}}}};
    settings["mode"] = "Shared";
    settings["buffer_ms"] = 17;
    ASSERT_TRUE(validate_test_document(document.dump(), records).ok);
    const auto& p = records.profiles[0].holes[0].forward.settings.preferences.common;
    EXPECT_FALSE(p.sample_rate_hz);
    EXPECT_FALSE(p.channel_count);
    EXPECT_FALSE(p.sample_format);
    EXPECT_EQ(p.mode, "Shared");
    EXPECT_EQ(p.buffer_ms, 17);
    settings["preferences"]["common"]["buffer_ms"] = 23;
    write(file, document.dump());
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(records).ok);
    const auto migrated = Json::parse(read(file));
    const auto& saved = migrated["profiles"][0]["holes"][0]["forward"]["settings"];
    EXPECT_EQ(migrated["schema_version"], 5);
    EXPECT_EQ(saved["preferences"]["common"]["buffer_ms"], 23);
    EXPECT_TRUE(saved["preferences"]["common"]["sample_rate_hz"].is_null());
    EXPECT_FALSE(saved.contains("format"));
    EXPECT_EQ(saved["legacy_format_extensions"]["vendor_note"], "preserve me");
    EXPECT_EQ(saved["legacy_format_extensions"]["wave"]["owner_note"], "µ");
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(records).ok);
    EXPECT_EQ(Json::parse(read(file)), migrated);
}
TEST_F(ProfileStorage, IndividualRecordVersionsAndEnvelopeExtensionsSurviveTransfer) {
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    ASSERT_TRUE(store.save(sample()).ok);
    const auto first = root / "profiles" / "VR Streaming.json";
    auto record = Json::parse(read(first));
    EXPECT_EQ(record["format"], "audiohole-profile");
    EXPECT_EQ(record["schema_version"], 1);
    EXPECT_FALSE(record.contains("profiles"));
    Profile output;
    ASSERT_TRUE(validate_profile_record(record.dump(), output).ok);
    EXPECT_EQ(output.notes, "Keep µ notes");
    auto future = record;
    future["schema_version"] = 2;
    EXPECT_FALSE(validate_profile_record(future.dump(), output).ok);
    EXPECT_EQ(output.id, "profile");
    EXPECT_FALSE(validate_test_document(record.dump(), records).ok);
    record["owner_extension"] = {{"notes", "preserve envelope"}};
    record["profile"]["holes"][0]["forward"]["settings"]["preferences"]["source"]["vendor"] = 42;
    write(first, record.dump());
    ASSERT_TRUE(store.load(records).ok);
    std::string transfer;
    ASSERT_TRUE(read_profile_document(file, transfer).ok);
    EXPECT_EQ(Json::parse(transfer)["schema_version"], 5);
    ProfileDirectoryPersistence target(root / "imported" / "configuration.json");
    ConfigurationRecords empty;
    ASSERT_TRUE(target.load(empty).ok);
    std::string expected;
    ASSERT_TRUE(read_profile_document(root / "imported" / "configuration.json", expected).ok);
    ASSERT_TRUE(
        write_profile_document(root / "imported" / "configuration.json", transfer, expected).ok);
    const auto imported = Json::parse(read(root / "imported" / "profiles" / "VR Streaming.json"));
    EXPECT_EQ(imported["owner_extension"], record["owner_extension"]);
    EXPECT_EQ(
        imported["profile"]["holes"][0]["forward"]["settings"]["preferences"]["source"]["vendor"],
        42);
    records.profiles[0].display_name = "Renamed";
    ASSERT_TRUE(store.save(records).ok);
    EXPECT_EQ(Json::parse(read(root / "profiles" / "Renamed.json"))["owner_extension"],
              record["owner_extension"]);
}
TEST(ProfileBuilder, EndpointInheritanceAndOverrideSwitchAreIndependentFromEngineSelection) {
    Snapshot snapshot;
    snapshot.operating_mode = OperatingMode::Test;
    for (bool capture : {true, false}) {
        Endpoint endpoint;
        endpoint.id = capture ? "source" : "destination";
        endpoint.capture = capture;
        endpoint.inspection = std::make_shared<DeviceInspection>();
        snapshot.endpoints.push_back(endpoint);
    }
    Profile profile;
    profile.id = "profile";
    profile.display_name = "Contract";
    Hole hole;
    hole.id = "hole";
    hole.forward.source = hole.reverse.source = "source";
    hole.forward.destination = hole.reverse.destination = "destination";
    hole.duplex = true;
    hole.advanced_tuning = true;
    auto& p = hole.forward.settings.preferences;
    p.common = {48000, 2, 20, "float32", "Shared"};
    p.source = {44100, {}, 10, {}, "Exclusive"};
    p.destination = {{}, 4, {}, "pcm_s16", {}};
    p.channel_map = {{0, 2}, {1, 3}};
    // Transient projections have no authority.
    hole.forward.settings.automatic = true;
    hole.forward.settings.mode = "Exclusive";
    hole.forward.settings.buffer_ms = 99;
    profile.holes.push_back(hole);
    PreparedProfile request;
    ASSERT_TRUE(prepare_profile(snapshot, profile, request).ok);
    ASSERT_EQ(request.paths.size(), 2u);
    EXPECT_EQ(request.paths[0].source_requirements,
              (EndpointPreferences{44100, 2, 10, "float32", "Exclusive"}));
    EXPECT_EQ(request.paths[0].destination_requirements,
              (EndpointPreferences{48000, 4, 20, "pcm_s16", "Shared"}));
    EXPECT_EQ(request.paths[0].channel_policy, "explicit_index_mapping");
    EXPECT_EQ(request.paths[1].source_requirements, EndpointPreferences{});
    EXPECT_EQ(request.paths[1].channel_policy, "default_index_mapping");
    profile.holes[0].advanced_tuning = false;
    ASSERT_TRUE(prepare_profile(snapshot, profile, request).ok);
    EXPECT_EQ(request.paths[0].preferences, Preferences{});
    EXPECT_EQ(request.paths[0].source_requirements, EndpointPreferences{});
    EXPECT_EQ(request.paths[0].mode, "Shared");
    EXPECT_EQ(request.paths[0].buffer_ms, 0);
    EXPECT_EQ(profile.holes[0].forward.settings.preferences, p);
    profile.holes[0].routing_engine = "future-engine";
    EXPECT_FALSE(prepare_profile(snapshot, profile, request).ok);
    EXPECT_EQ(request.paths.size(), 2u);
    profile.holes[0].enabled = false;
    ASSERT_TRUE(prepare_profile(snapshot, profile, request).ok);
    EXPECT_TRUE(request.paths.empty());
}
TEST(ProfileBuilder, ExplicitUnsupportedRequirementsDoNotSilentlyFallBack) {
    Snapshot snapshot;
    snapshot.operating_mode = OperatingMode::Test;
    for (bool capture : {true, false}) {
        Endpoint endpoint;
        endpoint.id = capture ? "source" : "destination";
        endpoint.capture = capture;
        endpoint.inspection = std::make_shared<DeviceInspection>();
        snapshot.endpoints.push_back(endpoint);
    }
    Profile profile;
    profile.id = "profile";
    profile.display_name = "Contract";
    Hole hole;
    hole.id = "hole";
    hole.forward.source = "source";
    hole.forward.destination = "destination";
    hole.advanced_tuning = true;
    hole.forward.settings.preferences.source = {96000, 2, {}, "float32", "Shared"};
    profile.holes.push_back(hole);
    PreparedProfile request;
    ASSERT_TRUE(prepare_profile(snapshot, profile, request).ok); // No evidence is not rejection.
    snapshot.endpoints[0].capabilities.push_back(
        {Format{96000, 2, "float32"}, "Shared", FormatSupport::Unsupported});
    const auto result = prepare_profile(snapshot, profile, request);
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.detail.find("unsupported"), std::string::npos);
    EXPECT_EQ(request.paths[0].source_requirements.sample_rate_hz, 96000);
    profile.holes[0].advanced_tuning = false;
    EXPECT_TRUE(prepare_profile(snapshot, profile, request).ok);
}
TEST_F(ProfileStorage, PartialPreferencesNormalizeAutomaticAndRejectMalformedValues) {
    TestJsonPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    auto data = sample();
    auto& h = data.profiles[0].holes[0];
    h.advanced_tuning = true;
    h.forward.settings.preferences.common.channel_count = 2;
    h.forward.settings.preferences.source.sample_rate_hz = 44100;
    h.forward.settings.preferences.destination.sample_rate_hz = 48000;
    h.forward.settings.preferences.channel_map = {{0, 0}, {1, 1}};
    ASSERT_TRUE(store.save(data).ok);
    auto json = Json::parse(read(file));
    auto& common = json["profiles"][0]["holes"][0]["forward"]["settings"]["preferences"]["common"];
    common["sample_rate_hz"] = "";
    common["sample_format"] = "Automatic";
    common["buffer_ms"] = 0;
    ASSERT_TRUE(validate_test_document(json.dump(), records).ok);
    EXPECT_FALSE(records.profiles[0].holes[0].forward.settings.preferences.common.sample_rate_hz);
    common["sample_rate_hz"] = 48000.5;
    EXPECT_FALSE(validate_test_document(json.dump(), records).ok);
    common["sample_rate_hz"] = nullptr;
    json["profiles"][0]["holes"][0]["forward"]["settings"]["preferences"]["channel_map"][0]
        ["source"] = -1;
    EXPECT_FALSE(validate_test_document(json.dump(), records).ok);
}
TEST(ProfileBuilder, AutomaticRequiresNoDeepEvidenceAndRetainsIndependentPartialIntent) {
    Snapshot snapshot;
    snapshot.operating_mode = OperatingMode::Test;
    for (bool capture : {true, false}) {
        Endpoint e;
        e.id = capture ? "capture" : "render";
        e.capture = capture;
        e.inspection = std::make_shared<DeviceInspection>();
        snapshot.endpoints.push_back(e);
    }
    Hole h;
    h.id = "hole";
    h.duplex = true;
    h.advanced_tuning = true;
    h.forward.source = h.reverse.source = "capture";
    h.forward.destination = h.reverse.destination = "render";
    h.forward.settings.preferences.common.channel_count = 2;
    h.forward.settings.preferences.channel_map = {{0, 0}, {1, 1}};
    h.reverse.settings.preferences.source.sample_rate_hz = 44100;
    h.reverse.settings.preferences.destination.sample_rate_hz = 48000;
    Profile p;
    p.id = "profile";
    p.display_name = "Test";
    p.holes.push_back(h);
    PreparedProfile request;
    ASSERT_TRUE(prepare_profile(snapshot, p, request).ok);
    ASSERT_EQ(request.paths.size(), 2u);
    EXPECT_TRUE(request.paths[0].runtime_negotiation);
    EXPECT_EQ(request.paths[0].mode, "Shared");
    EXPECT_EQ(request.paths[0].buffer_ms, 0);
    EXPECT_FALSE(request.paths[0].preferences.common.sample_rate_hz);
    EXPECT_EQ(request.paths[1].preferences.source.sample_rate_hz, 44100);
    EXPECT_EQ(request.paths[1].preferences.destination.sample_rate_hz, 48000);
    snapshot.endpoints[0].availability = Availability::Unavailable;
    EXPECT_TRUE(validate(snapshot, p).ok);
    auto prior = request.paths;
    EXPECT_FALSE(prepare_profile(snapshot, p, request).ok);
    EXPECT_EQ(request.paths.size(), prior.size());
}
TEST(ProfileBuilder, BufferOnlyApplyUpdatesExistingCanonicalPreference) {
    SimulatedController controller;
    auto candidate = controller.snapshot().profiles.front();
    auto& hole = candidate.holes.front();
    hole.advanced_tuning = true;
    hole.forward.settings.buffer_ms = 17;
    hole.forward.settings.preferences.common.buffer_ms = 17;
    ASSERT_TRUE(controller.save(candidate, false).ok);
    ASSERT_TRUE(controller.update_buffer(candidate.id, hole.id, false, 14).ok);
    const auto& saved = profile(controller.snapshot(), candidate.id)->holes.front();
    EXPECT_EQ(saved.forward.settings.buffer_ms, 14);
    EXPECT_EQ(saved.forward.settings.preferences.common.buffer_ms, 14);
}
TEST(ProfileBuilder, BufferApplyFromAutomaticMatchesEditorAndPreservesTheOtherDirection) {
    for (bool reverse : {false, true}) {
        SCOPED_TRACE(reverse ? "Return" : "Forward");
        SimulatedController controller;
        auto candidate = *profile(controller.snapshot(), "profile-duplex");
        candidate.holes.front().advanced_tuning = true;
        ASSERT_TRUE(controller.save(candidate, false).ok);
        ASSERT_TRUE(controller.activate(candidate.id).ok);
        controller.advance(300);
        const auto hole_id = candidate.holes.front().id;
        for (int milliseconds : {13, 0}) {
            auto editor = *profile(controller.snapshot(), candidate.id);
            auto& settings =
                (reverse ? editor.holes.front().reverse : editor.holes.front().forward).settings;
            settings.buffer_ms = milliseconds;
            settings.preferences.common.buffer_ms =
                milliseconds ? std::optional<int>(milliseconds) : std::nullopt;
            ASSERT_TRUE(controller.update_buffer(candidate.id, hole_id, reverse, milliseconds).ok);
            // The GUI uses this comparison to clear its unsaved marker after a
            // buffer-only Apply. Matching intent must have matching storage.
            const auto& stored = profile(controller.snapshot(), candidate.id)->holes.front();
            const auto& stored_settings = (reverse ? stored.reverse : stored.forward).settings;
            EXPECT_EQ(stored_settings.buffer_ms, settings.buffer_ms);
            EXPECT_EQ(stored_settings.preferences.common.buffer_ms,
                      settings.preferences.common.buffer_ms);
            EXPECT_EQ(*profile(controller.snapshot(), candidate.id), editor);
            for (const auto& path : controller.snapshot().paths)
                EXPECT_EQ(path.applied_buffer_ms,
                          path.reverse == reverse && milliseconds ? milliseconds : 6);
        }
    }
}
TEST(ProfileBuilder, DerivesTransportDetailsAndNeverGuessesChannelLayout) {
    EndpointPreferences p;
    p.sample_rate_hz = 48000;
    p.channel_count = 2;
    p.sample_format = "pcm_s24_packed";
    const auto packed = derive_pcm_format(p);
    ASSERT_TRUE(packed);
    EXPECT_EQ(packed->wave->block_align_bytes, 6);
    EXPECT_EQ(packed->wave->average_bytes_per_second, 288000);
    EXPECT_FALSE(packed->wave->channel_mask);
    p.sample_format = "pcm_s24_in_32";
    const auto container = derive_pcm_format(p);
    ASSERT_TRUE(container);
    EXPECT_EQ(container->wave->container_bits, 32);
    EXPECT_EQ(container->wave->valid_bits, 24);
    EXPECT_EQ(container->wave->block_align_bytes, 8);
    p.sample_rate_hz.reset();
    EXPECT_FALSE(derive_pcm_format(p));
}
TEST(ProfileBuilder, DefaultNamesUseNextAvailableSuffixWithoutChangingIds) {
    SimulatedController controller;
    const auto a = controller.create_profile("New Profile");
    const auto b = controller.create_profile("New Profile");
    EXPECT_NE(a, b);
    EXPECT_EQ(profile(controller.snapshot(), a)->display_name, "New Profile");
    EXPECT_EQ(profile(controller.snapshot(), b)->display_name, "New Profile (1)");
    EXPECT_EQ(unique_name("New Hole", {"New Hole", "New Hole (1)", "New Hole (3)"}),
              "New Hole (2)");
}
} // namespace
