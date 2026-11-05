#pragma once
#include "audiohole/scanner.hpp"

namespace audiohole::scanner::detail {
std::optional<std::string> read_optional(const fs::path& path);
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
