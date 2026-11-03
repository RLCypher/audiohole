#include "model/build_preferences.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

namespace audiohole {
namespace {
bool valid(const PreviewPreferences& values) {
    return !values.personality_id.empty() && values.personality_id.size() <= 64 &&
           std::all_of(values.personality_id.begin(), values.personality_id.end(),
                       [](unsigned char c) {
                           return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                                  c == '-';
                       }) &&
           values.startup_profile_id.size() <= 128 &&
           values.startup_profile_id.find('\0') == std::string::npos && values.notifications >= 0 &&
           values.notifications <= 2 && values.startup_mode >= 0 && values.startup_mode <= 2 &&
           values.startup_delay_seconds >= 0 && values.startup_delay_seconds <= 300 &&
           (values.appearance == "dark" || values.appearance == "light" ||
            values.appearance == "system");
}
int optional_integer(const nlohmann::json& root, const char* key, int fallback) {
    if (!root.contains(key))
        return fallback;
    const auto& value = root.at(key);
    if (!value.is_number_integer() || value < 0 || value > 300)
        throw std::runtime_error("Invalid preference integer");
    return value.get<int>();
}
} // namespace

struct BuildPreferences::Impl {
    std::filesystem::path root, file;
    PreviewPreferences values;
    std::optional<std::string> expected;
    nlohmann::json document = nlohmann::json::object();
    bool memory_only = false, ready = false;
};
BuildPreferences::BuildPreferences(std::filesystem::path root, bool memory)
    : impl_(std::make_unique<Impl>()) {
    impl_->root = std::move(root);
    impl_->file = impl_->root / "preferences.json";
    impl_->memory_only = memory;
}
BuildPreferences::~BuildPreferences() = default;
const PreviewPreferences& BuildPreferences::values() const {
    return impl_->values;
}
std::filesystem::path BuildPreferences::data_root(OperatingMode mode) const {
    return impl_->root / (mode == OperatingMode::Preview ? "preview" : "test");
}
std::filesystem::path BuildPreferences::test_configuration() const {
    return data_root(OperatingMode::Test) / "configuration.json";
}
Result BuildPreferences::load() {
    if (impl_->memory_only) {
        impl_->ready = true;
        return {true, "In-memory smoke preferences."};
    }
    try {
        impl_->ready = false;
        if (!std::filesystem::exists(impl_->file)) {
            impl_->expected.reset();
            impl_->values = {};
            impl_->document = nlohmann::json::object();
        } else {
            if (std::filesystem::is_symlink(impl_->file))
                throw std::runtime_error("Invalid preferences path");
            std::ifstream stream(impl_->file, std::ios::binary);
            std::string bytes(65537, '\0');
            stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!stream.eof() || stream.bad())
                throw std::runtime_error("Preferences too large/unreadable");
            bytes.resize(static_cast<std::size_t>(stream.gcount()));
            std::vector<std::set<std::string>> keys;
            const auto document = nlohmann::json::parse(
                bytes, [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value) {
                    if (depth > 16)
                        throw std::runtime_error("Preferences nesting too deep");
                    if (event == nlohmann::json::parse_event_t::object_start)
                        keys.emplace_back();
                    else if (event == nlohmann::json::parse_event_t::object_end)
                        keys.pop_back();
                    else if (event == nlohmann::json::parse_event_t::key &&
                             !keys.back().insert(value.get<std::string>()).second)
                        throw std::runtime_error("Duplicate preference field");
                    return true;
                });
            if (document.at("format") != "audiohole-preview-preferences" ||
                !document.at("schema_version").is_number_integer() ||
                document.at("schema_version") != 1)
                throw std::runtime_error("Unsupported preferences");
            PreviewPreferences values;
            values.preview_mode = document.at("preview_mode").get<bool>();
            values.show_preview_dialog = document.at("show_preview_dialog").get<bool>();
            values.personality_id = document.value("personality_id", std::string("default"));
            values.appearance = document.value("appearance", std::string("system"));
            values.notifications = optional_integer(document, "notifications", 0);
            values.startup_mode = optional_integer(document, "startup_mode", 0);
            values.startup_delay_seconds = optional_integer(document, "startup_delay_seconds", 5);
            values.startup_profile_id = document.value("startup_profile_id", std::string{});
            (void)document.dump();
            if (!valid(values))
                throw std::runtime_error("Invalid preferences");
            impl_->document = document;
            impl_->values = std::move(values);
            impl_->expected = std::move(bytes);
        }
        impl_->ready = true;
        return {true, "Preview Build preferences loaded."};
    } catch (const std::exception&) {
        return {false, "Preview Build preferences are invalid or unavailable. Existing data is "
                       "preserved; repair the file before saving preferences."};
    }
}
Result BuildPreferences::update(const PreviewPreferences& values) {
    if (!valid(values))
        return {false, "Invalid Preview Build preference; nothing saved."};
    if (!impl_->ready)
        return {false, "Preferences were not loaded successfully; existing data is preserved."};
    if (impl_->memory_only) {
        impl_->values = values;
        return {true, "Smoke preferences updated."};
    }
    if (values == impl_->values && impl_->expected)
        return {true, "Preferences unchanged."};
    try {
        auto document = impl_->document;
        document["format"] = "audiohole-preview-preferences";
        document["schema_version"] = 1;
        document["preview_mode"] = values.preview_mode;
        document["show_preview_dialog"] = values.show_preview_dialog;
        document["personality_id"] = values.personality_id;
        document["appearance"] = values.appearance;
        document["notifications"] = values.notifications;
        document["startup_mode"] = values.startup_mode;
        document["startup_delay_seconds"] = values.startup_delay_seconds;
        document["startup_profile_id"] = values.startup_profile_id;
        const auto bytes = document.dump(2) + "\n";
        const auto result = replace_document(impl_->file, bytes, impl_->expected);
        if (result.ok) {
            impl_->values = values;
            impl_->document = std::move(document);
            impl_->expected = bytes;
        }
        return result;
    } catch (const std::exception&) {
        return {false, "Invalid Preview Build preference encoding; existing data is preserved."};
    }
}
} // namespace audiohole
