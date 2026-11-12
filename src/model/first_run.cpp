#include "model/first_run.h"
#include "model/persistence.h"
namespace audiohole {
Result first_run_needed(const std::filesystem::path& marker, bool& needed) {
    try {
        if (std::filesystem::is_symlink(marker) ||
            (std::filesystem::exists(marker) && !std::filesystem::is_regular_file(marker)))
            return {false, "The first-run marker must be a regular file."};
        needed = !std::filesystem::exists(marker);
        return {true, "First-run marker checked."};
    } catch (const std::exception&) {
        return {false, "Cannot read the first-run marker beside AudioHole."};
    }
}
Result mark_first_run(const std::filesystem::path& marker) {
    bool needed = false;
    const auto checked = first_run_needed(marker, needed);
    if (!checked.ok || !needed)
        return checked;
    const auto written = replace_document(
        marker, "AudioHole has been run. Delete this file to replay the welcome/tutorial.\n", {});
    return written.ok ? written
                      : Result{false, "Cannot save AudioHole.has-run beside AudioHole. Check "
                                      "folder permissions or external changes."};
}
Result reset_first_run(const std::filesystem::path& marker) {
    bool needed = false;
    const auto checked = first_run_needed(marker, needed);
    if (!checked.ok || needed)
        return checked;
    try {
        if (!std::filesystem::remove(marker))
            return {false, "The first-run marker could not be removed."};
        return {true, "First-run marker removed; restart to replay the welcome/tutorial."};
    } catch (const std::exception&) {
        return {false,
                "Cannot remove the first-run marker beside AudioHole. Check folder permissions."};
    }
}
} // namespace audiohole
