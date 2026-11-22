#include "audiohole/plug_board/module.hpp"
#include "audiohole/plug_board/simulation.hpp"

#include <algorithm>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>

namespace audiohole::plug_board {
namespace {
struct MemoryState {
    std::mutex mutex;
    std::optional<std::string> bytes;
    bool fail = false;
    bool uncertain = false;
};
class MemoryStorage final : public CheckedStorage {
  public:
    explicit MemoryStorage(std::shared_ptr<MemoryState> value) : state(std::move(value)) {}
    StoredDocument read() override {
        std::lock_guard lock(state->mutex);
        return {state->bytes, {true, "read", {}}};
    }
    OperationResult replace(const std::optional<std::string>& expected,
                            const std::string& bytes) override {
        std::lock_guard lock(state->mutex);
        if (state->bytes != expected)
            return {false, "external_edit", "External edit detected."};
        if (state->fail)
            return {false, "injected_write_failure", "Injected storage failure."};
        state->bytes = bytes;
        if (state->uncertain)
            return {false, "injected_flush_failure",
                    "Replacement occurred but durability is unknown."};
        return {true, "durable_commit", {}};
    }

  private:
    std::shared_ptr<MemoryState> state;
};

class PlugBoardModule : public testing::Test {
  protected:
    std::shared_ptr<Simulation> simulation = std::make_shared<Simulation>();
    std::shared_ptr<MemoryState> memory = std::make_shared<MemoryState>();
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / new_id("plug-board-module");
    std::unique_ptr<Module> module;
    std::mutex changed_mutex;
    std::condition_variable changed;
    ProcessToken process{100, 50000, "C:\\Player\\Player.exe"};
    ApplicationIdentity identity{IdentityKind::Win32Image, process.executable_path};

    void start(bool isolated = false, bool host_defaults = false) {
        ModuleOptions options;
        options.data_root = root / "data";
        options.local_root = root / "local";
        options.memory_only = isolated;
        options.resolve_system_defaults = host_defaults;
        options.backend = [value = simulation] { return make_simulated_backend(value); };
        options.storage = [value = memory](const auto&) {
            return std::make_unique<MemoryStorage>(value);
        };
        module = std::make_unique<Module>(options);
        module->subscribe([this](auto) {
            std::lock_guard lock(changed_mutex);
            changed.notify_all();
        });
        ASSERT_TRUE(
            await([](const auto& snapshot) { return snapshot.phase != ModulePhase::Loading; }));
    }
    bool await(const std::function<bool(const ModuleSnapshot&)>& predicate) {
        std::unique_lock lock(changed_mutex);
        return changed.wait_for(lock, std::chrono::seconds(4),
                                [&] { return predicate(*module->snapshot()); });
    }
    EndpointInventory inventory(bool second_available = true, std::uint64_t revision = 1) {
        EndpointInventory value;
        value.revision = revision;
        value.endpoints = {{"device-one", "Speakers", "windows-one", true},
                           {"device-two", "Headphones", "windows-two", second_available}};
        value.default_windows_endpoints = {"windows-one", "windows-one", "windows-one"};
        return value;
    }
    SessionSnapshot sessions(SessionState state = SessionState::Active) {
        SessionObservation current;
        current.endpoint_id = "windows-two";
        current.instance_id = "player-active";
        current.identity = identity;
        current.process = process;
        current.display_name = "Player";
        current.state = state;
        auto inactive = current;
        inactive.endpoint_id = "windows-one";
        inactive.instance_id = "player-old";
        inactive.state = SessionState::Inactive;
        return {{current, inactive}, true, {}};
    }
    void discover() {
        simulation->set_sessions(sessions());
        simulation->set_processes({{identity, process}});
        module->set_inventory(inventory());
        module->set_visible(true);
        ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.applications.size() == 1; }));
    }
    std::string app() {
        return module->snapshot()->applications[0].id;
    }
    std::string board() {
        return module->snapshot()->selected_board_id;
    }
    CommandResult command(Command value) {
        auto result = module->submit(std::move(value));
        EXPECT_EQ(result.wait_for(std::chrono::seconds(4)), std::future_status::ready);
        return result.get();
    }
    void TearDown() override {
        simulation->block_policy(false);
        if (module) {
            auto stopped = module->begin_shutdown();
            EXPECT_EQ(stopped.wait_for(std::chrono::seconds(4)), std::future_status::ready);
            module.reset();
        }
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

TEST_F(PlugBoardModule, TutorialHasNoFileStorageAndUsesSuppliedApplicationArtwork) {
    ApplicationImage image;
    image.width = image.height = 2;
    image.rgba.assign(16, 255);
    image.source = "owner_tutorial_icon";
    image.application_specific = true;
    image.result = {true, "tutorial", "Fictional artwork."};
    simulation->set_images({{identity.value, image}});
    start(true);
    discover();
    module->request_images({app()});
    ASSERT_TRUE(await([](const auto& state) {
        return state.applications[0].image && state.applications[0].image->application_specific;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].image->source, "owner_tutorial_icon");
    EXPECT_TRUE(command(CreateSocket{board(), "Practice"}).ok);
    EXPECT_FALSE(memory->bytes.has_value());
    auto restore = module->restore_library(root / "real-library.json", true).get();
    EXPECT_FALSE(restore.ok);
    EXPECT_FALSE(module->export_diagnostics(root / "export.json", false).get().ok);
    ASSERT_EQ(module->begin_shutdown().wait_for(std::chrono::seconds(4)),
              std::future_status::ready);
    module.reset();
    EXPECT_FALSE(std::filesystem::exists(root));
}
TEST_F(PlugBoardModule, DefaultRoleNotificationsPreserveAuthoritativeHostBindings) {
    RawEndpointInventory outputs;
    outputs.defaults = {"windows-one", "windows-one", "windows-one"};
    // A deliberately different native list must never replace the host scanner.
    outputs.endpoints = {{"unrelated-native-id", "Wrong inventory", true}};
    simulation->set_outputs(outputs);
    start(true, true);
    module->set_inventory(inventory());
    ASSERT_TRUE(await([](const auto& state) {
        return state.inventory.endpoints.size() == 2 &&
               state.inventory.default_windows_endpoints[0] == "windows-one";
    }));
    outputs.defaults = {"windows-two", "windows-two", "windows-two"};
    simulation->set_outputs(outputs);
    ASSERT_TRUE(await([](const auto& state) {
        return state.inventory.default_windows_endpoints[0] == "windows-two";
    }));
    const auto state = module->snapshot();
    EXPECT_EQ(state->inventory.endpoints[0].logical_id, "device-one");
    EXPECT_EQ(state->inventory.endpoints[0].windows_id, "windows-one");
    EXPECT_EQ(state->inventory.endpoints[1].logical_id, "device-two");
    EXPECT_EQ(state->inventory.revision, 1u);
    EXPECT_EQ(state->platform_metrics.full_session_enumerations, 0u);
}

TEST_F(PlugBoardModule, StartupDoesNotEnumerateSessionsOrManageNewlyDiscoveredApplications) {
    start();
    EXPECT_EQ(module->snapshot()->platform_metrics.full_session_enumerations, 0u);
    discover();
    const auto snapshot = module->snapshot();
    ASSERT_EQ(snapshot->applications.size(), 1u);
    EXPECT_EQ(snapshot->applications[0].observation->sessions.size(), 2u);
    EXPECT_EQ(snapshot->applications[0].observation->active_endpoint_ids,
              std::vector<std::string>{"windows-two"});
    EXPECT_EQ(snapshot->applications[0].socket_id, "system");
    EXPECT_EQ(snapshot->applications[0].activity, Activity::Active);
    EXPECT_FALSE(snapshot->library->applications[0].policy_managed);
    EXPECT_EQ(simulation->policy_calls(), 0u);
}

TEST_F(PlugBoardModule, AudioHoleIsNotDiscoveredAsAnApplicationOrRouted) {
    start();
    auto batch = sessions();
    auto self = batch.sessions.front();
    self.identity = {IdentityKind::Win32Image, "C:\\Apps\\AudioHole-PlugBoard.exe"};
    self.display_name = "AudioHole";
    self.instance_id = "internal-app";
    self.process = ProcessToken{200, 300, self.identity.value};
    batch.sessions.push_back(self);
    simulation->set_sessions(batch);
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications.size() == 1 && snapshot.visible;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].display_name, "Player");
    EXPECT_EQ(module->snapshot()->library->applications.size(), 1u);
    EXPECT_EQ(simulation->policy_calls(), 0u);
}
TEST_F(PlugBoardModule, PreviouslySavedInternalApplicationsStayHiddenAndNeverBecomePendingTargets) {
    auto stored = make_library("board", utc_timestamp());
    ApplicationRecord record;
    record.id = "internal-app";
    record.display_name = "Old module";
    record.identity = {IdentityKind::Win32Image, "C:/Apps/AudioHole.exe"};
    record.policy_managed = true;
    stored.applications.push_back(record);
    stored.boards[0].working.application_assignments[record.id] = "system";
    {
        std::lock_guard lock(memory->mutex);
        memory->bytes = serialize_library(stored);
    }
    start();
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.visible; }));
    EXPECT_TRUE(module->snapshot()->applications.empty());
    EXPECT_FALSE(module->snapshot()->platform_metrics.process_watch_active);
    EXPECT_EQ(simulation->policy_calls(), 0u);
    EXPECT_EQ(module->snapshot()->library->applications.size(),
              1u); // Preserve old user JSON without using it.
}
TEST_F(PlugBoardModule, AGameWithAnAudioHoleDisplayLabelIsNotExcluded) {
    start();
    auto batch = sessions();
    for (auto& session : batch.sessions)
        session.display_name = "AudioHole.exe";
    simulation->set_sessions(batch);
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.applications.size() == 1; }));
    EXPECT_EQ(module->snapshot()->applications[0].display_name, "AudioHole");
}

TEST_F(PlugBoardModule, VerifiedExecutableAliasesCombineSessionsAndRouteEveryProcess) {
    auto library = make_library("board-alias", utc_timestamp());
    const ApplicationIdentity alias{IdentityKind::Win32Image, "C:\\Player\\Renderer.exe"};
    ApplicationRecord record;
    record.id = "app-alias";
    record.display_name = "Player";
    record.identity = identity;
    record.verified_aliases = {alias};
    library.applications.push_back(record);
    {
        std::lock_guard lock(memory->mutex);
        memory->bytes = serialize_library(library);
    }
    auto observed = sessions();
    auto renderer = observed.sessions.front();
    renderer.identity = alias;
    renderer.instance_id = "renderer-active";
    renderer.endpoint_id = "windows-one";
    renderer.process = ProcessToken{200, 60000, alias.value};
    observed.sessions.push_back(renderer);
    simulation->set_sessions(observed);
    simulation->set_processes({{identity, process}, {alias, *renderer.process}});
    start();
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications.size() == 1 && snapshot.applications[0].observation &&
               snapshot.applications[0].observation->sessions.size() == 3;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].observation->active_endpoint_ids.size(), 2u);
    EXPECT_EQ(module->snapshot()->applications[0].observed_routing, ObservationState::Multiple);
    ASSERT_TRUE(command(MoveApplication{board(), "app-alias", "system"}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].policy.roles.size(), 6u);
}

TEST_F(PlugBoardModule, ABoardSwitchInvalidatesModalContextEvenWithoutALibraryRevisionChange) {
    start();
    const auto first = board();
    const auto second = command(CreateBoard{"Second"}).created_id;
    ASSERT_TRUE(command(SelectBoard{first}).ok);
    const auto revision = module->snapshot()->library->revision;
    ASSERT_TRUE(command(SelectBoard{second}).ok);
    ASSERT_EQ(module->snapshot()->library->revision, revision);
    const auto result =
        module->submit(CreateSocket{first, "Stale modal answer"}, revision, first).get();
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.code, "stale_revision");
    EXPECT_EQ(find_board(*module->snapshot()->library, first)->working.sockets.size(), 1u);
}

TEST_F(PlugBoardModule, EditingAnIdentityOnDiskCannotReuseTheOldLiveProcessToken) {
    start();
    discover();
    const auto id = app();
    ASSERT_TRUE(command(MoveApplication{board(), id, "system"}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    module->set_visible(false);
    ASSERT_TRUE(await([](const auto& snapshot) { return !snapshot.visible; }));
    auto edited = *module->snapshot()->library;
    edited.applications[0].identity.value = "C:\\Different\\Different.exe";
    ++edited.revision;
    {
        std::lock_guard lock(memory->mutex);
        memory->bytes = serialize_library(edited);
    }
    ASSERT_TRUE(module->reload_library().get().ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::PendingProcess;
    }));
    EXPECT_EQ(simulation->policy_calls(), 1u);
    EXPECT_FALSE(module->snapshot()->applications[0].observation);
}

TEST_F(PlugBoardModule, ACorruptLocalDiagnosticLogIsPreservedAndReported) {
    const auto path = root / "local" / "diagnostics.json";
    ASSERT_TRUE(write_local_document(path, "{broken-local-log").ok);
    start();
    EXPECT_EQ(module->snapshot()->phase, ModulePhase::Ready);
    EXPECT_FALSE(module->snapshot()->diagnostic_storage.ok);
    EXPECT_EQ(module->snapshot()->diagnostic_storage.code, "diagnostic_log_invalid");
    ASSERT_TRUE(command(CreateSocket{board(), "Game"}).ok);
    ASSERT_EQ(module->begin_shutdown().wait_for(std::chrono::seconds(4)),
              std::future_status::ready);
    const auto preserved = read_local_document(path);
    ASSERT_TRUE(preserved.result.ok);
    EXPECT_EQ(preserved.bytes, "{broken-local-log");
}

TEST_F(PlugBoardModule, DesiredIntentIsDurableBeforePreferenceWorkStarts) {
    start();
    discover();
    const auto socket = command(CreateSocket{board(), "Game"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-two"}}).ok);
    simulation->block_policy(true);
    ASSERT_TRUE(command(MoveApplication{board(), app(), socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Applying;
    }));
    {
        std::lock_guard lock(memory->mutex);
        ASSERT_TRUE(memory->bytes);
        const auto persisted = parse_library(*memory->bytes);
        EXPECT_EQ(assigned_socket(find_board(persisted, board())->working, app()), socket);
        EXPECT_TRUE(find_application(persisted, app())->policy_managed);
    }
    simulation->block_policy(false);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].observed_routing, ObservationState::Matches);
}

TEST_F(PlugBoardModule, ViewDiagnosticsNeverEnrollAnAppOrSubmitPolicy) {
    start();
    discover();
    const auto revision = module->snapshot()->library->revision;
    module->record_view_event("view_drag_cancelled", "Canceled pointer gesture.",
                              {{"application_id", app()}});
    ASSERT_TRUE(await([](const auto& snapshot) {
        return std::any_of(snapshot.diagnostics.begin(), snapshot.diagnostics.end(),
                           [](const auto& event) { return event.code == "view_drag_cancelled"; });
    }));
    EXPECT_EQ(module->snapshot()->library->revision, revision);
    EXPECT_FALSE(module->snapshot()->library->applications[0].policy_managed);
    EXPECT_EQ(simulation->policy_calls(), 0u);
}

TEST_F(PlugBoardModule, KnownPlaybackIsObservedWhenRoleDefaultsDiffer) {
    start();
    simulation->set_sessions(sessions());
    auto outputs = inventory();
    outputs.default_windows_endpoints[2] = "windows-two";
    module->set_inventory(outputs);
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications.size() == 1 && snapshot.applications[0].observation_fresh;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].observed_routing, ObservationState::Observed);
}

TEST_F(PlugBoardModule, ImagesSurviveCloseRefreshAndReopenButANewProcessGetsANewLookup) {
    start();
    discover();
    const auto id = app();
    module->request_images({id});
    ASSERT_TRUE(
        await([](const auto& snapshot) { return snapshot.applications[0].image != nullptr; }));
    const auto original = module->snapshot()->applications[0].image;

    // A refresh invalidates source requests, not the pixels already displayed.
    ASSERT_TRUE(command(RefreshApplications{}).ok);
    EXPECT_EQ(module->snapshot()->applications[0].image, original);
    module->set_visible(false);
    ASSERT_TRUE(await([](const auto& snapshot) { return !snapshot.visible; }));
    EXPECT_EQ(module->snapshot()->applications[0].image, original);
    simulation->set_sessions({{}, true, {}});
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.visible && !snapshot.applications[0].observation;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].image, original);
    module->request_images({id});
    ASSERT_TRUE(await([&](const auto& snapshot) {
        return snapshot.applications[0].image && snapshot.applications[0].image != original;
    }));
    const auto closed = module->snapshot()->applications[0].image;

    // The same saved identity's next process lifetime must refresh its icon even
    // though a usable image remains on screen throughout discovery.
    ++process.creation_time;
    simulation->set_sessions(sessions());
    ASSERT_TRUE(await([&](const auto& snapshot) {
        const auto& observation = snapshot.applications[0].observation;
        return observation && !observation->verified_processes.empty() &&
               observation->verified_processes[0].creation_time == process.creation_time;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].image, closed);
    module->request_images({id});
    ASSERT_TRUE(await([&](const auto& snapshot) {
        return snapshot.applications[0].image && snapshot.applications[0].image != closed;
    }));
    EXPECT_EQ(simulation->policy_calls(), 0u);
}

TEST_F(PlugBoardModule, FailedStorageForbidsNativePolicyAndPreservesPreviousAssignment) {
    start();
    discover();
    const auto socket = command(CreateSocket{board(), "Game"}).created_id;
    {
        std::lock_guard lock(memory->mutex);
        memory->fail = true;
    }
    const auto result = command(MoveApplication{board(), app(), socket});
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.durable_intent);
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, "system");
    EXPECT_EQ(simulation->policy_calls(), 0u);
    {
        std::lock_guard lock(memory->mutex);
        memory->fail = false;
    }
}

TEST_F(PlugBoardModule, UnknownPostReplacementDurabilityStillForbidsPolicy) {
    start();
    discover();
    const auto socket = command(CreateSocket{board(), "Game"}).created_id;
    {
        std::lock_guard lock(memory->mutex);
        memory->uncertain = true;
    }
    EXPECT_FALSE(command(MoveApplication{board(), app(), socket}).ok);
    EXPECT_EQ(simulation->policy_calls(), 0u);
    EXPECT_FALSE(module->snapshot()->storage.ok);
    EXPECT_TRUE(module->snapshot()->catalog_write_pending);
    {
        std::lock_guard lock(memory->mutex);
        memory->uncertain = false;
    }
    ASSERT_TRUE(command(RetryRouting{app()}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
}

TEST_F(PlugBoardModule, StaleRevisionRejectsACommandWithoutChangingIntent) {
    start();
    const auto revision = module->snapshot()->library->revision;
    ASSERT_TRUE(command(CreateSocket{board(), "First"}).ok);
    const auto result = module->submit(CreateSocket{board(), "Stale"}, revision).get();
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.code, "stale_revision");
    EXPECT_EQ(find_board(*module->snapshot()->library, board())->working.sockets.size(), 2u);
}

TEST_F(PlugBoardModule, AcceptedPreferenceDoesNotClaimThatAnExistingStreamMoved) {
    start();
    discover();
    const auto socket = command(CreateSocket{board(), "Speakers"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-one"}}).ok);
    ASSERT_TRUE(command(MoveApplication{board(), app(), socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].observed_routing, ObservationState::Differs);
    EXPECT_EQ(module->snapshot()->applications[0].observation->active_endpoint_ids,
              std::vector<std::string>{"windows-two"});
}

TEST_F(PlugBoardModule, PartialRoleResultsStayPartialAndDoNotCauseAnAutomaticSetterLoop) {
    start();
    discover();
    simulation->reject_role(AudioRole::Communications);
    const auto socket = command(CreateSocket{board(), "Headphones"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-two"}}).ok);
    ASSERT_TRUE(command(MoveApplication{board(), app(), socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Partial;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].policy.roles.size(), 3u);
    const auto previous = module->snapshot()->publication;
    simulation->set_sessions(sessions());
    ASSERT_TRUE(await([&](const auto& snapshot) { return snapshot.publication > previous; }));
    EXPECT_EQ(simulation->policy_calls(), 1u);
}

TEST_F(PlugBoardModule, AnUnavailableEndpointIsRetainedAndReconciledOnReturn) {
    start();
    discover();
    const auto socket = command(CreateSocket{board(), "Headphones"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-two"}}).ok);
    module->set_inventory(inventory(false, 2));
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.inventory.revision == 2; }));
    ASSERT_TRUE(command(MoveApplication{board(), app(), socket}).ok);
    EXPECT_EQ(module->snapshot()->applications[0].policy.state, PolicyState::PendingDevice);
    EXPECT_EQ(simulation->policy_calls(), 0u);
    module->set_inventory(inventory(true, 3));
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, socket);
}

TEST_F(PlugBoardModule, AnInactiveApplicationCanWaitForItsProcessWhileTheViewIsHidden) {
    start();
    discover();
    const auto saved = app();
    simulation->set_sessions({{}, true, {}});
    simulation->set_processes({});
    ASSERT_TRUE(await([](const auto& snapshot) { return !snapshot.applications[0].observation; }));
    module->set_visible(false);
    ASSERT_TRUE(await([](const auto& snapshot) { return !snapshot.visible; }));
    const auto scans = module->snapshot()->platform_metrics.full_session_enumerations;
    ASSERT_TRUE(command(MoveApplication{board(), saved, "system"}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::PendingProcess;
    }));
    EXPECT_TRUE(module->snapshot()->platform_metrics.process_watch_active);
    ProcessToken restarted{101, 60000, identity.value};
    simulation->process_started({identity, restarted});
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(module->snapshot()->platform_metrics.full_session_enumerations, scans);
    EXPECT_TRUE(module->snapshot()->platform_metrics.process_watch_active);
    EXPECT_FALSE(module->snapshot()->applications[0].observation_fresh);
}

TEST_F(PlugBoardModule, AcceptedAssignmentRoutesEveryRelaunchWhileTheBoardIsHidden) {
    start();
    discover();
    const auto saved = app();
    const auto socket = command(CreateSocket{board(), "Sticky game"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-one"}}).ok);
    ASSERT_TRUE(command(MoveApplication{board(), saved, socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    simulation->set_sessions({{}, true, {}});
    ASSERT_TRUE(await([](const auto& snapshot) { return !snapshot.applications[0].observation; }));
    module->set_visible(false);
    ASSERT_TRUE(await([](const auto& snapshot) { return !snapshot.visible; }));
    const auto scans = module->snapshot()->platform_metrics.full_session_enumerations;
    // Neither instance first enters PendingProcess. The previous success must
    // keep the watch alive, and stale exited tokens must not poison its result.
    for (unsigned restart = 0; restart < 2; ++restart) {
        const ProcessToken replacement{101 + restart, 60000 + restart, identity.value};
        const auto calls = simulation->policy_calls();
        simulation->set_processes({});
        simulation->process_started({identity, replacement});
        ASSERT_TRUE(await([&](const auto& snapshot) {
            const auto& result = snapshot.applications[0].policy;
            return simulation->policy_calls() > calls && result.state == PolicyState::Accepted &&
                   result.roles.size() == 3 && result.roles[0].process == replacement;
        }));
        EXPECT_EQ(module->snapshot()->applications[0].socket_id, socket);
        EXPECT_EQ(simulation->requested_targets().back(),
                  std::optional<std::string>("windows-one"));
        EXPECT_TRUE(module->snapshot()->platform_metrics.process_watch_active);
    }
    EXPECT_EQ(module->snapshot()->platform_metrics.full_session_enumerations, scans);
}

TEST_F(PlugBoardModule, YearsOldAssignmentSurvivesShutdownAndReopensWithoutASaveButton) {
    start();
    discover();
    const auto saved = app();
    const auto socket = command(CreateSocket{board(), "Remember me"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-one"}}).ok);
    ASSERT_TRUE(command(MoveApplication{board(), saved, socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    const auto scans_before_restart =
        module->snapshot()->platform_metrics.full_session_enumerations;
    ASSERT_EQ(module->begin_shutdown().wait_for(std::chrono::seconds(4)),
              std::future_status::ready);
    module.reset();
    simulation->set_processes({});
    simulation->set_sessions({{}, true, {}});
    // Recent/Inactive is only a display classification. Simulate a last launch
    // in 2010 without waiting or depending on a wall-clock timeout in the test.
    const Document old_history{{"format", "audiohole-plug-board-local"},
                               {"schema_version", 1},
                               {"applications",
                                {{saved,
                                  {{"last_active_utc_ms", 1262304000000LL},
                                   {"last_active_windows_endpoints", {"windows-two"}}}}}}};
    ASSERT_TRUE(write_local_document(root / "local" / "history.json", old_history.dump(2)).ok);
    start();
    module->set_inventory(inventory());
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications.size() == 1 &&
               snapshot.applications[0].policy.state == PolicyState::PendingProcess;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].id, saved);
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, socket);
    EXPECT_EQ(module->snapshot()->applications[0].activity, Activity::Inactive);
    EXPECT_TRUE(module->snapshot()->platform_metrics.process_watch_active);
    const ProcessToken replacement{201, 90000, identity.value};
    simulation->process_started({identity, replacement});
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(simulation->requested_targets().back(), std::optional<std::string>("windows-one"));
    EXPECT_FALSE(module->snapshot()->visible);
    EXPECT_EQ(module->snapshot()->platform_metrics.full_session_enumerations, scans_before_restart);
}

TEST_F(PlugBoardModule, SavedAssignmentAppliesToAnAlreadyRunningAppAfterPlugBoardRestarts) {
    auto stored = make_library("sticky-board", utc_timestamp());
    ApplicationRecord record;
    record.id = "sticky-app";
    record.identity = identity;
    record.display_name = "Player";
    record.policy_managed = true;
    stored.applications.push_back(record);
    stored.boards[0].working.sockets.push_back({"sticky-socket", "Game", {"device-one"}});
    stored.boards[0].working.application_assignments[record.id] = "sticky-socket";
    {
        std::lock_guard lock(memory->mutex);
        memory->bytes = serialize_library(stored);
    }
    simulation->set_processes({{identity, process}});
    start();
    module->set_inventory(inventory());
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications.size() == 1 &&
               snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, "sticky-socket");
    EXPECT_EQ(simulation->requested_targets().back(), std::optional<std::string>("windows-one"));
    EXPECT_FALSE(module->snapshot()->visible);
    EXPECT_TRUE(module->snapshot()->platform_metrics.process_watch_active);
}

TEST_F(PlugBoardModule, ACommittedProfileActivationOverridesManualSelectionOnlyOncePerGeneration) {
    start();
    const auto original = board();
    const auto second = command(CreateBoard{"Profile board"}).created_id;
    ASSERT_TRUE(command(LinkProfile{"profile-one", second, true}).ok);
    ASSERT_TRUE(command(SelectBoard{original}).ok);
    module->profile_activated("profile-one", 1);
    ASSERT_TRUE(await([&](const auto& snapshot) { return snapshot.selected_board_id == second; }));
    ASSERT_TRUE(command(SelectBoard{original}).ok);
    module->profile_activated("profile-one", 1);
    module->sample_diagnostics();
    EXPECT_EQ(module->snapshot()->selected_board_id, original);
    module->profile_activated("profile-one", 2);
    ASSERT_TRUE(await([&](const auto& snapshot) { return snapshot.selected_board_id == second; }));
    module->set_profiles({});
    EXPECT_EQ(module->snapshot()->library->profile_associations.at("profile-one"), second);
}

TEST_F(PlugBoardModule, MalformedLibraryIsReadOnlyUntilExplicitValidatedRepair) {
    {
        std::lock_guard lock(memory->mutex);
        memory->bytes = "{ broken";
    }
    start();
    EXPECT_EQ(module->snapshot()->phase, ModulePhase::ReadOnlyError);
    EXPECT_FALSE(command(CreateBoard{"Must not overwrite"}).ok);
    {
        std::lock_guard lock(memory->mutex);
        EXPECT_EQ(memory->bytes, "{ broken");
    }
    const auto backup = root / "repair.json";
    ASSERT_TRUE(
        write_local_document(backup, serialize_library(make_library("repaired", utc_timestamp())))
            .ok);
    EXPECT_FALSE(module->restore_library(backup, false).get().ok);
    EXPECT_TRUE(module->restore_library(backup, true).get().ok);
    EXPECT_EQ(module->snapshot()->phase, ModulePhase::Ready);
    EXPECT_EQ(module->snapshot()->selected_board_id, "repaired");
}

TEST_F(PlugBoardModule, ShutdownCancelsOwnedPolicyWorkWithoutResettingUserPreferences) {
    start();
    discover();
    const auto socket = command(CreateSocket{board(), "Headphones"}).created_id;
    ASSERT_TRUE(command(SetSocketEndpoint{board(), socket, {"device-two"}}).ok);
    simulation->block_policy(true);
    ASSERT_TRUE(command(MoveApplication{board(), app(), socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Applying;
    }));
    const auto stopped = module->begin_shutdown();
    ASSERT_EQ(stopped.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    EXPECT_EQ(module->snapshot()->phase, ModulePhase::Stopped);
    EXPECT_TRUE(module->snapshot()->simulated);
    const auto publication = module->snapshot()->publication;
    simulation->set_sessions(sessions());
    EXPECT_EQ(module->snapshot()->publication, publication);
    EXPECT_EQ(simulation->requested_targets(),
              std::vector<std::optional<std::string>>{"windows-two"});
}

TEST(PlugBoardDiagnostics, DefaultExportRedactsPathsAndRawIdsButPreservesNativeErrorCodes) {
    ModuleSnapshot snapshot;
    snapshot.library = std::make_shared<const Library>(make_library("board", utc_timestamp()));
    DiagnosticEvent event;
    event.code = "native_failure";
    event.message = "Open C:\\Users\\Private\\Player.exe failed";
    event.details = {{"hresult", "0x80070057"},
                     {"executable_path", "C:\\Users\\Private\\Player.exe"},
                     {"logical_device_id", "logical-one"}};
    snapshot.diagnostics.push_back(event);
    ApplicationView app;
    app.id = "icon-evidence";
    auto image = std::make_shared<ApplicationImage>();
    image->evidence = {{"window_icon_kind", "ICON_SMALL2"},
                       {"shortcut_match", "verified_full_image"},
                       {"shortcut_path", "C:\\Users\\Private\\Game.lnk"}};
    app.image = image;
    snapshot.applications.push_back(app);
    const auto redacted = snapshot_document(snapshot).dump();
    EXPECT_EQ(redacted.find("Private"), std::string::npos);
    EXPECT_NE(redacted.find("0x80070057"), std::string::npos);
    EXPECT_NE(redacted.find("logical-one"), std::string::npos);
    EXPECT_NE(redacted.find("ICON_SMALL2"), std::string::npos);
    EXPECT_NE(redacted.find("verified_full_image"), std::string::npos);
    EXPECT_NE(snapshot_document(snapshot, true).dump().find("Private"), std::string::npos);
}
TEST_F(PlugBoardModule, InactiveFiltersAndClearingKeepSavedRoutesAndActiveSourcesVisible) {
    simulation->set_processes({{identity, process}});
    start();
    simulation->set_sessions(sessions(SessionState::Inactive));
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.applications.size() == 1; }));
    const auto id = app();
    const auto socket = command(CreateSocket{board(), "Games"}).created_id;
    ASSERT_TRUE(command(MoveApplication{board(), id, socket}).ok);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].policy.state == PolicyState::Accepted;
    }));
    const auto calls = simulation->policy_calls();
    ASSERT_TRUE(command(ClearInactiveApplications{{id}}).ok);
    EXPECT_TRUE(module->snapshot()->applications[0].hidden_inactive);
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, socket);
    ASSERT_TRUE(command(RenameApplication{id, "My game"}).ok);
    EXPECT_EQ(module->snapshot()->applications[0].display_name, "My game");
    EXPECT_EQ(module->snapshot()->applications[0].group_name, "Player");
    ASSERT_TRUE(command(SetInactiveExemptions{{"my game.exe"}}).ok);
    EXPECT_EQ(simulation->policy_calls(), calls);
    simulation->set_sessions(sessions(SessionState::Active));
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].activity == Activity::Active;
    }));
    EXPECT_FALSE(module->snapshot()->applications[0].hidden_inactive);
    EXPECT_FALSE(find_application(*module->snapshot()->library, id)->inactive_hidden);
    ASSERT_TRUE(
        command(ClearInactiveApplications{{id}}).ok); // A stale clear cannot hide an active source.
    EXPECT_FALSE(find_application(*module->snapshot()->library, id)->inactive_hidden);
    simulation->set_sessions(sessions(SessionState::Inactive));
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.applications[0].activity == Activity::RecentlyActive;
    }));
    EXPECT_FALSE(module->snapshot()->applications[0].hidden_inactive);
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, socket);
}
TEST_F(PlugBoardModule, FailedAppListSavesDoNotChangeLabelsFiltersAssignmentsOrPolicy) {
    start();
    simulation->set_sessions(sessions(SessionState::Inactive));
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.applications.size() == 1; }));
    const auto id = app();
    {
        std::lock_guard lock(memory->mutex);
        memory->fail = true;
    }
    EXPECT_FALSE(command(RenameApplication{id, "Not saved"}).ok);
    EXPECT_EQ(module->snapshot()->applications[0].display_name, "Player");
    EXPECT_FALSE(command(ClearInactiveApplications{{id}}).ok);
    EXPECT_FALSE(module->snapshot()->applications[0].hidden_inactive);
    EXPECT_FALSE(command(SetInactiveExemptions{{"Player"}}).ok);
    EXPECT_TRUE(module->snapshot()->library->inactive_exempt_apps.empty());
    EXPECT_EQ(module->snapshot()->applications[0].socket_id, "system");
    EXPECT_EQ(simulation->policy_calls(), 0u);
    {
        std::lock_guard lock(memory->mutex);
        memory->fail = false;
    }
}
TEST_F(PlugBoardModule, ClearedRowsPersistAndUnchangedSharedHostDoesNotDuplicateAfterRestart) {
    identity = {};
    process.executable_path = "C:\\Windows\\powershell.exe";
    auto observed = sessions(SessionState::Inactive);
    for (auto& session : observed.sessions)
        session.display_name = "powershell";
    simulation->set_sessions(observed);
    start();
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.applications.size() == 1; }));
    const auto id = app();
    ASSERT_TRUE(command(RenameApplication{id, "Named script"}).ok);
    ASSERT_TRUE(command(ClearInactiveApplications{{id}}).ok);
    ASSERT_TRUE(module->begin_shutdown().get().ok);
    module.reset();
    start();
    module->set_inventory(inventory());
    module->set_visible(true);
    ASSERT_TRUE(await([](const auto& snapshot) {
        return snapshot.visible && !snapshot.applications.empty() &&
               snapshot.applications[0].observation.has_value();
    }));
    ASSERT_EQ(module->snapshot()->applications.size(), 1u);
    EXPECT_EQ(app(), id);
    EXPECT_EQ(module->snapshot()->applications[0].display_name, "Named script");
    EXPECT_TRUE(module->snapshot()->applications[0].hidden_inactive);
    EXPECT_EQ(find_application(*module->snapshot()->library, id)->identity.kind,
              IdentityKind::Unresolved);
    // Another process lifetime is kept separate, even with the same PID/name.
    for (auto& session : observed.sessions)
        session.process->creation_time += 1;
    simulation->set_sessions(observed);
    ASSERT_TRUE(await([](const auto& snapshot) { return snapshot.applications.size() == 2; }));
    EXPECT_EQ(simulation->policy_calls(), 0u);
}
} // namespace
} // namespace audiohole::plug_board
