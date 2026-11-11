#include "model/controller.h"
#include "presentation/messages.h"
#include <algorithm>
#include <gtest/gtest.h>

using namespace audiohole;

TEST(AdvancedTuning, NewHolesIgnoreRetainedUnsupportedOverridesUntilExplicitlyEnabled) {
    SimulatedController controller;
    auto candidate = controller.snapshot().profiles.front();
    auto& hole = candidate.holes.front();
    EXPECT_FALSE(controller.make_hole().advanced_tuning);
    EXPECT_FALSE(hole.advanced_tuning);
    hole.forward.settings.preferences.common = {96000, 2, 20, "float32", "Exclusive"};
    hole.forward.settings.refresh_projection();
    const auto retained = hole.forward;
    PreparedProfile request;
    ASSERT_TRUE(prepare_profile(controller.snapshot(), candidate, request).ok);
    EXPECT_EQ(request.paths.front().buffer_ms, 0);
    EXPECT_EQ(request.paths.front().mode, "Shared");
    EXPECT_EQ(request.paths.front().format.rate, 48000);
    EXPECT_EQ(hole.forward, retained);
    hole.advanced_tuning = true;
    const auto previous = request.paths.front().format;
    EXPECT_FALSE(prepare_profile(controller.snapshot(), candidate, request).ok);
    EXPECT_EQ(request.paths.front().format, previous); // Failed validation is transactional.
    hole.advanced_tuning = false;
    EXPECT_TRUE(validate(controller.snapshot(), candidate).ok);
    hole.advanced_tuning = true;
    EXPECT_EQ(hole.forward, retained);
}

TEST(AdvancedTuning, DuplexDirectionsKeepIndependentOverridesAndIndividualAutomaticValues) {
    SimulatedController controller;
    auto candidate = controller.snapshot().profiles[1];
    auto& hole = candidate.holes.front();
    hole.advanced_tuning = true;
    hole.forward.settings.set_buffer_preference(11);
    hole.forward.settings.preferences.common.mode = "Exclusive";
    hole.forward.settings.refresh_projection();
    // Reverse remains format/mode/buffer Automatic, independently of Forward.
    PreparedProfile prepared;
    ASSERT_TRUE(prepare_profile(controller.snapshot(), candidate, prepared).ok);
    ASSERT_EQ(prepared.paths.size(), 2u);
    EXPECT_EQ(prepared.paths[0].buffer_ms, 11);
    EXPECT_EQ(prepared.paths[0].mode, "Exclusive");
    EXPECT_EQ(prepared.paths[1].buffer_ms, 0);
    EXPECT_EQ(prepared.paths[1].mode, "Shared");
    const auto retained = hole;
    hole.advanced_tuning = false;
    ASSERT_TRUE(prepare_profile(controller.snapshot(), candidate, prepared).ok);
    EXPECT_EQ(prepared.paths[0].buffer_ms, 0);
    EXPECT_EQ(prepared.paths[0].mode, "Shared");
    hole.advanced_tuning = true;
    EXPECT_EQ(hole, retained);
}

TEST(AdvancedTuning, SavingPolicyDoesNotStartOrReconfigureTheLiveCopy) {
    SimulatedController controller;
    auto candidate = controller.snapshot().profiles.front();
    const auto id = candidate.id;
    ASSERT_TRUE(controller.activate(id).ok);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().paths.front().applied_buffer_ms, 6);
    candidate.holes.front().advanced_tuning = true;
    candidate.holes.front().forward.settings.set_buffer_preference(17);
    ASSERT_TRUE(controller.save(candidate, false).ok);
    EXPECT_EQ(controller.snapshot().paths.front().applied_buffer_ms, 6);
    // A directional update cannot opt an automatic live route into Advanced.
    ASSERT_TRUE(controller.update_buffer(id, candidate.holes.front().id, false, 19).ok);
    EXPECT_EQ(controller.snapshot().paths.front().applied_buffer_ms, 6);
    ASSERT_TRUE(controller.activate(id).ok);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().paths.front().applied_buffer_ms, 19);
    auto restored = *profile(controller.snapshot(), id);
    restored.holes.front().advanced_tuning = false;
    ASSERT_TRUE(controller.save(restored, false).ok);
    EXPECT_EQ(controller.snapshot().paths.front().applied_buffer_ms, 19);
    ASSERT_TRUE(controller.activate(id).ok);
    controller.advance(300);
    EXPECT_EQ(controller.snapshot().paths.front().applied_buffer_ms, 6);
    EXPECT_EQ(profile(controller.snapshot(), id)->holes.front().forward.settings.buffer_ms, 19);
    controller.stop();
    ASSERT_TRUE(controller.save(candidate, false).ok);
    EXPECT_FALSE(controller.snapshot().armed);
}

TEST(AdvancedTuning, ManualAcknowledgmentRequiresAConfirmedSimulatedApplication) {
    SimulatedController controller;
    auto candidate = controller.snapshot().profiles.front();
    candidate.holes.front().advanced_tuning = true;
    auto count = [&] {
        return std::count_if(
            controller.snapshot().events.begin(), controller.snapshot().events.end(),
            [](const auto& event) { return event.type == "manual-override-applied"; });
    };
    ASSERT_TRUE(controller.save(candidate, false).ok);
    EXPECT_EQ(count(), 0);
    ASSERT_TRUE(controller.activate(candidate.id).ok); // All parameters Automatic.
    controller.advance(300);
    EXPECT_EQ(count(), 0);
    candidate.holes.front().forward.settings.set_buffer_preference(15);
    ASSERT_TRUE(controller.save(candidate, true).ok);
    EXPECT_EQ(count(), 0);
    controller.advance(300);
    EXPECT_EQ(count(), 1);
    controller.advance(1000);
    EXPECT_EQ(count(), 1);
}

TEST(AdvancedTuning, AuthoredPersonalityReactionsKeepFactsAndConfigurationIndependent) {
    presentation::Messages messages(4);
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    SimulatedController controller;
    const auto original = controller.snapshot().profiles;
    for (auto event :
         {presentation::Message::AdvancedTuningEnabled,
          presentation::Message::ManualOverrideApplied, presentation::Message::AutomaticRestored}) {
        for (const auto* id : {"default", "uwu"}) {
            ASSERT_GE(messages.variants(event, id).size(), 3u);
            const auto first = messages.show(event, id, 1);
            EXPECT_EQ(first.technical, presentation::technical(event));
            EXPECT_NE(first.response_id, messages.show(event, id, 2).response_id);
        }
        const auto vanilla = messages.show(event, "vanilla", 1);
        EXPECT_TRUE(vanilla.commentary.empty());
        EXPECT_EQ(vanilla.technical, messages.show(event, "vanilla", 2).technical);
    }
    EXPECT_EQ(controller.snapshot().profiles, original);
}
TEST(AdvancedTuning, RoutingEngineSelectionIsIndependentOfOverrideEligibility) {
    SimulatedController controller;
    auto candidate = controller.snapshot().profiles.front();
    auto& hole = candidate.holes.front();
    EXPECT_EQ(controller.make_hole().routing_engine, "automatic");
    PreparedProfile prepared;
    ASSERT_TRUE(prepare_profile(controller.snapshot(), candidate, prepared).ok);
    EXPECT_EQ(prepared.paths.front().routing_engine, "automatic");
    hole.routing_engine = "future-test-engine";
    EXPECT_FALSE(prepare_profile(controller.snapshot(), candidate, prepared).ok);
    EXPECT_EQ(hole.routing_engine, "future-test-engine");
    hole.advanced_tuning = true;
    const auto unavailable = prepare_profile(controller.snapshot(), candidate, prepared);
    EXPECT_FALSE(unavailable.ok);
    EXPECT_NE(unavailable.detail.find("Routing Engine is unavailable"), std::string::npos);
    hole.routing_engine = "automatic";
    ASSERT_TRUE(controller.save(candidate, false).ok);
    EXPECT_FALSE(controller.snapshot().armed);
    EXPECT_TRUE(controller.snapshot().paths.empty());
}
