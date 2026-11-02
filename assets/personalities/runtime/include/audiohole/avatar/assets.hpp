#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace audiohole::avatar {
// Immutable straight-alpha RGBA. Decoders must return exactly 512x768 pixels.
struct Pixels {
    int width = 512, height = 768;
    std::vector<std::uint8_t> rgba;
    std::string digest;
};
using Image = std::shared_ptr<const Pixels>;
using Decoder = std::function<Pixels(std::span<const std::uint8_t>)>;
struct Behavior {
    bool finite = false, interruptible = true;
    int priority = 0, cooldown_ms = 0, static_hold_ms = 1500;
};
struct Clip {
    std::string personality, style, requested, resolved, source;
    Behavior behavior;
    std::vector<Image> frames;
    std::vector<int> durations_ms;
    Image still;
    bool loop = true;
    std::vector<std::string> diagnostics;
    std::vector<std::string> states, styles;
    int duration_ms() const;
};
using ClipPtr = std::shared_ptr<const Clip>;
struct LoadOptions {
    bool approved_only = true;
    bool persistent_context = false;
    std::size_t decoded_budget = 64 * 1024 * 1024;
    std::function<bool()> cancelled;
};
// A repository is worker-thread confined. load() does file I/O, integrity checks
// and decoding. Returned clips own their pixels; no I/O occurs during playback.
class Repository {
  public:
    Repository(std::filesystem::path root, Decoder decoder,
               std::size_t retained_budget = 128 * 1024 * 1024);
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    ClipPtr load(const std::string& personality, const std::string& style, const std::string& state,
                 bool static_only = false, const LoadOptions& options = {});
    const std::filesystem::path& root() const {
        return root_;
    }
    std::size_t resident_bytes() const;
    std::size_t peak_resident_bytes() const;

  private:
    struct Budget;
    std::shared_ptr<Budget> budget_;
    std::filesystem::path root_;
    Decoder decoder_;
    std::map<std::string, std::weak_ptr<const Pixels>> cache_;
};
std::string sha256(std::span<const std::uint8_t> bytes);
std::string display_name(const std::string& personality);
// WIC on Windows, wxImage PNG handler on other wx-supported platforms.
// Initialize the non-Windows image handler once, before starting loader threads.
void initialize_image_decoder();
Decoder native_decoder();
} // namespace audiohole::avatar
