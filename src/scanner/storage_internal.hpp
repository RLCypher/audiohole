#pragma once
#include "audiohole/scanner.hpp"

namespace audiohole::scanner::detail {
std::optional<std::string> read_optional(const fs::path& path);
// Same-directory metadata rename with no destination replacement. Throws on an
// existing destination or OS failure; caller owns the portable store lock.
void move_no_replace(const fs::path& source, const fs::path& destination);
Json parse_json(const std::string& bytes);
class StoreLock {
 public:
  explicit StoreLock(const fs::path& root);
  ~StoreLock();
  StoreLock(const StoreLock&) = delete;
  StoreLock& operator=(const StoreLock&) = delete;
 private:
  void* handle_ = nullptr;
  int fd_ = -1;
};
}
