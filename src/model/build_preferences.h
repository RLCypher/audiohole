#pragma once
#include "model/persistence.h"

namespace audiohole {
struct PreviewPreferences {
    bool preview_mode = true;
    bool show_preview_dialog = true;
    bool hold_expressions = false;
    std::string personality_id = "default";
    std::string appearance = "system";
    int notifications = 0, startup_mode = 0, startup_delay_seconds = 5;
    std::string startup_profile_id;
    bool operator==(const PreviewPreferences&) const = default;
};
// Preview-Build-only preferences and namespace resolution. GUI code sees values
// and Results, never JSON. Current operating mode belongs to Controller::snapshot;
// preview_mode here specifies the NEXT launch only.
class BuildPreferences {
  public:
    explicit BuildPreferences(std::filesystem::path isolated_root = {}, bool memory_only = false);
    ~BuildPreferences();
    const PreviewPreferences& values() const;
    Result load();
    Result update(const PreviewPreferences&);
    std::filesystem::path data_root(OperatingMode) const;
    std::filesystem::path test_configuration() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace audiohole
