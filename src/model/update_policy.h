#pragma once
#include "model/controller.h"
#include <compare>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace audiohole::updates {
struct Version {
    unsigned major = 0, minor = 0, patch = 0;
    auto operator<=>(const Version&) const = default;
    std::string normalized() const;
    std::string display() const;
};
std::optional<Version> parse_version(std::string_view);
struct Installer {
    std::string url, sha256;
    std::uint64_t bytes = 0;
    bool operator==(const Installer&) const = default;
};
struct Release {
    std::string version, notes;
    std::optional<Installer> installer;
    bool operator==(const Release&) const = default;
    std::string page_url() const;
};
struct State {
    std::int64_t last_checked_unix = 0;
    std::string skipped_version;
    std::optional<Release> cached;
    bool operator==(const State&) const = default;
};
inline constexpr std::int64_t check_interval_seconds = 7 * 24 * 60 * 60;
inline constexpr std::uint64_t maximum_installer_bytes = 512ULL * 1024 * 1024;
inline constexpr char latest_release_url[] =
    "https://api.github.com/repos/RLCypher/audiohole/releases/latest";
// Pure policy/validation. No network, routing, or filesystem effects.
bool check_due(const State&, bool enabled, std::int64_t now);
bool should_notify(const State&, std::string_view current_version);
Release parse_release(std::string_view github_json);
bool valid(const Release&);

// GUI-owner-thread state, beside preferences.json. Checked atomic writes preserve
// external edits and unknown root fields. Memory-only smoke never touches disk.
class Store {
  public:
    explicit Store(std::filesystem::path file = {}, bool memory_only = false);
    ~Store();
    Result load();
    Result save(const State&);
    const State& values() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace audiohole::updates
