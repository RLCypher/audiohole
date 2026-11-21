#pragma once
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>
namespace audiohole {
struct ScanSessionLimits {
    std::size_t folders = 16;
    std::uintmax_t bytes = 512ULL * 1024 * 1024;
};
// Scanner-worker only. Owns optional diagnostics, never saved device records.
// Prune completed archives before writing; protect the active scan and skip
// optional writes when that scan alone exhausts the storage budget.
class ScanSessionStore {
  public:
    ScanSessionStore(std::filesystem::path root, std::filesystem::path active,
                     ScanSessionLimits = {});
    bool write(const std::filesystem::path&, std::string_view, bool append = false,
               bool metadata = false, bool atomic_json = false);
    bool truncated() const {
        return truncated_;
    }

  private:
    struct Entry {
        std::filesystem::path path;
        std::filesystem::file_time_type age;
        std::uintmax_t bytes = 0;
        bool owned = false;
    };
    std::filesystem::path root_, active_;
    ScanSessionLimits limits_;
    std::vector<Entry> entries_;
    std::uintmax_t total_ = 0;
    bool truncated_ = false;
    bool reserve(std::uintmax_t, bool metadata);
};
} // namespace audiohole
