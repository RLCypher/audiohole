#include "model/build_preferences.h"
#include "model/latency_preferences.h"
#include "model/routing_service.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace audiohole;
namespace {
using Json = nlohmann::json;
struct LatencyV4 : testing::Test {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("AudioHole-v4-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    static std::string read(const std::filesystem::path& p) {
        std::ifstream f(p);
        return {std::istreambuf_iterator<char>(f), {}};
    }
};
TEST_F(LatencyV4, EightCanonicalTimingsSurviveStorageAndEngineProjectionWithoutChangingSibling) {
    auto file = root / "configuration.json";
    ProfileDirectoryPersistence store(file);
    ConfigurationRecords records;
    ASSERT_TRUE(store.load(records).ok);
    Profile profile;
    profile.id = "v4";
    profile.display_name = "Timing contract";
    Hole h;
    h.id = "hole";
    h.display_name = "Route";
    h.duplex = true;
    h.forward.source = "input";
    h.forward.destination = "output";
    h.reverse.source = "return-input";
    h.reverse.destination = "return-output";
    h.forward.advanced_tuning = true;
    const TimingValues values{35.5, 10.5, 10, 1, 7, {}, {}, 22};
    ASSERT_TRUE(set_timing_values(h.forward.settings.preferences, values).ok);
    const auto reverse = h.reverse;
    profile.holes.push_back(h);
    records.profiles.push_back(profile);
    ASSERT_TRUE(store.save(records).ok);
    auto document = Json::parse(read(root / "profiles" / "Timing contract.json"));
    EXPECT_EQ(document["schema_version"], 4);
    const auto& prefs = document["profile"]["holes"][0]["forward"]["settings"]["preferences"];
    EXPECT_TRUE(prefs["destination"]["period_ms"].is_null());
    EXPECT_TRUE(prefs["source"]["buffer_ms"].is_null());
    EXPECT_EQ(prefs["common"]["buffer_ms"], 1);
    EXPECT_EQ(prefs["queue_target_ms"], 35.5);
    document["owner_extension"] = {{"keep", "yes"}};
    document["profile"]["holes"][0]["forward"]["settings"]["preferences"]["destination"]["vendor"] =
        42;
    std::ofstream(root / "profiles" / "Timing contract.json") << document.dump(2);
    ASSERT_TRUE(store.load(records).ok);
    EXPECT_EQ(timing_values(records.profiles[0].holes[0].forward.settings.preferences), values);
    EXPECT_EQ(records.profiles[0].holes[0].reverse, reverse);
    auto engine_profile = routing_profile(records.profiles[0]);
    ASSERT_TRUE(engine_profile.ok());
    auto plan = engine::prepare_profile(*engine_profile.value);
    ASSERT_TRUE(plan.ok());
    EXPECT_EQ(plan.value->routes[0].queue_target_ms, 35.5);
    EXPECT_EQ(plan.value->routes[0].source.period_ms, 7);
    EXPECT_EQ(plan.value->routes[0].destination.period_ms, 10);
    EXPECT_EQ(plan.value->routes[0].source.buffer_ms, 1);
    EXPECT_EQ(plan.value->routes[0].destination.buffer_ms, 22);
    ASSERT_TRUE(store.save(records).ok);
    const auto saved = Json::parse(read(root / "profiles" / "Timing contract.json"));
    EXPECT_EQ(saved["owner_extension"]["keep"], "yes");
    EXPECT_EQ(
        saved["profile"]["holes"][0]["forward"]["settings"]["preferences"]["destination"]["vendor"],
        42);
    for (const Json bad : {Json(0), Json(-1), Json("Auto"), Json(true)}) {
        auto invalid = saved;
        invalid["profile"]["holes"][0]["forward"]["settings"]["preferences"]["common"]
               ["period_ms"] = bad;
        Profile untouched = records.profiles[0];
        EXPECT_FALSE(validate_profile_record(invalid.dump(), untouched).ok);
        EXPECT_EQ(untouched, records.profiles[0]);
    }
}
TEST_F(LatencyV4, WorkerPoliciesPersistSeparatelyAndRejectUnknownChoicesWithoutReplacingFile) {
    BuildPreferences preferences(root);
    ASSERT_TRUE(preferences.load().ok);
    EXPECT_EQ(preferences.values().scheduling_task, "automatic");
    auto next = preferences.values();
    next.scheduling_task = "audio";
    next.audio_thread_priority = "high";
    ASSERT_TRUE(preferences.update(next).ok);
    BuildPreferences restarted(root);
    ASSERT_TRUE(restarted.load().ok);
    EXPECT_EQ(restarted.values(), next);
    const auto original = read(root / "preferences.json");
    next.audio_thread_priority = "realtime";
    EXPECT_FALSE(restarted.update(next).ok);
    EXPECT_EQ(read(root / "preferences.json"), original);
}
} // namespace
