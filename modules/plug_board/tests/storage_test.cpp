#include "audiohole/plug_board/library.hpp"
#include "audiohole/plug_board/storage.hpp"

#include <fstream>
#include <gtest/gtest.h>

namespace audiohole::plug_board {
namespace {
class PlugBoardStorage : public testing::Test {
  protected:
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() / new_id("plug-board-storage");
    std::filesystem::path file = directory / "library.json";
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};

TEST_F(PlugBoardStorage, MissingAndExistingDocumentsRemainDistinct) {
    auto storage = make_disk_storage(file);
    ASSERT_TRUE(storage->read().result.ok);
    EXPECT_FALSE(storage->read().bytes);
    ASSERT_TRUE(storage->replace(std::nullopt, "first").ok);
    EXPECT_EQ(storage->read().bytes, "first");
    EXPECT_FALSE(storage->replace(std::nullopt, "wrong expectation").ok);
    EXPECT_EQ(storage->read().bytes, "first");
}

TEST_F(PlugBoardStorage, ExternalEditIsNeverOverwritten) {
    auto storage = make_disk_storage(file);
    ASSERT_TRUE(storage->replace(std::nullopt, "first").ok);
    std::ofstream(file) << "external user edit";
    const auto result = storage->replace(std::string("first"), "second");
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.code, "external_edit");
    EXPECT_EQ(storage->read().bytes, "external user edit");
}

TEST_F(PlugBoardStorage, ThreeBackupsRetainPreviousBytesAndNoTemporaryFilesLeak) {
    auto storage = make_disk_storage(file);
    std::optional<std::string> expected;
    for (const auto* value : {"first", "second", "third", "fourth", "fifth"}) {
        ASSERT_TRUE(storage->replace(expected, value).ok);
        expected = value;
    }
    EXPECT_EQ(read_local_document(file.string() + ".bak1").bytes, "fourth");
    EXPECT_EQ(read_local_document(file.string() + ".bak2").bytes, "third");
    EXPECT_EQ(read_local_document(file.string() + ".bak3").bytes, "second");
    EXPECT_EQ(storage->read().bytes, "fifth");
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        EXPECT_NE(entry.path().extension(), ".tmp");
}

TEST_F(PlugBoardStorage, ASecondOwnerCannotOpenTheSameLibrary) {
    auto first = make_disk_storage(file);
    EXPECT_THROW(make_disk_storage(file), std::exception);
    first.reset();
    EXPECT_NO_THROW(make_disk_storage(file));
}

TEST_F(PlugBoardStorage, ADirectoryOrOversizedFileIsAnErrorNotAnEmptyLibrary) {
    std::filesystem::create_directories(file);
    auto storage = make_disk_storage(file);
    EXPECT_FALSE(storage->read().result.ok);
    EXPECT_FALSE(storage->replace(std::nullopt, "replacement").ok);
    EXPECT_TRUE(std::filesystem::is_directory(file));
    EXPECT_FALSE(read_local_document(file, 1).result.ok);
}

TEST_F(PlugBoardStorage, MalformedLibraryBytesRemainAvailableForExplicitRepair) {
    std::filesystem::create_directories(directory);
    std::ofstream(file) << "{ incomplete";
    auto storage = make_disk_storage(file);
    const auto loaded = storage->read();
    ASSERT_TRUE(loaded.result.ok);
    ASSERT_TRUE(loaded.bytes);
    EXPECT_THROW(parse_library(*loaded.bytes), std::exception);
    EXPECT_EQ(storage->read().bytes, "{ incomplete");
}

#ifndef _WIN32
TEST_F(PlugBoardStorage, SymbolicLinkCannotRedirectLibraryWrites) {
    std::filesystem::create_directories(directory);
    const auto target = directory / "user-data.txt";
    std::ofstream(target) << "keep";
    std::filesystem::create_symlink(target, file);
    auto storage = make_disk_storage(file);
    EXPECT_FALSE(storage->read().result.ok);
    EXPECT_FALSE(storage->replace(std::nullopt, "overwrite").ok);
    EXPECT_EQ(read_local_document(target).bytes, "keep");
}
#endif
} // namespace
} // namespace audiohole::plug_board
