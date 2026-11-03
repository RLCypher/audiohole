#pragma once
#include "model/controller.h"
#include <filesystem>
#include <memory>
#include <optional>

namespace audiohole {
// Configuration records only: no live plans, pending operations or telemetry.
struct ConfigurationRecords {
    std::vector<Endpoint> devices;
    std::vector<Profile> profiles;
};
// GUI-thread I/O boundary. A future production serializer can replace this
// provider without changing widgets or the configuration controller.
class ConfigurationPersistence {
  public:
    virtual ~ConfigurationPersistence() = default;
    virtual Result load(ConfigurationRecords&) = 0;
    virtual Result save(const ConfigurationRecords&) = 0;
};
// Provisional TEST schema, never a production schema. Bounded UTF-8 JSON, stable
// IDs and unknown fields survive round trips. External edits cause save rejection.
// Atomic replacement preserves the previous file on validation/write failures.
class TestJsonPersistence final : public ConfigurationPersistence {
  public:
    explicit TestJsonPersistence(std::filesystem::path file);
    ~TestJsonPersistence();
    Result load(ConfigurationRecords&) override;
    Result save(const ConfigurationRecords&) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Shared atomic-document primitive for provisional records and build preferences.
// expected==nullopt means the file must still be absent. Does not create backups
// by overwriting another file and never deletes the destination before replacement.
Result replace_document(const std::filesystem::path&, const std::string& bytes,
                        const std::optional<std::string>& expected);
} // namespace audiohole
