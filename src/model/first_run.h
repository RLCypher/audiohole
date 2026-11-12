#pragma once
#include "model/controller.h"
#include <filesystem>
namespace audiohole {
// Control-thread filesystem operations. Marker lives beside audiohole.exe, not
// in a registry key. Errors never silently redirect it to another directory.
Result first_run_needed(const std::filesystem::path& marker, bool& needed);
Result mark_first_run(const std::filesystem::path& marker);
Result reset_first_run(const std::filesystem::path& marker);
} // namespace audiohole
