#include "taskbar_icon_cache.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace audiohole::plug_board::windows_detail {
namespace {
constexpr std::size_t maximum_entries = 128;
constexpr std::size_t maximum_rgba_bytes = 4 * 1024 * 1024;
constexpr std::size_t maximum_document_bytes = 10 * 1024 * 1024;

bool taskbar_pixels(const ApplicationImage& image) {
    return image.result.ok && image.application_specific && image.source.starts_with("taskbar_") &&
           image.width >= 16 && image.width <= 256 && image.height >= 16 && image.height <= 256 &&
           image.rgba.size() == image.width * image.height * 4 && image.evidence.is_object() &&
           image.evidence.dump().size() <= 4096;
}
std::string hex(const std::vector<std::uint8_t>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(bytes.size() * 2, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index * 2] = digits[bytes[index] >> 4];
        result[index * 2 + 1] = digits[bytes[index] & 15];
    }
    return result;
}
std::vector<std::uint8_t> unhex(const std::string& text, std::size_t bytes) {
    if (text.size() != bytes * 2)
        throw std::runtime_error("Retained icon byte count is invalid.");
    auto digit = [](char value) -> unsigned {
        if (value >= '0' && value <= '9')
            return static_cast<unsigned>(value - '0');
        if (value >= 'a' && value <= 'f')
            return static_cast<unsigned>(value - 'a' + 10);
        throw std::runtime_error("Retained icon contains invalid hexadecimal pixels.");
    };
    std::vector<std::uint8_t> result(bytes);
    for (std::size_t index = 0; index < bytes; ++index)
        result[index] =
            static_cast<std::uint8_t>((digit(text[index * 2]) << 4) | digit(text[index * 2 + 1]));
    return result;
}
} // namespace

TaskbarIconCache::TaskbarIconCache(std::filesystem::path file) : file_(std::move(file)) {
    if (file_.empty())
        return;
    const auto stored = read_local_document(file_, maximum_document_bytes);
    try {
        if (!stored.result.ok)
            throw std::runtime_error(stored.result.detail);
        if (!stored.bytes)
            return;
        const auto doc = Document::parse(*stored.bytes);
        if (doc.at("format") != "audiohole-plug-board-icon-cache" ||
            doc.at("schema_version") != 1 || doc.at("resolution_revision") != 4 ||
            !doc.at("entries").is_array() || doc.at("entries").size() > maximum_entries)
            throw std::runtime_error("Retained icon cache has an unsupported format or size.");
        for (const auto& value : doc.at("entries")) {
            if (!value.at("width").is_number_integer() || !value.at("height").is_number_integer())
                throw std::runtime_error("Retained icon dimensions must be integers.");
            const auto key = value.at("key").get<std::string>();
            const auto width = value.at("width").get<std::int64_t>();
            const auto height = value.at("height").get<std::int64_t>();
            if (key.empty() || key.size() > 16384 || width < 16 || width > 256 || height < 16 ||
                height > 256)
                throw std::runtime_error("Retained icon identity or dimensions are invalid.");
            ApplicationImage image;
            image.width = static_cast<unsigned>(width);
            image.height = static_cast<unsigned>(height);
            image.rgba =
                unhex(value.at("rgba_hex").get<std::string>(), image.width * image.height * 4);
            image.source = value.at("source").get<std::string>();
            image.application_specific = true;
            image.result = {true, "retained_taskbar_icon", {}};
            image.evidence = value.at("evidence");
            if (!taskbar_pixels(image))
                throw std::runtime_error("Retained icon is not valid taskbar evidence.");
            entries_.insert_or_assign(key, Entry{std::move(image), ++sequence_});
            trim();
        }
        load_result_ = {true, "icon_cache_loaded", {}};
    } catch (const std::exception& error) {
        // A broken/future cache cannot affect routing or overwrite the evidence
        // on disk. Live icons still resolve and remain cached for this lifetime.
        entries_.clear();
        writable_ = false;
        load_result_ = {false, "icon_cache_unavailable", error.what()};
    }
}

const ApplicationImage* TaskbarIconCache::find(const std::string& key) {
    const auto found = entries_.find(key);
    if (found == entries_.end())
        return nullptr;
    found->second.used = ++sequence_;
    return &found->second.image;
}

void TaskbarIconCache::remember(const std::string& key, const ApplicationImage& image) {
    if (key.empty() || key.size() > 16384 || !taskbar_pixels(image))
        return;
    const auto found = entries_.find(key);
    if (found != entries_.end() && found->second.image.width == image.width &&
        found->second.image.height == image.height && found->second.image.source == image.source &&
        found->second.image.rgba == image.rgba && found->second.image.evidence == image.evidence) {
        found->second.used = ++sequence_;
        return;
    }
    entries_.insert_or_assign(key, Entry{image, ++sequence_});
    trim();
    dirty_ = true;
}

void TaskbarIconCache::trim() {
    auto bytes = [&] {
        std::size_t result = 0;
        for (const auto& [key, entry] : entries_)
            result += entry.image.rgba.size();
        return result;
    };
    while (entries_.size() > maximum_entries || bytes() > maximum_rgba_bytes) {
        const auto oldest = std::min_element(entries_.begin(), entries_.end(),
                                             [](const auto& left, const auto& right) {
                                                 return left.second.used < right.second.used;
                                             });
        entries_.erase(oldest);
    }
}

OperationResult TaskbarIconCache::flush() {
    if (!std::exchange(dirty_, false) || file_.empty() || !writable_)
        return {true, "icon_cache_memory_only", {}};
    try {
        Document entries = Document::array();
        std::vector<std::pair<std::string, const Entry*>> ordered;
        for (const auto& [key, entry] : entries_)
            ordered.emplace_back(key, &entry);
        std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
            return left.second->used < right.second->used;
        });
        for (const auto& [key, entry] : ordered) {
            const auto& image = entry->image;
            entries.push_back({{"key", key},
                               {"width", image.width},
                               {"height", image.height},
                               {"source", image.source},
                               {"evidence", image.evidence},
                               {"rgba_hex", hex(image.rgba)}});
        }
        const auto bytes = Document{{"format", "audiohole-plug-board-icon-cache"},
                                    {"schema_version", 1},
                                    {"resolution_revision", 4},
                                    {"entries", std::move(entries)}}
                               .dump(2) +
                           '\n';
        if (bytes.size() > maximum_document_bytes)
            throw std::runtime_error("Retained icon cache exceeds its document bound.");
        const auto result = write_local_document(file_, bytes);
        if (!result.ok)
            throw std::runtime_error(result.detail);
        return {true, "icon_cache_saved", {}};
    } catch (const std::exception& error) {
        writable_ = false;
        return {false, "icon_cache_write_failed", error.what()};
    }
}

} // namespace audiohole::plug_board::windows_detail
