#pragma once
#include "model/persistence.h"
#include <set>

namespace audiohole {
// Test environment: real configuration I/O, no simulated or actual audio/device
// operations. One authoritative snapshot. Mutations publish only after a provider
// commits, so rejected writes cannot silently replace the visible saved records.
class ConfigurationController final : public Controller {
  public:
    explicit ConfigurationController(std::unique_ptr<ConfigurationPersistence>);
    const Snapshot& snapshot() const override {
        return state_;
    }
    std::string last_edit_error() const override {
        return edit_error_;
    }
    Result save(Profile, bool apply) override;
    std::string create_profile(const std::string&, const std::string& copy_from = {}) override;
    Result remove_profile(const std::string&) override;
    Hole make_hole() override;
    Result update_endpoint(const Endpoint&) override;
    Result activate(const std::string&, bool = false) override;
    Result schedule(const std::string&, int) override;
    Result beep(const std::string&) override;
    void stop() override {}
    void advance(int) override {}
    void cancel_beep() override {}
    void monitor(bool enabled) override;
    void fail_path(const std::string&, bool) override;
    void recover() override;
    void clear_diagnostics() override;
    void scan(bool enabled, bool = false, ScanScope = ScanScope::Update,
              const std::string& = {}) override;

  private:
    std::unique_ptr<ConfigurationPersistence> persistence_;
    Snapshot state_;
    std::string edit_error_;
    std::uint64_t next_id_ = 1, event_sequence_ = 0;
    bool loaded_ = false;
    std::set<std::string> used_ids_;
    void event(std::string type, std::string detail);
    Result unavailable(const std::string& operation);
    Result commit(ConfigurationRecords, const std::string& detail);
    std::string new_id(const std::string& prefix);
};
} // namespace audiohole
