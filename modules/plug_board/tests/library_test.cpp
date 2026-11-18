#include "audiohole/plug_board/library.hpp"
#include "audiohole/plug_board/socket_styles.hpp"

#include <gtest/gtest.h>

namespace audiohole::plug_board {
namespace {
const std::string now = "2026-11-17T06:00:00.000Z";

class PlugBoardLibrary : public testing::Test {
  protected:
    Library library = make_library("board-default", now);
    std::string selected = "board-default";
    unsigned sequence = 0;
    LibraryEdit edit(const Command& command) {
        auto result = prepare_edit(library, selected, command, now, [&](const auto& prefix) {
            return prefix + '-' + std::to_string(++sequence);
        });
        library = result.library;
        selected = result.selected_board_id;
        return result;
    }
    void application(const std::string& id = "app-player") {
        ApplicationRecord record;
        record.id = id;
        record.display_name = "Player";
        record.identity = {IdentityKind::Win32Image, "C:\\Player\\" + id + ".exe"};
        library.applications.push_back(std::move(record));
    }
};

TEST_F(PlugBoardLibrary, NewLibraryHasOnlySystemAndNoFictionalApplications) {
    ASSERT_EQ(library.boards.size(), 1u);
    ASSERT_EQ(library.boards[0].working.sockets.size(), 1u);
    EXPECT_EQ(library.boards[0].working.sockets[0].id, "system");
    EXPECT_FALSE(library.boards[0].working.sockets[0].endpoint.logical_device_id);
    EXPECT_FALSE(library.boards[0].working.settings.show_inactive_sources);
    EXPECT_EQ(library.boards[0].working.settings.recently_active_seconds, 300u);
    EXPECT_TRUE(library.applications.empty());
    EXPECT_EQ(serialize_library(parse_library(serialize_library(library))),
              serialize_library(library));
}

TEST_F(PlugBoardLibrary, WorkingIntentAndExplicitSaveAreDistinct) {
    const auto socket = edit(CreateSocket{selected, "Game"}).created_id;
    edit(SetSocketEndpoint{selected, socket, {"device-game"}});
    const auto& board = library.boards[0];
    EXPECT_NE(board.working_revision, board.saved_revision);
    EXPECT_EQ(board.saved.sockets.size(), 1u);
    edit(SaveBoard{selected});
    EXPECT_EQ(library.boards[0].working_revision, library.boards[0].saved_revision);
    EXPECT_EQ(library.boards[0].saved.sockets.size(), 2u);
}

TEST_F(PlugBoardLibrary, EndpointEditsPreserveForwardCompatibleFields) {
    library.boards[0].working.sockets[0].endpoint.extra["future_binding_hint"] = "keep";
    edit(SetSocketEndpoint{selected, "system", {"device-new"}});
    const auto roundtrip = parse_library(serialize_library(library));
    EXPECT_EQ(roundtrip.boards[0].working.sockets[0].endpoint.extra.at("future_binding_hint"),
              "keep");
}

TEST_F(PlugBoardLibrary, SocketAppearancePersistsWithoutManagingApplicationsOrRouting) {
    application();
    const auto socket = edit(CreateSocket{selected, "Game"}).created_id;
    const auto styled = edit(SetSocketAppearance{selected, socket, "red", "sports_esports"});
    EXPECT_FALSE(styled.reconcile);
    EXPECT_FALSE(library.applications[0].policy_managed);
    EXPECT_TRUE(library.boards[0].working.application_assignments.empty());
    const auto restored = parse_library(serialize_library(library));
    EXPECT_EQ(restored.boards[0].working.sockets[1].appearance.color, "red");
    EXPECT_EQ(restored.boards[0].working.sockets[1].appearance.icon, "sports_esports");
    const auto original = selected;
    const auto copy = edit(SaveBoardAs{selected, "Styled copy"}).created_id;
    EXPECT_EQ(find_board(library, copy)->working.sockets[1].appearance.color, "red");
    EXPECT_EQ(find_board(library, original)->working.sockets[1].appearance.icon, "sports_esports");
}
TEST_F(PlugBoardLibrary, EveryOfferedColorAndIconIsValidAndUnknownFutureFieldsSurvive) {
    EXPECT_EQ(socket_colors.size(), 16u);
    EXPECT_EQ(socket_icons.size(), 18u);
    for (const auto& color : socket_colors)
        EXPECT_NO_THROW(edit(SetSocketAppearance{selected, "system", std::string(color.id), {}}));
    for (const auto& icon : socket_icons)
        EXPECT_NO_THROW(edit(SetSocketAppearance{selected, "system", {}, std::string(icon.id)}));
    auto& style = library.boards[0].working.sockets[0].appearance;
    style.extra["future_outline"] = "keep";
    style.color = "future-color";
    style.icon = "future-icon";
    library = parse_library(serialize_library(library));
    edit(RenameSocket{selected, "system", "Main"});
    EXPECT_EQ(library.boards[0].working.sockets[0].appearance.extra.at("future_outline"), "keep");
    EXPECT_EQ(library.boards[0].working.sockets[0].appearance.color, "future-color");
    EXPECT_THROW(edit(SetSocketAppearance{selected, "system", "invalid-menu-color", {}}),
                 std::runtime_error);
    EXPECT_THROW(edit(SetSocketAppearance{selected, "system", {}, "invalid-menu-icon"}),
                 std::runtime_error);
    edit(SetSocketAppearance{selected, "system", "default", "none"});
    EXPECT_EQ(library.boards[0].working.sockets[0].appearance.icon, "none");
}

TEST_F(PlugBoardLibrary, UnresolvedAndSystemIdentitiesCannotCarryArbitraryRoutingEvidence) {
    application();
    library.applications[0].identity = {IdentityKind::Unresolved, "unverified.exe"};
    EXPECT_THROW(validate_library(library), std::runtime_error);
    library.applications[0].identity = {IdentityKind::SystemSounds, "ordinary.exe"};
    EXPECT_THROW(validate_library(library), std::runtime_error);
}

TEST_F(PlugBoardLibrary, SaveAsRemapsSocketsButKeepsApplicationIdsAndOriginalBoard) {
    application();
    const auto socket = edit(CreateSocket{selected, "Game"}).created_id;
    edit(MoveApplication{selected, "app-player", socket});
    const auto original = selected;
    const auto copy = edit(SaveBoardAs{selected, "Copy"});
    ASSERT_EQ(library.boards.size(), 2u);
    const auto* board = find_board(library, copy.created_id);
    ASSERT_NE(board, nullptr);
    EXPECT_NE(board->working.sockets[1].id, socket);
    EXPECT_EQ(assigned_socket(board->working, "app-player"), board->working.sockets[1].id);
    EXPECT_EQ(assigned_socket(find_board(library, original)->working, "app-player"), socket);
    EXPECT_EQ(library.applications.size(), 1u);
    EXPECT_EQ(library.default_board_id, original);
    EXPECT_EQ(board->working_revision, board->saved_revision);
}

TEST_F(PlugBoardLibrary, SocketDeletionRequiresConfirmationAndRetainsAssignedApplications) {
    application();
    const auto socket = edit(CreateSocket{selected, "Game"}).created_id;
    edit(MoveApplication{selected, "app-player", socket});
    EXPECT_THROW(edit(DeleteSocket{selected, socket, false}), std::runtime_error);
    edit(DeleteSocket{selected, socket, true});
    EXPECT_EQ(assigned_socket(library.boards[0].working, "app-player"), "system");
    EXPECT_EQ(library.applications.size(), 1u);
    EXPECT_TRUE(library.applications[0].policy_managed);
}

TEST_F(PlugBoardLibrary, SystemCannotBeDeletedOrReordered) {
    edit(CreateSocket{selected, "Game"});
    EXPECT_THROW(edit(DeleteSocket{selected, "system", true}), std::runtime_error);
    EXPECT_THROW(edit(ReorderSocket{selected, "system", 1}), std::runtime_error);
    EXPECT_THROW(edit(ReorderSocket{selected, library.boards[0].working.sockets[1].id, 0}),
                 std::runtime_error);
}

TEST_F(PlugBoardLibrary, ReorderDoesNotChangeSocketIdentityOrWidthRelatedConfiguration) {
    const auto first = edit(CreateSocket{selected, "Game"}).created_id;
    const auto second = edit(CreateSocket{selected, "Media"}).created_id;
    edit(ReorderSocket{selected, second, 1});
    EXPECT_EQ(library.boards[0].working.sockets[1].id, second);
    EXPECT_EQ(library.boards[0].working.sockets[2].id, first);
}

TEST_F(PlugBoardLibrary, FinalBoardCannotBeDeleted) {
    EXPECT_THROW(edit(DeleteBoard{selected, {}, true}), std::runtime_error);
}

TEST_F(PlugBoardLibrary, DeletingDefaultRequiresASurvivorAndRemapsProfileLinksAtomically) {
    const auto original = selected;
    edit(LinkProfile{"profile-one", original, true});
    const auto survivor = edit(CreateBoard{"Second"}).created_id;
    EXPECT_THROW(edit(DeleteBoard{original, {}, true}), std::runtime_error);
    EXPECT_THROW(edit(DeleteBoard{original, survivor, false}), std::runtime_error);
    edit(DeleteBoard{original, survivor, true});
    EXPECT_EQ(library.default_board_id, survivor);
    EXPECT_EQ(library.profile_associations.at("profile-one"), survivor);
    EXPECT_EQ(library.boards.size(), 1u);
    EXPECT_NO_THROW(parse_library(serialize_library(library)));
}

TEST_F(PlugBoardLibrary, BoardSwitchKeepsUnsavedDraftsAndSavedCheckpoints) {
    const auto original = selected;
    edit(CreateSocket{selected, "Unsaved"});
    const auto second = edit(CreateBoard{"Second"}).created_id;
    edit(SelectBoard{original});
    EXPECT_EQ(find_board(library, original)->working.sockets.size(), 2u);
    EXPECT_EQ(find_board(library, original)->saved.sockets.size(), 1u);
    EXPECT_EQ(find_board(library, second)->working.sockets.size(), 1u);
}

TEST_F(PlugBoardLibrary, DiscoveryDoesNotTakePolicyOwnershipButExplicitMovesDo) {
    application();
    EXPECT_FALSE(library.applications[0].policy_managed);
    edit(SelectBoard{selected, false});
    EXPECT_FALSE(library.applications[0].policy_managed);
    edit(MoveApplication{selected, "app-player", "system"});
    EXPECT_TRUE(library.applications[0].policy_managed);
}

TEST_F(PlugBoardLibrary, EndpointChangeManagesOnlyApplicationsInThatSocket) {
    application("app-one");
    application("app-two");
    const auto socket = edit(CreateSocket{selected, "Game"}).created_id;
    edit(MoveApplication{selected, "app-one", socket});
    edit(SetSocketEndpoint{selected, socket, {"logical-output"}});
    EXPECT_TRUE(library.applications[0].policy_managed);
    EXPECT_FALSE(library.applications[1].policy_managed);
}

TEST_F(PlugBoardLibrary, MissingDevicesRemainValidIntent) {
    edit(SetSocketEndpoint{selected, "system", {"missing-logical-device"}});
    const auto roundtrip = parse_library(serialize_library(library));
    EXPECT_EQ(roundtrip.boards[0].working.sockets[0].endpoint.logical_device_id,
              "missing-logical-device");
}

TEST_F(PlugBoardLibrary, DefaultAndProfileSettingsDoNotChangeBoardCheckpointRevisions) {
    const auto before = library.boards[0].working_revision;
    edit(SetDefaultBoard{selected});
    edit(LinkProfile{"profile-one", selected, true});
    EXPECT_EQ(library.boards[0].working_revision, before);
    EXPECT_EQ(library.boards[0].saved_revision, before);
}

TEST_F(PlugBoardLibrary, UnknownFieldsSurviveEditsAndSavedCopies) {
    auto document = Document::parse(serialize_library(library));
    document["future"] = {{"keep", Document::array({1, "two"})}};
    document["boards"][0]["future_board"] = 42;
    document["boards"][0]["working"]["sockets"][0]["endpoint"]["future_endpoint"] = true;
    document["boards"][0]["working"]["settings"]["future_setting"] = "keep";
    library = parse_library(document.dump());
    edit(SaveBoard{selected});
    edit(SaveBoardAs{selected, "Copy"});
    const auto saved = Document::parse(serialize_library(library));
    EXPECT_EQ(saved["future"], document["future"]);
    EXPECT_EQ(saved["boards"][1]["future_board"], 42);
    EXPECT_EQ(saved["boards"][1]["saved"]["sockets"][0]["endpoint"]["future_endpoint"], true);
    EXPECT_EQ(saved["boards"][1]["saved"]["settings"]["future_setting"], "keep");
}

TEST_F(PlugBoardLibrary, DuplicateKeysAndUnsupportedOrFractionalVersionsAreRejected) {
    auto bytes = serialize_library(library);
    bytes.insert(1, "\"revision\":99,");
    EXPECT_THROW(parse_library(bytes), std::runtime_error);
    for (const Document& version : {Document(2), Document(1.0), Document("1")}) {
        auto document = Document::parse(serialize_library(library));
        document["schema_version"] = version;
        EXPECT_THROW(parse_library(document.dump()), std::exception);
    }
}

TEST_F(PlugBoardLibrary, DanglingReferencesAndDuplicateIdentityAreRejected) {
    application();
    auto bad = library;
    bad.boards[0].working.application_assignments["app-player"] = "deleted-socket";
    EXPECT_THROW(serialize_library(bad), std::runtime_error);
    bad = library;
    bad.applications.push_back(bad.applications[0]);
    bad.applications.back().id = "different-id";
    EXPECT_THROW(serialize_library(bad), std::runtime_error);
    bad = library;
    bad.profile_associations["profile-one"] = "deleted-board";
    EXPECT_THROW(serialize_library(bad), std::runtime_error);
}

TEST_F(PlugBoardLibrary, SameFilenameInDifferentDirectoriesRemainsTwoApplications) {
    application("app-one");
    application("app-two");
    library.applications[0].identity.value = "C:\\First\\Player.exe";
    library.applications[1].identity.value = "D:\\Second\\Player.exe";
    EXPECT_EQ(parse_library(serialize_library(library)).applications.size(), 2u);
}

TEST_F(PlugBoardLibrary, MalformedOrderingIntervalsAndTimestampsAreRejected) {
    auto bad = library;
    bad.boards[0].working.sockets[0].id = "not-system";
    EXPECT_THROW(serialize_library(bad), std::runtime_error);
    EXPECT_THROW(edit(SetBoardSettings{selected, true, 0}), std::runtime_error);
    EXPECT_THROW(edit(SetBoardSettings{selected, false, 86401}), std::runtime_error);
    bad = library;
    bad.boards[0].saved_revision = 1;
    EXPECT_THROW(serialize_library(bad), std::runtime_error);
    bad = library;
    bad.boards[0].created_utc = "2026-02-30T00:00:00Z";
    EXPECT_THROW(serialize_library(bad), std::runtime_error);
}

TEST_F(PlugBoardLibrary, ExcessiveDepthAndOversizedInputAreRejectedBeforeUse) {
    EXPECT_THROW(parse_library(std::string(4 * 1024 * 1024 + 1, ' ')), std::runtime_error);
    auto document = Document::parse(serialize_library(library));
    Document nested = "leaf";
    for (unsigned depth = 0; depth < 70; ++depth)
        nested = Document{{"next", std::move(nested)}};
    document["future"] = std::move(nested);
    EXPECT_THROW(parse_library(document.dump()), std::runtime_error);
}

TEST(PlugBoardUtc, MillisecondsAndCalendarValidationAreExplicit) {
    ASSERT_TRUE(utc_milliseconds("1970-01-01T00:00:00Z"));
    EXPECT_EQ(*utc_milliseconds("1970-01-01T00:00:00Z"), 0);
    EXPECT_EQ(*utc_milliseconds("1970-01-01T00:00:00.12Z"), 120);
    EXPECT_FALSE(utc_milliseconds("2026-02-30T00:00:00Z"));
    EXPECT_FALSE(utc_milliseconds("2026-01-01T-0:00:00Z"));
    EXPECT_FALSE(utc_milliseconds("2026-01-01T00:00:00+00:00"));
    EXPECT_TRUE(utc_milliseconds(utc_timestamp()));
}
} // namespace
} // namespace audiohole::plug_board
