#pragma once
#include "window_icon.hpp"
#include <filesystem>

namespace audiohole::plug_board::windows_detail {
// Finite, worker-owned Shell-link index. Links are read, never resolved/launched.
// A custom icon is eligible only for an exact verified target image or AppID;
// matching display names, filenames and guessed Steam IDs are forbidden.
struct ShortcutIconEntry {
    std::string path, target, app_id, resource;
    unsigned priority = 0;
};
struct ShortcutIconSnapshot {
    std::string resource, path;
    std::string match = "no_verified_custom_icon";
};
std::vector<ShortcutIconEntry>
read_shortcut_icons(const std::vector<std::filesystem::path>& roots = {});
ShortcutIconSnapshot observe_shortcut_icon(const ApplicationObservation&, const WindowIconSnapshot&,
                                           const std::vector<ShortcutIconEntry>&);
} // namespace audiohole::plug_board::windows_detail
