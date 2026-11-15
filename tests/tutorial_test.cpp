#include "model/build_preferences.h"
#include "model/tutorial.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace audiohole;
namespace {
struct Lesson {
    Tutorial lesson;
    SimulatedController sandbox{true};
    Profile draft;
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
        navigate(1);
        navigate(2);
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
        navigate(1);
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
        navigate(0);
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
    void route() {
        ASSERT_TRUE(sandbox.activate(draft.id).ok);
        event("routing_started");
        sandbox.advance(9999);
        event("simulated_routing_running");
        EXPECT_EQ(lesson.beat(), "N027");
        sandbox.advance(1);
        event("simulated_routing_running");
    }
};
TEST(Tutorial, CatalogKeepsExactDialogueBranchesAndNoSpeakerPrefix) {
    auto catalog = nlohmann::json::parse(tutorial_catalog());
    EXPECT_EQ(catalog.size(), 67u);
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
    EXPECT_NE(t.view().text.find("wobbly desk"), std::string::npos);
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
    EXPECT_EQ(h.sandbox.snapshot().endpoints.size(), 4u);
}
TEST(Tutorial, TwoDistinctExplicitDeviceSavesAreRequired) {
    Lesson h;
    h.to_scan();
    h.scan();
    h.choice();
    h.navigate(1);
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
        h.sandbox.advance(9999);
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
        EXPECT_EQ(h.lesson.beat(), "N033");
        h.navigate(3);
        EXPECT_FALSE(h.lesson.complete());
        h.choice();
        EXPECT_TRUE(h.lesson.complete());
        EXPECT_EQ(h.lesson.beat(), "N035");
        h.choice(branch);
        EXPECT_EQ(h.lesson.beat(), "N036");
        EXPECT_NE(h.lesson.view().text.find("young grasshopper"), std::string::npos);
        EXPECT_NE(h.lesson.view().text.find("exit and restart"), std::string::npos);
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
    h.navigate(3);
    EXPECT_EQ(h.lesson.beat(), "N028");
    EXPECT_TRUE(h.lesson.view().sightseeing);
    EXPECT_TRUE(h.lesson.view().choices.empty());
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"profiles_tab"});
    EXPECT_FALSE(h.lesson.choose(before.choices[0].id));
    h.navigate(0);
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
    EXPECT_EQ(configuration_devices(sandbox.snapshot()).size(), 4u);
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
    h.navigate(1);
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
    h.navigate(3);
    EXPECT_TRUE(h.lesson.view().sightseeing);
    EXPECT_TRUE(h.lesson.view().choices.empty());
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"scan_tab"});
    h.lesson.click(1000);
    EXPECT_TRUE(h.lesson.view().sightseeing);
    EXPECT_TRUE(h.lesson.view().choices.empty());
    h.navigate(2);
    EXPECT_FALSE(h.lesson.view().sightseeing);
    EXPECT_EQ(h.lesson.view().beat, before.beat);
    EXPECT_EQ(h.lesson.view().text, before.text);
    EXPECT_EQ(h.lesson.view().targets, before.targets);
    h.scan();
    h.navigate(3);
    EXPECT_EQ(h.lesson.view().targets, std::vector<std::string>{"scan_tab"});
    h.navigate(2);
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
    f.navigate(1);
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
    f.navigate(0);
    EXPECT_EQ(f.lesson.beat(), "N017");
    EXPECT_TRUE(f.lesson.view().text.starts_with("Looks like you got a little ahead of me"));
    f.page = 2;
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
    f.navigate(2);
    EXPECT_EQ(f.lesson.view().targets, std::vector<std::string>{"profiles_tab"});
    f.navigate(0);
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
