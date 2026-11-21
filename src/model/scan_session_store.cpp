#include "model/scan_session_store.h"
#include "audiohole/scanner.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace audiohole {
namespace fs = std::filesystem;
ScanSessionStore::ScanSessionStore(fs::path root, fs::path active, ScanSessionLimits limits)
    : root_(std::move(root)), active_(std::move(active)), limits_(limits) {
    if (!limits_.folders || !limits_.bytes || active_.parent_path() != root_ ||
        fs::is_symlink(root_) || fs::is_symlink(active_))
        throw std::runtime_error("Invalid scan-session storage root or limits.");
    for (const auto& item : fs::directory_iterator(root_)) {
        if (item.is_symlink() || !item.is_directory())
            continue;
        Entry entry{item.path(), item.last_write_time(), 0,
                    item.path() == active_ || item.path().filename().generic_u8string().find(
                                                  u8"-scan-") != std::u8string::npos};
        for (const auto& file : fs::recursive_directory_iterator(item.path())) {
            if (file.is_symlink() || !file.is_regular_file())
                continue;
            const auto bytes = file.file_size();
            if (bytes > std::numeric_limits<std::uintmax_t>::max() - entry.bytes)
                throw std::runtime_error("Scan-session size overflow.");
            entry.bytes += bytes;
        }
        if (entry.bytes > std::numeric_limits<std::uintmax_t>::max() - total_)
            throw std::runtime_error("Scan-session total overflow.");
        total_ += entry.bytes;
        entries_.push_back(std::move(entry));
    }
    std::sort(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
        return a.age == b.age ? a.path < b.path : a.age < b.age;
    });
    (void)reserve(0, true);
}
bool ScanSessionStore::reserve(std::uintmax_t additional, bool metadata) {
    // Reserve space for compact result/completion records when optional data
    // reaches the limit. Metadata still obeys the same overall byte ceiling.
    const auto headroom = std::min<std::uintmax_t>(64 * 1024, limits_.bytes / 8);
    const auto cap = limits_.bytes - (metadata ? 0 : headroom);
    const auto fits = [&] {
        return entries_.size() <= limits_.folders && total_ <= cap && additional <= cap - total_;
    };
    for (auto it = entries_.begin(); !fits() && it != entries_.end();) {
        if (it->path == active_ || !it->owned) {
            ++it;
            continue;
        }
        std::error_code error;
        fs::remove_all(it->path, error);
        if (error) {
            ++it;
            continue;
        }
        total_ -= it->bytes;
        it = entries_.erase(it);
    }
    if (!fits())
        truncated_ = true;
    return fits();
}
bool ScanSessionStore::write(const fs::path& relative, std::string_view bytes, bool append,
                             bool metadata, bool atomic_json) {
    if (relative.empty() || relative.has_root_path())
        throw std::runtime_error("Scan-session path must be relative.");
    auto destination = active_;
    for (const auto& part : relative) {
        if (part == "." || part == ".." || part.empty() ||
            part.generic_u8string().find(u8':') != std::u8string::npos)
            throw std::runtime_error("Invalid scan-session path component.");
        destination /= part;
        if (fs::is_symlink(destination))
            throw std::runtime_error("Linked scan-session paths are not supported.");
    }
    const auto previous = fs::exists(destination) ? fs::file_size(destination) : 0;
    const auto additional = append                    ? bytes.size()
                            : bytes.size() > previous ? bytes.size() - previous
                                                      : 0;
    if (!reserve(additional, metadata))
        return false;
    fs::create_directories(destination.parent_path());
    if (atomic_json) {
        // Preserve checked, durable no-clobber JSON checkpoints.
        scanner::make_atomic_writer()->write(destination, std::string(bytes), std::nullopt);
    } else {
        std::ofstream output(destination,
                             std::ios::binary | (append ? std::ios::app : std::ios::trunc));
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output)
            throw std::runtime_error("Cannot write scan-session diagnostics.");
    }
    const auto actual = append ? previous + bytes.size() : bytes.size();
    total_ = total_ - previous + actual;
    for (auto& entry : entries_)
        if (entry.path == active_) {
            entry.bytes = entry.bytes - previous + actual;
            break;
        }
    return true;
}
} // namespace audiohole
