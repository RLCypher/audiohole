#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace audiohole {
struct BundleImportPlan;
// Provisional GUI scan limits, revalidated by the scanner before any Deep worker.
// These are inspection requests, never routing preferences or capability claims.
struct ScanOptions {
    std::string mode = "shared";
    std::string operating_context;
    unsigned attempts = 8, duration_ms = 2000, attempt_timeout_ms = 10000,
             device_timeout_ms = 120000, system_timeout_ms = 600000;
    bool consent = false;
};
enum class ScannerActionKind {
    Reload,
    RefreshAvailability,
    SetHidden,
    Export,
    EditMetadata,
    ClearInventory,
    ExportDevices,
    ExportProfiles,
    InspectDevices,
    InspectProfiles,
    ImportBundle
};
struct ScannerActionRequest {
    ScannerActionKind kind = ScannerActionKind::Reload;
    std::string logical_id;
    bool hidden = false;
    bool confirmed = false; // Per-request destructive clear authorization; never persisted.
    std::filesystem::path output;
    std::string operating_context;
    std::string display_name, notes, expected_path, expected_bytes;
    std::shared_ptr<const BundleImportPlan> import_plan{};
};
inline constexpr std::array<const char*, 9> inspection_view_names{
    "Overview",     "Identity",    "Reported formats", "Deep evidence", "Driver",
    "Scan history", "Diagnostics", "Device JSON",      "Local binding"};
// Immutable worker-produced presentation data. Widgets read no device files or
// scanner JSON. Raw views retain explicit unknown values and exact evidence.
struct DeviceInspection {
    std::string source, path, observed_state, last_observed;
    std::string diagnostic_endpoint_id; // Local binding only; never serialized in a profile.
    std::string edit_bytes;             // Opaque snapshot used only for checked metadata saves.
    bool warning = false;
    bool unusable =
        false; // Reserved for explicit backend evidence; never inferred from one rejected tuple.
    std::array<std::string, 9> views;
};
struct ScannerStatus {
    bool busy = false, available = false;
    std::uint64_t generation = 0, completed_generation = 0, inventory_revision = 0;
    std::string phase, result = "unavailable", detail, session_path;
    std::shared_ptr<const BundleImportPlan> import_plan{};
};
} // namespace audiohole
