#include "model/build_preferences.h"
#include "model/update_policy.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace audiohole::updates {
using Json = nlohmann::json;
TEST(UpdatePolicy, ComparesApplicationVersionsInsteadOfBuilds) {
    EXPECT_EQ(parse_version("v0.1"), parse_version("0.1.0"));
    EXPECT_GT(*parse_version("0.10.0"), *parse_version("0.9.99"));
    EXPECT_EQ(parse_version("0.1.1")->display(), "v0.1.1");
    EXPECT_EQ(parse_version("0.1.0")->display(), "v0.1");
    for (const auto* bad :
         {"", "1", "0.1.1-beta", "0.1.1.2", "a0f69f9", "-1.2", "1.65536.0", "1..2"})
        EXPECT_FALSE(parse_version(bad)) << bad;
}
TEST(UpdatePolicy, WeeklyChecksAndImmediateCachedRemindersAreIndependent) {
    State state{100, {}, Release{"0.1.1", "Changes", {}}};
    EXPECT_FALSE(check_due(state, false, 100 + check_interval_seconds));
    EXPECT_FALSE(check_due(state, true, 101));
    EXPECT_TRUE(check_due(state, true, 100 + check_interval_seconds));
    EXPECT_TRUE(check_due(state, true, 99));    // Clock adjustment cannot suppress checks forever.
    EXPECT_TRUE(should_notify(state, "0.1.0")); // Remind means the next start, not next week.
    state.skipped_version = "v0.1.1";
    EXPECT_FALSE(should_notify(state, "0.1.0"));
    state.cached->version = "0.1.2";
    EXPECT_TRUE(should_notify(state, "0.1.0"));
    EXPECT_FALSE(should_notify(state, "0.1.2"));
    EXPECT_FALSE(should_notify(state, "0.2.0"));
}
Json release_document() {
    return {
        {"tag_name", "v0.1.1"},
        {"draft", false},
        {"prerelease", false},
        {"body", "A short changelog."},
        {"assets",
         Json::array({{{"name", "Install AudioHole v0.1.1.exe"},
                       {"browser_download_url", "https://github.com/RLCypher/audiohole/releases/"
                                                "download/v0.1.1/Install.AudioHole.v0.1.1.exe"},
                       {"size", 123},
                       {"digest", "sha256:" + std::string(64, 'a')}}})}};
}
TEST(UpdatePolicy, AcceptsStableReleaseAndOnlyVerifiedRepositoryInstaller) {
    auto json = release_document();
    auto release = parse_release(json.dump());
    EXPECT_EQ(release.version, "0.1.1");
    ASSERT_TRUE(release.installer);
    EXPECT_EQ(release.notes, "A short changelog.");
    for (const auto* url : {"http://github.com/RLCypher/audiohole/releases/download/a",
                            "https://evil.example/setup.exe",
                            "https://github.com/RLCypher/other/releases/download/a"}) {
        json["assets"][0]["browser_download_url"] = url;
        EXPECT_FALSE(parse_release(json.dump()).installer);
    }
    json = release_document();
    json["assets"][0]["digest"] = nullptr;
    EXPECT_FALSE(parse_release(json.dump()).installer);
    json["body"] = nullptr;
    EXPECT_TRUE(parse_release(json.dump()).notes.empty());
    json = release_document();
    json["assets"][0]["size"] = maximum_installer_bytes + 1;
    EXPECT_FALSE(parse_release(json.dump()).installer);
    json["prerelease"] = true;
    EXPECT_THROW(parse_release(json.dump()), std::exception);
    EXPECT_THROW(parse_release(std::string(512 * 1024 + 1, 'x')), std::exception);
}
TEST(UpdatePolicy, BoundsChangelogWithoutBreakingUtf8) {
    auto json = release_document();
    json["body"] = std::string(1999, 'x') + "é" + std::string(1000, 'y');
    auto release = parse_release(json.dump());
    EXPECT_EQ(release.notes, std::string(1999, 'x') + "\n…");
    EXPECT_NO_THROW(Json(release.notes).dump());
}
class UpdateState : public testing::Test {
  protected:
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("AudioHole-updates-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    void SetUp() override {
        std::filesystem::create_directories(root);
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};
TEST_F(UpdateState, PersistsCachedReminderAndSkipAndPreservesExternalEdits) {
    const auto file = root / "updates.json";
    Store store(file);
    ASSERT_TRUE(store.load().ok);
    State state{100, "0.1.1", Release{"0.1.2", "New release", {}}};
    ASSERT_TRUE(store.save(state).ok);
    Store restarted(file);
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_EQ(restarted.values(), state);
    Json json;
    std::ifstream(file) >> json;
    json["future_field"] = "retained";
    std::ofstream(file) << json.dump();
    state.skipped_version.clear();
    EXPECT_FALSE(restarted.save(state).ok);
    ASSERT_TRUE(restarted.load().ok);
    ASSERT_TRUE(restarted.save(state).ok);
    std::ifstream(file) >> json;
    EXPECT_EQ(json["future_field"], "retained");
    std::filesystem::remove(file);
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_EQ(restarted.values(), State{});
}
TEST_F(UpdateState, RejectsCorruptOrOversizedStateWithoutOverwritingIt) {
    const auto file = root / "updates.json";
    for (const auto& invalid :
         {std::string("{broken"), std::string(65537, 'x'),
          std::string(R"({"format":"audiohole-update-state","schema_version":99})")}) {
        std::ofstream(file) << invalid;
        Store store(file);
        EXPECT_FALSE(store.load().ok);
        EXPECT_FALSE(store.save({}).ok);
        EXPECT_EQ(std::filesystem::file_size(file), invalid.size());
    }
}
TEST_F(UpdateState, OptInPreferenceIsBackwardCompatibleAndPersistent) {
    BuildPreferences preferences(root);
    ASSERT_TRUE(preferences.load().ok);
    EXPECT_FALSE(preferences.values().check_updates_automatically);
    auto next = preferences.values();
    next.check_updates_automatically = true;
    ASSERT_TRUE(preferences.update(next).ok);
    BuildPreferences restarted(root);
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_TRUE(restarted.values().check_updates_automatically);
    Json json;
    std::ifstream(root / "preferences.json") >> json;
    json.erase("check_updates_automatically");
    std::ofstream(root / "preferences.json") << json.dump();
    BuildPreferences legacy(root);
    ASSERT_TRUE(legacy.load().ok);
    EXPECT_FALSE(legacy.values().check_updates_automatically);
}
} // namespace audiohole::updates
