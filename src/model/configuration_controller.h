#pragma once
#include "model/diagnostic_service.h"
#include "model/persistence.h"
#include "model/routing_service.h"
#include "model/scanner_service.h"
#include <chrono>
#include <set>

namespace audiohole {
// Test environment: real configuration I/O and optional owned scanner service;
// an optional service owns real routing, another owns temporary device checks.
// One authoritative snapshot. Mutations publish
// only after a provider commits, so rejected writes cannot silently replace the visible saved
// records.
class ConfigurationController final : public Controller {
  public:
    explicit ConfigurationController(std::unique_ptr<ConfigurationPersistence>,
                                     std::unique_ptr<ScannerService> = {},
                                     std::unique_ptr<DiagnosticService> = {},
                                     std::unique_ptr<RoutingService> = {});
    ~ConfigurationController() override;
    const Snapshot& snapshot() const override {
        return state_;
    }
    std::string last_edit_error() const override {
        return edit_error_;
    }
    Result save(Profile, bool apply) override;
    Result update_buffer(const std::string&, const std::string&, bool, int) override;
    std::string create_profile(const std::string&, const std::string& copy_from = {}) override;
    Result remove_profile(const std::string&) override;
    Hole make_hole() override;
    Result update_endpoint(const Endpoint&) override;
    Result activate(const std::string&, bool = false) override;
    Result schedule(const std::string&, int) override;
    Result beep(const std::string&) override;
    void stop() override;
    void advance(int) override;
    void cancel_beep() override;
    Result check_level(const std::string&) override;
    void monitor(bool enabled) override;
    void fail_path(const std::string&, bool) override;
    void recover() override;
    void wait_for_devices(bool enabled) override {
        wait_for_devices_ = enabled;
        routed_inventory_revision_ = 0;
    }
    void audio_worker_policy(const std::string&, const std::string&) override;
    void clear_diagnostics() override;
    void set_logging(bool) override;
    void scan(bool enabled, bool = false, ScanScope = ScanScope::Update, const std::string& = {},
              const ScanOptions& = {}) override;
    Result scanner_action(const ScannerActionRequest&) override;

  private:
    bool bundle_job_ = false;
    std::unique_ptr<ConfigurationPersistence> persistence_;
    std::unique_ptr<ScannerService> scanner_;
    std::unique_ptr<DiagnosticService> device_checks_;
    std::unique_ptr<RoutingService> routing_;
    std::uint64_t routing_generation_ = 0, routing_revision_ = 0;
    std::chrono::steady_clock::time_point last_engine_log_{};
    bool wait_for_devices_ = true;
    std::uint64_t routed_inventory_revision_ = 0;
    // Exact accepted Apply intent survives asynchronous cleanup. A later Save
    // does not replace it, and explicit Stop cancels it before another open.
    std::optional<Profile> pending_apply_;
    Result start_profile(const Profile&);
    void refresh_waiting_routes();
    bool routing_active() const;
    void poll_routing();
    std::string diagnostic_binding_;
    Result start_check(const std::string&, bool capture);
    std::vector<Endpoint> scanner_devices_;
    bool scanner_job_is_scan_ = false;
    Snapshot state_;
    std::string edit_error_;
    std::uint64_t next_id_ = 1, event_sequence_ = 0;
    bool loaded_ = false;
    std::set<std::string> used_ids_;
    void event(std::string type, std::string detail);
    Result unavailable(const std::string& operation);
    Result commit(ConfigurationRecords, const std::string& detail);
    std::string new_id(const std::string& prefix);
    void merge_scanner_devices();
};
} // namespace audiohole
