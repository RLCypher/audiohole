#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace audiohole::presentation {
// Standard selections map to stable catalog IDs; commentary itself is external
// data. Custom catalogs use their stable string IDs directly.
enum class Mode { Norm, Vanilla, UwU };
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
    Simplex,
    DeviceSaved,
    ScanLevelChanged,
    ScanScopeChanged,
    DiagnosticsCopied,
    DiagnosticsExported,
    DiagnosticsCleared,
    Dfa101,
    Dfa121,
    Dfa134,
    Dfa222,
    Dfa256,
    Dfa300,
    Dfa314,
    Dfa333,
    Dfa401,
    Dfa403,
    Dfa404,
    Dfa409,
    Dfa410,
    Dfa413,
    Dfa418,
    Dfa429,
    Dfa451,
    Dfa500,
    Dfa503,
    Dfa621,
    Dfa707,
    Dfa777,
    Dfa808,
    Dfa909,
    Dfa911,
    Dfa1024,
    DfaNegative,
    DfaAboveMaximum,
    Bother,
    Dream,
    AdvancedTuningEnabled,
    ManualOverrideApplied,
    AutomaticRestored,
    Count
};
struct Variant {
    std::string text;
    unsigned weight = 1;
    std::string id{}, category{}, subcategory{}, expression{}, fallback_expression{};
};
struct Copy {
    std::string technical, commentary, fallback_detail;
    // Selected data for a future presenter; no animation or engine calls here.
    std::string personality_id{}, response_id{}, category{}, subcategory{}, expression{},
        fallback_expression{};
};
struct Personality {
    std::string id, display_name;
    std::map<std::string, std::vector<Variant>> events;
    std::string character_name{}, description{}, avatar_style{};
    std::vector<std::string> avatar_states{};
    std::filesystem::path package_root{}, avatar_registry{}, avatar_manifest{}, preview_image{};
    std::vector<std::string> expressions{};
    int order = 100;
    bool packaged = false, avatar_available = false;
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
enum class DfaInputStatus { Valid, Invalid, Negative, AboveMaximum };
struct DfaInput {
    DfaInputStatus status = DfaInputStatus::Invalid;
    int hundredths = 0; // Meaningful only for Valid; never an invalid substitute.
};
// Parse signed decimal text (optional exponent/outer whitespace), check its raw
// mathematical range, then round half-up to integer hundredths. No floating point,
// digit limit, clamping or state mutation; work/storage is linear in input length.
// Negative zero is zero. Non-finite/malformed input is Invalid.
DfaInput parse_dfa(std::string_view input);
// Canonical dot-decimal display, exactly two fractional digits. Throws for invalid
// integer values; this formats transient presentation state, not a disk schema.
std::string dfa_text(int hundredths);

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
    // Explicit reload discovers folders and legacy flat JSON catalogs.
    // Invalid/deleted files therefore cannot leave stale commentary active.
    std::vector<std::string> load_directory(const std::filesystem::path& directory);
    LoadResult load_package(const std::filesystem::path& directory);
    std::vector<Personality> personalities() const;
    const Personality* package(const std::string& id) const;
    // Whole-personality selection; empty means presentation disabled.
    std::string resolve_selection(const std::string& requested) const;
    bool available(const std::string& id) const;
    std::vector<Variant> variants(Message event, const std::string& id) const;
    // Missing events use only compiled operational facts, never another catalog.
    // The GUI resolves whole-personality selection first. Neither fallback consumes randomness.
    // Occurrences cache selection; only distinct events may choose anew, without immediate repeats.
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
