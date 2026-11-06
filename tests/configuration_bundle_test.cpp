#include "audiohole/test_suite.hpp"
#include "model/configuration_bundle.h"
#include "model/configuration_controller.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <miniz.h>
#include <thread>

namespace audiohole {
namespace {
namespace scan = scanner;
using Json = scan::Json;
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
void write(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << content;
}
class ConfigurationBundle : public testing::Test {
  protected:
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / scan::new_logical_id("bundle-test");
    std::filesystem::path source = root / "source", target = root / "target",
                          local = root / "local";
    std::filesystem::path archive = root / "profiles.zip";
    std::string device_id;
    void SetUp() override {
#ifdef _WIN32
        auto native = std::filesystem::absolute(root).make_preferred().native();
        if (!native.starts_with(L"\\\\?\\")) {
            root = std::filesystem::path(native.starts_with(L"\\\\")
                                             ? L"\\\\?\\UNC\\" + native.substr(2)
                                             : L"\\\\?\\" + native);
            source = root / "source";
            target = root / "target";
            local = root / "local";
            archive = root / "profiles.zip";
        }
#endif
        scan::Endpoint endpoint;
        endpoint.endpoint_id = "transient-windows-id-never-imported";
        endpoint.direction = "capture";
        endpoint.device_name = "Desk microphone µ 🎙";
        endpoint.state = "active";
        endpoint.fingerprint = {{"serial", "portable-serial"}};
        scan::DeviceStore store(source, root / "source-local");
        ASSERT_EQ(store.reconcile({{endpoint}, {}, true, false}, "2026-11-06T01:00:00Z").result,
                  "completed");
        auto device = store.inventory().devices.front();
        device_id = device.record.at("logical_id");
        auto record = device.record;
        record["notes"] = "User notes 🐱";
        record["hidden"] = true;
        record["user_extension"] = {{"keep", 42}};
        write(device.path, record.dump(2));
        ConfigurationController controller(
            std::make_unique<TestJsonPersistence>(source / "configuration.json"));
        const auto id = controller.create_profile("Portable profile");
        auto p = *profile(controller.snapshot(), id);
        auto h = controller.make_hole();
        h.display_name = "Portable hole";
        h.notes = "Keep this hole's notes";
        h.forward.source = device_id;
        h.forward.destination = "render-not-yet-configured";
        h.forward.settings.automatic = false;
        h.forward.settings.format = {
            48000, 2, "pcm_s24_in_32",
            WaveFormat{32, 24, 65534, 3, "{00000001-0000-0010-8000-00AA00389B71}", 8, 384000}};
        p.holes.push_back(h);
        ASSERT_TRUE(controller.save(p, false).ok);
        auto config = Json::parse(bytes(source / "configuration.json"));
        config["profiles"][0]["custom_profile_data"] = "preserve";
        write(source / "configuration.json", config.dump(2));
    }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    void export_profiles() {
        const auto result = export_configuration_bundle(source, root / "source-local",
                                                        BundleKind::Profiles, archive);
        ASSERT_TRUE(result.ok) << result.detail;
    }
    std::shared_ptr<const BundleImportPlan> preview(BundleKind kind = BundleKind::Profiles) {
        std::shared_ptr<const BundleImportPlan> plan;
        const auto result = inspect_configuration_bundle(target, local, kind, archive, plan);
        EXPECT_TRUE(result.ok) << result.detail;
        return plan;
    }
};
TEST_F(ConfigurationBundle,
       ProfilesRoundTripWithDeviceIdentityHiddenNotesUnknownFieldsAndExactFormat) {
    export_profiles();
    auto plan = preview();
    ASSERT_TRUE(plan);
    EXPECT_FALSE(std::filesystem::exists(target / "configuration.json"));
    ASSERT_TRUE(import_configuration_bundle(*plan).ok);
    scan::DeviceStore store(target, local);
    const auto inventory = store.inventory();
    ASSERT_TRUE(inventory.complete);
    ASSERT_EQ(inventory.devices.size(), 1u);
    const auto& device = inventory.devices.front();
    EXPECT_EQ(device.record["logical_id"], device_id);
    EXPECT_EQ(device.record["notes"], "User notes 🐱");
    EXPECT_TRUE(device.record["hidden"].get<bool>());
    EXPECT_EQ(device.record["user_extension"]["keep"], 42);
    EXPECT_TRUE(device.local_binding.empty());
    EXPECT_FALSE(std::filesystem::exists(local / "device_bindings.json"));
    ConfigurationController restarted(
        std::make_unique<TestJsonPersistence>(target / "configuration.json"));
    const auto& p = restarted.snapshot().profiles.front();
    EXPECT_EQ(p.holes[0].forward.source, device_id);
    EXPECT_EQ(p.holes[0].notes, "Keep this hole's notes");
    EXPECT_EQ(p.holes[0].forward.settings.format.wave->valid_bits, 24);
    EXPECT_EQ(
        Json::parse(bytes(target / "configuration.json"))["profiles"][0]["custom_profile_data"],
        "preserve");
    EXPECT_FALSE(restarted.snapshot().armed);
    const auto original = bytes(target / "configuration.json");
    auto repeated = preview();
    ASSERT_TRUE(repeated);
    ASSERT_TRUE(import_configuration_bundle(*repeated).ok);
    EXPECT_EQ(bytes(target / "configuration.json"),
              original); // Idempotent identity-preserving import.
}
TEST_F(ConfigurationBundle, DeviceOnlyBundleLeavesProfilesAndUnrelatedDataUntouched) {
    ASSERT_TRUE(
        export_configuration_bundle(source, root / "source-local", BundleKind::Devices, archive)
            .ok);
    write(target / "configuration.json", "Not touched by device-only import");
    write(target / "user-file.txt", "Keep me");
    auto plan = preview(BundleKind::Devices);
    ASSERT_TRUE(plan);
    ASSERT_TRUE(import_configuration_bundle(*plan).ok);
    EXPECT_EQ(bytes(target / "configuration.json"), "Not touched by device-only import");
    EXPECT_EQ(bytes(target / "user-file.txt"), "Keep me");
}
TEST_F(ConfigurationBundle, ConflictingIdentityIsRejectedBeforeAnyReplacement) {
    export_profiles();
    ASSERT_TRUE(import_configuration_bundle(*preview()).ok);
    scan::DeviceStore store(target, local);
    const auto path = store.inventory().devices.front().path;
    auto modified = Json::parse(bytes(path));
    modified["notes"] = "Local edit must win";
    write(path, modified.dump(2));
    const auto original = bytes(path);
    std::shared_ptr<const BundleImportPlan> plan;
    EXPECT_FALSE(
        inspect_configuration_bundle(target, local, BundleKind::Profiles, archive, plan).ok);
    EXPECT_EQ(bytes(path), original);
}
TEST_F(ConfigurationBundle, ChangesAfterPreviewAndCancellationNeverWrite) {
    export_profiles();
    auto plan = preview();
    ASSERT_TRUE(plan);
    write(target / "configuration.json", "External content");
    EXPECT_FALSE(import_configuration_bundle(*plan).ok);
    EXPECT_EQ(bytes(target / "configuration.json"), "External content");
    std::filesystem::remove(target / "configuration.json");
    std::stop_source cancel;
    cancel.request_stop();
    EXPECT_FALSE(import_configuration_bundle(*plan, cancel.get_token()).ok);
    EXPECT_FALSE(std::filesystem::exists(target / "devices"));
}
TEST_F(ConfigurationBundle,
       FailedProfileCommitRollsBackNewDeviceFilesAndKeepsOriginalConfiguration) {
    export_profiles();
    ConfigurationController existing(
        std::make_unique<TestJsonPersistence>(target / "configuration.json"));
    ASSERT_FALSE(existing.create_profile("Existing local profile").empty());
    // Avoid the deliberately identical generated profile ID from two fresh roots.
    auto config = Json::parse(bytes(target / "configuration.json"));
    config["profiles"][0]["id"] = "local-profile";
    write(target / "configuration.json", config.dump(2));
    const auto original = bytes(target / "configuration.json");
    auto plan = preview();
    ASSERT_TRUE(plan);
    unsigned writes = 0;
    const auto result = import_configuration_bundle(
        *plan, {}, [&](const auto& path, const auto& content, const auto& expected) {
            ++writes;
            if (path.filename() == "configuration.json")
                return Result{false, "Injected disk failure"};
            return replace_document(path, content, expected);
        });
    EXPECT_FALSE(result.ok);
    EXPECT_GE(writes, 2u);
    EXPECT_EQ(bytes(target / "configuration.json"), original);
    scan::DeviceStore store(target, local);
    EXPECT_TRUE(store.inventory().devices.empty());
}
void malicious_zip(const std::filesystem::path& path,
                   const std::vector<std::pair<std::string, std::string>>& files) {
    mz_zip_archive zip{};
    ASSERT_TRUE(mz_zip_writer_init_heap(&zip, 0, 0));
    unsigned index = 0;
    std::vector<std::pair<std::string, std::string>> replacements;
    for (const auto& [name, content] : files) {
        // miniz itself refuses some absolute names. Substitute equal-length
        // header names after creation to exercise the importer with real attacks.
        const std::string safe(name.size(), static_cast<char>('a' + index++));
        ASSERT_TRUE(mz_zip_writer_add_mem(&zip, safe.c_str(), content.data(), content.size(),
                                          MZ_BEST_SPEED));
        replacements.emplace_back(safe, name);
    }
    void* buffer = nullptr;
    size_t count = 0;
    ASSERT_TRUE(mz_zip_writer_finalize_heap_archive(&zip, &buffer, &count));
    std::string archive(static_cast<char*>(buffer), count);
    for (const auto& [safe, name] : replacements) {
        for (std::size_t pos = 0; (pos = archive.find(safe, pos)) != std::string::npos;
             pos += name.size())
            archive.replace(pos, safe.size(), name);
    }
    write(path, archive);
    mz_free(buffer);
    mz_zip_writer_end(&zip);
}
TEST_F(ConfigurationBundle, UnsafePathsDuplicateEntriesAndFutureVersionsAreRejected) {
    Json manifest{{"format", "audiohole-configuration-bundle"},
                  {"schema_version", 1},
                  {"kind", "devices"},
                  {"files", Json::array()}};
    for (const auto& name : {"../escaped.json", "/absolute.json",
                             "devices/recording/../../escaped.json", "C:/bad.json"}) {
        malicious_zip(archive, {{"manifest.json", manifest.dump()}, {name, "{}"}});
        std::shared_ptr<const BundleImportPlan> plan;
        EXPECT_FALSE(
            inspect_configuration_bundle(target, local, BundleKind::Devices, archive, plan).ok);
    }
    malicious_zip(archive,
                  {{"manifest.json", manifest.dump()}, {"MANIFEST.JSON", manifest.dump()}});
    std::shared_ptr<const BundleImportPlan> plan;
    EXPECT_FALSE(
        inspect_configuration_bundle(target, local, BundleKind::Devices, archive, plan).ok);
    manifest["schema_version"] = 2;
    malicious_zip(archive, {{"manifest.json", manifest.dump()}});
    EXPECT_FALSE(
        inspect_configuration_bundle(target, local, BundleKind::Devices, archive, plan).ok);
    EXPECT_FALSE(std::filesystem::exists(root / "escaped.json"));
}
TEST_F(ConfigurationBundle, ExistingArchiveIsNeverReplacedAndWrongImporterIsRejected) {
    export_profiles();
    const auto original = bytes(archive);
    EXPECT_FALSE(
        export_configuration_bundle(source, root / "source-local", BundleKind::Profiles, archive)
            .ok);
    EXPECT_EQ(bytes(archive), original);
    std::shared_ptr<const BundleImportPlan> plan;
    EXPECT_FALSE(
        inspect_configuration_bundle(target, local, BundleKind::Devices, archive, plan).ok);
}
TEST_F(ConfigurationBundle,
       BackgroundImportPublishesProfilesAndRefreshesPersistenceAndIdAllocation) {
    export_profiles();
    ConfigurationController controller(
        std::make_unique<TestJsonPersistence>(target / "configuration.json"),
        std::make_unique<ScannerService>(
            ScannerServiceOptions{target, local, {}, "bundle-fixture"}));
    auto finish = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (controller.snapshot().scanner.busy && std::chrono::steady_clock::now() < deadline) {
            controller.advance(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_FALSE(controller.snapshot().scanner.busy);
        EXPECT_EQ(controller.snapshot().scanner.result, "completed")
            << controller.snapshot().scanner.detail;
    };
    finish();
    ScannerActionRequest inspect;
    inspect.kind = ScannerActionKind::InspectProfiles;
    inspect.output = archive;
    ASSERT_TRUE(controller.scanner_action(inspect).ok);
    finish();
    ASSERT_TRUE(controller.snapshot().scanner.import_plan);
    ScannerActionRequest import;
    import.kind = ScannerActionKind::ImportBundle;
    import.confirmed = true;
    import.import_plan = controller.snapshot().scanner.import_plan;
    ASSERT_TRUE(controller.scanner_action(import).ok);
    finish();
    ASSERT_EQ(controller.snapshot().profiles.size(), 1u);
    EXPECT_EQ(controller.snapshot().profiles[0].holes[0].forward.source, device_id);
    const auto prior = controller.snapshot().profiles[0].id;
    const auto next = controller.create_profile("After import");
    EXPECT_FALSE(next.empty());
    EXPECT_NE(next, prior);
    ConfigurationController restarted(
        std::make_unique<TestJsonPersistence>(target / "configuration.json"));
    EXPECT_EQ(restarted.snapshot().profiles.size(), 2u);
}
TEST_F(ConfigurationBundle, LinkedDeviceDirectoriesAreRejectedWithoutFollowingThem) {
    export_profiles();
    std::filesystem::create_directories(target / "devices");
    std::error_code error;
    std::filesystem::create_directory_symlink(source / "devices" / "recording",
                                              target / "devices" / "recording", error);
    if (error)
        GTEST_SKIP() << "Directory links unavailable: " << error.message();
    std::shared_ptr<const BundleImportPlan> plan;
    EXPECT_FALSE(
        inspect_configuration_bundle(target, local, BundleKind::Profiles, archive, plan).ok);
    EXPECT_FALSE(
        export_configuration_bundle(target, local, BundleKind::Devices, root / "linked.zip").ok);
    EXPECT_FALSE(std::filesystem::exists(root / "linked.zip"));
}
} // namespace
} // namespace audiohole
