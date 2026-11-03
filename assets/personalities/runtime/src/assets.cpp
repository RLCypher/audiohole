#include "audiohole/avatar/assets.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <nlohmann/json.hpp>
#include <numeric>
#include <set>
#include <stdexcept>

namespace audiohole::avatar {
namespace {
using Json = nlohmann::json;
constexpr std::size_t image_bytes = 512 * 768 * 4;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
int integral(const Json& value, int minimum, int maximum) {
    require(value.is_number_integer() && value >= minimum && value <= maximum,
            "Expected bounded integer metadata");
    return value.get<int>();
}
std::vector<std::uint8_t> read(const std::filesystem::path& path, std::size_t maximum) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    require(stream.good(), "Missing/unreadable file: " + path.filename().string());
    const auto size = stream.tellg();
    require(size > 0 && static_cast<std::uint64_t>(size) <= maximum, "File size outside contract");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    require(stream.good(), "Incomplete file read");
    return bytes;
}
Json document(const std::filesystem::path& path) {
    const auto bytes = read(path, 2 * 1024 * 1024);
    std::vector<std::set<std::string>> keys;
    auto callback = [&](int depth, Json::parse_event_t event, Json& parsed) {
        require(depth <= 16, "Avatar JSON nesting exceeds contract");
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        if (event == Json::parse_event_t::key)
            require(keys.back().insert(parsed.get<std::string>()).second, "Duplicate JSON key");
        if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    };
    return Json::parse(bytes.begin(), bytes.end(), callback);
}
bool beneath(const std::filesystem::path& file, const std::filesystem::path& root) {
    auto relative = file.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}
std::filesystem::path owned(const std::filesystem::path& root, const std::string& relative,
                            bool generated) {
    require(!relative.empty() && relative.find('\\') == std::string::npos &&
                relative.find(':') == std::string::npos,
            "Invalid portable path");
    std::filesystem::path path(relative);
    require(!path.is_absolute(), "Absolute path rejected");
    for (const auto& part : path)
        require(part != "..", "Path traversal rejected");
    require(!generated || relative.starts_with("Generated/"), "Asset path outside Generated");
    const auto base = std::filesystem::weakly_canonical(root);
    const auto result = std::filesystem::weakly_canonical(base / path);
    require(beneath(result, base), "Path/symlink crosses personality boundary");
    if (generated)
        require(beneath(result, std::filesystem::weakly_canonical(base / "Generated")),
                "Asset escaped Generated");
    return result;
}
void png_contract(std::span<const std::uint8_t> bytes) {
    constexpr std::array<std::uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    require(bytes.size() >= 33 && std::equal(signature.begin(), signature.end(), bytes.begin()),
            "Invalid PNG signature");
    auto integer = [&](std::size_t p) {
        return (std::uint32_t(bytes[p]) << 24) | (std::uint32_t(bytes[p + 1]) << 16) |
               (std::uint32_t(bytes[p + 2]) << 8) | bytes[p + 3];
    };
    require(std::string(reinterpret_cast<const char*>(bytes.data() + 12), 4) == "IHDR" &&
                integer(16) == 512 && integer(20) == 768 && bytes[24] == 8 && bytes[25] == 6,
            "Expected 512x768 8-bit RGBA PNG");
    bool end = false;
    for (std::size_t p = 8; p + 12 <= bytes.size();) {
        const auto count = integer(p);
        require(count <= bytes.size() - p - 12, "Truncated PNG chunk");
        const std::string name(reinterpret_cast<const char*>(bytes.data() + p + 4), 4);
        require(name != "acTL", "Operating asset must be a single PNG frame");
        if (name == "IEND") {
            require(count == 0 && p + 12 == bytes.size(), "Invalid PNG end");
            end = true;
            break;
        }
        p += count + 12;
    }
    require(end, "Missing PNG end");
}
Behavior behavior(const Json& entry) {
    Behavior result;
    if (!entry.contains("behavior"))
        return result;
    const auto& value = entry.at("behavior");
    const auto mode = value.at("mode").get<std::string>();
    require(mode == "persistent" || mode == "finite", "Invalid playback mode");
    result.finite = mode == "finite";
    result.priority = integral(value.at("priority"), 0, 100);
    result.interruptible = value.at("interruptible").get<bool>();
    result.cooldown_ms = integral(value.at("cooldown_ms"), 0, 3600000);
    result.static_hold_ms = integral(value.at("static_hold_ms"), 16, 60000);
    require(result.priority >= 0 && result.priority <= 100 && result.cooldown_ms >= 0 &&
                result.cooldown_ms <= 3600000 && result.static_hold_ms >= 16 &&
                result.static_hold_ms <= 60000,
            "Behavior outside contract");
    require(value.at("return_to") == "persistent" && value.at("reduced_motion") == "static",
            "Unsupported return/reduced-motion policy");
    return result;
}
bool available(const Json& asset, const LoadOptions& options) {
    const auto status = asset.at("status").get<std::string>();
    if (status == "approved") {
        const auto& review = asset.at("review");
        require(!review.at("reviewer").get<std::string>().empty() &&
                    review.at("reviewed_at").get<std::string>().size() >= 10,
                "Approval lacks review record");
        return true;
    }
    return !options.approved_only && status == "validated";
}
} // namespace

int Clip::duration_ms() const {
    return std::accumulate(durations_ms.begin(), durations_ms.end(), 0);
}
std::string display_name(const std::string& identity) {
    if (identity == "default")
        return "Norm";
    if (identity == "uwu")
        return "Horu";
    if (identity == "vanilla")
        return "Vanilla";
    return identity;
}
struct Repository::Budget {
    std::size_t maximum;
    std::atomic<std::size_t> live{0}, peak{0};
    explicit Budget(std::size_t value) : maximum(value) {}
};
std::size_t Repository::resident_bytes() const {
    return budget_->live.load();
}
std::size_t Repository::peak_resident_bytes() const {
    return budget_->peak.load();
}
Repository::Repository(std::filesystem::path root, Decoder decoder, std::size_t retained_budget)
    : budget_(std::make_shared<Budget>(retained_budget)), root_(std::move(root)),
      decoder_(std::move(decoder)) {
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(root_, error);
    if (!error)
        root_ = std::move(canonical);
}

ClipPtr Repository::load(const std::string& personality, const std::string& style,
                         const std::string& state, bool static_only, const LoadOptions& options) {
    auto result = std::make_shared<Clip>();
    result->personality = personality;
    result->style = style;
    result->requested = state;
    result->resolved = state;
    result->source = "hide";
    std::set<std::string> retained;
    std::size_t decoded = 0;
    auto cancelled = [&] { require(!options.cancelled || !options.cancelled(), "Load cancelled"); };
    try {
        cancelled();
        const auto registry = document(root_ / "registry.json");
        require(registry.at("schema_version") == 1, "Unsupported registry version");
        const auto relative = registry.at("packs").at(personality).get<std::string>();
        const auto manifest = owned(root_, relative, false);
        require(manifest.filename() == "manifest.json" && manifest.parent_path() != root_,
                "Invalid registry manifest");
        const auto data = document(manifest);
        const auto pack = manifest.parent_path();
        require(data.at("schema_version") == 1 && data.at("personality_id") == personality,
                "Manifest identity/version mismatch");
        require(data.at("technical") == Json{{"width", 512},
                                             {"height", 768},
                                             {"format", "PNG"},
                                             {"bit_depth", 8},
                                             {"color_model", "RGBA"}},
                "Manifest technical contract mismatch");
        require(data.contains("character_id") && data.at("character_id").is_string(),
                "Missing character identity");
        require(data.at("styles")
                    .at(data.at("default_style").get<std::string>())
                    .at("enabled")
                    .get<bool>(),
                "Default style must be enabled");
        for (auto& [name, value] : data.at("styles").items())
            result->styles.push_back(name);
        const auto& info = data.at("styles").at(style);
        require(info.at("enabled").get<bool>(), "Selected visual style is unavailable");
        const auto anchor = info.at("anchor").get<std::vector<int>>();
        for (const auto& point : info.at("anchor"))
            integral(point, 0, 767);
        require(anchor.size() == 2 && anchor[0] >= 0 && anchor[0] < 512 && anchor[1] >= 0 &&
                    anchor[1] < 768,
                "Invalid anchor");
        const auto& states = info.at("states");
        for (auto& [name, value] : states.items())
            result->states.push_back(name);
        require(states.contains("idle"), "Missing owned mandatory IDLE");
        for (const auto* required :
             {"idle", "happy", "thinking", "working", "talking", "confused", "concerned", "error",
              "angry", "surprised", "celebrating", "sleeping"})
            require(states.contains(required), "Missing standard state declaration");
        if (options.persistent_context && states.contains(state) &&
            behavior(states.at(state)).finite)
            static_only = true;
        for (auto iterator = cache_.begin(); iterator != cache_.end();) {
            if (iterator->second.expired())
                iterator = cache_.erase(iterator);
            else
                ++iterator;
        }
        auto image = [&](const Json& item, const std::string& expected) -> Image {
            cancelled();
            require(item.at("path") == expected && item.at("anchor") == anchor,
                    "Noncanonical frame path/anchor");
            for (const auto& point : item.at("anchor"))
                integral(point, 0, 767);
            const auto path = owned(pack, item.at("path").get<std::string>(), true);
            const auto bytes = read(path, 8 * 1024 * 1024);
            png_contract(bytes);
            const auto expected_sha = item.at("sha256").get<std::string>();
            require(expected_sha.size() == 64 && sha256(bytes) == expected_sha,
                    "Frame SHA-256 mismatch: " + expected);
            if (retained.insert(expected_sha).second) {
                decoded += image_bytes;
                require(decoded <= options.decoded_budget, "Decoded clip exceeds memory budget");
            }
            const auto key = personality + "/" + expected_sha;
            if (auto existing = cache_[key].lock())
                return existing;
            auto pixels = decoder_(bytes);
            require(pixels.width == 512 && pixels.height == 768 &&
                        pixels.rgba.size() == image_bytes,
                    "Decoder dimensions/buffer mismatch");
            bool clear = false, foreground = false;
            for (std::size_t i = 3; i < pixels.rgba.size(); i += 4) {
                clear |= pixels.rgba[i] == 0;
                foreground |= pixels.rgba[i] >= 240;
            }
            require(clear && foreground, "Empty artwork or opaque background");
            for (std::size_t corner : {std::size_t(0), std::size_t(511), std::size_t(767 * 512),
                                       std::size_t(768 * 512 - 1)})
                require(pixels.rgba[corner * 4 + 3] == 0, "PNG corners must be transparent");
            pixels.digest = expected_sha;
            const auto previous = budget_->live.fetch_add(image_bytes);
            if (previous + image_bytes > budget_->maximum) {
                budget_->live.fetch_sub(image_bytes);
                throw std::runtime_error("Repository retained-pixel memory budget exceeded");
            }
            auto peak = budget_->peak.load();
            while (peak < previous + image_bytes &&
                   !budget_->peak.compare_exchange_weak(peak, previous + image_bytes)) {
            }
            Pixels* allocated = nullptr;
            try {
                allocated = new Pixels(std::move(pixels));
            } catch (...) {
                budget_->live.fetch_sub(image_bytes);
                throw;
            }
            // The budget object outlives the repository if a renderer still owns
            // its last frame. Weak cache entries never retain decoded buffers.
            Image shared(allocated, [budget = budget_](const Pixels* pointer) {
                delete pointer;
                budget->live.fetch_sub(image_bytes);
            });
            cache_[key] = shared;
            return shared;
        };
        auto load_asset = [&](const std::string& target, bool animated) -> std::vector<Image> {
            const auto& entry = states.at(target);
            const auto& asset = entry.at(animated ? "animation" : "static");
            require(available(asset, options), "Asset is unavailable or unapproved");
            const std::string base = "Generated/" + style + "/" + target;
            if (!animated)
                return {image(asset, base + ".png")};
            const auto& frames = asset.at("frames");
            require(frames.is_array() && frames.size() >= 2 && frames.size() <= 240,
                    "Invalid animation frame count");
            auto policy = behavior(entry);
            const auto transition = asset.at("transition").get<std::string>();
            require(asset.at("loop").get<bool>() == !policy.finite &&
                        integral(asset.at("priority"), 0, 100) == policy.priority &&
                        asset.at("interruptible") == policy.interruptible &&
                        integral(asset.at("cooldown_ms"), 0, 3600000) == policy.cooldown_ms &&
                        (transition == "cut" || transition == "finish_then_persistent"),
                    "Animation/behavior policy mismatch");
            std::vector<Image> images;
            std::int64_t duration = 0;
            for (std::size_t index = 0; index < frames.size(); ++index) {
                const auto& frame = frames[index];
                const auto ms = integral(frame.at("duration_ms"), 16, 60000);
                require(ms >= 16 && ms <= 60000, "Frame duration outside contract");
                duration += ms;
                require(duration <= 3600000, "Clip duration outside contract");
                auto number = std::to_string(index);
                number.insert(number.begin(), 4 - number.size(), '0');
                images.push_back(image(frame, base + "/" + number + ".png"));
            }
            return images;
        };
        // A valid owned IDLE is mandatory even when the requested clip exists.
        Image idle;
        try {
            idle = load_asset("idle", false).front();
        } catch (const std::exception&) {
            idle = load_asset("idle", true).front();
        }
        std::vector<std::pair<std::string, bool>> attempts;
        if (!static_only)
            attempts.emplace_back(state, true);
        attempts.emplace_back(state, false);
        if (state != "idle") {
            if (!static_only)
                attempts.emplace_back("idle", true);
            attempts.emplace_back("idle", false);
        }
        for (const auto& [target, animated] : attempts) {
            try {
                auto images = load_asset(target, animated);
                result->frames = std::move(images);
                result->resolved = target;
                result->behavior = behavior(states.at(target));
                result->source = target == state ? (animated ? "animation" : "static")
                                                 : (animated ? "idle_animation" : "idle_static");
                result->loop = animated && !result->behavior.finite;
                if (animated)
                    for (const auto& frame : states.at(target).at("animation").at("frames"))
                        result->durations_ms.push_back(frame.at("duration_ms").get<int>());
                try {
                    result->still = load_asset(target, false).front();
                } catch (const std::exception&) {
                    result->still = result->frames.front();
                }
                return result;
            } catch (const std::exception& error) {
                result->diagnostics.push_back(target + ": " + error.what());
                cancelled();
            }
        }
    } catch (const std::exception& error) {
        result->diagnostics.push_back(error.what());
    }
    return result;
}
} // namespace audiohole::avatar
