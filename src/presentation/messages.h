#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
namespace audiohole::presentation {
enum class Mode { Normal, Vanilla, UwU };
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
    Personality
};
struct Variant {
    std::string text;
    unsigned weight = 1;
};
struct Copy {
    std::string technical, commentary;
};
std::string technical(Message);
std::vector<Variant> variants(Message, Mode);
// Transient presentation-local state; never stored in the controller or configuration.
// GUI-thread only. The injected selector receives an exclusive positive bound
// and must return [0, bound). Selection excludes the last eligible variant before
// applying weights; repeated calls for the same event occurrence reuse the copy.
// No reference to routing, configuration, diagnostics, or persistent storage exists.
class Messages {
  public:
    explicit Messages(std::uint32_t seed = 0x41554449);
    explicit Messages(std::function<unsigned(unsigned)> choose);
    // The default selector closes over this object's PRNG; copying/moving would
    // leave a selector referring to the old object's lifetime.
    Messages(const Messages&) = delete;
    Messages& operator=(const Messages&) = delete;
    Messages(Messages&&) = delete;
    Messages& operator=(Messages&&) = delete;

    // Technical text never varies. Vanilla bypasses selection entirely.
    Copy show(Message, Mode, std::uint64_t occurrence);

  private:
    std::mt19937 random_;
    std::function<unsigned(unsigned)> choose_;
    struct History {
        int last = -1;
        std::uint64_t occurrence = 0;
        bool valid = false;
        Copy copy;
    };
    std::map<std::pair<Message, Mode>, History> history_;
};
} // namespace audiohole::presentation
