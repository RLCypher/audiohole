#pragma once
#include "model/controller.h"
#include <filesystem>
#include <functional>
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
// Reuse the Test serializer's validation for portable bundle payloads. Merging
// adds profiles only; conflicting profile/hole identities are never regenerated.
Result validate_test_document(const std::string&, ConfigurationRecords&);
// Canonical individual profile envelope: audiohole-profile, record version 3.
Result validate_profile_record(const std::string&, Profile&);
Result merge_test_profiles(const std::optional<std::string>& existing, const std::string& incoming,
                           std::string& merged);
// Named profiles are authoritative; the aggregate representation is retained for
// transfer and legacy loading. No migration happens until an explicit save.
class ProfileDirectoryPersistence final : public ConfigurationPersistence {
  public:
    explicit ProfileDirectoryPersistence(std::filesystem::path legacy_file);
    ~ProfileDirectoryPersistence();
    Result load(ConfigurationRecords&) override;
    Result save(const ConfigurationRecords&) override;

  private:
    friend Result
    write_profile_document(const std::filesystem::path&, const std::string&, const std::string&,
                           std::function<Result(const std::filesystem::path&, const std::string&,
                                                const std::optional<std::string>&)>);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
Result profile_record_path(const std::filesystem::path& legacy_file, const std::string& id,
                           std::filesystem::path& output);
Result read_profile_document(const std::filesystem::path& legacy_file, std::string& document);
Result write_profile_document(const std::filesystem::path& legacy_file, const std::string& document,
                              const std::string& expected,
                              std::function<Result(const std::filesystem::path&, const std::string&,
                                                   const std::optional<std::string>&)>
                                  writer = {});
} // namespace audiohole
