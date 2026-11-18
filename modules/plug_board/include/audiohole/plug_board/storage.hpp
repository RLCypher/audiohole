#pragma once

#include "audiohole/plug_board/policy_experiment.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace audiohole::plug_board {

struct StoredDocument {
    std::optional<std::string> bytes;
    OperationResult result;
};

// Owned by one non-UI service thread. replace() compares expected bytes twice,
// keeps three checked backup generations, flushes a same-directory temporary and
// replaces atomically. Failure never authorizes policy submission. If replacement
// occurred but durability is uncertain, read() reveals the actual bytes so the
// service can report and reconcile its memory without claiming a durable commit.
class CheckedStorage {
  public:
    virtual ~CheckedStorage() = default;
    virtual StoredDocument read() = 0;
    virtual OperationResult replace(const std::optional<std::string>& expected,
                                    const std::string& replacement) = 0;
};

// The disk implementation holds an exclusive cooperating-writer lock for its
// lifetime. External editors are detected through expected-byte comparisons;
// they cannot participate in an OS atomic compare-and-swap. It never deletes a
// user's malformed library or silently loads a backup as current configuration.
std::unique_ptr<CheckedStorage> make_disk_storage(const std::filesystem::path& library_file);
std::unique_ptr<CheckedStorage> make_memory_storage();

// Local history/diagnostics helpers use the same flush-and-replace discipline,
// without a second library lock or backup rotation. All I/O stays off GUI/audio.
OperationResult write_local_document(const std::filesystem::path&, const std::string&);
StoredDocument read_local_document(const std::filesystem::path&,
                                   std::size_t maximum_bytes = 4 * 1024 * 1024);

} // namespace audiohole::plug_board
