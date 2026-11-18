#pragma once

#include "audiohole/plug_board/windows.hpp"
#include <nlohmann/json.hpp>

namespace audiohole::plug_board::harness {

using Json = nlohmann::ordered_json;

// All evidence paths belong to a freshly created directory supplied explicitly
// by the caller. CREATE_NEW and FlushFileBuffers protect recovery evidence from
// replacement and from a merely buffered acknowledgement before native writes.
class EvidenceDirectory final : public ExperimentEvidence {
  public:
    explicit EvidenceDirectory(std::filesystem::path root);
    const std::filesystem::path& root() const;
    void write(const std::wstring& filename, const Json& document);
    OperationResult prepare(const RecoveryPlan&) override;
    OperationResult finish(const ExperimentResult&) override;

  private:
    std::filesystem::path root_;
};

Json operation_json(const OperationResult&);
Json inspection_json(const WindowsInspection&);
Json result_json(const ExperimentResult&);
Json recovery_json(const RecoveryPlan&);
Json icons_json(const std::vector<IconExport>&);
Json watcher_json(const ProcessStartProbe&);

// Strict bounded parser for this harness's versioned recovery evidence. PIDs in
// the file are historical tokens; main rebinds only to its exact disposable fixture.
RecoveryPlan read_recovery_plan(const std::filesystem::path&);

} // namespace audiohole::plug_board::harness
