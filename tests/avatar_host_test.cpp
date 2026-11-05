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
    av::Repository repository(messages.package("uwu")->avatar_registry.parent_path(),
                              fake_decoder());
    const auto authored =
        repository.load("uwu", messages.package("uwu")->avatar_style, copy.expression);
    const auto duration =
        authored->duration_ms() ? authored->duration_ms() : authored->behavior.static_hold_ms;
    now += duration;
    EXPECT_TRUE(host.tick(now).reaction);
    EXPECT_EQ(host.tick(now).index, reaction.frame_count - 1);
    now += 1999;
    EXPECT_TRUE(host.tick(now).reaction);
    ++now;
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
TEST(AvatarIntegration, SleepsAfterTwoMinutesDespitePollingAndWakesOnInputOrReaction) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    audiohole::Snapshot snapshot;
    snapshot.state = audiohole::State::Running;
    std::int64_t now = 100;
    host.observe(snapshot, false, now);
    for (const auto* id : {"default", "uwu", "vanilla"}) {
        const auto selected_at = now;
        host.select(messages.package(id), now);
        wait_for(host, now, "idle");
        for (int elapsed : {30000, 60000, 90000, 119999}) {
            now = selected_at + elapsed;
            ++snapshot.revision;
            host.observe(snapshot, false, now);
            EXPECT_EQ(host.tick(now).state, "idle");
        }
        now = selected_at + 120000;
        EXPECT_EQ(host.tick(now).baseline, "sleeping");
        const auto asleep = wait_for(host, now, "sleeping");
        EXPECT_FALSE(asleep.reaction);
        EXPECT_EQ(asleep.personality, id);
        if (std::string(id) != "vanilla")
            EXPECT_GT(asleep.frame_count, 1);

        host.activity(now);
        EXPECT_EQ(host.baseline().expression, "idle");
        wait_for(host, now, "idle");
        now += 119000;
        host.activity(now); // Editing without generating dialogue also postpones sleep.
        now += 1000;
        EXPECT_EQ(host.tick(now).state, "idle");
        now += 119000;
        wait_for(host, now, "sleeping");
        const auto copy = messages.show(p::Message::Ready, id, 42);
        ASSERT_TRUE(host.react(p::Message::Ready, copy, 42, now).accepted);
        EXPECT_EQ(host.baseline().expression, "idle");
        now += 10000;
        // Load freshness is separate from the already-tested reaction display hold.
        EXPECT_NE(host.tick(now).baseline, "sleeping");
    }
}
TEST(AvatarIntegration, BusyAndWarningConditionsStayAwakeAndWakeASleepingAvatar) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    audiohole::Snapshot snapshot;
    std::int64_t now = 0;
    host.select(messages.package("uwu"), now);
    wait_for(host, now, "idle");
    now = 120000;
    wait_for(host, now, "sleeping");
    snapshot.scanning = true;
    host.observe(snapshot, false, now);
    EXPECT_EQ(host.baseline().expression, "working");
    wait_for(host, now, "working");
    now += 120000;
    EXPECT_EQ(host.tick(now).baseline, "working");
    snapshot.scanning = false;
    for (auto state :
         {audiohole::State::Starting, audiohole::State::Recovering, audiohole::State::Failed}) {
        snapshot.state = state;
        host.observe(snapshot, false, now);
        now += 120000;
        EXPECT_NE(host.tick(now).baseline, "sleeping");
    }
    snapshot.state = audiohole::State::Running;
    host.observe(snapshot, true, now);
    now += 120000;
    EXPECT_EQ(host.tick(now).baseline, "concerned");
    host.observe(snapshot, false, now);
    const auto quiet_since = now;
    wait_for(host, now, "idle");
    now = quiet_since + 119999;
    EXPECT_EQ(host.tick(now).baseline, "idle");
    ++now;
    EXPECT_EQ(host.tick(now).baseline, "sleeping");
}
TEST(AvatarIntegration, MissingSleepingCapabilityKeepsOwnedIdle) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    auto package = *messages.package("uwu");
    std::erase(package.avatar_states, "sleeping");
    p::AvatarHost host(fake_decoder());
    std::int64_t now = 0;
    host.select(&package, now);
    wait_for(host, now, "idle");
    now = 120000;
    EXPECT_EQ(host.tick(now).state, "idle");
}
TEST(AvatarIntegration, HeldExpressionStillYieldsToSleepAndOperationalWarnings) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    host.hold_expressions(true);
    std::int64_t now = 0;
    host.select(messages.package("uwu"), now);
    wait_for(host, now, "idle");
    const auto copy = messages.show(p::Message::Bother, "uwu", 1);
    ASSERT_TRUE(host.react(p::Message::Bother, copy, 1, now).accepted);
    const auto occurred_at = now;
    wait_for(host, now, copy.expression, true);
    now += 10000;
    EXPECT_TRUE(host.tick(now).holding);
    now = occurred_at + 120000;
    EXPECT_EQ(host.tick(now).baseline, "sleeping");
    EXPECT_FALSE(wait_for(host, now, "sleeping").reaction);
    host.activity(now);
    wait_for(host, now, "idle");
    ASSERT_TRUE(host.react(p::Message::Bother, copy, 2, now).accepted);
    wait_for(host, now, copy.expression, true);
    now += 10000;
    EXPECT_TRUE(host.tick(now).holding);
    audiohole::Snapshot snapshot;
    snapshot.state = audiohole::State::Failed;
    host.observe(snapshot, true, now);
    EXPECT_FALSE(wait_for(host, now, "error").reaction);
}
TEST(AvatarIntegration, DfaExpressionsFollowTheSelectedResponseMetadata) {
    p::Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    std::int64_t now = 0;
    std::uint64_t occurrence = 0;
    for (const auto* id : {"default", "uwu"}) {
        host.select(messages.package(id), now);
        wait_for(host, now, "idle");
        for (auto event : {p::Message::Dfa121, p::Message::Dfa404, p::Message::Dfa777}) {
            const auto copy = messages.show(event, id, ++occurrence);
            ASSERT_TRUE(host.react(event, copy, occurrence, now).accepted);
            const auto frame = wait_for(host, now, copy.expression, true);
            EXPECT_EQ(frame.state, copy.expression);
            EXPECT_EQ(frame.personality, id);
            now += 10000;
            EXPECT_FALSE(host.tick(now).reaction);
        }
    }
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

TEST(AvatarIntegration, ContinuouslyLoopingCosmeticReactionStillYieldsToInactivitySleep) {
    p::Messages messages(4);
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    p::AvatarHost host(fake_decoder());
    audiohole::Snapshot snapshot;
    std::int64_t now = 100;
    host.observe(snapshot, false, now);
    host.select(messages.package("uwu"), now);
    host.reaction_playback(2, true);
    wait_for(host, now, "idle");
    const auto copy = messages.show(p::Message::Bother, "uwu", 1);
    ASSERT_TRUE(host.react(p::Message::Bother, copy, 1, now).accepted);
    wait_for(host, now, copy.expression, true);
    now += 120001;
    host.tick(now);
    EXPECT_FALSE(wait_for(host, now, "sleeping").reaction);
}
