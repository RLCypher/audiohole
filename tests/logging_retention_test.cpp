#include "model/controller.h"
#include "model/first_run.h"
#include "model/scan_session_store.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
namespace audiohole {
namespace fs = std::filesystem;
class SessionRetention : public testing::Test {
  protected:
    fs::path root = fs::temp_directory_path() /
                    ("AudioHole-retention-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path sessions = root / "sessions", active = sessions / "active";
    void SetUp() override {
        fs::create_directories(active);
    }
    void TearDown() override {
        std::error_code error;
        fs::remove_all(root, error);
    }
    fs::path archive(const char* name, std::size_t bytes, int hours) {
        auto p = sessions / (std::string(name) + "-scan-test");
        fs::create_directory(p);
        std::ofstream(p / "progress.jsonl") << std::string(bytes, 'x');
        fs::last_write_time(p, fs::file_time_type::clock::now() - std::chrono::hours(hours));
        return p;
    }
    std::uintmax_t size() {
        std::uintmax_t bytes = 0;
        for (const auto& file : fs::recursive_directory_iterator(sessions))
            if (file.is_regular_file())
                bytes += file.file_size();
        return bytes;
    }
};
TEST_F(SessionRetention, RemovesOldestCompletedFoldersAndProtectsActiveScan) {
    auto oldest = archive("oldest", 100, 3);
    auto recent = archive("recent", 100, 1);
    ScanSessionStore store(sessions, active, {2, 1024});
    EXPECT_FALSE(fs::exists(oldest));
    EXPECT_TRUE(fs::exists(recent));
    EXPECT_TRUE(store.write("progress.jsonl", "active", true));
    EXPECT_TRUE(fs::exists(active));
}
TEST_F(SessionRetention, EnforcesTotalBytesAndPreservesEssentialDataOutsideSessions) {
    auto oldest = archive("oldest", 200, 3);
    auto recent = archive("recent", 200, 1);
    std::ofstream(root / "device.json") << "essential device result";
    ScanSessionStore store(sessions, active, {16, 512});
    ASSERT_TRUE(store.write("progress.jsonl", std::string(200, 'x')));
    EXPECT_FALSE(fs::exists(oldest));
    EXPECT_TRUE(fs::exists(recent));
    EXPECT_FALSE(store.write("oversized.json", std::string(600, 'x')));
    EXPECT_TRUE(store.truncated());
    EXPECT_TRUE(store.write("result.json", "completed", false, true));
    EXPECT_LE(size(), 512u);
    EXPECT_TRUE(fs::exists(active / "progress.jsonl"));
    EXPECT_TRUE(fs::exists(root / "device.json"));
}
TEST_F(SessionRetention, ManyDevicesStopOptionalWritesWithoutDeletingActiveResults) {
    ScanSessionStore store(sessions, active, {16, 4096});
    unsigned kept = 0;
    for (unsigned i = 0; i < 2000; ++i)
        kept +=
            store.write(fs::path("devices") / (std::to_string(i) + ".json"), std::string(64, 'x'));
    EXPECT_LT(kept, 2000u);
    EXPECT_GT(kept, 0u);
    EXPECT_TRUE(store.write("result.json", "completed", false, true));
    EXPECT_LE(size(), 4096u);
    EXPECT_THROW(store.write("../outside", "bad"), std::runtime_error);
}
TEST_F(SessionRetention, RetentionDoesNotDeleteUnrelatedDirectories) {
    const auto unrelated = sessions / "personal-files";
    fs::create_directory(unrelated);
    std::ofstream(unrelated / "notes.txt") << std::string(600, 'x');
    ScanSessionStore store(sessions, active, {16, 512});
    EXPECT_FALSE(store.write("optional.json", "data"));
    EXPECT_TRUE(fs::exists(unrelated / "notes.txt"));
    EXPECT_TRUE(fs::exists(active));
}
TEST_F(SessionRetention, LicenseAcceptanceIsIndependentOfTutorialReplay) {
    auto receipt = root / "license-acceptance.json";
    auto tutorial = root / "AudioHole.has-run";
    bool needed = false;
    ASSERT_TRUE(license_acceptance_needed(receipt, needed).ok);
    EXPECT_TRUE(needed);
    EXPECT_FALSE(fs::exists(receipt)); // Opening/declining alone records nothing.
    ASSERT_TRUE(accept_license(receipt).ok);
    ASSERT_TRUE(license_acceptance_needed(receipt, needed).ok);
    EXPECT_FALSE(needed);
    EXPECT_FALSE(fs::exists(tutorial));
    ASSERT_TRUE(mark_first_run(tutorial).ok);
    ASSERT_TRUE(reset_first_run(tutorial).ok);
    ASSERT_TRUE(license_acceptance_needed(receipt, needed).ok);
    EXPECT_FALSE(needed);
    EXPECT_TRUE(accept_license(receipt).ok);
}
TEST(DiagnosticCapture, DefaultOffPreservesOperationalEventsAndAlwaysShowsScanOutput) {
    SimulatedController controller;
    EXPECT_FALSE(controller.snapshot().logging_enabled);
    EXPECT_TRUE(controller.snapshot().diagnostic_events.empty());
    controller.create_profile("First");
    EXPECT_FALSE(controller.snapshot().events.empty());
    EXPECT_TRUE(controller.snapshot().diagnostic_events.empty());
    controller.set_logging(true);
    controller.create_profile("Captured");
    EXPECT_FALSE(controller.snapshot().diagnostic_events.empty());
    controller.set_logging(false);
    const auto count = controller.snapshot().diagnostic_events.size();
    controller.create_profile("Not captured");
    EXPECT_EQ(controller.snapshot().diagnostic_events.size(), count);
    controller.scan(true, false, ScanScope::Full);
    EXPECT_GT(controller.snapshot().diagnostic_events.size(), count);
    controller.clear_diagnostics();
    EXPECT_TRUE(controller.snapshot().diagnostic_events.empty());
    EXPECT_TRUE(controller.snapshot().events.empty());
}
} // namespace audiohole
