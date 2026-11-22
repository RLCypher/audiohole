#include "audiohole/plug_board/application_list.hpp"
#include <algorithm>
#include <gtest/gtest.h>

namespace audiohole::plug_board {
namespace {
constexpr auto now = "2026-11-22T12:00:00.000Z";
Library example() {
    auto library = make_library("board", now);
    ApplicationRecord record;
    record.id = "game";
    record.display_name = "Godot.exe";
    record.identity = {IdentityKind::Win32Image, "C:\\Games\\Godot.exe"};
    record.extra["future_app"] = 7;
    library.applications.push_back(record);
    library.boards[0].working.sockets.push_back({"games", "Games", {"device"}});
    library.boards[0].working.application_assignments["game"] = "games";
    library.boards[0].saved = library.boards[0].working;
    library.extra["future_root"] = "retained";
    return library;
}
TEST(PlugBoardAppList,
     FiltersOnlyExactNamesAndNormalizesCommonSuggestionsWithoutReplacingCustomEntries) {
    const auto names = parse_exemptions("My game, PowerShell.exe, powershell,\n Custom app\n");
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names.front(), "My game");
    auto all = names;
    all.insert(all.end(), common_host_names().begin(), common_host_names().end());
    all = normalize_exemptions(all);
    EXPECT_EQ(all.front(), "My game");
    EXPECT_EQ(std::count_if(all.begin(), all.end(),
                            [](const auto& s) { return normalized_app_name(s) == "powershell"; }),
              1);
    auto library = example();
    library.inactive_exempt_apps = {"godot"};
    EXPECT_TRUE(inactive_entry_hidden(library, library.applications[0]));
    library.inactive_exempt_apps = {"god", "*", "Godot other"};
    EXPECT_FALSE(inactive_entry_hidden(library, library.applications[0]));
    EXPECT_THROW(parse_exemptions(std::string(65537, 'x')), std::runtime_error);
    std::vector<std::string> too_many;
    for (unsigned i = 0; i < 257; ++i)
        too_many.push_back("app " + std::to_string(i));
    EXPECT_THROW(normalize_exemptions(too_many), std::runtime_error);
}
TEST(PlugBoardAppList, RenameClearAndExemptionsRoundTripWithoutChangingAnyRoutingIntent) {
    auto original = example();
    auto edited =
        prepare_edit(original, "board", RenameApplication{"game", "My specific game"}, now);
    edited = prepare_edit(edited.library, "board", ClearInactiveApplications{{"game"}}, now);
    edited =
        prepare_edit(edited.library, "board", SetInactiveExemptions{{"powershell", "pwsh"}}, now);
    const auto roundtrip = parse_library(serialize_library(edited.library));
    const auto& app = roundtrip.applications[0];
    EXPECT_EQ(app.id, original.applications[0].id);
    EXPECT_EQ(app.identity, original.applications[0].identity);
    EXPECT_EQ(app.policy_managed, original.applications[0].policy_managed);
    EXPECT_EQ(app.display_name, "My specific game");
    EXPECT_EQ(app.detected_name, "Godot.exe");
    EXPECT_TRUE(app.inactive_hidden);
    EXPECT_EQ(app.extra.at("future_app"), 7);
    EXPECT_EQ(roundtrip.extra.at("future_root"), "retained");
    EXPECT_EQ(roundtrip.boards[0].working.application_assignments,
              original.boards[0].working.application_assignments);
    EXPECT_EQ(roundtrip.boards[0].saved.application_assignments,
              original.boards[0].saved.application_assignments);
    EXPECT_FALSE(edited.reconcile);
    EXPECT_EQ(application_group_name(app), "Godot");
}
TEST(PlugBoardAppList, LegacyFilesDefaultToVisibleAndNoExemptionsAndRejectMalformedFields) {
    auto document = parse_document(serialize_library(example()));
    document.erase("inactive_exempt_apps");
    for (auto& app : document["applications"]) {
        app.erase("detected_name");
        app.erase("host_name");
        app.erase("inactive_hidden");
    }
    const auto legacy = parse_library(document.dump());
    EXPECT_TRUE(legacy.inactive_exempt_apps.empty());
    EXPECT_FALSE(legacy.applications[0].inactive_hidden);
    EXPECT_EQ(legacy.applications[0].detected_name, "Godot.exe");
    document["inactive_exempt_apps"] = "powershell";
    EXPECT_THROW(parse_library(document.dump()), std::exception);
    document["inactive_exempt_apps"] = Document::array();
    document["applications"][0]["inactive_hidden"] = "yes";
    EXPECT_THROW(parse_library(document.dump()), std::exception);
}
TEST(PlugBoardAppList, GroupLabelNeverChangesTheUnderlyingSharedHostIdentity) {
    auto library = example();
    auto& app = library.applications[0];
    app.identity = {};
    app.host_name = "PowerShell.exe";
    app.display_name = "My script";
    EXPECT_EQ(application_group_name(app), "powershell");
    EXPECT_EQ(app.identity.kind, IdentityKind::Unresolved);
    ApplicationObservation observation;
    SessionObservation session;
    session.process =
        ProcessToken{1, 20, "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"};
    observation.sessions.push_back(session);
    EXPECT_EQ(application_host_name(observation), "powershell");
}
} // namespace
} // namespace audiohole::plug_board
