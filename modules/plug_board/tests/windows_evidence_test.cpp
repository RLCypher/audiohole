#include "audiohole/plug_board/module.hpp"
#include "evidence.hpp"
#include "support.hpp"

#include <array>
#include <atomic>
#include <fstream>
#include <gtest/gtest.h>

namespace audiohole::plug_board::harness {
namespace {

// These Windows tests exercise file evidence and metadata only. They do not
// instantiate the private policy adapter or change any routing preference.
class WindowsEvidence : public testing::Test {
  protected:
    std::filesystem::path root;
    void SetUp() override {
        static std::atomic<unsigned> sequence{0};
        root = std::filesystem::temp_directory_path() /
               (L"audiohole-plug-board-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1)));
        ASSERT_FALSE(std::filesystem::exists(root));
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    RecoveryPlan plan() const {
        OriginalPreferences original;
        original.process = {1234, 987654321, "C:/Disposable fixture/plug_board_fixture.exe"};
        original.endpoints = {"console-original", std::nullopt, "comms-original"};
        return {"test-operation", "temporary-target", {original}};
    }
    std::filesystem::path input(const Json& document) {
        std::filesystem::create_directories(root);
        const auto file = root / L"input.json";
        std::ofstream stream(file, std::ios::binary);
        stream << document.dump(2);
        stream.close();
        return file;
    }
};

TEST_F(WindowsEvidence, ExistingDirectoryIsNeverTreatedAsAnOwnedFreshOutput) {
    std::filesystem::create_directory(root);
    std::ofstream(root / L"keep.txt") << "existing user data";
    EXPECT_THROW(EvidenceDirectory{root}, std::runtime_error);
    EXPECT_TRUE(std::filesystem::exists(root / L"keep.txt"));
}

TEST_F(WindowsEvidence, DurableRecoveryRoundTripRetainsEachOriginalAndDoesNotReusePid) {
    EvidenceDirectory evidence(root);
    ASSERT_TRUE(evidence.prepare(plan()).ok);
    const auto recovered = read_recovery_plan(root / L"recovery.json");
    ASSERT_EQ(recovered.originals.size(), 1u);
    EXPECT_EQ(recovered.originals[0].endpoints, plan().originals[0].endpoints);
    EXPECT_EQ(recovered.originals[0].process.executable_path,
              plan().originals[0].process.executable_path);
    EXPECT_EQ(recovered.originals[0].process.pid, 0u);
    EXPECT_EQ(recovered.originals[0].process.creation_time, 0u);
}

TEST_F(WindowsEvidence, RecoveryFileCannotBeOverwrittenByASecondPreparation) {
    EvidenceDirectory evidence(root);
    ASSERT_TRUE(evidence.prepare(plan()).ok);
    auto changed = plan();
    changed.temporary_endpoint_id = "different-target";
    EXPECT_FALSE(evidence.prepare(changed).ok);
    EXPECT_EQ(read_recovery_plan(root / L"recovery.json").temporary_endpoint_id,
              "temporary-target");
}

TEST_F(WindowsEvidence, FinalResultDoesNotReplaceTheRecoveryRecord) {
    EvidenceDirectory evidence(root);
    ASSERT_TRUE(evidence.prepare(plan()).ok);
    ExperimentResult result;
    result.outcome = "recovery_required";
    result.restoration_complete = false;
    ASSERT_TRUE(evidence.finish(result).ok);
    EXPECT_TRUE(std::filesystem::exists(root / L"recovery.json"));
    std::ifstream stream(root / L"policy-result.json");
    const auto saved = Json::parse(stream);
    EXPECT_FALSE(saved.at("restoration_complete").get<bool>());
    EXPECT_EQ(saved.at("outcome"), "recovery_required");
}

TEST_F(WindowsEvidence, UnknownOrFractionalVersionIsRejected) {
    for (const Json& version : {Json(2), Json(1.0), Json("1")}) {
        auto document = recovery_json(plan());
        document["schema_version"] = version;
        EXPECT_THROW(read_recovery_plan(input(document)), std::exception);
    }
}

TEST_F(WindowsEvidence, DuplicateOriginalFieldsAreRejectedInsteadOfLastValueWinning) {
    std::filesystem::create_directory(root);
    auto text = recovery_json(plan()).dump();
    text.insert(1, "\"schema_version\":9,");
    const auto file = root / L"duplicate.json";
    std::ofstream(file, std::ios::binary) << text;
    EXPECT_THROW(read_recovery_plan(file), std::runtime_error);
}

TEST_F(WindowsEvidence, IncompleteOrNonStringOriginalEndpointIsRejected) {
    auto document = recovery_json(plan());
    document["originals"][0]["roles"].erase("communications");
    EXPECT_THROW(read_recovery_plan(input(document)), std::exception);
    document = recovery_json(plan());
    document["originals"][0]["roles"]["console"] = false;
    EXPECT_THROW(read_recovery_plan(input(document)), std::exception);
}

TEST_F(WindowsEvidence, DeepExtensionCannotExhaustTheRecoveryParser) {
    auto document = recovery_json(plan());
    Json extension = "leaf";
    for (int depth = 0; depth < 70; ++depth) {
        extension = Json{{"nested", std::move(extension)}};
    }
    document["extension"] = std::move(extension);
    EXPECT_THROW(read_recovery_plan(input(document)), std::runtime_error);
}

TEST_F(WindowsEvidence, UnicodeExecutableEvidenceSurvivesJsonRoundTrip) {
    auto original = plan();
    original.originals[0].process.executable_path =
        windows_detail::utf8(L"C:\\テスト\\音声\\fixture.exe");
    const auto recovered = read_recovery_plan(input(recovery_json(original)));
    EXPECT_EQ(recovered.originals[0].process.executable_path,
              original.originals[0].process.executable_path);
}

TEST_F(WindowsEvidence, FilenameCannotEscapeOwnedEvidenceDirectory) {
    EvidenceDirectory evidence(root);
    EXPECT_THROW(evidence.write(L"../outside.json", Json::object()), std::runtime_error);
}

#ifdef PLUG_BOARD_HAS_ICON_FIXTURE
TEST_F(WindowsEvidence, ExtendedUnicodeExecutableExportsItsIconAndRetainsIdentity) {
    // Use our own GUI's embedded icon, without starting that executable. Locate
    // it both in the build's bin directory and in an installed package's root.
    // A literal '%' and comma in the copied filename also catch accidental
    // environment expansion or resource-ordinal parsing of a verified path.
    std::wstring module(32768, L'\0');
    const auto length =
        GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    ASSERT_GT(length, 0u);
    ASSERT_LT(length, module.size());
    module.resize(length);
    const auto directory = std::filesystem::path(module).parent_path();
    std::filesystem::path launcher;
    for (const auto& candidate : {directory / L"AudioHole-PlugBoard.exe",
                                  directory.parent_path() / L"AudioHole-PlugBoard.exe"}) {
        if (std::filesystem::is_regular_file(candidate)) {
            launcher = candidate;
            break;
        }
    }
    ASSERT_FALSE(launcher.empty()) << "The matching GUI icon fixture must be built/installed.";
    std::filesystem::create_directory(root);
    const auto copied = root / L"音声%SystemRoot%,7.exe";
    std::filesystem::copy_file(launcher, copied);
    windows_detail::Handle file(CreateFileW(copied.c_str(), FILE_READ_ATTRIBUTES,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                            nullptr));
    ASSERT_TRUE(file);
    std::wstring canonical(32768, L'\0');
    const auto count = GetFinalPathNameByHandleW(
        file.get(), canonical.data(), static_cast<DWORD>(canonical.size()), FILE_NAME_NORMALIZED);
    ASSERT_GT(count, 0u);
    ASSERT_LT(count, canonical.size());
    canonical.resize(count);
    ASSERT_TRUE(canonical.starts_with(L"\\\\?\\"));
    ApplicationObservation application;
    application.observation_key = "embedded-icon-regression-fixture";
    application.identity = {IdentityKind::Win32Image, windows_detail::utf8(canonical)};
    const auto identity = application.identity.value;
    const auto exported = export_windows_icons({application}, root / L"icons");
    ASSERT_EQ(exported.size(), 1u);
    ASSERT_TRUE(exported[0].result.ok) << exported[0].result.detail;
    EXPECT_TRUE(exported[0].application_specific) << exported[0].result.detail;
    EXPECT_NE(exported[0].source, "generic_fallback");
    EXPECT_EQ(application.identity.value, identity);
    EXPECT_GT(exported[0].width, 0u);
    EXPECT_GT(exported[0].height, 0u);
    EXPECT_EQ(exported[0].rgba.size(), exported[0].width * exported[0].height * 4u);
    std::array<unsigned char, 8> signature{};
    std::ifstream png(exported[0].file, std::ios::binary);
    png.read(reinterpret_cast<char*>(signature.data()), signature.size());
    EXPECT_EQ(signature, (std::array<unsigned char, 8>{137, 80, 78, 71, 13, 10, 26, 10}));
}
#endif

TEST(PlugBoardWindowsMetadata, CurrentProcessHasAStableLiveToken) {
    if (inspect_windows_environment().wine) {
        GTEST_SKIP() << "Wine has an aborting GetApplicationUserModelId stub; native metadata "
                        "validation remains pending.";
    }
    const auto first = inspect_process(GetCurrentProcessId());
    const auto second = inspect_process(GetCurrentProcessId());
    ASSERT_TRUE(first.result.ok) << first.result.detail;
    EXPECT_EQ(first.token, second.token);
    EXPECT_NE(first.token.creation_time, 0u);
    EXPECT_FALSE(first.token.executable_path.empty());
}

TEST(PlugBoardWindowsBackend, HiddenHostAdapterOwnsNoDiscoveryWorkerOrPolicyWrites) {
    // Read-only lifetime test: never activates a policy setter, scans processes,
    // opens audio streams, or subscribes a session manager.
    auto backend = make_windows_backend(false);
    ASSERT_TRUE(backend->initialize({}).ok);
    backend->observe(false, {}, 1);
    const auto measured = backend->metrics(true);
    EXPECT_FALSE(measured.discovery_active);
    EXPECT_FALSE(measured.process_watch_active);
    EXPECT_EQ(measured.owned_workers, 0u);
    EXPECT_EQ(measured.full_session_enumerations, 0u);
    EXPECT_EQ(measured.process_snapshots, 0u);
    EXPECT_EQ(measured.policy_reads, 0u);
    EXPECT_EQ(measured.policy_writes, 0u);
    EXPECT_TRUE(measured.process_sample_tick_ms.has_value());
    EXPECT_TRUE(measured.process_cpu_100ns.has_value());
    EXPECT_TRUE(measured.private_bytes.has_value());
    EXPECT_NO_THROW(backend->shutdown());
    EXPECT_EQ(backend->metrics(false).owned_workers, 0u);
}

} // namespace
} // namespace audiohole::plug_board::harness
