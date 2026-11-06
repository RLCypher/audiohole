#pragma once
#include "model/controller.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <stop_token>

namespace audiohole {
enum class BundleKind { Devices, Profiles };
// Immutable preview token. File bytes and destination preconditions stay private
// to the persistence service; the GUI displays only the summary.
struct BundleImportPlan {
    struct Data;
    std::string summary;
    std::shared_ptr<const Data> data;
};
Result export_configuration_bundle(const std::filesystem::path& portable,
                                   const std::filesystem::path& local, BundleKind,
                                   const std::filesystem::path& destination);
Result inspect_configuration_bundle(const std::filesystem::path& portable,
                                    const std::filesystem::path& local, BundleKind,
                                    const std::filesystem::path& archive,
                                    std::shared_ptr<const BundleImportPlan>&);
// Revalidates preview preconditions before writing. Adds new device/history files
// and atomically commits profiles last; rollback never removes external edits.
// Optional injected writer is a fault-testing seam, not a GUI serialization API.
using BundleWriter = std::function<Result(const std::filesystem::path&, const std::string&,
                                          const std::optional<std::string>&)>;
Result import_configuration_bundle(const BundleImportPlan&, std::stop_token = {},
                                   BundleWriter = {});
} // namespace audiohole
