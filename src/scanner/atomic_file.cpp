#include "storage_internal.hpp"

#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace audiohole::scanner::detail {
Json parse_json(const std::string& bytes) {
  std::map<int, std::set<std::string>> keys;
  return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
    if (depth > 128) throw std::runtime_error("JSON nesting exceeds 128 levels");
    if (event == Json::parse_event_t::object_start) keys[depth + 1].clear();
    if (event == Json::parse_event_t::key && !keys[depth].insert(value.get<std::string>()).second)
      throw std::runtime_error("Duplicate JSON object key: " + value.get<std::string>());
    return true;
  });
}
std::optional<std::string> read_optional(const fs::path& path) {
  if (!fs::exists(path)) return std::nullopt;
  if (fs::is_symlink(fs::symlink_status(path)) || !fs::is_regular_file(path))
    throw std::runtime_error("Expected a regular file: " + path_utf8(path));
  // Device records and the binding cache are bounded; large histories belong in
  // separate files. This also prevents a damaged file from exhausting memory.
  if (fs::file_size(path) > 16 * 1024 * 1024)
    throw std::runtime_error("JSON file exceeds 16 MiB: " + path_utf8(path));
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot read file: " + path_utf8(path));
  std::string bytes((std::istreambuf_iterator<char>(in)), {});
  if (in.bad()) throw std::runtime_error("File read failed: " + path_utf8(path));
  return bytes;
}

StoreLock::StoreLock(const fs::path& root) {
  fs::create_directories(root);
  const auto path = root / ".scanner.lock";
#ifdef _WIN32
  handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle_ == INVALID_HANDLE_VALUE) {
    handle_ = nullptr;
    throw std::runtime_error("Cannot acquire scanner lock: " + path_utf8(path));
  }
#else
  fd_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd_ < 0) throw std::runtime_error("Cannot open scanner lock: " + path_utf8(path));
  if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error("Another scanner is using: " + path_utf8(root));
  }
#endif
}
StoreLock::~StoreLock() {
#ifdef _WIN32
  if (handle_) CloseHandle(handle_);
#else
  if (fd_ >= 0) { flock(fd_, LOCK_UN); ::close(fd_); }
#endif
}
}  // namespace audiohole::scanner::detail

namespace audiohole::scanner {
namespace {
void replace_file(const fs::path& destination, const std::string& bytes,
                  const std::optional<std::string>& expected, bool backup, bool validate = true) {
  if (bytes.size() > 16 * 1024 * 1024) throw std::runtime_error("JSON file exceeds 16 MiB");
  if (detail::read_optional(destination) != expected)
    throw std::runtime_error("File changed since load: " + path_utf8(destination));
  if (expected && *expected == bytes) return;
  const auto permissions = fs::status(destination).permissions();
  if (expected && (permissions & (fs::perms::owner_write | fs::perms::group_write |
                                  fs::perms::others_write)) == fs::perms::none)
    throw std::runtime_error("File is read-only: " + path_utf8(destination));

  fs::create_directories(destination.parent_path());
  auto temporary = destination;
  temporary += "." + new_logical_id("tmp");
  struct Cleanup {
    fs::path path;
    ~Cleanup() { std::error_code ignored; fs::remove(path, ignored); }
  } cleanup{temporary};
#ifdef _WIN32
  const auto attributes = GetFileAttributesW(destination.c_str());
  if (expected && attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY))
    throw std::runtime_error("File is read-only: " + path_utf8(destination));
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary file");
  DWORD written = 0;
  const bool ok = bytes.size() <= MAXDWORD &&
    WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
    written == bytes.size() && FlushFileBuffers(file);
  CloseHandle(file);
  if (!ok) throw std::runtime_error("Cannot write and flush temporary file");
#else
  int file = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
  if (file < 0) throw std::runtime_error("Cannot create temporary file");
  std::size_t offset = 0;
  bool ok = true;
  while (offset < bytes.size()) {
    const auto n = ::write(file, bytes.data() + offset, bytes.size() - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    offset += static_cast<std::size_t>(n);
  }
  if (expected) ok = ok && ::fchmod(file, static_cast<mode_t>(permissions) & 0777) == 0;
  ok = ok && ::fsync(file) == 0;
  const auto close_result = ::close(file);
  if (!ok || close_result != 0) throw std::runtime_error("Cannot write and flush temporary file");
#endif
  // Validate the actual file bytes before replacement.
  if (detail::read_optional(temporary) != std::optional<std::string>(bytes))
    throw std::runtime_error("Temporary file validation failed");
  if (validate) {
    const auto validated = detail::parse_json(bytes);
    if (!validated.is_object()) throw std::runtime_error("Persistent JSON must be an object");
  }
  if (detail::read_optional(destination) != expected)
    throw std::runtime_error("File changed before replacement: " + path_utf8(destination));
  if (backup && expected) {
    auto previous = destination; previous += ".bak";
    // A corrupt generated summary is still backed up byte-for-byte before it is
    // regenerated. Backup bytes must not be interpreted as a new JSON document.
    replace_file(previous, *expected, detail::read_optional(previous), false, false);
  }
  if (detail::read_optional(destination) != expected)
    throw std::runtime_error("File changed during backup: " + path_utf8(destination));
#ifdef _WIN32
  const bool replaced = expected
    ? ReplaceFileW(destination.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr) != 0
    : MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
  if (!replaced) throw std::runtime_error("Atomic replacement failed: " + path_utf8(destination));
#else
  // link() is a no-clobber commit for new files; rename() atomically replaces old ones.
  const auto result = expected ? ::rename(temporary.c_str(), destination.c_str())
                               : ::link(temporary.c_str(), destination.c_str());
  if (result != 0) throw std::runtime_error("Atomic replacement failed: " + path_utf8(destination));
  int directory = ::open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directory >= 0) { (void)::fsync(directory); ::close(directory); }
#endif
}

class NativeWriter final : public AtomicWriter {
 public:
  void write(const fs::path& path, const std::string& bytes,
             const std::optional<std::string>& expected) override {
    replace_file(path, bytes, expected, true);
  }
};
}
std::shared_ptr<AtomicWriter> make_atomic_writer() { return std::make_shared<NativeWriter>(); }
}  // namespace audiohole::scanner
