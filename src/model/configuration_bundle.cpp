#include "model/configuration_bundle.h"
#include "audiohole/test_suite.hpp"
#include "model/persistence.h"
#include "scanner/storage_internal.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <miniz.h>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace audiohole {
namespace {
namespace scan = scanner;
using Json = scan::Json;
namespace fs = std::filesystem;
fs::path native_path(const std::string& value) {
    auto path = scan::utf8_path(value);
    return path.make_preferred();
}
constexpr std::size_t file_limit = 16 * 1024 * 1024;
constexpr std::size_t bundle_limit = 128 * 1024 * 1024;
struct TempDirectory {
    fs::path path = fs::temp_directory_path() / scan::new_logical_id("audiohole-bundle");
    TempDirectory() {
        if (!fs::create_directory(path))
            throw std::runtime_error("Cannot create isolated bundle staging directory");
    }
    ~TempDirectory() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
void check_path(const fs::path& path) {
    auto current = fs::absolute(path).lexically_normal();
    for (; !current.empty(); current = current.parent_path()) {
#ifdef _WIN32
        const auto attrs = GetFileAttributesW(current.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Bundle paths must not traverse reparse points");
#else
        if (fs::is_symlink(fs::symlink_status(current)))
            throw std::runtime_error("Bundle paths must not traverse links");
#endif
        if (current == current.parent_path())
            break;
    }
}
std::optional<std::string> read(const fs::path& path, std::size_t limit = file_limit) {
    check_path(path);
    if (!fs::exists(path))
        return {};
    if (!fs::is_regular_file(path) || fs::file_size(path) > limit)
        throw std::runtime_error("Bundle input is not a bounded regular file: " +
                                 scan::path_utf8(path));
    std::ifstream input(path, std::ios::binary);
    std::string bytes;
    char block[65536];
    while (input) {
        input.read(block, sizeof(block));
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - bytes.size())
            throw std::runtime_error("Bundle input grew beyond its size budget");
        bytes.append(block, count);
    }
    if (input.bad() || !input.eof())
        throw std::runtime_error("Cannot read bundle input");
    return bytes;
}
void check_inventory_paths(const fs::path& root) {
    check_path(root);
    check_path(root / "devices" / "recording");
    check_path(root / "devices" / "playback");
    check_path(root / "probe_history");
}
void stage_file(const fs::path& path, const std::string& bytes) {
    if (fs::exists(path))
        throw std::runtime_error("Duplicate staged filename");
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
        throw std::runtime_error("Cannot stage bundle file");
}
std::string key(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool payload_path(const std::string& name) {
    const auto path = native_path(name);
    if (name.find('\\') != std::string::npos || path.is_absolute() ||
        name.find(':') != std::string::npos)
        return false;
    for (const auto& part : path)
        if (part == "." || part == ".." || part.empty())
            return false;
    if (name == "configuration.json")
        return true;
    const auto filename = scan::path_utf8(path.filename());
    if (path.extension() != ".json" || filename != scan::safe_filename(filename))
        return false;
    const auto parent = path.parent_path().generic_string();
    return parent == "devices/recording" || parent == "devices/playback" ||
           parent == "probe_history";
}
using Files = std::map<std::string, std::string>;
struct ZipReader {
    mz_zip_archive zip{};
    bool open = false;
    ~ZipReader() {
        if (open)
            mz_zip_reader_end(&zip);
    }
};
Files read_zip(const std::string& bytes) {
    ZipReader reader;
    if (!mz_zip_reader_init_mem(&reader.zip, bytes.data(), bytes.size(), 0))
        throw std::runtime_error("Invalid bundle ZIP");
    reader.open = true;
    const auto count = mz_zip_reader_get_num_files(&reader.zip);
    if (count > 8192)
        throw std::runtime_error("Bundle contains too many files");
    Files files;
    std::set<std::string> names;
    std::uint64_t total = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat info{};
        if (!mz_zip_reader_file_stat(&reader.zip, i, &info) || info.m_is_directory ||
            info.m_is_encrypted || (info.m_external_attr >> 16 & 0170000) == 0120000)
            throw std::runtime_error("Bundle contains an unsupported entry");
        const auto length = mz_zip_reader_get_filename(&reader.zip, i, nullptr, 0);
        if (length == 0 || length > 1024)
            throw std::runtime_error("Invalid bundle filename length");
        std::string name(length, '\0');
        mz_zip_reader_get_filename(&reader.zip, i, name.data(), length);
        name.pop_back();
        if (name.find('\0') != std::string::npos ||
            (name != "manifest.json" && !payload_path(name)) || !names.insert(key(name)).second)
            throw std::runtime_error("Unsafe or duplicate bundle filename");
        total += info.m_uncomp_size;
        if (info.m_uncomp_size > file_limit || total > bundle_limit)
            throw std::runtime_error("Expanded bundle exceeds its size budget");
        std::string content(static_cast<std::size_t>(info.m_uncomp_size), '\0');
        if (!mz_zip_reader_extract_to_mem(&reader.zip, i, content.data(), content.size(), 0))
            throw std::runtime_error("Bundle CRC/decompression failed");
        (void)scan::detail::parse_json(content);
        files.emplace(name, std::move(content));
    }
    const auto manifest = scan::detail::parse_json(files.at("manifest.json"));
    if (manifest.at("format") != "audiohole-configuration-bundle" ||
        !manifest.at("schema_version").is_number_integer() || manifest.at("schema_version") != 1 ||
        !manifest.at("files").is_array())
        throw std::runtime_error("Unsupported configuration bundle version");
    std::set<std::string> listed;
    for (const auto& path : manifest.at("files")) {
        const auto name = path.get<std::string>();
        if (!payload_path(name) || !files.contains(name) || !listed.insert(name).second)
            throw std::runtime_error("Bundle manifest does not match its files");
    }
    if (listed.size() + 1 != files.size())
        throw std::runtime_error("Bundle has unlisted files");
    return files;
}
std::set<std::string> references(const ConfigurationRecords& records) {
    std::set<std::string> ids;
    for (const auto& profile : records.profiles)
        for (const auto& hole : profile.holes)
            for (const auto* d : {&hole.forward, &hole.reverse}) {
                if (!d->source.empty())
                    ids.insert(d->source);
                if (!d->destination.empty())
                    ids.insert(d->destination);
            }
    return ids;
}
} // namespace
struct BundleImportPlan::Data {
    fs::path portable, local, archive;
    std::string archive_bytes;
    struct Write {
        fs::path path;
        std::string bytes;
        std::optional<std::string> expected;
    };
    std::vector<Write> writes;
    std::map<fs::path, std::optional<std::string>> observed;
    std::set<fs::path> inventory_paths;
};

Result export_configuration_bundle(const fs::path& portable, const fs::path& local, BundleKind kind,
                                   const fs::path& destination) {
    try {
        check_inventory_paths(portable);
        check_path(local);
        check_path(destination);
        scan::DeviceStore store(portable, local);
        const auto inventory = store.inventory();
        if (!inventory.complete)
            throw std::runtime_error("Fix invalid device records before exporting");
        Files files;
        std::set<std::string> needed;
        if (kind == BundleKind::Profiles) {
            const auto config = read(portable / "configuration.json", 2 * 1024 * 1024);
            if (!config)
                throw std::runtime_error("Save a profile before exporting profiles");
            ConfigurationRecords records;
            const auto valid = validate_test_document(*config, records);
            if (!valid.ok)
                throw std::runtime_error(valid.detail);
            needed = references(records);
            auto json = scan::detail::parse_json(*config);
            json["devices"] = Json::array(); // Scanner records remain authoritative.
            files["configuration.json"] = json.dump(2) + "\n";
        }
        std::set<std::string> found;
        for (const auto& device : inventory.devices) {
            check_path(device.path);
            const auto id = device.record.at("logical_id").get<std::string>();
            if (kind == BundleKind::Profiles && !needed.contains(id))
                continue;
            found.insert(id);
            const auto relative = device.path.lexically_relative(portable).generic_u8string();
            const std::string name(reinterpret_cast<const char*>(relative.data()), relative.size());
            if (!payload_path(name))
                throw std::runtime_error("Invalid device filename");
            // Preserve filename-authoritative normalization and unknown fields.
            files[name] = device.record.dump(2) + "\n";
            const auto& caps = device.record.at("probed_capabilities");
            if (caps.contains("test_attempts") && caps.at("test_attempts").is_array())
                for (const auto& attempt : caps.at("test_attempts")) {
                    const auto attempt_id = attempt.value("attempt_id", "");
                    const auto history = "probe_history/" + attempt_id + ".json";
                    if (!attempt_id.empty() && payload_path(history))
                        if (const auto content = read(portable / native_path(history)))
                            files[history] = *content;
                }
        }
        Json unresolved = Json::array();
        for (const auto& id : needed)
            if (!found.contains(id))
                unresolved.push_back(id);
        Json manifest{{"format", "audiohole-configuration-bundle"},
                      {"schema_version", 1},
                      {"kind", kind == BundleKind::Devices ? "devices" : "profiles"},
                      {"profile_format", "audiohole-test-configuration"},
                      {"device_schema_version", 2},
                      {"unresolved_device_ids", unresolved},
                      {"runtime_bindings_included", false},
                      {"startup_authorized", false},
                      {"files", Json::array()}};
        TempDirectory stage;
        std::size_t total = 0;
        for (const auto& [name, bytes] : files) {
            total += bytes.size();
            if (bytes.size() > file_limit || total > bundle_limit || files.size() > 8191)
                throw std::runtime_error("Bundle exceeds its size budget");
            manifest["files"].push_back(name);
            stage_file(stage.path / native_path(name), bytes);
        }
        stage_file(stage.path / "manifest.json", manifest.dump(2) + "\n");
        scan::zip_results(stage.path, destination);
        return {true, "Configuration bundle exported. Device identities and notes retained; no "
                      "local bindings or startup authorization included."};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

Result inspect_configuration_bundle(const fs::path& portable, const fs::path& local,
                                    BundleKind kind, const fs::path& archive,
                                    std::shared_ptr<const BundleImportPlan>& output) {
    try {
        check_inventory_paths(portable);
        check_path(local);
        const auto bytes = read(archive, bundle_limit);
        if (!bytes)
            throw std::runtime_error("Bundle file is missing");
        const auto files = read_zip(*bytes);
        const auto manifest = scan::detail::parse_json(files.at("manifest.json"));
        if (manifest.at("kind") != (kind == BundleKind::Devices ? "devices" : "profiles"))
            throw std::runtime_error("Choose the matching Devices or Profiles bundle importer");
        if ((kind == BundleKind::Profiles) != files.contains("configuration.json"))
            throw std::runtime_error("Bundle profile payload does not match its kind");
        auto data = std::make_shared<BundleImportPlan::Data>();
        data->portable = portable;
        data->local = local;
        data->archive = archive;
        data->archive_bytes = *bytes;
        scan::DeviceStore store(portable, local);
        const auto current = store.inventory();
        if (!current.complete)
            throw std::runtime_error("Fix invalid local device records before importing");
        std::map<std::string, const scan::SavedDevice*> by_id;
        for (const auto& device : current.devices) {
            check_path(device.path);
            by_id.emplace(device.record.at("logical_id"), &device);
            data->observed[device.path] = device.original_bytes;
            data->inventory_paths.insert(device.path);
        }
        TempDirectory staging;
        for (const auto& [name, content] : files)
            if (name.starts_with("devices/"))
                stage_file(staging.path / native_path(name), content);
        scan::DeviceStore imported_store(staging.path, staging.path / "local");
        const auto imported = imported_store.inventory();
        if (!imported.complete)
            throw std::runtime_error("Bundle contains invalid or conflicting device records");
        std::size_t added = 0, reused = 0;
        for (const auto& device : imported.devices) {
            const auto id = device.record.at("logical_id").get<std::string>();
            const auto previous = by_id.find(id);
            if (previous != by_id.end()) {
                if (nlohmann::json(previous->second->record) != nlohmann::json(device.record))
                    throw std::runtime_error("Conflicting device identity: " + id +
                                             ". Existing record was not overwritten.");
                ++reused;
            } else {
                auto path = portable / device.path.lexically_relative(staging.path);
                path.make_preferred();
                if (read(path))
                    throw std::runtime_error(
                        "Device filename conflicts with a different identity: " +
                        scan::path_utf8(path.filename()));
                data->writes.push_back({path, device.original_bytes, {}});
                data->observed[path] = {};
                ++added;
            }
        }
        for (const auto& [name, content] : files) {
            if (!name.starts_with("probe_history/"))
                continue;
            const auto path = portable / native_path(name);
            const auto previous = read(path);
            data->observed[path] = previous;
            if (previous && nlohmann::json(scan::detail::parse_json(*previous)) !=
                                nlohmann::json(scan::detail::parse_json(content)))
                throw std::runtime_error(
                    "Conflicting probe history; existing evidence was not overwritten");
            if (!previous)
                data->writes.push_back({path, content, {}});
        }
        std::size_t profiles = 0;
        if (kind == BundleKind::Profiles) {
            ConfigurationRecords records;
            const auto valid = validate_test_document(files.at("configuration.json"), records);
            if (!valid.ok || !records.devices.empty())
                throw std::runtime_error("Invalid profile bundle configuration");
            profiles = records.profiles.size();
            const auto path = portable / "configuration.json";
            const auto previous = read(path, 2 * 1024 * 1024);
            data->observed[path] = previous;
            std::string combined;
            const auto merged =
                merge_test_profiles(previous, files.at("configuration.json"), combined);
            if (!merged.ok)
                throw std::runtime_error(merged.detail);
            const int previous_version =
                previous ? scan::detail::parse_json(*previous).at("schema_version").get<int>() : 3;
            if (previous && previous_version < 3) {
                auto backup = path;
                backup += ".v" + std::to_string(previous_version) + ".bak";
                const auto old_backup = read(backup);
                data->observed[backup] = old_backup;
                if (old_backup && old_backup != previous)
                    throw std::runtime_error(
                        "Conflicting legacy backup; import would not replace it");
                if (!old_backup)
                    data->writes.push_back({backup, *previous, {}});
            }
            data->writes.push_back(
                {path, std::move(combined), previous}); // Commit profile references last.
        }
        auto preview = std::make_shared<BundleImportPlan>();
        preview->data = std::move(data);
        preview->summary =
            "Import preview: " + std::to_string(added) + " new devices, " + std::to_string(reused) +
            " identical devices reused, " + std::to_string(profiles) +
            " profiles checked.\nNo existing IDs or differing records will be replaced.\n" +
            "Imported devices have no new local Windows binding; run an Update scan to resolve "
            "them. "
            "Unconfigured/missing references remain drafts. Routing will not start.";
        output = std::move(preview);
        return {true, output->summary};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

Result import_configuration_bundle(const BundleImportPlan& plan, std::stop_token stop,
                                   BundleWriter writer) {
    if (!plan.data)
        return {false, "No validated bundle preview"};
    const auto& data = *plan.data;
    std::vector<const BundleImportPlan::Data::Write*> created;
    std::unique_ptr<scan::detail::StoreLock> portable_lock, local_lock;
    try {
        if (stop.stop_requested())
            return {false, "Import canceled before writing"};
        check_inventory_paths(data.portable);
        check_path(data.local);
        if (read(data.archive, bundle_limit) != std::optional<std::string>(data.archive_bytes))
            throw std::runtime_error("Bundle changed after preview; inspect it again");
        scan::DeviceStore store(data.portable, data.local);
        const auto current = store.inventory();
        if (!current.complete)
            throw std::runtime_error("Local inventory is no longer valid");
        std::set<fs::path> paths;
        for (const auto& d : current.devices)
            paths.insert(d.path);
        if (paths != data.inventory_paths)
            throw std::runtime_error("Device inventory changed after preview");
        portable_lock = std::make_unique<scan::detail::StoreLock>(data.portable);
        local_lock = std::make_unique<scan::detail::StoreLock>(data.local);
        for (const auto& [path, bytes] : data.observed)
            if (read(path) != bytes)
                throw std::runtime_error(
                    "A destination changed after preview; nothing was overwritten");
        if (!writer)
            writer = [](const auto& path, const auto& bytes, const auto& expected) {
                if (bytes.size() <= 2 * 1024 * 1024)
                    return replace_document(path, bytes, expected);
                try {
                    scan::make_atomic_writer()->write(path, bytes, expected);
                    return Result{true, "written"};
                } catch (const std::exception& e) {
                    return Result{false, e.what()};
                }
            };
        for (const auto& write : data.writes) {
            if (stop.stop_requested())
                throw std::runtime_error("Import canceled");
            check_path(write.path);
            if (!write.expected)
                created.push_back(&write);
            // Device/history records have the scanner's 16 MiB budget; profile
            // serialization has its stricter 2 MiB limit already validated.
            const auto result = writer(write.path, write.bytes, write.expected);
            if (!result.ok)
                throw std::runtime_error(result.detail);
        }
        return {true, "Bundle imported. Stable IDs and notes retained; routing remains stopped."};
    } catch (const std::exception& error) {
        std::string detail = error.what();
        for (auto it = created.rbegin(); it != created.rend(); ++it) {
            try {
                const auto current = read((*it)->path);
                if (current == std::optional<std::string>((*it)->bytes))
                    fs::remove((*it)->path);
                else if (current)
                    detail += "\nA concurrently edited imported file was retained: " +
                              scan::path_utf8((*it)->path);
            } catch (...) {
                detail += "\nCould not roll back an imported file; inspect the Test data folder.";
            }
        }
        return {false, detail};
    }
}
} // namespace audiohole
