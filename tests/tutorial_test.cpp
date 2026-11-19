#include "model/build_preferences.h"
#include "model/tutorial.h"
#include <array>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace audiohole;
namespace {
struct Lesson {
    Tutorial lesson;
    SimulatedController sandbox{true};
    Profile draft;
    TutorialPlugBoard practice;
    int page = 0;
    void event(const char* action) {
        lesson.event(action, sandbox.snapshot(), draft, draft.holes.empty() ? -1 : 0, page);
    }
    void choice(int index = 0) {
        const auto choices = lesson.view().choices;
        ASSERT_GT(choices.size(), static_cast<std::size_t>(index));
        ASSERT_TRUE(lesson.choose(choices[index].id));
    }
    void navigate(int target) {
        page = target;
        event("navigation");
    }
    void to_scan() {
        choice();
        choice();
        choice();
        navigate(DevicesPage);
        navigate(ScanningPage);
        choice();
        choice();
        ASSERT_EQ(lesson.beat(), "N007");
        event("scan_options");
    }
    void scan() {
        sandbox.scan(true, true, ScanScope::Full);
        event("scan_started");
        sandbox.advance(20000);
        event("scan_completed");
    }
    void to_route() {
        to_scan();
        scan();
        choice();
        navigate(DevicesPage);
        choice();
        choice();
        event("device_edit");
        auto mic = *endpoint(sandbox.snapshot(), "tutorial-mic");
        mic.display_name = "My headset mic";
        sandbox.update_endpoint(mic);
        event("device_saved");
        auto out = *endpoint(sandbox.snapshot(), "tutorial-speakers");
        out.display_name = "My headphones";
        sandbox.update_endpoint(out);
        event("device_saved");
        navigate(ProfilesPage);
        const auto id = sandbox.create_profile("Practice");
        draft = *profile(sandbox.snapshot(), id);
        event("profile_created");
        EXPECT_EQ(lesson.beat(), "N018A");
        EXPECT_EQ(lesson.view().targets, std::vector<std::string>{"profile:" + draft.id});
        event("profile_renamed");
        EXPECT_EQ(lesson.beat(), "N018A"); // Confirming the default is insufficient.
        draft.display_name = "My practice profile";
        ASSERT_TRUE(sandbox.save(draft, false).ok);
        event("profile_renamed");
        EXPECT_EQ(lesson.beat(), "N018");
        draft.holes.push_back(sandbox.make_hole());
        event("hole_created");
        auto& h = draft.holes[0];
        EXPECT_EQ(lesson.beat(), "N018B");
        EXPECT_EQ(lesson.view().targets, std::vector<std::string>{"hole:" + h.id});
        event("hole_renamed");
        EXPECT_EQ(lesson.beat(), "N018B");
        h.display_name = "My two-way connection";
        event("hole_renamed");
        EXPECT_EQ(lesson.beat(), "N019");
        h.duplex = true;
        event("return");
        h.forward.source = "tutorial-mic";
        event("endpoints");
        h.forward.destination = "tutorial-mixer-out";
        event("endpoints");
        h.forward.exclusive_mode_preferred = true;
        event("exclusive");
        h.reverse.source = "tutorial-mixer-in";
        event("endpoints");
        h.reverse.destination = "tutorial-speakers";
        event("endpoints");
        ASSERT_EQ(lesson.beat(), "N024");
        ASSERT_TRUE(sandbox.save(draft, false).ok);
        event("saved");
        choice();
    }
    void to_plug_transition() {
        to_route();
        route();
        for (int i = 0; i < 5; ++i)
            choice();
        ASSERT_EQ(lesson.beat(), "N032");
    }
    void to_plug(int transition = 0, int introduction = 0) {
        to_plug_transition();
        choice(transition);
        ASSERT_EQ(lesson.beat(), "N033");
        choice(introduction);
        ASSERT_EQ(lesson.beat(), "PB001");
    }
    void plug_lesson(int branch = 0, bool finish = true) {
        ASSERT_EQ(lesson.beat(), "PB001");
        auto& board = practice;
        board = {};
        board.sockets = {{"system", "System", ""}};
        board.game_app = "game";
        board.music_app = "music";
        lesson.plug_board(board);
        navigate(PlugBoardPage);
        ASSERT_EQ(lesson.beat(), "PB002");
        choice(branch);
        ASSERT_EQ(lesson.beat(), "PB003");
        EXPECT_TRUE(
            lesson.view().text.starts_with(branch == 0 ? "Exactly." : "As many as you please."));
        choice(branch);
        ASSERT_EQ(lesson.beat(), "PB004");
        board.sockets.push_back({"songs", "My playlist", ""});
        lesson.plug_board(board);
        ASSERT_EQ(lesson.beat(), "PB006");
        board.sockets[1].endpoint = "tutorial-music";
        lesson.plug_board(board);
        ASSERT_EQ(lesson.beat(), "PB007");
        board.sockets.push_back({"explosions", "Explosions", ""});
        lesson.plug_board(board);
        ASSERT_EQ(lesson.beat(), "PB009");
        board.sockets[2].endpoint = "tutorial-game";
        lesson.plug_board(board);
        ASSERT_EQ(lesson.beat(), "PB010");
        board.assignments["game"] = "explosions";
        lesson.plug_board(board);
        ASSERT_EQ(lesson.beat(), "PB011");
        board.assignments["music"] = "songs";
        lesson.plug_board(board);
        ASSERT_EQ(lesson.beat(), "PB012");
        if (finish) {
            choice(branch);
            ASSERT_EQ(lesson.beat(), "PB013");
        }
    }
    void route() {
        ASSERT_TRUE(sandbox.activate(draft.id).ok);
        event("routing_started");
        sandbox.advance(7999);
        event("simulated_routing_running");
        EXPECT_EQ(lesson.beat(), "N027");
        sandbox.advance(1);
        event("simulated_routing_running");
    }
};
TEST(Tutorial, CatalogKeepsExactDialogueBranchesAndNoSpeakerPrefix) {
    auto catalog = nlohmann::json::parse(tutorial_catalog());
    EXPECT_EQ(catalog.size(), 88u);
    EXPECT_FALSE(catalog.contains("PB005"));
    EXPECT_FALSE(catalog.contains("PB008"));
    EXPECT_EQ(catalog.at("N033").at("variants").size(), 2u);
    EXPECT_EQ(catalog.at("PB001").at("variants").size(), 2u);
    EXPECT_TRUE(catalog.at("PB014").at("choices").empty());
    for (const auto& beat : catalog) {
        if (beat.contains("text"))
            EXPECT_FALSE(beat["text"].get<std::string>().starts_with("NORM:"));
        for (const auto& v : beat.at("variants"))
            EXPECT_FALSE(v.at("text").get<std::string>().starts_with("NORM:"));
        if (!beat.at("choices").empty())
            EXPECT_EQ(beat["choices"].size(), 2u);
    }
    Tutorial t;
    auto old = t.view().choices[1].id;
    ASSERT_TRUE(t.choose(old));
    EXPECT_EQ(t.beat(), "N002");
    EXPECT_NE(t.view().text.find("keeping my desk from wobbling"), std::string::npos);
    EXPECT_FALSE(t.choose(old));
}
TEST(Tutorial, CompletionComesOnlyFromConfirmedSimulationEvents) {
    Lesson h;
    h.to_scan();
    h.lesson.tick(999999);
    EXPECT_EQ(h.lesson.beat(), "N008");
    h.sandbox.scan(true, true, ScanScope::Full);
    h.event("scan_started");
    h.sandbox.advance(19999);
    h.event("scan_completed");
    EXPECT_EQ(h.lesson.beat(), "N009");
    EXPECT_TRUE(h.sandbox.snapshot().endpoints.empty());
    h.sandbox.advance(1);
    h.event("scan_completed");
    EXPECT_EQ(h.lesson.beat(), "N010");
    EXPECT_EQ(h.sandbox.snapshot().endpoints.size(), tutorial_devices().size());
}
TEST(Tutorial, TwoDistinctExplicitDeviceSavesAreRequired) {
    Lesson h;
    h.to_scan();
    h.scan();
    h.choice();
    h.navigate(DevicesPage);
    h.choice();
    h.choice();
    const auto guided = h.lesson.view().targets;
    EXPECT_NE(std::find(guided.begin(), guided.end(), "device:tutorial-mic"), guided.end());
    EXPECT_NE(std::find(guided.begin(), guided.end(), "device:tutorial-speakers"), guided.end());
    h.event("device_edit");
    auto device = *endpoint(h.sandbox.snapshot(), "tutorial-mic");
    device.display_name = "One";
    h.sandbox.update_endpoint(device);
    h.event("device_saved");
    const auto remaining = h.lesson.view().targets;
    EXPECT_EQ(std::find(remaining.begin(), remaining.end(), "device:tutorial-mic"),
              remaining.end());
    EXPECT_NE(std::find(remaining.begin(), remaining.end(), "device:tutorial-speakers"),
              remaining.end());
    device.display_name = "One again";
    h.sandbox.update_endpoint(device);
    h.event("device_saved");
    EXPECT_EQ(h.lesson.beat(), "N015");
    EXPECT_NE(h.lesson.view().text.find("One down"), std::string::npos);
}
TEST(Tutorial, BothMicrophoneBranchesCompleteWithoutMeasuringAnyAudio) {
    for (int branch : {0, 1}) {
        Lesson h;
        h.to_route();
        EXPECT_EQ(h.lesson.beat(), "N026");
        ASSERT_TRUE(h.sandbox.activate(h.draft.id).ok);
        h.event("routing_started");
        h.sandbox.advance(7999);
        h.event("simulated_routing_running");
        EXPECT_EQ(h.lesson.beat(), "N027");
        h.sandbox.advance(1);
        h.event("simulated_routing_running");
        EXPECT_EQ(h.lesson.beat(), "N027B");
        h.choice();
        h.choice();
        EXPECT_EQ(h.lesson.beat(), "N028");
        h.lesson.tick(999999);
        EXPECT_EQ(h.lesson.beat(), "N028");
        EXPECT_EQ(h.lesson.view().choices[0].label, "I said something");
        EXPECT_EQ(h.lesson.view().choices[1].label, "Testing 1, 2");
        EXPECT_FALSE(h.sandbox.snapshot().monitoring);
        EXPECT_FALSE(h.sandbox.snapshot().input_peak);
        h.choice(branch);
        EXPECT_EQ(h.lesson.beat(), branch == 0 ? "N030-B" : "N030-A");
        h.choice();
        h.choice();
        h.choice();
        h.choice();
        h.plug_lesson(branch);
        h.navigate(SettingsPage);
        EXPECT_FALSE(h.lesson.complete());
        h.choice();
        EXPECT_TRUE(h.lesson.complete());
        EXPECT_EQ(h.lesson.beat(), "N035");
        h.choice(branch);
        EXPECT_EQ(h.lesson.beat(), "N036");
        EXPECT_NE(h.lesson.view().text.find("young grasshopper"), std::string::npos);
        EXPECT_NE(h.lesson.view().text.find(branch == 0 ? "exit and restart now"
                                                        : "Hit End Tutorial and restart"),
                  std::string::npos);
        EXPECT_TRUE(h.lesson.view().choices.empty());
        EXPECT_EQ(h.sandbox.snapshot().operating_mode, OperatingMode::Tutorial);
    }
}
TEST(Tutorial, ClickOverlayRestoresExactVariantChoicesAndHighlightWithoutProgress) {
    Tutorial t;
    t.choose(t.view().choices[1].id);
    const auto before = t.view();
    t.click(1000);
    EXPECT_NE(t.view().text, before.text);
    EXPECT_EQ(t.view().choices[0].id, before.choices[0].id);
    t.tick(6999);
    EXPECT_NE(t.view().text, before.text);
    t.tick(7000);
    EXPECT_EQ(t.view().text, before.text);
    EXPECT_EQ(t.view().expression, before.expression);
    t.click(12000);
    ASSERT_TRUE(t.choose(before.choices[0].id));
    EXPECT_EQ(t.beat(), "N003");
    const auto next = t.view().text;
    t.tick(999999);
    EXPECT_EQ(t.view().text, next);
}
TEST(Tutorial, CancelledOperationsCanRetryAndLateResultsCannotSkipSteps) {
    Lesson h;
    h.to_scan();
    h.sandbox.scan(true, true, ScanScope::Full);
    h.event("scan_started");
    h.sandbox.scan(false);
    h.event("scan-cancelled");
    EXPECT_EQ(h.lesson.beat(), "N008");
    h.sandbox.advance(60000);
    h.event("scan_completed");
    EXPECT_EQ(h.lesson.beat(), "N008");
    h.scan();
    EXPECT_EQ(h.lesson.beat(), "N010");
}
TEST(Tutorial, UserActionsDismissClickQuipsAndBypassReminderRetainsMicrophoneChoices) {
    Lesson h;
    h.to_route();
    h.route();
    h.choice();
    h.choice();
    ASSERT_EQ(h.lesson.beat(), "N028");
    const auto before = h.lesson.view();
    h.lesson.click(1000);
    EXPECT_NE(h.lesson.view().text, before.text);
    h.event("exclusive"); // An unrelated, confirmed edit does not complete the step.
    EXPECT_EQ(h.lesson.view().text, before.text);
    h.navigate(SettingsPage);
    EXPECT_EQ(h.lesson.beat(), "N028");
    EXPECT_TRUE(h.lesson.view().sightseeing);
    EXPECT_TRUE(h.lesson.view().choices.empty());
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"profiles_tab"});
    EXPECT_FALSE(h.lesson.choose(before.choices[0].id));
    h.navigate(ProfilesPage);
    EXPECT_EQ(h.lesson.view().text, before.text);
    EXPECT_EQ(h.lesson.view().choices[0].id, before.choices[0].id);
    EXPECT_FALSE(h.lesson.complete());
}
TEST(Tutorial, ExclusivePreferenceAndNamesDoNotChangeStableAssignments) {
    Lesson h;
    h.to_route();
    const auto& hole = h.draft.holes[0];
    EXPECT_TRUE(tutorial_assignment(hole));
    EXPECT_FALSE(hole.reverse.exclusive_mode_preferred);
    EXPECT_EQ(hole.forward.source, "tutorial-mic");
    EXPECT_EQ(endpoint(h.sandbox.snapshot(), "tutorial-mic")->display_name, "My headset mic");
}
TEST(Tutorial, SkippingIsExplicitAndInactivityNeverMarksCompletion) {
    Tutorial t;
    t.tick(999999999);
    EXPECT_FALSE(t.complete());
    EXPECT_EQ(t.beat(), "N001");
    t.skip();
    EXPECT_TRUE(t.complete());
    EXPECT_EQ(t.beat(), "B010");
}
TEST(Tutorial, FictionalInventoryIsOfferedAndDiscoveryPreservesEditsOnRescan) {
    SimulatedController sandbox(true);
    const auto revision = sandbox.snapshot().scanner.inventory_revision;
    sandbox.scan(true, true, ScanScope::Full);
    sandbox.advance(20000);
    EXPECT_GT(sandbox.snapshot().scanner.inventory_revision, revision);
    EXPECT_EQ(configuration_devices(sandbox.snapshot()).size(), tutorial_devices().size());
    auto edited = *endpoint(sandbox.snapshot(), "tutorial-mic");
    edited.display_name = "Desk mic";
    edited.notes = "Keep notes";
    ASSERT_TRUE(sandbox.update_endpoint(edited).ok);
    sandbox.scan(true, true, ScanScope::Full);
    sandbox.advance(20000);
    EXPECT_EQ(endpoint(sandbox.snapshot(), edited.id)->display_name, "Desk mic");
    EXPECT_EQ(endpoint(sandbox.snapshot(), edited.id)->notes, "Keep notes");
    BuildPreferences preferences({}, true);
    preferences.load();
    EXPECT_THROW(preferences.data_root(OperatingMode::Tutorial), std::logic_error);
}

} // namespace

TEST(Tutorial, DeviceEditorGuidanceTracksIntendedSelectionAndConfirmedSave) {
    Lesson h;
    h.to_scan();
    h.scan();
    h.choice();
    h.navigate(DevicesPage);
    h.choice();
    h.choice();
    ASSERT_EQ(h.lesson.beat(), "N014");
    auto has = [&](std::string_view device, bool dirty, const char* target) {
        const auto targets = h.lesson.view(device, dirty).targets;
        return std::find(targets.begin(), targets.end(), target) != targets.end();
    };
    EXPECT_FALSE(has("", false, "device_name"));
    EXPECT_FALSE(has("tutorial-mixer-in", true, "device_name"));
    EXPECT_FALSE(has("tutorial-mixer-out", true, "device_save"));
    EXPECT_TRUE(has("tutorial-mic", false, "device_name"));
    EXPECT_FALSE(has("tutorial-mic", false, "device_save"));
    h.event("device_edit");
    EXPECT_TRUE(has("tutorial-mic", true, "device_save"));
    auto mic = *endpoint(h.sandbox.snapshot(), "tutorial-mic");
    mic.display_name = "Any valid operator name";
    ASSERT_TRUE(h.sandbox.update_endpoint(mic).ok);
    h.event("device_saved");
    EXPECT_FALSE(has("tutorial-mic", false, "device_name"));
    EXPECT_FALSE(has("tutorial-mic", false, "device_save"));
    EXPECT_FALSE(has("tutorial-mic", false, "device:tutorial-mic"));
    EXPECT_FALSE(has("tutorial-mic", true, "device_name"));
    EXPECT_FALSE(has("tutorial-mic", true, "device_save"));
    EXPECT_TRUE(has("tutorial-mic", false, "device:tutorial-speakers"));
}
TEST(Tutorial, WorkingGreetingsNeverSwitchPersonalityOrAddTutorialChoices) {
    for (const auto outcome : {OnboardingOutcome::Completed, OnboardingOutcome::Skipped}) {
        const auto norm = onboarding_greeting(outcome, "default");
        ASSERT_TRUE(norm);
        EXPECT_EQ(norm->expression, "mischievous");
        EXPECT_NE(norm->text.find(outcome == OnboardingOutcome::Completed
                                      ? "Training wheels are off"
                                      : "skipping the tutorial"),
                  std::string::npos);
        EXPECT_FALSE(onboarding_greeting(outcome, "vanilla"));
        EXPECT_FALSE(onboarding_greeting(outcome, "unknown-personality"));
    }
    EXPECT_FALSE(onboarding_greeting(OnboardingOutcome::Completed, "uwu"));
    const auto horu = onboarding_greeting(OnboardingOutcome::Skipped, "uwu");
    ASSERT_TRUE(horu);
    EXPECT_EQ(horu->expression, "mischievous");
    EXPECT_TRUE(horu->text.starts_with("Ehehe~"));
    EXPECT_FALSE(onboarding_greeting(OnboardingOutcome::None, "default"));
    Tutorial lesson;
    EXPECT_EQ(lesson.beat(), "N001");
    EXPECT_EQ(lesson.view().choices.size(), 2u);
    EXPECT_EQ(
        lesson.view().expression,
        nlohmann::json::parse(tutorial_catalog()).at("N001").at("expression").get<std::string>());
}

TEST(Tutorial, SightseeingOnlyHighlightsRequiredTabAndRestoresExactBranch) {
    Lesson h;
    h.to_scan();
    const auto before = h.lesson.view();
    h.navigate(SettingsPage);
    EXPECT_TRUE(h.lesson.view().sightseeing);
    EXPECT_TRUE(h.lesson.view().choices.empty());
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"scan_tab"});
    h.lesson.click(1000);
    EXPECT_TRUE(h.lesson.view().sightseeing);
    EXPECT_TRUE(h.lesson.view().choices.empty());
    h.navigate(ScanningPage);
    EXPECT_FALSE(h.lesson.view().sightseeing);
    EXPECT_EQ(h.lesson.view().beat, before.beat);
    EXPECT_EQ(h.lesson.view().text, before.text);
    EXPECT_EQ(h.lesson.view().targets, before.targets);
    h.scan();
    h.navigate(SettingsPage);
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"scan_tab"});
    h.navigate(ScanningPage);
    EXPECT_FALSE(h.lesson.view().choices.empty());
}

TEST(Tutorial, DeletedDependenciesRecoverAndCompletionLiftsRecovery) {
    Lesson f;
    f.to_route();
    f.draft.holes.clear();
    f.event("hole_deleted");
    EXPECT_EQ(f.lesson.beat(), "N018");
    EXPECT_EQ(f.lesson.view().expression, "surprised");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"hole_new"});
    EXPECT_NE(f.lesson.view().text.find("put it back"), std::string::npos);
    f.draft.holes.push_back(f.sandbox.make_hole());
    f.event("hole_created");
    EXPECT_EQ(f.lesson.beat(), "N018B");
    EXPECT_NE(f.lesson.view().text.find("Same deal"), std::string::npos);
    ASSERT_TRUE(f.sandbox.remove_profile(f.draft.id).ok);
    f.draft = {};
    f.event("observe");
    EXPECT_EQ(f.lesson.beat(), "N017");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profile_new"});
    const auto id = f.sandbox.create_profile("Replacement");
    f.draft = *profile(f.sandbox.snapshot(), id);
    f.event("profile_created");
    EXPECT_EQ(f.lesson.beat(), "N018A");
    f.lesson.skip();
    ASSERT_TRUE(f.sandbox.remove_profile(id).ok);
    f.event("observe");
    EXPECT_TRUE(f.lesson.complete());
    EXPECT_EQ(f.lesson.beat(), "B010");
}
TEST(Tutorial, InvalidScanChoicesGuideBackToOptionsAndEarlyProfilesAreExplained) {
    Lesson f;
    f.to_scan();
    f.event("scan_options_invalid");
    EXPECT_EQ(f.lesson.beat(), "N007");
    EXPECT_EQ(f.lesson.view().targets, (std::vector<std::string>{"scan_level", "scan_scope"}));
    f.event("scan_options");
    f.scan();
    f.choice();
    f.navigate(DevicesPage);
    f.choice();
    f.choice();
    auto mic = *endpoint(f.sandbox.snapshot(), "tutorial-mic");
    mic.display_name = "Mic";
    f.sandbox.update_endpoint(mic);
    f.event("device_saved");
    auto speakers = *endpoint(f.sandbox.snapshot(), "tutorial-speakers");
    speakers.display_name = "Phones";
    f.sandbox.update_endpoint(speakers);
    f.event("device_saved");
    f.sandbox.create_profile("Ahead of Norm");
    f.navigate(ProfilesPage);
    EXPECT_EQ(f.lesson.beat(), "N017");
    EXPECT_TRUE(f.lesson.view().text.starts_with("Ah, you've already started without me."));
    f.page = ScanningPage;
    f.event("observe");
    f.lesson.repeat();
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profiles_tab"});
    EXPECT_TRUE(f.lesson.view().choices.empty());
}

TEST(Tutorial, ClearingEarlierAssignmentGuidesBackUntilThatRequirementIsRepaired) {
    Lesson f;
    f.to_route();
    ASSERT_EQ(f.lesson.beat(), "N026");
    f.draft.holes[0].forward.destination.clear();
    f.event("observe");
    EXPECT_EQ(f.lesson.beat(), "N021");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"forward_destination"});
    EXPECT_NE(f.lesson.view().text.find("Sure. No rush"), std::string::npos);
    f.navigate(ScanningPage);
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profiles_tab"});
    f.navigate(ProfilesPage);
    f.draft.holes[0].forward.destination = "tutorial-mixer-out";
    f.event("endpoints");
    EXPECT_EQ(f.lesson.beat(), "N021A");
}

TEST(Tutorial, WrongSelectionDisabledHoleAndUnsavedEditsGuideTheirActualPrerequisites) {
    Lesson f;
    f.to_route();
    const auto owned = f.draft;
    f.draft = {};
    f.event("observe");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profile:" + owned.id});
    f.draft = owned;
    const auto hole = f.draft.holes[0];
    auto other_hole = hole;
    other_hole.id = "other-hole";
    f.draft.holes.push_back(other_hole);
    f.lesson.event("observe", f.sandbox.snapshot(), f.draft, 1, f.page);
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"hole:" + hole.id});
    f.draft.holes.pop_back();
    f.draft.holes[0].enabled = false;
    f.event("observe");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"hole_enabled"});
    f.draft.holes[0].enabled = true;
    f.draft.notes = "A later unsaved edit";
    f.event("observe");
    EXPECT_EQ(f.lesson.beat(), "N024");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profile_save"});
    f.lesson.skip();
    EXPECT_TRUE(f.lesson.view().targets.empty());
}

TEST(Tutorial, ARevertedButStillDirtyEditorRequiresSaveBeforeStarting) {
    Lesson f;
    f.to_route();
    f.event("observe_dirty");
    EXPECT_EQ(f.lesson.beat(), "N024");
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profile_save"});
    EXPECT_FALSE(f.lesson.ready_to_route(f.draft, 0));
}

TEST(Tutorial, PlugBoardChecksAssignmentsAtTheChoiceAndPreservesOptionalExploration) {
    Lesson h;
    h.to_plug();
    h.plug_lesson(1, false);
    h.practice.assignments["game"] = "songs";
    h.lesson.plug_board(h.practice);
    const auto next = h.lesson.view().choices[0].id;
    EXPECT_FALSE(h.lesson.choose(next));
    EXPECT_EQ(h.lesson.beat(), "PB010");
    EXPECT_NE(h.lesson.view().text.find("Before we move on"), std::string::npos);
    h.practice.assignments["game"] = "explosions";
    h.lesson.plug_board(h.practice);
    ASSERT_TRUE(h.lesson.choose(next));
    EXPECT_EQ(h.lesson.beat(), "PB013");
    h.practice.sockets.clear(); // Exploration is allowed, but Settings must revalidate.
    h.lesson.plug_board(h.practice);
    EXPECT_EQ(h.lesson.beat(), "PB013");
    h.navigate(SettingsPage);
    EXPECT_EQ(h.lesson.beat(), "PB004");
    EXPECT_FALSE(h.lesson.complete());
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"plug_board_tab"});
    EXPECT_TRUE(h.lesson.view().text.starts_with("Oh, come on. We were using that one!"));
}
TEST(Tutorial, DeletingARequiredPracticeSocketRestoresItsCreationSection) {
    const auto catalog = nlohmann::json::parse(tutorial_catalog());
    for (bool music : {true, false}) {
        Lesson h;
        h.to_plug();
        h.plug_lesson(0, false);
        const auto peer = h.practice.sockets[music ? 2 : 1];
        const auto peer_app = music ? "game" : "music";
        const auto affected_app = music ? "music" : "game";
        const auto removed_id = music ? "songs" : "explosions";
        std::erase_if(h.practice.sockets,
                      [&](const auto& socket) { return socket.id == removed_id; });
        h.practice.assignments[affected_app] = "system";
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), music ? "PB004" : "PB007");
        const auto recovery = h.lesson.view();
        EXPECT_EQ(recovery.text, catalog.at("PB-R09").at("text").get<std::string>());
        EXPECT_EQ(recovery.expression, "surprised");
        EXPECT_EQ(recovery.targets, std::vector<std::string>{"plug_create"});
        EXPECT_TRUE(recovery.choices.empty());
        h.lesson.click(1000);
        EXPECT_NE(h.lesson.view().text, recovery.text);
        h.lesson.tick(7000);
        EXPECT_EQ(h.lesson.view().text, recovery.text);
        h.practice.sockets.push_back({"replacement", "Any replacement name", ""});
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), music ? "PB006" : "PB009");
        h.practice.sockets.back().endpoint = music ? "tutorial-music" : "tutorial-game";
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), music ? "PB011" : "PB010");
        h.practice.assignments[affected_app] = removed_id;
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), music ? "PB011" : "PB010");
        EXPECT_FALSE(h.lesson.complete());
        h.practice.assignments[affected_app] = "replacement";
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), "PB012");
        const auto retained =
            std::find_if(h.practice.sockets.begin(), h.practice.sockets.end(),
                         [&](const auto& socket) { return socket.id == peer.id; });
        ASSERT_NE(retained, h.practice.sockets.end());
        EXPECT_EQ(retained->name, peer.name);
        EXPECT_EQ(retained->endpoint, peer.endpoint);
        EXPECT_EQ(h.practice.assignments.at(peer_app), peer.id);
        h.choice();
        h.navigate(SettingsPage);
        EXPECT_EQ(h.lesson.beat(), "N034");
        h.choice();
        EXPECT_TRUE(h.lesson.complete());
        h.practice.sockets.clear();
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), "N035");
    }
}
TEST(Tutorial, PracticeSocketCreationBeforeItsCueCannotSatisfyTheNewSocketObjective) {
    Lesson h;
    h.to_plug();
    TutorialPlugBoard practice;
    practice.sockets = {{"system", "System", ""}, {"early", "Early socket", "tutorial-music"}};
    h.lesson.plug_board(practice);
    h.navigate(PlugBoardPage);
    h.choice();
    h.choice();
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB004");
    practice.sockets.push_back({"new", " \t", ""});
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB004");
    practice.sockets.back().name = "Anything I choose";
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB006");
    practice.sockets.back().endpoint = "tutorial-chat";
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB006");
    practice.sockets.back().endpoint = "tutorial-music";
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB007");
    practice.sockets.push_back({"new-game", "Anything I choose", ""});
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB007");
    practice.sockets.back().name = "Another arbitrary label";
    h.lesson.plug_board(practice);
    EXPECT_EQ(h.lesson.beat(), "PB009");
}

TEST(Tutorial, BothTransitionChoicesSelectCompletePlugBoardIntroductionsAndWaitForNavigation) {
    for (int transition : {0, 1}) {
        for (int introduction : {0, 1}) {
            Lesson h;
            h.to_plug_transition();
            h.choice(transition);
            ASSERT_EQ(h.lesson.beat(), "N033");
            EXPECT_TRUE(h.lesson.view().text.starts_with(transition == 0
                                                             ? "See? You're getting the hang"
                                                             : "You can, but you don't need to."));
            ASSERT_EQ(h.lesson.view().choices.size(), 2u);
            EXPECT_EQ(h.lesson.view().choices[0].label, "Sounds organized");
            EXPECT_EQ(h.lesson.view().choices[1].label, "How bad could it be?");
            EXPECT_TRUE(h.lesson.view().targets.empty());
            h.navigate(SettingsPage); // The obsolete N033-to-Settings shortcut is forbidden.
            EXPECT_EQ(h.lesson.beat(), "N033");
            EXPECT_FALSE(h.lesson.complete());
            h.navigate(ProfilesPage);
            const auto choice = h.lesson.view().choices[introduction].id;
            ASSERT_TRUE(h.lesson.choose(choice));
            EXPECT_FALSE(h.lesson.choose(choice));
            ASSERT_EQ(h.lesson.beat(), "PB001");
            EXPECT_EQ(h.lesson.view().expression, introduction == 0 ? "talking" : "deadpan");
            EXPECT_NE(h.lesson.view().text.find(introduction == 0 ? "Black Friday 1996"
                                                                  : "Black Friday back in '96"),
                      std::string::npos);
            EXPECT_NE(h.lesson.view().text.find("Tickle Me Elmo"), std::string::npos);
            EXPECT_TRUE(h.lesson.view().choices.empty());
            EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"plug_board_tab"});
            h.lesson.tick(999999);
            h.event("observe");
            EXPECT_EQ(h.lesson.beat(), "PB001");
            h.navigate(PlugBoardPage);
            EXPECT_EQ(h.lesson.beat(), "PB002");
        }
    }
}

TEST(Tutorial, SettingsRevalidatesEndpointsAssignmentsAndDeletedSocketsAfterExploration) {
    const std::array<const char*, 6> required{"PB006", "PB009", "PB010", "PB011", "PB004", "PB007"};
    for (int change = 0; change < static_cast<int>(required.size()); ++change) {
        Lesson h;
        h.to_plug();
        h.plug_lesson();
        if (change < 2)
            h.practice.sockets[change + 1].endpoint = "tutorial-chat";
        else if (change < 4)
            h.practice.assignments[change == 2 ? "game" : "music"] = "system";
        else
            h.practice.sockets.erase(h.practice.sockets.begin() + (change == 4 ? 1 : 2));
        h.lesson.plug_board(h.practice);
        EXPECT_EQ(h.lesson.beat(), "PB013");
        h.navigate(SettingsPage);
        EXPECT_EQ(h.lesson.beat(), required[change]);
        EXPECT_FALSE(h.lesson.complete());
        EXPECT_FALSE(h.lesson.choose("N034/0"));
        EXPECT_TRUE(h.lesson.view().choices.empty());
        EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"plug_board_tab"});
        const auto reason = h.lesson.view().text;
        h.navigate(PlugBoardPage);
        EXPECT_EQ(h.lesson.view().text, reason);
        ASSERT_FALSE(h.lesson.view().targets.empty());
        EXPECT_EQ(h.lesson.view().targets.front(),
                  change < 2
                      ? std::string("plug_endpoint:") + (change == 0 ? "songs" : "explosions")
                  : change < 4 ? std::string("plug_app:") + (change == 2 ? "game" : "music")
                               : "plug_create");
    }
}

TEST(Tutorial, SettingsBridgeIsOneMessageWithItsChoicesAndRechecksBeforeCompleting) {
    Lesson h;
    h.to_plug();
    h.plug_lesson();
    h.navigate(SettingsPage);
    ASSERT_EQ(h.lesson.beat(), "N034");
    const auto catalog = nlohmann::json::parse(tutorial_catalog());
    const auto arrival = h.lesson.view();
    EXPECT_EQ(arrival.text, catalog.at("PB014").at("text").get<std::string>() + "\n\n" +
                                catalog.at("N034").at("text").get<std::string>());
    EXPECT_EQ(arrival.expression, "talking");
    ASSERT_EQ(arrival.choices.size(), 2u);
    EXPECT_EQ(arrival.choices[0].label, "Got it");
    EXPECT_EQ(arrival.choices[1].label, "What’s locked?");
    h.lesson.click(1000);
    EXPECT_FALSE(h.lesson.view().text.starts_with("Ah, there you are."));
    h.lesson.tick(7000);
    EXPECT_EQ(h.lesson.view().text, arrival.text);
    h.practice.assignments["music"] = "system";
    h.lesson.plug_board(h.practice);
    EXPECT_FALSE(h.lesson.choose(arrival.choices[0].id));
    EXPECT_FALSE(h.lesson.complete());
    EXPECT_EQ(h.lesson.beat(), "PB011");
    h.navigate(PlugBoardPage);
    h.practice.assignments["music"] = "songs";
    h.lesson.plug_board(h.practice);
    EXPECT_EQ(h.lesson.beat(), "PB012");
    h.choice();
    h.navigate(SettingsPage);
    h.choice();
    EXPECT_EQ(h.lesson.beat(), "N035");
    EXPECT_TRUE(h.lesson.complete());
}
