#include "model/tutorial.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <tuple>
namespace audiohole {
namespace {
using Json = nlohmann::json;
const Json& catalog() {
    static const auto data = Json::parse(tutorial_catalog());
    return data;
}
const std::map<std::string, std::string> choices_next{
    {"N001", "N002"},   {"N002", "N003"},   {"N003", "N004"},  {"N006", "N006A"},
    {"N006A", "N007"},  {"N010", "N011"},   {"N012", "N013"},  {"N013", "N014"},
    {"N025", "N026"},   {"N027B", "N027A"}, {"N027A", "N028"}, {"N030-A", "N031"},
    {"N030-B", "N031"}, {"N031", "N032"},   {"N032", "N033"},  {"N034", "N035"},
    {"N035", "N036"}};
const std::map<std::string, std::vector<std::string>> targets{
    {"N004", {"devices_tab"}},
    {"N005", {"scan_tab"}},
    {"N007", {"scan_level", "scan_scope"}},
    {"N008", {"scan_start"}},
    {"N011", {"devices_tab"}},
    {"N016", {"profiles_tab"}},
    {"N017", {"profile_new"}},
    {"N018", {"hole_new"}},
    {"N019", {"return"}},
    {"N020", {"forward_source"}},
    {"N021", {"forward_destination"}},
    {"N021A", {"exclusive"}},
    {"N022", {"return_source"}},
    {"N023", {"return_destination"}},
    {"N024", {"profile_save"}},
    {"N026", {"start"}},
    {"N032", {"stop"}},
    {"N033", {"settings_tab"}}};
} // namespace
std::vector<Endpoint> tutorial_devices() {
    std::vector<Endpoint> devices;
    for (const auto& [id, name, capture] : std::vector<std::tuple<std::string, std::string, bool>>{
             {"tutorial-mixer-in", "MixBoard Input", true},
             {"tutorial-mic",
              "Microphone - FakeTech USB Audio Device 6675 (4-FakeTech USB Audio Device)", true},
             {"tutorial-mixer-out", "MixBoard Output", false},
             {"tutorial-speakers",
              "Speakers - FakeTech USB Audio Device 636B (3-FakeTech USB Audio Device)", false}}) {
        Endpoint e;
        e.id = id;
        e.display_name = e.windows_name = name;
        e.capture = capture;
        e.notes = "Fictional tutorial endpoint; no hardware access.";
        e.formats = {Format{}};
        devices.push_back(std::move(e));
    }
    return devices;
}
bool tutorial_assignment(const Hole& h) {
    return h.enabled && h.duplex && h.forward.source == "tutorial-mic" &&
           h.forward.destination == "tutorial-mixer-out" &&
           h.reverse.source == "tutorial-mixer-in" &&
           h.reverse.destination == "tutorial-speakers" && h.forward.exclusive_mode_preferred;
}
Tutorial::Tutorial(bool replay) {
    if (replay)
        reminder_ = "B012";
}
void Tutorial::enter(const std::string& beat) {
    beat_ = beat;
    reminder_.clear();
    overlay_.clear();
}
TutorialView Tutorial::view(std::string_view selected_device, bool device_dirty) const {
    TutorialView out;
    out.beat = beat_;
    out.complete = complete_;
    const auto& beat = catalog().at(beat_);
    const auto* line = &beat;
    if (beat.contains("variants")) {
        const auto found = incoming_.find(beat_);
        for (const auto& variant : beat["variants"]) {
            if (!line->contains("text"))
                line = &variant;
            if (found != incoming_.end() && variant.at("from") == found->second.first &&
                variant.at("label") == found->second.second) {
                line = &variant;
                break;
            }
        }
    }
    if (!reminder_.empty())
        line = &catalog().at(reminder_);
    if (!overlay_.empty())
        line = &catalog().at(overlay_);
    out.text = line->at("text").get<std::string>();
    out.expression = line->at("expression").get<std::string>();
    for (std::size_t i = 0; i < beat.at("choices").size(); ++i)
        out.choices.push_back(
            {beat_ + "/" + std::to_string(i), beat["choices"][i].get<std::string>()});
    if (const auto found = targets.find(beat_); found != targets.end())
        out.targets = found->second;
    if (beat_ == "N014" || beat_ == "N015") {
        if ((selected_device == "tutorial-mic" || selected_device == "tutorial-speakers") &&
            !renamed_.contains(std::string(selected_device))) {
            out.targets.push_back("device_name");
            if (device_dirty)
                out.targets.push_back("device_save");
        }
        for (const auto* id : {"tutorial-mic", "tutorial-speakers"})
            if (!renamed_.contains(id))
                out.targets.push_back(std::string("device:") + id);
    }
    if (beat_ == "N018A")
        out.targets.push_back("profile:" + profile_id_);
    if (beat_ == "N018B")
        out.targets.push_back("hole:" + hole_id_);
    if (sightseeing_page_ >= 0) {
        const auto& reminder = catalog().at("B001");
        out.text = reminder.at("text").get<std::string>();
        out.expression = reminder.at("expression").get<std::string>();
        out.choices.clear();
        out.targets = {sightseeing_page_ == 0   ? "profiles_tab"
                       : sightseeing_page_ == 1 ? "devices_tab"
                       : sightseeing_page_ == 2 ? "scan_tab"
                                                : "settings_tab"};
        out.sightseeing = true;
    }
    return out;
}
std::optional<OnboardingGreeting> onboarding_greeting(OnboardingOutcome outcome,
                                                      std::string_view personality) {
    const char* key = outcome == OnboardingOutcome::Completed && personality == "default" ? "N037"
                      : outcome == OnboardingOutcome::Skipped && personality == "default" ? "N037A"
                      : outcome == OnboardingOutcome::Skipped && personality == "uwu"     ? "N037B"
                                                                                          : nullptr;
    if (!key)
        return {};
    const auto& entry = catalog().at(key);
    return OnboardingGreeting{entry.at("text").get<std::string>(),
                              entry.at("expression").get<std::string>()};
}
bool Tutorial::choose(const std::string& id) {
    const auto current = view();
    const auto choice = std::find_if(current.choices.begin(), current.choices.end(),
                                     [&](const auto& c) { return c.id == id; });
    if (choice == current.choices.end())
        return false;
    const auto next = beat_ == "N028" ? (choice->label == "I said something" ? "N030-B" : "N030-A")
                                      : choices_next.at(beat_);
    if (beat_ == "N028")
        acknowledged_ = true;
    if (next == "N035") {
        if (!saved_ || !routed_ || !acknowledged_ || !settings_ || renamed_.size() < 2)
            return false;
        complete_ = true;
    }
    incoming_[next] = {beat_, choice->label};
    enter(next);
    return true;
}
void Tutorial::remind(const std::string& id) {
    if (!catalog().contains(id))
        return;
    overlay_.clear();
    reminder_ = id;
}
void Tutorial::repeat() {
    remind("B009");
}
bool Tutorial::ready_to_route(const Profile& p, int hole) const {
    return saved_ && p.id == profile_id_ && hole >= 0 &&
           static_cast<std::size_t>(hole) < p.holes.size() && p.holes[hole].id == hole_id_ &&
           tutorial_assignment(p.holes[hole]);
}
void Tutorial::skip() {
    complete_ = true;
    enter("B010");
}
void Tutorial::click(std::int64_t now) {
    if (now < click_after_)
        return;
    click_after_ = now + 350;
    std::string context = complete_                            ? "C007"
                          : beat_ == "N009"                    ? "C004"
                          : beat_ == "N028"                    ? "C006"
                          : beat_ >= "N017" && beat_ <= "N026" ? "C003"
                          : beat_ >= "N011" && beat_ <= "N016" ? "C002"
                          : beat_ == "N010"                    ? "C005"
                                                               : "C001";
    // Contextual first click, then rotate authored general reactions without repeats.
    overlay_ = clicks_++ % 3 == 0 ? context : clicks_ % 3 == 0 ? "C008" : "C009";
    overlay_until_ = now + 6000;
}
void Tutorial::tick(std::int64_t now) {
    if (!overlay_.empty() && now >= overlay_until_)
        overlay_.clear();
}
void Tutorial::event(std::string_view action, const Snapshot& s, const Profile& draft, int hole,
                     int page) {
    // Confirmed user actions dismiss a click quip even when they do not finish
    // the objective. Background progress events must leave its timer intact.
    for (const auto user_action :
         {"navigation", "scan_options", "scan_started", "device_edit", "device_saved",
          "profile_created", "hole_created", "profile_renamed", "hole_renamed", "return",
          "endpoints", "exclusive", "saved", "routing_started", "stopped"})
        if (action == user_action)
            overlay_.clear();
    if (action == "navigation") {
        sightseeing_page_ = -1;
        if (beat_ == "N004" && page == 1)
            enter("N005");
        else if (beat_ == "N005" && page == 2)
            enter("N006");
        else if (beat_ == "N011" && page == 1)
            enter("N012");
        else if (beat_ == "N016" && page == 0)
            enter("N017");
        else if (beat_ == "N033" && page == 3) {
            settings_ = true;
            enter("N034");
        } else {
            const int expected = beat_ == "N004" || (beat_ >= "N011" && beat_ <= "N015") ? 1
                                 : beat_ >= "N005" && beat_ <= "N010"                    ? 2
                                 : beat_ >= "N033"                                       ? 3
                                                                                         : 0;
            sightseeing_page_ = !complete_ && page != expected ? expected : -1;
            if (sightseeing_page_ >= 0)
                remind("B001");
            else {
                reminder_.clear();
                overlay_.clear();
            }
        }
        return;
    }
    if (action == "scan_options" && beat_ == "N007") {
        enter("N008");
        return;
    }
    if (action == "scan_started" && beat_ == "N008" && s.scanning) {
        enter("N009");
        return;
    }
    if (action == "scan_completed" && beat_ == "N009" && !s.scanning && s.endpoints.size() == 4) {
        enter("N010");
        return;
    }
    if ((action == "scan-cancelled" || action == "scan-failed") && beat_ == "N009") {
        enter("N008");
        return;
    }
    if (action == "device_edit" && beat_ == "N014") {
        enter("N015");
        return;
    }
    if (action == "device_edit" && beat_ == "N015") {
        remind("B003");
        return;
    }
    if (action == "device_saved" && (beat_ == "N014" || beat_ == "N015")) {
        renamed_.clear();
        for (const auto& original : tutorial_devices()) {
            const auto* current = endpoint(s, original.id);
            if (current && !current->display_name.empty() &&
                current->display_name != original.display_name)
                renamed_.insert(original.id);
        }
        if (renamed_.size() >= 2)
            enter("N016");
        else {
            enter("N015");
            remind("B004");
        }
        return;
    }
    if (action == "profile_created" && beat_ == "N017" && !draft.id.empty()) {
        profile_id_ = draft.id;
        initial_profile_name_ = draft.display_name;
        enter("N018A");
        return;
    }
    if (action == "profile_renamed" && beat_ == "N018A" && draft.id == profile_id_ &&
        !draft.display_name.empty() && draft.display_name != initial_profile_name_) {
        const auto* stored = profile(s, profile_id_);
        if (stored && stored->display_name == draft.display_name)
            enter("N018");
        return;
    }
    if (hole < 0 || static_cast<std::size_t>(hole) >= draft.holes.size())
        return;
    const auto& h = draft.holes[hole];
    if (action == "hole_created" && beat_ == "N018" && draft.id == profile_id_) {
        hole_id_ = h.id;
        initial_hole_name_ = h.display_name;
        enter("N018B");
        return;
    }
    if (draft.id != profile_id_ || h.id != hole_id_)
        return;
    if (action == "hole_renamed" && beat_ == "N018B" && !h.display_name.empty() &&
        h.display_name != initial_hole_name_) {
        enter("N019");
        return;
    }
    if (action == "return" && beat_ == "N019") {
        if (h.duplex)
            enter("N020");
        else
            remind("B005");
        return;
    }
    if (action == "endpoints") {
        std::string next;
        if (beat_ == "N020" && h.forward.source == "tutorial-mic")
            next = "N021";
        else if (beat_ == "N021" && h.forward.destination == "tutorial-mixer-out")
            next = "N021A";
        else if (beat_ == "N022" && h.reverse.source == "tutorial-mixer-in")
            next = "N023";
        else if (beat_ == "N023" && tutorial_assignment(h))
            next = "N024";
        if (!next.empty())
            enter(next);
        else if (beat_ == "N020" || beat_ == "N021" || beat_ == "N022" || beat_ == "N023")
            remind("B002");
        return;
    }
    if (action == "exclusive" && beat_ == "N021A" && h.forward.exclusive_mode_preferred) {
        enter("N022");
        return;
    }
    if (action == "saved" && beat_ == "N024") {
        const auto* stored = profile(s, profile_id_);
        if (stored && std::any_of(stored->holes.begin(), stored->holes.end(), [&](const auto& x) {
                return x.id == hole_id_ && tutorial_assignment(x);
            })) {
            saved_ = true;
            enter("N025");
        } else
            remind("B006");
        return;
    }
    if (action == "routing_started" && beat_ == "N026" && saved_ && s.armed &&
        tutorial_assignment(h)) {
        enter("N027");
        return;
    }
    if (action == "simulated_routing_running" && beat_ == "N027" && saved_ &&
        s.state == State::Running) {
        routed_ = true;
        enter("N027B");
        return;
    }
    if (action == "stopped" && !acknowledged_ &&
        (beat_ == "N027" || beat_ == "N027B" || beat_ == "N027A" || beat_ == "N028")) {
        routed_ = false;
        enter("N026");
    }
}
} // namespace audiohole
