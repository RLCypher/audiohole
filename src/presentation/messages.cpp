#include "presentation/messages.h"
#include <array>
#include <stdexcept>
namespace audiohole::presentation {
namespace {
struct Entry {
    const char* technical;
    const char* normal;
    const char* normal2;
    const char* uwu;
    const char* uwu2;
};
constexpr std::array<Entry, 24> catalog{
    {{"Ready to configure simulated routing.", "Alright. Let's wire this up.",
      "A cable without the desk clutter.",
      "Hewwo~ Weady to open some howes? I've been waiting fow you~ (⁄ ⁄>⁄ω⁄<⁄ ⁄)",
      "Aww~ so many pwetty howes, so many ways to connect~ OwO"},
     {"Hole created in the editor.", "Another hole. Naturally.", "Room for one more connection.",
      "Anothew howe?! My, someone's feeling adventuwous today~ OwO",
      "A fwesh wittle opening~ Pwease choose whewe it goes! :3"},
     {"Hole enabled in the editor.", "Hole's open.", "Ready for a connection.",
      "Aww~ you opened it fow me! Now put something thwough it~ :3",
      "Weady and waiting to be fiwwed with signaw~ >w<"},
     {"Hole disabled in the editor.", "Hole's closed.", "Taking that one out of the plan.",
      "Aww, cwosing up awweady? I was just getting stawted~ (╥﹏╥)",
      "A wittle west fow that howe~ I'll be hewe when you need it! :3"},
     {"Source selected in the editor.", "Source is connected.", "Found the input end.",
      "Ooh, a fwesh input! I can wowk with that~ (¬‿¬)",
      "That's a pwetty input~ Pwease put it wight hewe~ UwU"},
     {"Destination selected in the editor.", "Output assigned.", "That's the other end sorted.",
      "That's whewe you want it to come out? How vewy specific of you~ UwU",
      "Mmm~ a pwace fow all that output! (｡♥‿♥｡)"},
     {"Simulated routing is Running. No audio is processed.", "Signal's moving. In theory.",
      "The imaginary cable is connected.",
      "We'we connected~ Now wet me heaw you make some noise! >w<",
      "Ooh~ a nice tight connection, just how I wike it~ :3"},
     {"Simulated routing stopped.", "That's enough of that.", "Cable put away.",
      "Aww, finished awweady? I was having so much fun~ (⁄ ⁄>⁄ω⁄<⁄ ⁄)",
      "Puwwing out fow now? I'll keep youw howes weady~ UwU"},
     {"Duplex selected. Each direction has independent settings.", "Both ways. Ambitious.",
      "Two directions, one hole.",
      "B-both ways at once?! You weawwy know how to keep a howe busy~ OwO (｡♥‿♥｡)",
      "Fwont and back~ Oh my, that's a vewy busy connection! >w<"},
     {"Simulated profile activated.", "New setup's in.", "A different arrangement of cables.",
      "Aww~ a whowe new position fow evewything! I wove a good weawwangement~ :3",
      "A fwesh way to get evewything plugged in~ (¬‿¬)"},
     {"Simulated startup activation is pending.", "Holding that configuration.",
      "Waiting for the appointed moment.",
      "Just a wittle anticipation~ Don't make me wait too wong! (⁄ ⁄>⁄ω⁄<⁄ ⁄)",
      "I'll be weady when it's time to swip it in~ OwO"},
     {"Simulated device reconnected.", "Back where it belongs.", "Plug found its way home.",
      "You came back! I kept youw howe nice and weady~ UwU",
      "Ooh~ swiding back in wike you nevew weft! :3"},
     {"Simulated device disconnected.", "Someone pulled the plug.", "One end went missing.",
      "You puwwed out without wawning?! At weast say goodbye fiwst~ (╥﹏╥)",
      "Aww~ an empty howe again? Come back soon~ >w<"},
     {"Simulated source activity detected. Destination audibility is untested.",
      "Yep, something's coming in.", "The pretend input is busy.",
      "Ooh! I can feew the signaw coming in~ Keep going! >w<",
      "A wittle signaw tickwe~ Just what this howe needed! :3"},
     {"No simulated source activity. Silence does not establish disconnection.",
      "Nothing coming in yet.", "Quiet at the input.",
      "It's so quiet in hewe... give me a wittle something? :3",
      "Aww~ this howe's waiting fow a tiny tickwe~ UwU"},
     {"Select a destination for simulated Test Beep. NO SOUND will be emitted.",
      "Let's see where this would come out.", "Pick the output, then test the idea.",
      "Wanna make it squeak? Pick youw output and give it a wittle beep~ (¬‿¬)",
      "A wittle beep, just fow that howe~ :3"},
     {"Simulated Test Beep interval completed. NO SOUND was emitted.",
      "That was a theoretical beep.", "Audibility still requires real hardware testing.",
      "Beep~! A pwetend squeak fow youw chosen howe~ UwU",
      "Aww~ the imaginary tickwe is finished! >w<"},
     {"Select an available playback destination before testing.", "Pick an output first.",
      "Even a pretend beep needs somewhere to go.",
      "P-pwease pick an output fiwst~ I can't beep into the void! >w<",
      "A wittle destination pwease~ Which howe gets the beep? :3"},
     {"This setup may introduce latency or feedback. Review the settings.",
      "I wouldn't, but it's your gear.", "Worth checking that cable loop.",
      "Ooh, that's a naughty wittle config~ It might feed back, but if you insist... (¬‿¬)",
      "That connection's getting adventuwous~ Pwease check the detaiws! OwO"},
     {"Enter a valid value; review the detailed validation message.", "Skill issue. Try again.",
      "That value needs another look.",
      "Nuh-uh~ that numbew doesn't fit in this howe. Twy again, pwease? :3",
      "A wittle adjustment pwease~ That one's not weady to swip in! >w<"},
     {"Simulated recovery is in progress.", "Trying to get it back.",
      "Working on the missing connection.",
      "Hang on, sweetie~ I'm twying to get you plugged back in! (╥﹏╥)",
      "I'll get that wittle connection weady again~ UwU"},
     {"DFA acknowledgment only. No operation performed.", "DFA executed. Nothing happened.",
      "Still doing absolutely nothing.",
      "Nyaa~ I did absowutewy nothing! Did you wike that? (⁄ ⁄>⁄ω⁄<⁄ ⁄)",
      "Aww~ all that clicking and not a singwe thing done~ :3"},
     {"Pending simulated operation cancelled.", "Changed your mind. Fair.", "That's off the list.",
      "Aww~ stopping the anticipation? I'll wait fow you~ UwU",
      "No mowe teasing that connection fow now~ :3"},
     {"Presentation mode changed. Operational behavior is unchanged.",
      "Same engineer, different commentary.", "The cables didn't notice.",
      "Ooh~ a new wittle voice, same pwetty howes! OwO",
      "Dwessing up the commentary~ The connections stay exactwy whewe they awe! :3"}}};
} // namespace
std::string technical(Message event) {
    return catalog.at(static_cast<std::size_t>(event)).technical;
}
std::vector<Variant> variants(Message event, Mode mode) {
    if (mode == Mode::Vanilla)
        return {};
    const auto& e = catalog.at(static_cast<std::size_t>(event));
    std::vector<Variant> result = mode == Mode::Normal
                                      ? std::vector<Variant>{{e.normal}, {e.normal2}}
                                      : std::vector<Variant>{{e.uwu}, {e.uwu2}};
    if (event == Message::Dfa) {
        if (mode == Mode::Normal) {
            result.push_back({"No drivers were harmed in the making of this nothing."});
            result.push_back({"A remarkably uneventful button."});
            result.push_back({"Task failed successfully at doing nothing.", 1});
        } else {
            result.push_back({"Ooh~ you pwessed my button! And I did nothing at all~ >w<"});
            result.push_back({"So much anticipation, so wittle action~ (¬‿¬)"});
            result.push_back({"A wawe wittle nothing, just fow you~ (｡♥‿♥｡)", 1});
        }
        result.front().weight = 3;
    }
    return result;
}
Messages::Messages(std::uint32_t seed) : random_(seed) {
    choose_ = [this](unsigned total) {
        return std::uniform_int_distribution<unsigned>(0, total - 1)(random_);
    };
}
Messages::Messages(std::function<unsigned(unsigned)> choose) : choose_(std::move(choose)) {}
Copy Messages::show(Message event, Mode mode, std::uint64_t occurrence) {
    auto& h = history_[{event, mode}];
    if (h.valid && h.occurrence == occurrence)
        return h.copy;
    Copy result{technical(event), {}};
    const auto pool = variants(event, mode);
    unsigned total = 0;
    for (std::size_t i = 0; i < pool.size(); ++i)
        if (pool.size() == 1 || static_cast<int>(i) != h.last)
            total += pool[i].weight;
    if (total) {
        unsigned choice = choose_(total);
        if (choice >= total)
            throw std::out_of_range("Injected presentation selector exceeded its bound");
        for (std::size_t i = 0; i < pool.size(); ++i) {
            if (pool.size() != 1 && static_cast<int>(i) == h.last)
                continue;
            if (choice < pool[i].weight) {
                result.commentary = pool[i].text;
                h.last = static_cast<int>(i);
                break;
            }
            choice -= pool[i].weight;
        }
    }
    h.copy = result;
    h.occurrence = occurrence;
    h.valid = true;
    return result;
}
} // namespace audiohole::presentation
