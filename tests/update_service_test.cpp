#include "platform/windows/update_service.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>
#include <windows.h>

namespace audiohole::windows {
namespace fs = std::filesystem;
class FixtureTransport final : public UpdateTransport {
  public:
    std::string bytes = "MZfixture";
    bool wait_for_cancel = false;
    std::atomic<bool> entered = false;
    void fetch(const std::string&, std::uint64_t, std::stop_token stop, const Sink& sink) override {
        entered = true;
        if (wait_for_cancel) {
            while (!stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            throw std::runtime_error("Cancelled fixture download.");
        }
        sink(std::as_bytes(std::span(bytes.data(), bytes.size())));
    }
    void cancel() override {}
};
class UpdateDownload : public testing::Test {
  protected:
    fs::path root = fs::temp_directory_path() /
                    ("AudioHole-update-service-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    FixtureTransport* transport = nullptr;
    std::unique_ptr<UpdateService> service;
    updates::Release release{
        "0.1.1", "Fixture changes",
        updates::Installer{"https://github.com/RLCypher/audiohole/releases/download/v0.1.1/"
                           "Install.AudioHole.v0.1.1.exe",
                           "78a83403fcb3b7a3fc1df1abbeb6dfad652c01e561efd9ba6985f7c75c44245a", 9}};
    void SetUp() override {
        fs::create_directory(root);
        auto fixture = std::make_unique<FixtureTransport>();
        transport = fixture.get();
        service = std::make_unique<UpdateService>(root / "cache", std::move(fixture));
    }
    void TearDown() override {
        service.reset();
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    UpdateResult result() {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < until) {
            if (auto result = service->take_result())
                return std::move(*result);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ADD_FAILURE() << "Update fixture worker timed out";
        return {};
    }
    std::size_t files() {
        std::size_t count = 0;
        if (fs::exists(root / "cache"))
            for (const auto& entry : fs::recursive_directory_iterator(root / "cache"))
                count += entry.is_regular_file();
        return count;
    }
};
TEST_F(UpdateDownload, VerifiesAndSealsInstallerUntilLaunchOrCancellation) {
    ASSERT_TRUE(service->download(release));
    auto completed = result();
    ASSERT_TRUE(completed.error.empty()) << completed.error;
    ASSERT_TRUE(completed.installer);
    const auto file = completed.installer->path();
    EXPECT_TRUE(fs::exists(file));
    const auto writer = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
    EXPECT_EQ(writer, INVALID_HANDLE_VALUE);
    if (writer != INVALID_HANDLE_VALUE)
        CloseHandle(writer);
    const auto reader = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
    EXPECT_NE(reader, INVALID_HANDLE_VALUE);
    if (reader != INVALID_HANDLE_VALUE)
        CloseHandle(reader);
    EXPECT_FALSE(DeleteFileW(file.c_str()));
    completed.installer.reset();
    EXPECT_FALSE(fs::exists(file));
}
TEST_F(UpdateDownload, RejectsChecksumSizeAndExecutableSignatureMismatchAndCleansUp) {
    for (int kind : {0, 1, 2}) {
        auto invalid = release;
        if (kind == 0)
            invalid.installer->sha256 = std::string(64, '0');
        if (kind == 1)
            invalid.installer->bytes = 8;
        if (kind == 2) {
            transport->bytes = "ZZfixture";
            invalid.installer->sha256 =
                "ab41c3bdcb831bcc31a82eb45df9cb92aff3125f2dd832c60e4b5685a3c8c920";
        }
        ASSERT_TRUE(service->download(invalid));
        auto completed = result();
        EXPECT_FALSE(completed.error.empty());
        EXPECT_FALSE(completed.installer);
        EXPECT_EQ(files(), 0u);
    }
}
TEST_F(UpdateDownload, CancellationNeverPublishesOrRetainsInstaller) {
    transport->wait_for_cancel = true;
    ASSERT_TRUE(service->download(release));
    service->cancel();
    auto completed = result();
    EXPECT_TRUE(completed.cancelled);
    EXPECT_FALSE(completed.installer);
    EXPECT_EQ(files(), 0u);
}
TEST_F(UpdateDownload, FetchesReleaseOffThreadAndBoundsResponseWithoutTouchingOtherFiles) {
    fs::create_directories(root / "cache" / "personal");
    std::ofstream(root / "cache" / "personal" / "keep.txt") << "untouched";
    transport->bytes = R"({"tag_name":"v0.1.1","body":"Changes","assets":[]})";
    ASSERT_TRUE(service->check());
    auto completed = result();
    ASSERT_TRUE(completed.release) << completed.error;
    EXPECT_EQ(completed.release->version, "0.1.1");
    EXPECT_TRUE(fs::exists(root / "cache" / "personal" / "keep.txt"));
    transport->bytes.assign(512 * 1024 + 1, 'x');
    ASSERT_TRUE(service->check());
    completed = result();
    EXPECT_FALSE(completed.error.empty());
    EXPECT_FALSE(completed.release);
}
} // namespace audiohole::windows
