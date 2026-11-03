#include "presentation/avatar_host.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

namespace p = audiohole::presentation;
namespace av = audiohole::avatar;
namespace {
av::Decoder fake_decoder() {
    return [](std::span<const std::uint8_t>) {
        av::Pixels pixels;
        pixels.rgba.resize(512 * 768 * 4);
        pixels.rgba[(384 * 512 + 256) * 4 + 3] = 255; // Opaque foreground with transparent corners.
        return pixels;
    };
}
av::Presentation wait_for(p::AvatarHost& host, std::int64_t& now, const std::string& state,
                          bool reaction = false) {
    for (int attempt = 0; attempt < 2000; ++attempt) {
        auto frame = host.tick(++now);
        if (frame.image && frame.state == state && frame.reaction == reaction)
            return frame;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const auto final = host.tick(now);
    for (const auto& diagnostic : final.diagnostics)
        ADD_FAILURE() << diagnostic;
    ADD_FAILURE() << "Avatar load timed out: " << state;
    return host.tick(now);
}
TEST(AvatarIntegration, AllOwnedPacksLoadAndIdleAnimationAdvances) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    std::int64_t now = 100;
    for (const auto* id : {"default", "uwu", "vanilla"}) {
        const auto* package = messages.package(id);
        ASSERT_TRUE(package && package->avatar_available);
        host.select(package, now);
        auto frame = wait_for(host, now, "idle");
        ASSERT_NE(frame.image, nullptr);
        EXPECT_EQ(frame.personality, id);
        EXPECT_EQ(frame.image->width, 512);
        EXPECT_EQ(frame.image->height, 768);
        EXPECT_GT(frame.frame_count, 1);
        bool changed = false;
        const auto first = frame.index;
        for (int tick = 0; tick < 500; ++tick) {
            now += 16;
            changed = changed || host.tick(now).index != first;
        }
        EXPECT_TRUE(changed);
        EXPECT_LE(host.decoded_bytes(), 128u * 1024 * 1024);
    }
    host.shutdown();
    EXPECT_FALSE(host.enabled());
    EXPECT_FALSE(host.tick(now).image);
}
TEST(AvatarIntegration, UsesSelectedResponseMetadataAndReturnsToCurrentBaseline) {
    p::Messages messages(9);
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    audiohole::Snapshot snapshot;
    snapshot.state = audiohole::State::Running;
    snapshot.active_profile = "live";
    std::int64_t now = 100;
    host.observe(snapshot, false, now);
    host.select(messages.package("uwu"), now);
    wait_for(host, now, "idle");
    const auto copy = messages.show(p::Message::Bother, "uwu", 42);
    ASSERT_TRUE(host.react(p::Message::Bother, copy, 42, now).accepted);
    const auto reaction = wait_for(host, now, copy.expression, true);
    EXPECT_EQ(reaction.personality, "uwu");
    EXPECT_FALSE(host.react(p::Message::Bother, copy, 42, now).accepted);
    now += 60000;
    EXPECT_EQ(host.tick(now).state, "idle");
    EXPECT_FALSE(host.tick(now).reaction);
    EXPECT_EQ(host.baseline().expression, "idle");
}
TEST(AvatarIntegration, OperationalChangesCancelCosmeticReactionsAndRejectStaleSuccess) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    std::int64_t now = 100;
    audiohole::Snapshot snapshot;
    snapshot.state = audiohole::State::Running;
    host.observe(snapshot, false, now);
    host.select(messages.package("uwu"), now);
    wait_for(host, now, "idle");
    auto copy = messages.show(p::Message::Bother, "uwu", 1);
    host.react(p::Message::Bother, copy, 1, now);
    snapshot.state = audiohole::State::Failed;
    host.observe(snapshot, true, now);
    const auto failure = wait_for(host, now, "error");
    EXPECT_FALSE(failure.reaction);
    EXPECT_FALSE(host.react(p::Message::Bother, copy, 2, now).accepted);
    EXPECT_FALSE(
        host.react(p::Message::Started, messages.show(p::Message::Started, "uwu", 3), 3, now)
            .accepted);
    snapshot.state = audiohole::State::Running;
    host.observe(snapshot, false, now);
    EXPECT_EQ(wait_for(host, now, "idle").baseline, "idle");
    // Routine revision changes do not restart idle playback.
    const auto epoch = host.baseline().revision;
    ++snapshot.revision;
    host.observe(snapshot, false, now);
    EXPECT_EQ(host.baseline().revision, epoch);
    snapshot.events.push_back(
        {1, "error", "New authoritative failure", audiohole::Severity::Error});
    host.observe(snapshot, true, now);
    EXPECT_GT(host.baseline().revision, epoch);
    EXPECT_EQ(host.baseline().expression, "concerned");
}
TEST(AvatarIntegration, IdentitySwitchClearsOldFramesAndMissingResourcesRemainIndependent) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    std::int64_t now = 100;
    host.select(messages.package("default"), now);
    wait_for(host, now, "idle");
    host.select(messages.package("uwu"), now);
    const auto during = host.tick(now);
    EXPECT_TRUE(!during.image || during.personality == "uwu");
    EXPECT_EQ(wait_for(host, now, "idle").personality, "uwu");
    auto foreign = messages.show(p::Message::Bother, "default", 1);
    EXPECT_FALSE(host.react(p::Message::Bother, foreign, 1, now).accepted);
    host.select(nullptr, now);
    EXPECT_FALSE(host.tick(now).image);
    EXPECT_FALSE(host.enabled());
    auto missing = *messages.package("uwu");
    missing.avatar_registry = missing.package_root / "absent" / "registry.json";
    host.select(&missing, now);
    av::Presentation failed;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        failed = host.tick(++now);
        if (!failed.diagnostics.empty())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    EXPECT_FALSE(failed.image);
    EXPECT_FALSE(failed.diagnostics.empty());
}
} // namespace

TEST(AvatarIntegration, RejectsExcessiveRuntimeJsonNestingWithoutLoadingArt) {
    struct Temporary {
        std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("audiohole-avatar-depth-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ~Temporary() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } temporary;
    std::filesystem::create_directories(temporary.path / "pack");
    std::ofstream(temporary.path / "registry.json")
        << R"({"schema_version":1,"packs":{"uwu":"pack/manifest.json"}})";
    auto nested = std::string("0");
    for (int level = 0; level < 64; ++level)
        nested = "[" + nested + "]";
    std::ofstream(temporary.path / "pack/manifest.json") << "{\"excessive\":" << nested << "}";
    av::Repository repository(temporary.path, fake_decoder());
    const auto clip = repository.load("uwu", "chibi", "idle");
    EXPECT_TRUE(clip->frames.empty());
    EXPECT_TRUE(
        std::any_of(clip->diagnostics.begin(), clip->diagnostics.end(),
                    [](const auto& line) { return line.find("nesting") != std::string::npos; }));
}
