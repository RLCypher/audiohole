#include "audiohole/plug_board/storage.hpp"
#include "audiohole/plug_board/library.hpp"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace audiohole::plug_board {
namespace {

OperationResult failure(const char* code, const std::string& message) {
    return {false, code, message};
}

// The temporary belongs only to this replacement. Cleanup never removes the
// destination, a lock, a backup or a temporary created by another process.
class TemporaryFile {
  public:
    explicit TemporaryFile(std::filesystem::path value) : path(std::move(value)) {}
    ~TemporaryFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    std::filesystem::path path;
};

void write_flushed(const std::filesystem::path& file, const std::string& bytes) {
#ifdef _WIN32
    const auto handle = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::system_error(GetLastError(), std::system_category(),
                                "Create temporary document");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            DWORD written = 0;
            const auto chunk =
                static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 1024 * 1024));
            if (!WriteFile(handle, bytes.data() + offset, chunk, &written, nullptr) || written == 0)
                throw std::system_error(GetLastError(), std::system_category(),
                                        "Write temporary document");
            offset += written;
        }
        if (!FlushFileBuffers(handle))
            throw std::system_error(GetLastError(), std::system_category(),
                                    "Flush temporary document");
    } catch (...) {
        CloseHandle(handle);
        throw;
    }
    CloseHandle(handle);
#else
    const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0)
        throw std::system_error(errno, std::generic_category(), "Create temporary document");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
                throw std::system_error(errno, std::generic_category(), "Write temporary document");
            offset += static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor) != 0)
            throw std::system_error(errno, std::generic_category(), "Flush temporary document");
    } catch (...) {
        ::close(descriptor);
        throw;
    }
    ::close(descriptor);
#endif
}

void replace_file(const std::filesystem::path& temporary,
                  const std::filesystem::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error(GetLastError(), std::system_category(), "Replace document");
#else
    if (::rename(temporary.c_str(), destination.c_str()) != 0)
        throw std::system_error(errno, std::generic_category(), "Replace document");
    const auto directory = destination.parent_path().empty() ? std::filesystem::current_path()
                                                             : destination.parent_path();
    const int descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0)
        throw std::system_error(errno, std::generic_category(),
                                "Open document directory for flush");
    const int result = ::fsync(descriptor);
    const int error = errno;
    ::close(descriptor);
    if (result != 0)
        throw std::system_error(error, std::generic_category(), "Flush document directory");
#endif
}

// The lock coordinates module instances. It intentionally remains as an empty
// file after release: unlinking a lock inode could allow two owners to coexist.
class DiskStorage final : public CheckedStorage {
  public:
    explicit DiskStorage(std::filesystem::path file) : file_(std::move(file)) {
        if (!file_.parent_path().empty())
            std::filesystem::create_directories(file_.parent_path());
        auto lock = file_;
        lock += ".lock";
#ifdef _WIN32
        lock_ = CreateFileW(lock.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error(
                "Another Plug Board instance owns this library, or its lock cannot be opened.");
#else
        lock_ = ::open(lock.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (lock_ < 0)
            throw std::system_error(errno, std::generic_category(), "Open library lock");
        if (::flock(lock_, LOCK_EX | LOCK_NB) != 0) {
            ::close(lock_);
            lock_ = -1;
            throw std::runtime_error("Another Plug Board instance owns this library.");
        }
#endif
    }
    ~DiskStorage() override {
#ifdef _WIN32
        if (lock_ != INVALID_HANDLE_VALUE)
            CloseHandle(lock_);
#else
        if (lock_ >= 0)
            ::close(lock_);
#endif
    }
    StoredDocument read() override {
        return read_local_document(file_);
    }
    OperationResult replace(const std::optional<std::string>& expected,
                            const std::string& replacement) override {
        try {
            auto current = read();
            if (!current.result.ok)
                return current.result;
            if (current.bytes != expected)
                return failure("external_edit",
                               "The library changed outside Plug Board. Reload it before editing.");
            TemporaryFile temporary(file_.parent_path() / (new_id("library") + ".tmp"));
            write_flushed(temporary.path, replacement);
            // Backups are copies, never a move of the current library. A failure
            // during rotation leaves the authoritative current file present.
            if (expected) {
                for (int generation = 3; generation >= 1; --generation) {
                    std::optional<std::string> backup = expected;
                    if (generation > 1) {
                        const auto previous = read_local_document(backup_path(generation - 1));
                        if (!previous.result.ok)
                            return previous.result;
                        backup = previous.bytes;
                    }
                    if (backup) {
                        const auto saved = write_local_document(backup_path(generation), *backup);
                        if (!saved.ok)
                            return saved;
                    }
                }
            }
            current = read();
            if (!current.result.ok)
                return current.result;
            if (current.bytes != expected)
                return failure(
                    "external_edit",
                    "An external edit arrived during preparation. The current file was preserved.");
            replace_file(temporary.path, file_);
            return {true, "durable_commit", "The working configuration was durably recorded."};
        } catch (const std::exception& error) {
            return failure("storage_failed", error.what());
        }
    }

  private:
    std::filesystem::path file_;
#ifdef _WIN32
    HANDLE lock_ = INVALID_HANDLE_VALUE;
#else
    int lock_ = -1;
#endif
    std::filesystem::path backup_path(int generation) const {
        auto result = file_;
        result += ".bak" + std::to_string(generation);
        return result;
    }
};
} // namespace

StoredDocument read_local_document(const std::filesystem::path& file, std::size_t maximum_bytes) {
    try {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(file, error);
        if (status.type() == std::filesystem::file_type::not_found)
            return {std::nullopt, {true, "missing", "No document exists yet."}};
        if (error)
            return {std::nullopt,
                    failure("read_failed", "Cannot inspect the document: " + error.message())};
        if (!std::filesystem::is_regular_file(status))
            return {
                std::nullopt,
                failure("read_failed",
                        "The document must be a regular file, not a directory or symbolic link.")};
        if (std::filesystem::file_size(file) > maximum_bytes)
            return {std::nullopt, failure("read_failed", "The document exceeds its size bound.")};
        std::ifstream input(file, std::ios::binary);
        if (!input)
            return {std::nullopt, failure("read_failed", "The document could not be opened.")};
        std::string bytes;
        char chunk[8192];
        while (input.read(chunk, sizeof(chunk)) || input.gcount()) {
            bytes.append(chunk, static_cast<std::size_t>(input.gcount()));
            if (bytes.size() > maximum_bytes)
                return {std::nullopt,
                        failure("read_failed",
                                "The document grew beyond its size bound while reading.")};
        }
        if (!input.eof())
            return {std::nullopt,
                    failure("read_failed", "The document could not be read completely.")};
        return {std::move(bytes), {true, "read", "Document read."}};
    } catch (const std::exception& error) {
        return {std::nullopt, failure("read_failed", error.what())};
    }
}

OperationResult write_local_document(const std::filesystem::path& file, const std::string& bytes) {
    try {
        std::error_code status_error;
        const auto status = std::filesystem::symlink_status(file, status_error);
        if (status.type() != std::filesystem::file_type::not_found &&
            (status_error || !std::filesystem::is_regular_file(status)))
            return failure(
                "local_write_failed",
                "The destination must be a regular file, not a directory or symbolic link.");
        if (!file.parent_path().empty())
            std::filesystem::create_directories(file.parent_path());
        TemporaryFile temporary(file.parent_path() / (new_id("local") + ".tmp"));
        write_flushed(temporary.path, bytes);
        replace_file(temporary.path, file);
        return {true, "durable_commit", "Local document recorded."};
    } catch (const std::exception& error) {
        return failure("local_write_failed", error.what());
    }
}

std::unique_ptr<CheckedStorage> make_disk_storage(const std::filesystem::path& file) {
    return std::make_unique<DiskStorage>(file);
}
std::unique_ptr<CheckedStorage> make_memory_storage() {
    class MemoryStorage final : public CheckedStorage {
        std::optional<std::string> bytes_;

      public:
        StoredDocument read() override {
            return {bytes_, {true, "memory", "In-memory tutorial library."}};
        }
        OperationResult replace(const std::optional<std::string>& expected,
                                const std::string& replacement) override {
            if (expected != bytes_)
                return {false, "conflict", "Tutorial library changed."};
            bytes_ = replacement;
            return {true, "memory", "Tutorial intent saved in memory."};
        }
    };
    return std::make_unique<MemoryStorage>();
}

} // namespace audiohole::plug_board
