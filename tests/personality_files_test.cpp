#include "model/controller.h"
#include "presentation/messages.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <set>

namespace presentation = audiohole::presentation;
TEST(PersonalityMetadata, BotherPoolsHaveExactCategoryCountsAndSelectionMetadata) {
    presentation::Messages messages(7);
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (const auto& id : {"default", "uwu"}) {
        const auto pool = messages.variants(presentation::Message::Bother, id);
        ASSERT_EQ(pool.size(), 30u);
        std::map<std::string, int> counts;
        std::set<std::string> identifiers;
        for (const auto& response : pool) {
            EXPECT_EQ(response.category, "bother");
            EXPECT_TRUE(identifiers.insert(response.id).second);
            EXPECT_FALSE(response.expression.empty());
            EXPECT_EQ(response.fallback_expression, "idle");
            ++counts[response.subcategory];
        }
        EXPECT_EQ(counts["general"], 10);
        EXPECT_EQ(counts["tips"], 10);
        EXPECT_EQ(counts["easter_egg"], 5);
        EXPECT_EQ(counts["invented_fact"], 5);
        const auto first = messages.show(presentation::Message::Bother, id, 1);
        const auto same = messages.show(presentation::Message::Bother, id, 1);
        const auto next = messages.show(presentation::Message::Bother, id, 2);
        EXPECT_EQ(first.response_id, same.response_id);
        EXPECT_NE(first.response_id, next.response_id);
        EXPECT_EQ(first.personality_id, id);
        EXPECT_FALSE(first.expression.empty());
    }
}
namespace {
// Each test owns its temporary directory; cleanup runs even on a failed assertion.
class PersonalityFiles : public testing::Test {
  protected:
    std::filesystem::path directory;
    void SetUp() override {
        static std::atomic<unsigned> serial{0};
        directory = std::filesystem::temp_directory_path() /
                    ("AudioHole-personalities-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "-" + std::to_string(serial++));
        std::filesystem::create_directories(directory);
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::filesystem::path write(const std::string& bytes,
                                const std::string& filename = "custom.json") {
        const auto path = directory / filename;
        std::ofstream stream(path, std::ios::binary);
        stream << bytes;
        stream.close();
        EXPECT_TRUE(stream.good());
        return path;
    }
};
} // namespace
TEST_F(PersonalityFiles, OnlyVanillaIsBuiltInAndNeverConsumesRandomness) {
    presentation::Messages messages([](unsigned) -> unsigned {
        ADD_FAILURE() << "Vanilla/fallback consumed randomness";
        return 0;
    });
    EXPECT_TRUE(messages.available("vanilla"));
    EXPECT_FALSE(messages.available("default"));
    EXPECT_FALSE(messages.available("uwu"));
    for (int index = 0; index < static_cast<int>(presentation::Message::Count); ++index) {
        const auto event = static_cast<presentation::Message>(index);
        EXPECT_EQ(messages.show(event, "vanilla", 1).technical, presentation::technical(event));
        EXPECT_TRUE(messages.show(event, "vanilla", 2).commentary.empty());
        const auto missing = messages.show(event, "absent", 1);
        EXPECT_TRUE(missing.commentary.empty());
        EXPECT_FALSE(missing.fallback_detail.empty());
    }
}
TEST_F(PersonalityFiles, MissingEventsFallBackWithoutDeactivatingCustomPersonality) {
    presentation::Messages messages;
    auto result = messages.load_file(write(
        R"({"schema_version":1,"id":"custom","display_name":"Custom voice","events":{"ready":["Custom ready."]}})"));
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(messages.show(presentation::Message::Ready, "custom", 1).commentary, "Custom ready.");
    const auto missing = messages.show(presentation::Message::ScanStarted, "custom", 2);
    EXPECT_EQ(missing.technical, presentation::technical(presentation::Message::ScanStarted));
    EXPECT_TRUE(missing.commentary.empty());
    EXPECT_FALSE(missing.fallback_detail.empty());
    EXPECT_TRUE(messages.available("custom"));
    EXPECT_EQ(messages.show(presentation::Message::Ready, "custom", 3).commentary, "Custom ready.");
}
TEST_F(PersonalityFiles, ResponseMetadataIsValidatedBeforePublishing) {
    presentation::Messages messages;
    const std::string prefix =
        R"({"schema_version":2,"id":"custom","display_name":"Custom","events":{"ready":[)";
    const std::string suffix = "]}}";
    const std::string valid =
        R"({"id":"custom_001","category":"status","text":"Ready.","expression":"happy","fallback_expression":"idle"})";
    ASSERT_TRUE(messages.load_file(write(prefix + valid + suffix)).ok);
    const auto selected = messages.show(presentation::Message::Ready, "custom", 1);
    EXPECT_EQ(selected.response_id, "custom_001");
    EXPECT_EQ(selected.expression, "happy");
    EXPECT_EQ(selected.fallback_expression, "idle");
    for (
        const auto& invalid :
        {prefix + valid + "," + valid + suffix,
         prefix +
             R"({"id":"custom_002","category":"status","text":"Bad face.","expression":"invented_face"})" +
             suffix,
         prefix + R"({"category":"status","text":"No ID.","expression":"happy"})" + suffix}) {
        EXPECT_FALSE(messages.load_file(write(invalid)).ok);
        EXPECT_FALSE(messages.available("custom"));
    }
}
TEST_F(PersonalityFiles, InvalidFilesCannotOverrideFactsOrActivatePartialCatalogs) {
    presentation::Messages messages;
    for (
        const auto& bytes :
        {std::string("{bad JSON"),
         std::string(R"({"schema_version":999,"id":"custom","display_name":"Custom","events":{}})"),
         std::string(
             R"({"schema_version":1,"id":"vanilla","display_name":"Override","events":{}})"),
         std::string(
             R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":["Allowed",{"text":"Bad","weight":0}]}})"),
         std::string(
             R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":[]}})"),
         std::string(
             R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":["Duplicate","Duplicate"]}})"),
         std::string(
             R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{},"technical":"False success"})"),
         std::string(
             R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":["Control\u001b"]}})"),
         std::string(256 * 1024 + 1, ' ')}) {
        EXPECT_FALSE(messages.load_file(write(bytes)).ok);
        EXPECT_FALSE(messages.available("custom"));
        EXPECT_EQ(messages.show(presentation::Message::Ready, "custom", 1).technical,
                  presentation::technical(presentation::Message::Ready));
    }
}
TEST_F(PersonalityFiles, ReloadDropsDeletedAndMangledFilesWithoutRetainingStaleData) {
    presentation::Messages messages;
    const std::string valid =
        R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":["Ready."]}})";
    write(valid);
    EXPECT_TRUE(messages.load_directory(directory).empty());
    EXPECT_TRUE(messages.available("custom"));
    write("{broken");
    EXPECT_FALSE(messages.load_directory(directory).empty());
    EXPECT_FALSE(messages.available("custom"));
    write(valid);
    messages.load_directory(directory);
    EXPECT_TRUE(messages.available("custom"));
    std::filesystem::remove(directory / "custom.json");
    messages.load_directory(directory);
    EXPECT_FALSE(messages.available("custom"));
    EXPECT_TRUE(messages.available("vanilla"));
}
TEST_F(PersonalityFiles, WeightedSelectionAndSinglePoolAreDeterministic) {
    presentation::Messages messages([](unsigned upper) { return upper - 1; });
    ASSERT_TRUE(
        messages
            .load_file(write(
                R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":[{"text":"Common","weight":9},{"text":"Rare","weight":1}],"dfa":["Only one"]}})"))
            .ok);
    EXPECT_EQ(messages.show(presentation::Message::Ready, "custom", 1).commentary, "Rare");
    EXPECT_EQ(messages.show(presentation::Message::Ready, "custom", 2).commentary, "Common");
    EXPECT_EQ(messages.show(presentation::Message::Dfa, "custom", 1).commentary, "Only one");
    EXPECT_EQ(messages.show(presentation::Message::Dfa, "custom", 2).commentary, "Only one");
}
TEST_F(PersonalityFiles, AuthoredPoolsCoverAllEventsAndDfaPositionsExactly) {
    presentation::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (const auto& id : {"default", "uwu"}) {
        EXPECT_EQ(messages.variants(presentation::Message::Dfa, id).size(), 23u);
        for (int index = 0; index < static_cast<int>(presentation::Message::Count); ++index)
            EXPECT_FALSE(messages.variants(static_cast<presentation::Message>(index), id).empty())
                << id << ": " << index;
    }
    for (auto [value, event] : std::vector<std::pair<int, presentation::Message>>{
             {0, presentation::Message::DfaMinimum},
             {69, presentation::Message::DfaNice},
             {420, presentation::Message::DfaDank},
             {666, presentation::Message::DfaMetal},
             {670, presentation::Message::DfaSixSeven},
             {690, presentation::Message::DfaNice},
             {1100, presentation::Message::DfaMaximum}})
        EXPECT_EQ(presentation::dfa_event(value), event);
    EXPECT_EQ(presentation::dfa_event(669), presentation::Message::Dfa);
    EXPECT_EQ(presentation::dfa_event(-1100), presentation::Message::Dfa);
    EXPECT_EQ(messages.show(presentation::dfa_event(0), "default", 1).commentary,
              "Done fucking around.");
    EXPECT_EQ(messages.show(presentation::dfa_event(0), "uwu", 1).commentary,
              "Done fucking awound~ :3");
    EXPECT_EQ(messages.show(presentation::dfa_event(670), "uwu", 1).commentary, "SIX SEVEN!");
    EXPECT_EQ(messages.show(presentation::dfa_event(420), "default", 1).commentary, "DANK!");
    EXPECT_EQ(messages.show(presentation::dfa_event(1100), "default", 1).commentary,
              "It goes louder now.");
}
TEST_F(PersonalityFiles, EveryDfaResponseLeavesOperationalStateUnchanged) {
    audiohole::SimulatedController controller;
    ASSERT_TRUE(controller.activate("profile-duplex").ok);
    controller.advance(300);
    const auto before = controller.snapshot();
    presentation::Messages messages;
    messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR);
    for (int value = 0; value <= 1100; ++value)
        (void)messages.show(presentation::dfa_event(value), "uwu",
                            static_cast<std::uint64_t>(value + 1));
    EXPECT_EQ(controller.snapshot().revision, before.revision);
    EXPECT_EQ(controller.snapshot().events.size(), before.events.size());
    EXPECT_EQ(controller.snapshot().state, before.state);
}
TEST(SimulatorPresentation, ScanLifecycleProducesDistinctEventsAndStopCancelsIt) {
    audiohole::SimulatedController controller;
    controller.scan(true);
    EXPECT_EQ(controller.snapshot().events.back().type, "scan-start");
    controller.advance(100);
    EXPECT_EQ(controller.snapshot().events.back().type, "scan-start");
    controller.advance(3000);
    EXPECT_EQ(controller.snapshot().events.back().type, "scan-complete");
    controller.scan(true);
    controller.scan(false);
    EXPECT_EQ(controller.snapshot().events.back().type, "scan-cancelled");
    controller.scan(true, false, audiohole::ScanScope::Selected, "not-a-device");
    EXPECT_EQ(controller.snapshot().events.back().type, "scan-failed");
    controller.scan(true);
    controller.stop();
    EXPECT_FALSE(controller.snapshot().scanning);
}

TEST_F(PersonalityFiles, DuplicateKeysExcessiveNestingAndConflictingIdsAreRejected) {
    presentation::Messages messages;
    EXPECT_FALSE(
        messages
            .load_file(write(
                R"({"schema_version":1,"id":"custom","id":"other","display_name":"Custom","events":{}})"))
            .ok);
    const auto deep = std::string(100, '[') + "0" + std::string(100, ']');
    EXPECT_FALSE(messages.load_file(write(deep)).ok);
    const std::string same =
        R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":["Ready."]}})";
    write(same, "one.json");
    write(same, "two.json");
    EXPECT_FALSE(messages.load_directory(directory).empty());
    EXPECT_FALSE(messages.available("custom"));
}

TEST_F(PersonalityFiles, FailedExplicitReloadInvalidatesTheSameFileAndIdEditsDropOldIdentity) {
    presentation::Messages messages;
    auto path = write(
        R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{"ready":["Ready."]}})");
    ASSERT_TRUE(messages.load_file(path).ok);
    write("{broken");
    EXPECT_FALSE(messages.load_file(path).ok);
    EXPECT_FALSE(messages.available("custom"));
    write(R"({"schema_version":1,"id":"custom","display_name":"Custom","events":{}})");
    ASSERT_TRUE(messages.load_file(path).ok);
    write(R"({"schema_version":1,"id":"renamed","display_name":"Renamed","events":{}})");
    ASSERT_TRUE(messages.load_file(path).ok);
    EXPECT_FALSE(messages.available("custom"));
    EXPECT_TRUE(messages.available("renamed"));
}

TEST_F(PersonalityFiles, EditorAndScanCalloutsKeepFactsStableAcrossPersonalities) {
    presentation::Messages messages(42);
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (auto event : {presentation::Message::BufferChanged, presentation::Message::ScanStarted,
                       presentation::Message::ScanCompleted}) {
        for (const auto* id : {"default", "uwu"}) {
            const auto first = messages.show(event, id, 1);
            EXPECT_FALSE(first.commentary.empty());
            EXPECT_EQ(messages.show(event, id, 1).commentary, first.commentary);
            EXPECT_NE(messages.show(event, id, 2).commentary, first.commentary);
            EXPECT_EQ(first.technical, presentation::technical(event));
        }
        EXPECT_TRUE(messages.show(event, "vanilla", 1).commentary.empty());
    }
    for (int value : {0, 69, 420, 666, 670, 690, 1100})
        EXPECT_EQ(presentation::technical(presentation::dfa_event(value)), "DFA Acknowledged");
}

namespace {
std::string sample_manifest(const std::string& id = "sample",
                            const std::string& responses = "responses.json") {
    return R"({"schema_version":1,"api_version":1,"id":")" + id +
           R"(","display_name":"Sample visitor","character_name":"Visitor","description":"Install by copying this folder","dialogue":{"responses":")" +
           responses +
           R"(","bother":null,"dfa":null},"expressions":["idle","happy"],"avatar":{"manifest":"avatar/pack/manifest.json","registry":"avatar/registry.json"}})";
}
std::string sample_responses(const std::string& id = "sample") {
    return R"({"schema_version":2,"id":")" + id +
           R"(","display_name":"Sample visitor","events":{"ready":[{"id":"visitor_ready_001","category":"status","text":"Visitor ready.","expression":"happy","fallback_expression":"idle"}]}})";
}
} // namespace
TEST_F(PersonalityFiles, FolderInstallRemovalAndSavedSelectionUseStableIdentity) {
    const auto folder = directory / "Any folder name";
    std::filesystem::create_directories(folder);
    write(sample_manifest(), "Any folder name/personality.json");
    write(sample_responses(), "Any folder name/responses.json");
    presentation::Messages messages;
    EXPECT_TRUE(messages.load_directory(directory).empty());
    EXPECT_EQ(messages.resolve_selection("sample"), "sample");
    ASSERT_NE(messages.package("sample"), nullptr);
    EXPECT_EQ(messages.package("sample")->character_name, "Visitor");
    EXPECT_FALSE(messages.package("sample")->avatar_available);
    EXPECT_TRUE(messages.variants(presentation::Message::Bother, "sample").empty());
    EXPECT_EQ(messages.show(presentation::Message::Ready, "sample", 1).commentary,
              "Visitor ready.");
    std::filesystem::remove_all(folder);
    EXPECT_TRUE(messages.load_directory(directory).empty());
    EXPECT_EQ(messages.resolve_selection("sample"), "");
    EXPECT_TRUE(messages.personalities().empty());
}
TEST_F(PersonalityFiles, PackageRejectsTraversalAbsoluteAndLinkedResources) {
    write(sample_responses(), "responses.json");
    presentation::Messages messages;
    for (const auto& path :
         {"../responses.json", "/tmp/responses.json", "C:/responses.json", "a\\\\responses.json",
          "responses.json:stream", "a/../responses.json"}) {
        write(sample_manifest("sample", path), "personality.json");
        EXPECT_FALSE(messages.load_package(directory).ok) << path;
    }
    const auto nested = directory / "pack";
    std::filesystem::create_directory(nested);
    write(sample_manifest(), "pack/personality.json");
    std::error_code error;
    std::filesystem::create_symlink(directory / "responses.json", nested / "responses.json", error);
    if (!error)
        EXPECT_FALSE(messages.load_package(nested).ok);
}
TEST_F(PersonalityFiles, MissingRequiredAndInvalidOptionalRejectWholePackage) {
    write(sample_manifest(), "personality.json");
    presentation::Messages messages;
    EXPECT_FALSE(messages.load_package(directory).ok);
    write(sample_responses(), "responses.json");
    EXPECT_TRUE(messages.load_package(directory).ok);
    // Same path reload invalidates the old identity on any structural error.
    write("{}", "responses.json");
    EXPECT_FALSE(messages.load_package(directory).ok);
    EXPECT_FALSE(messages.available("sample"));
}
TEST_F(PersonalityFiles, DuplicateFolderIdentitiesAreAllRejected) {
    for (const auto& folder : {"first", "second", "third"}) {
        std::filesystem::create_directory(directory / folder);
        write(sample_manifest(), std::string(folder) + "/personality.json");
        write(sample_responses(), std::string(folder) + "/responses.json");
    }
    presentation::Messages messages;
    EXPECT_FALSE(messages.load_directory(directory).empty());
    EXPECT_FALSE(messages.available("sample"));
}
TEST_F(PersonalityFiles, MissingOptionalFeatureAndCapabilitiesNeverBorrowFromOtherPackages) {
    presentation::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    ASSERT_TRUE(
        messages
            .load_file(write(
                R"({"schema_version":1,"id":"legacy","display_name":"Legacy","events":{"ready":["Old commentary."]}})"))
            .ok);
    EXPECT_TRUE(messages.package("legacy")->preview_image.empty());
    EXPECT_EQ(messages.show(presentation::Message::Bother, "legacy", 1).commentary, "");
    EXPECT_EQ(messages.resolve_selection("uwu"), "uwu");
    EXPECT_FALSE(messages.package("uwu")->preview_image.empty());
    EXPECT_NE(messages.package("uwu")->preview_image, messages.package("default")->preview_image);
    EXPECT_EQ(messages.package("uwu")->preview_image.parent_path().parent_path(),
              messages.package("uwu")->package_root);
    EXPECT_TRUE(messages.package("uwu")->avatar_available);
    EXPECT_EQ(messages.resolve_selection("removed"), "default");
    EXPECT_EQ(messages.resolve_selection("legacy"), "legacy");
}
TEST_F(PersonalityFiles, AbsentOptionalResourceDisablesFeatureButMalformedPresentResourceRejects) {
    auto manifest = sample_manifest();
    const auto position = manifest.find("\"bother\":null");
    ASSERT_NE(position, std::string::npos);
    manifest.replace(position, std::string("\"bother\":null").size(),
                     "\"bother\":\"optional.json\"");
    write(manifest, "personality.json");
    write(sample_responses(), "responses.json");
    presentation::Messages messages;
    ASSERT_TRUE(messages.load_package(directory).ok);
    EXPECT_TRUE(messages.variants(presentation::Message::Bother, "sample").empty());
    write("{malformed", "optional.json");
    EXPECT_FALSE(messages.load_package(directory).ok);
    EXPECT_FALSE(messages.available("sample"));
}
TEST_F(PersonalityFiles, AvatarReferencesRejectWrongIdentityAndRegistryEscape) {
    write(sample_manifest(), "personality.json");
    write(sample_responses(), "responses.json");
    std::filesystem::create_directories(directory / "avatar" / "pack");
    write(R"({"schema_version":1,"personality_id":"wrong","styles":{}})",
          "avatar/pack/manifest.json");
    presentation::Messages messages;
    EXPECT_FALSE(messages.load_package(directory).ok);
    write(R"({"schema_version":1,"personality_id":"sample","styles":{}})",
          "avatar/pack/manifest.json");
    write(R"({"schema_version":1,"packs":{"sample":"../avatar/pack/manifest.json"}})",
          "avatar/registry.json");
    EXPECT_FALSE(messages.load_package(directory).ok);
    write(R"({"schema_version":1,"packs":{"sample":"pack/manifest.json"}})",
          "avatar/registry.json");
    ASSERT_TRUE(messages.load_package(directory).ok);
    // This means structurally present, not approved/decoded artwork or playback.
    EXPECT_TRUE(messages.package("sample")->avatar_available);
}
