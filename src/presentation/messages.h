#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace audiohole::presentation {
// Built-in selection aliases are retained for existing callers/tests; commentary
// itself is external data. Custom catalogs use their stable string IDs directly.
enum class Mode { Normal, Vanilla, UwU };
std::string mode_id(Mode mode);
enum class Message {
    Ready,
    Created,
    Enabled,
    Disabled,
    Source,
    Destination,
    Started,
    Stopped,
    Duplex,
    Activated,
    Pending,
    Reconnected,
    Disconnected,
    Signal,
    NoSignal,
    BeepReady,
    BeepComplete,
    BeepRejected,
    Questionable,
    Invalid,
    Recovering,
    Dfa,
    Cancelled,
    Personality,
    ScanStarted,
    ScanCompleted,
    ScanCancelled,
    ScanFailed,
    DfaMinimum,
    DfaNice,
    DfaDank,
    DfaMetal,
    DfaSixSeven,
    DfaMaximum,
    BufferChanged,
    Count
};
struct Variant {
    std::string text;
    unsigned weight = 1;
};
struct Copy {
    std::string technical, commentary, fallback_detail;
};
struct Personality {
    std::string id, display_name;
    std::map<std::string, std::vector<Variant>> events;
};
struct LoadResult {
    bool ok;
    std::string id, detail;
};
std::string technical(Message);
std::string event_id(Message);
// Exact hundredths avoid floating point equality. Ordinary positions map to Dfa;
// callers decide when a value has settled before requesting local commentary.
Message dfa_event(int hundredths);

// GUI-thread only. Owns validated data and transient repetition history, never
// controller/configuration state. No file reads occur during show(). Vanilla is
// always compiled in; external files can supply commentary only, never facts.
class Messages {
  public:
    explicit Messages(std::uint32_t seed = 0x41554449);
    explicit Messages(std::function<unsigned(unsigned)> choose);
    Messages(const Messages&) = delete;
    Messages& operator=(const Messages&) = delete;
    Messages(Messages&&) = delete;
    Messages& operator=(Messages&&) = delete;

    // Transactional: validate the whole UTF-8 JSON document before replacing an
    // ID. Failure invalidates a previous catalog from that same path, returns
    // neutral guidance, and never activates a partial catalog.
    LoadResult load_file(const std::filesystem::path& path);
    // Explicit reload clears optional catalogs, then loads sorted *.json files.
    // Invalid/deleted files therefore cannot leave stale commentary active.
    std::vector<std::string> load_directory(const std::filesystem::path& directory);
    std::vector<Personality> personalities() const;
    bool available(const std::string& id) const;
    std::vector<Variant> variants(Message event, const std::string& id) const;
    // Missing events use Vanilla individually. Unknown personality IDs use
    // Vanilla entirely. Neither fallback consumes randomness. Occurrences cache
    // selection; only distinct events may choose anew, without immediate repeats.
    Copy show(Message, const std::string& id, std::uint64_t occurrence);
    Copy show(Message event, Mode mode, std::uint64_t occurrence) {
        return show(event, mode_id(mode), occurrence);
    }

  private:
    std::mt19937 random_;
    // The default callback closes over random_; this object must not move.
    std::function<unsigned(unsigned)> choose_;
    std::map<std::string, Personality> catalogs_;
    std::map<std::string, std::filesystem::path> catalog_sources_;
    struct History {
        int last = -1;
        std::uint64_t occurrence = 0;
        bool valid = false;
        Copy copy;
    };
    std::map<std::pair<Message, std::string>, History> history_;
};
} // namespace audiohole::presentation
