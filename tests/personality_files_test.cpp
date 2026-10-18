#include "model/controller.h"
#include "presentation/messages.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>

namespace presentation = audiohole::presentation;
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
TEST_F(PersonalityFiles, InvalidFilesCannotOverrideFactsOrActivatePartialCatalogs) {
    presentation::Messages messages;
    for (
        const auto& bytes :
        {std::string("{bad JSON"),
         std::string(R"({"schema_version":2,"id":"custom","display_name":"Custom","events":{}})"),
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
