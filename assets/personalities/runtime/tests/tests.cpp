#include "audiohole/avatar/session.hpp"
#include "presenter.hpp"
#include "proposed_adapter.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

namespace av = audiohole::avatar;
namespace bridge = audiohole::avatar::integration;
using Json = nlohmann::json;
namespace {
int passed = 0, failed = 0;
void expect(bool value, const std::string& reason) {
    if (!value)
        throw std::runtime_error(reason);
}
template <class Function> void test(const std::string& name, Function function) {
    try {
        function();
        ++passed;
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
        ++failed;
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
}
av::Image pixels(const std::string& id = "fixture") {
    auto result = std::make_shared<av::Pixels>();
    result->digest = id;
    result->rgba.resize(512 * 768 * 4);
    result->rgba[1000 * 4 + 3] = 255;
    return result;
}
av::ClipPtr clip(std::string state, bool finite = false, int priority = 0, int cooldown = 0,
                 std::string personality = "uwu") {
    auto value = std::make_shared<av::Clip>();
    value->personality = std::move(personality);
    value->style = "chibi";
    value->requested = state;
    value->resolved = state;
    value->source = "animation";
    value->behavior = {finite, true, priority, cooldown, 1500};
    value->loop = !finite;
    value->frames = {pixels(state + "0"), pixels(state + "1")};
    value->still = pixels(state + "still");
    value->durations_ms = {100, 200};
    return value;
}
av::Player player() {
    av::Player result;
    result.reset("uwu", "chibi", 0);
    result.context("idle", 1, 0, 0);
    result.install_baseline(clip("idle"), 1, 0);
    return result;
}
struct Fixture {
    std::filesystem::path root;
    Json manifest;
    Fixture() {
        root = std::filesystem::temp_directory_path() /
               ("audiohole-avatar-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const std::filesystem::path source = AVATAR_TEST_ROOT;
        std::filesystem::create_directories(root / "UwU/Generated/chibi/idle");
        std::ifstream(source / "UwU/manifest.json") >> manifest;
        std::filesystem::copy_file(source / "registry.json", root / "registry.json");
        std::filesystem::copy_file(source / "UwU/Generated/chibi/idle.png",
                                   root / "UwU/Generated/chibi/idle.png");
        for (int i = 0; i < 12; ++i) {
            auto name = std::to_string(i);
            name.insert(name.begin(), 4 - name.size(), '0');
            std::filesystem::copy_file(source / ("UwU/Generated/chibi/idle/" + name + ".png"),
                                       root / ("UwU/Generated/chibi/idle/" + name + ".png"));
        }
        save();
    }
    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    void save() {
        std::ofstream(root / "UwU/manifest.json") << manifest.dump(2);
    }
    av::Repository repository() {
        return av::Repository(root, [](std::span<const std::uint8_t>) { return *pixels(); });
    }
};
av::Presentation wait(av::Session& session, std::int64_t& now) {
    for (int i = 0; i < 1000; ++i) {
        auto frame = session.tick(++now);
        if (!session.loading())
            return frame;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Async load timed out");
}
} // namespace

int main(int argc, char** argv) {
    std::filesystem::path report;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--report")
            report = argv[++i];
    Json decoded_pixels = Json::object();
    test("SHA-256 known vectors", [] {
        expect(av::sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
               "empty vector");
        const std::string text = "abc";
        expect(av::sha256(
                   std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size())) ==
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               "abc vector");
    });
    test("Timing respects exposure boundaries and loop", [] {
        auto value = player();
        expect(value.tick(99).index == 0, "frame 0 hold");
        expect(value.tick(100).index == 1, "frame 1 boundary");
        expect(value.tick(300).index == 0, "loop boundary");
    });
    test("Finite reaction returns to newest baseline", [] {
        auto value = player();
        expect(value.react(clip("happy", true, 50), {"happy", "one", 1, 0, 2000, 50}, 0).accepted,
               "start");
        value.context("working", 2, 20, 50);
        value.install_baseline(clip("working"), 2, 60);
        expect(value.tick(299).state == "happy", "reaction retained");
        expect(value.tick(300).state == "working", "latest baseline restored");
    });
    test("Critical baseline interrupts success immediately", [] {
        auto value = player();
        value.react(clip("happy", true, 30), {"happy", "one", 1, 0, 2000, 30}, 0);
        value.context("error", 2, 100, 10);
        expect(!value.tick(10).image, "no stale success during error load");
        value.install_baseline(clip("error"), 2, 20);
        expect(value.tick(20).state == "error", "error installed");
        expect(value.react(clip("happy", true, 30), {"happy", "two", 2, 20, 2000, 30}, 20).reason ==
                   "baseline_severity",
               "success blocked");
    });
    test("Authoritative recovery invalidates an old high-priority reaction", [] {
        auto value = player();
        value.react(clip("panicking", true, 80), {"panicking", "disconnected", 1, 0, 2000, 80}, 0);
        value.context("idle", 2, 0, 20, true);
        value.install_baseline(clip("idle"), 2, 20);
        expect(value.tick(20).state == "idle", "no stale panic after recovery");
    });
    test("Deduplication cooldown and stale event rejection", [] {
        auto value = player();
        auto reaction = clip("happy", true, 50, 1000);
        value.react(reaction, {"happy", "one", 1, 0, 2000, 50}, 0);
        value.tick(300);
        expect(value.react(reaction, {"happy", "one", 1, 0, 2000, 50}, 301).reason ==
                   "duplicate_event",
               "duplicate");
        expect(value.react(reaction, {"happy", "two", 1, 301, 2000, 50}, 301).reason == "cooldown",
               "cooldown");
        expect(value.react(reaction, {"happy", "three", 0, 301, 2000, 50}, 302).reason ==
                   "stale_context",
               "old revision");
        expect(value.react(reaction, {"happy", "four", 1, 0, 100, 50}, 400).reason ==
                   "stale_or_invalid_event",
               "expired event");
    });
    test("Priority and interruptibility", [] {
        auto value = player();
        auto locked = std::make_shared<av::Clip>(*clip("surprised", true, 70));
        locked->behavior.interruptible = false;
        value.react(locked, {"surprised", "one", 1, 0, 2000, 70}, 0);
        expect(!value.react(clip("happy", true, 70), {"happy", "two", 1, 1, 2000, 70}, 1).accepted,
               "equal priority cannot interrupt");
        expect(value.react(clip("error", true, 90), {"error", "three", 1, 2, 2000, 90}, 2).accepted,
               "higher severity interrupts");
        expect(value.react(clip("error", true, 100, 5000), {"error", "four", 1, 3, 2000, 100}, 3)
                   .accepted,
               "active severity escalation bypasses the old cooldown");
    });
    test("Personality isolation and switch clears reactions", [] {
        auto value = player();
        expect(
            !value.react(clip("error", true, 80, 0, "vanilla"), {"error", "one", 1, 0, 2000, 80}, 0)
                 .accepted,
            "cross personality rejected");
        value.reset("default", "chibi", 1);
        expect(!value.tick(1).image, "old Horu frame cleared");
    });
    test("Pause speed restart step reduced motion", [] {
        auto value = player();
        value.pause(true, 50);
        expect(value.tick(100).index == 0, "pause holds time");
        value.step(100);
        expect(value.tick(100).index == 1, "step boundary");
        value.restart(100);
        expect(value.tick(100).index == 0, "restart");
        value.pause(false, 100);
        value.speed(2, 100);
        expect(value.tick(150).index == 1, "2x clock");
        value.reduced_motion(true);
        expect(value.tick(160).image->digest == "idlestill", "owned static used");
    });
    test("Clock rejects reversal and invalid speed", [] {
        auto value = player();
        value.tick(10);
        bool rejected = false;
        try {
            value.tick(9);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected, "clock reversal");
        rejected = false;
        try {
            value.speed(0, 10);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected, "speed bounds");
    });
    test("All real operational states and healthy routing mapping", [] {
        using State = audiohole::State;
        expect(bridge::baseline(State::Running, false).state == "idle", "Running is ambient");
        expect(bridge::baseline(State::Ready, false).state == "idle", "Ready is ambient");
        expect(bridge::baseline(State::Running, true).state == "working", "active scan");
        expect(bridge::baseline(State::Recovering, false).state == "working", "recovery");
        expect(bridge::baseline(State::Failed, true).priority == 100, "failure beats scan");
        expect(bridge::baseline(State::Degraded, false).state == "concerned", "degraded");
        expect(bridge::baseline(State::Starting, false).state == "working", "starting");
        expect(bridge::baseline(State::Stopping, false).state == "working", "stopping");
        expect(bridge::baseline(State::Stopped, false).state == "idle", "stopped");
    });
    test("Repeated healthy snapshots never restart ambient time", [] {
        auto value = player();
        for (int i = 1; i <= 1000; ++i)
            value.context("idle", i + 1, 0, i);
        expect(value.tick(1000).index == 1, "ambient exposure clock continues");
    });
    test("Recovered snapshots suppress stale retained disconnection feedback", [] {
        using State = audiohole::State;
        using Message = audiohole::presentation::Message;
        expect(!bridge::reaction_for_snapshot(Message::Disconnected, "uwu", State::Running, false),
               "no panic from a batch whose current state is healthy");
        expect(bridge::reaction_for_snapshot(Message::Disconnected, "uwu", State::Degraded, false)
                       ->state == "panicking",
               "actual degraded disconnection retains Horu reaction");
        expect(!bridge::reaction_for_snapshot(Message::ScanCompleted, "uwu", State::Running, true),
               "old completion cannot interrupt a newer scan");
    });
    test("Manifest loader normal path and unknown-state fallback", [] {
        Fixture f;
        auto repository = f.repository();
        auto result = repository.load("uwu", "chibi", "idle");
        expect(result->frames.size() == 12, "real manifest frames");
        result = repository.load("uwu", "chibi", "nonexistent");
        expect(result->resolved == "idle" && result->source == "idle_animation" &&
                   result->frames.size() == 12 && result->personality == "uwu" &&
                   result->requested == "nonexistent" && !result->diagnostics.empty(),
               "unknown request must retain identity/diagnostics and use owned Idle animation");
        result = repository.load("uwu", "chibi", "nonexistent", true);
        expect(result->resolved == "idle" && result->source == "idle_static" &&
                   result->frames.size() == 1,
               "static-only unknown request must use owned Idle still");
    });
    test("Intentional static animations stay quiet while malformed metadata falls back", [] {
        Fixture f;
        auto& animation = f.manifest["styles"]["chibi"]["states"]["idle"]["animation"];
        animation["status"] = "not_generated";
        f.save();
        auto result = f.repository().load("uwu", "chibi", "idle");
        expect(result->source == "static" && result->frames.size() == 1 &&
                   result->diagnostics.empty(),
               "intentional static must not produce an animation warning");
        animation = nullptr;
        f.save();
        result = f.repository().load("uwu", "chibi", "idle");
        expect(result->source == "static" && result->frames.size() == 1 &&
                   !result->diagnostics.empty(),
               "malformed animation metadata must report failure and try owned static");
    });
    test("Corrupt or missing animation falls back to own static", [] {
        Fixture f;
        std::ofstream(f.root / "UwU/Generated/chibi/idle/0005.png") << "corrupt";
        auto result = f.repository().load("uwu", "chibi", "idle");
        expect(result->source == "static" && result->frames.size() == 1, "static fallback");
    });
    test("Unavailable style and identity mismatch hide", [] {
        Fixture f;
        expect(f.repository().load("uwu", "regular", "idle")->frames.empty(),
               "regular cannot borrow chibi");
        f.manifest["personality_id"] = "vanilla";
        f.save();
        expect(f.repository().load("uwu", "chibi", "idle")->frames.empty(),
               "wrong personality cannot load");
    });
    test("Traversal and wrong anchor are rejected", [] {
        Fixture f;
        f.manifest["styles"]["chibi"]["states"]["idle"]["static"]["path"] =
            "../Norm/Generated/chibi/idle.png";
        for (auto& item : f.manifest["styles"]["chibi"]["states"]["idle"]["animation"]["frames"])
            item["anchor"] = {0, 0};
        f.save();
        expect(f.repository().load("uwu", "chibi", "idle")->frames.empty(), "bad paths/anchors");
    });
    test("Symlinks cannot cross personality", [] {
#ifndef _WIN32
        Fixture f;
        const auto old = f.root / "UwU/Generated";
        std::filesystem::rename(old, f.root / "foreign");
        std::filesystem::create_directory_symlink(f.root / "foreign", old);
        expect(f.repository().load("uwu", "chibi", "idle")->frames.empty(), "symlink escape");
#endif
    });
    test("Unapproved assets never reach production", [] {
        Fixture f;
        f.manifest["styles"]["chibi"]["states"]["idle"]["static"]["status"] = "validated";
        f.manifest["styles"]["chibi"]["states"]["idle"]["animation"]["status"] = "validated";
        f.save();
        expect(f.repository().load("uwu", "chibi", "idle")->frames.empty(), "production gate");
    });
    test("Memory budget bounds decoded clip", [] {
        Fixture f;
        av::LoadOptions options;
        options.decoded_budget = 1024;
        expect(f.repository().load("uwu", "chibi", "idle", false, options)->frames.empty(),
               "budget fails safely");
    });
    test("Decoded buffers deduplicate and release despite weak cache", [] {
        Fixture f;
        auto repository = f.repository();
        auto first = repository.load("uwu", "chibi", "idle");
        const auto bytes = repository.resident_bytes();
        auto second = repository.load("uwu", "chibi", "idle");
        expect(bytes > 0 && repository.resident_bytes() == bytes, "shared frame buffers reused");
        first.reset();
        second.reset();
        expect(repository.resident_bytes() == 0, "weak cache releases decoded pixels");
        av::Repository limited(
            f.root, [](std::span<const std::uint8_t>) { return *pixels(); }, 1024);
        expect(limited.load("uwu", "chibi", "idle")->frames.empty() &&
                   limited.resident_bytes() == 0,
               "repository budget fails safely");
    });
    test("Duplicate JSON keys invalidate manifest", [] {
        Fixture f;
        std::ofstream(f.root / "UwU/manifest.json")
            << "{\"schema_version\":1,\"schema_version\":1}";
        expect(f.repository().load("uwu", "chibi", "idle")->frames.empty(), "duplicate rejection");
    });
    test("Fractional frame timing is rejected rather than truncated", [] {
        Fixture f;
        for (auto& item : f.manifest["styles"]["chibi"]["states"]["idle"]["animation"]["frames"])
            item["duration_ms"] = 100.5;
        f.save();
        expect(f.repository().load("uwu", "chibi", "idle")->source == "static",
               "invalid integer timing falls back to static");
    });
    test("Async selection discards stale personality results", [] {
        Fixture f;
        av::Session session(f.root, [](std::span<const std::uint8_t>) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            return *pixels();
        });
        std::int64_t now = 0;
        session.select("uwu", "chibi", now);
        session.select("default", "chibi", ++now);
        auto frame = wait(session, now);
        expect(frame.personality == "default" && !frame.image, "never show previous personality");
        session.shutdown();
        session.shutdown();
    });
    test("Rapid baseline changes coalesce and latest context wins", [] {
        Fixture f;
        av::Session session(f.root, [](std::span<const std::uint8_t>) { return *pixels(); });
        std::int64_t now = 0;
        session.select("uwu", "chibi", now);
        for (std::uint64_t i = 2; i <= 1000; ++i)
            session.baseline("idle", i, 0, ++now);
        auto frame = wait(session, now);
        expect(frame.image && frame.baseline == "idle" && session.player().revision() == 1000,
               "latest request applied");
    });
    test("Last owned frame freezes after failure and clears on switch", [] {
        Fixture f;
        av::Session session(f.root, [](std::span<const std::uint8_t>) { return *pixels(); });
        std::int64_t now = 0;
        session.select("uwu", "chibi", now);
        expect(bool(wait(session, now).image), "initial owned frame");
        std::filesystem::remove_all(f.root / "UwU/Generated");
        session.baseline("working", 2, 20, ++now);
        auto frame = wait(session, now);
        expect(frame.image && frame.source == "last_valid_frame", "owned retained frame");
        session.select("vanilla", "logo", ++now);
        expect(!wait(session, now).image, "cannot retain across personality");
        session.shutdown();
        expect(session.tick(++now).source == "shutdown", "shutdown releases frames");
    });
    test("Async reaction dedup does not issue repeated loads", [] {
        Fixture f;
        av::Session session(f.root, [](std::span<const std::uint8_t>) { return *pixels(); });
        std::int64_t now = 0;
        session.select("uwu", "chibi", now);
        wait(session, now);
        av::Reaction event{"happy", "same", 1, now, 2000, 30};
        expect(session.react(event, now).accepted, "first event queued");
        expect(session.react(event, now).reason == "duplicate_event",
               "duplicate rejected before decode");
    });
    test("Operational sequences recover to current truth", [] {
        auto value = player();
        auto apply = [&](audiohole::State state, bool scanning, std::uint64_t revision, int now) {
            const auto next = bridge::baseline(state, scanning);
            value.context(next.state, revision, next.priority, now);
            value.install_baseline(clip(next.state), revision, now);
        };
        apply(audiohole::State::Stopped, false, 2, 1);
        apply(audiohole::State::Ready, false, 3, 2);
        apply(audiohole::State::Running, false, 4, 3);
        expect(value.tick(3).state == "idle", "startup ready healthy remains ambient");
        apply(audiohole::State::Running, true, 5, 4);
        expect(value.tick(4).state == "working", "scan workload");
        apply(audiohole::State::Ready, false, 6, 5);
        value.react(clip("celebrating", true, 35), {"celebrating", "scan", 6, 5, 2000, 35}, 5);
        expect(value.tick(305).state == "idle", "discovery completion returns ready baseline");
        apply(audiohole::State::Degraded, false, 7, 306);
        value.react(clip("panicking", true, 80), {"panicking", "disconnect", 7, 306, 2000, 80},
                    306);
        apply(audiohole::State::Failed, false, 8, 307);
        expect(value.tick(307).state == "error", "failure interrupts disconnection reaction");
        apply(audiohole::State::Recovering, false, 9, 308);
        expect(value.tick(308).state == "working", "recovering workload");
        apply(audiohole::State::Running, false, 10, 309);
        expect(value.tick(309).state == "idle", "healthy recovery ambient");
    });
    test("Native decoder loads all approved static and animated states", [&] {
        av::initialize_image_decoder();
        av::Repository repository(AVATAR_TEST_ROOT, av::native_decoder());
        const std::filesystem::path root = AVATAR_TEST_ROOT;
        Json registry;
        std::ifstream(root / "registry.json") >> registry;
        int static_count = 0, sequence_count = 0;
        double maximum_ms = 0;
        for (auto& [identity, path] : registry.at("packs").items()) {
            Json manifest;
            std::ifstream(root / path.get<std::string>()) >> manifest;
            const auto style = manifest.at("default_style").get<std::string>();
            for (auto& [state, entry] : manifest.at("styles").at(style).at("states").items()) {
                if (entry.at("static").at("status") != "approved")
                    continue;
                const auto start = std::chrono::steady_clock::now();
                const auto still = repository.load(identity, style, state, true);
                expect(still->frames.size() == 1, identity + "/" + state + " static decode");
                ++static_count;
                const auto prefix = std::filesystem::path(path.get<std::string>()).parent_path();
                decoded_pixels[(prefix / entry.at("static").at("path").get<std::string>())
                                   .generic_string()] = av::sha256(still->frames.front()->rgba);
                const auto sequence = repository.load(identity, style, state);
                expect(!sequence->frames.empty(), identity + "/" + state + " animation decode");
                if (entry.at("animation").at("status") == "approved") {
                    std::string detail =
                        identity + "/" + state +
                        ": all operating frames decoded; source=" + sequence->source;
                    for (const auto& diagnostic : sequence->diagnostics)
                        detail += "; " + diagnostic;
                    expect(sequence->frames.size() == entry.at("animation").at("frames").size(),
                           detail);
                    ++sequence_count;
                    for (std::size_t i = 0; i < sequence->frames.size(); ++i)
                        decoded_pixels[(prefix / entry.at("animation")
                                                     .at("frames")
                                                     .at(i)
                                                     .at("path")
                                                     .get<std::string>())
                                           .generic_string()] =
                            av::sha256(sequence->frames[i]->rgba);
                }
                maximum_ms = std::max(maximum_ms, std::chrono::duration<double, std::milli>(
                                                      std::chrono::steady_clock::now() - start)
                                                      .count());
            }
        }
        expect(static_count == 52 && sequence_count == 49, "full baseline loaded");
        std::cout << "MEASURE native load maximum per state (static + animation): " << maximum_ms
                  << " ms\n";
    });
    if (!report.empty()) {
        std::filesystem::create_directories(report.parent_path());
        std::ofstream(report) << Json{{"passed", passed},
                                      {"failed", failed},
                                      {"decoded_rgba_sha256", decoded_pixels}}
                                     .dump(2)
                              << '\n';
    }
    std::cout << "RESULT " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
