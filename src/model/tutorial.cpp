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
    {"N001", "N002"},   {"N002", "N003"},   {"N003", "N004"},   {"N006", "N006A"},
    {"N006A", "N007"},  {"N010", "N011"},   {"N012", "N013"},   {"N013", "N014"},
    {"N025", "N026"},   {"N027B", "N027A"}, {"N027A", "N028"},  {"N030-A", "N031"},
    {"N030-B", "N031"}, {"N031", "N032"},   {"N032", "N033"},   {"N033", "PB001"},
    {"N034", "N035"},   {"N035", "N036"},   {"PB002", "PB003"}, {"PB003", "PB004"},
    {"PB012", "PB013"}};
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
    {"PB001", {"plug_board_tab"}},
    {"PB002", {"plug_columns"}},
    {"PB003", {"plug_socket:system", "plug_endpoint:system"}},
    {"PB004", {"plug_create"}},
    {"PB007", {"plug_create"}},
    {"PB013", {"settings_tab"}}};
} // namespace
std::vector<Endpoint> tutorial_devices() {
    std::vector<Endpoint> devices;
    for (const auto& [id, name, capture] : std::vector<std::tuple<std::string, std::string, bool>>{
             {"tutorial-mixer-in", "MixBoard Mic Input", true},
             {"tutorial-mic",
              "Microphone - FakeTech USB Audio Device 6675 (4-FakeTech USB Audio Device)", true},
             {"tutorial-mixer-out", "MixBoard Output", false},
             {"tutorial-boomslab", "Soundbar - BoomSlab 9001 (USB Audio Device)", false},
             {"tutorial-music", "MixBoard Music", false},
             {"tutorial-game", "MixBoard Game", false},
             {"tutorial-chat", "MixBoard Chat", false},
             {"tutorial-aux", "MixBoard Aux", false},
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
    if (beat == "PB004" || beat == "PB007") {
        plug_existing_.clear();
        for (const auto& socket : plug_state_.sockets)
            plug_existing_.insert(socket.id);
    }
    reminder_.clear();
    overlay_.clear();
}
int Tutorial::expected_page() const {
    if (beat_.starts_with("PB"))
        return beat_ == "PB013" ? SettingsPage : PlugBoardPage;
    return beat_ == "N004" || (beat_ >= "N011" && beat_ <= "N015") ? DevicesPage
           : beat_ >= "N005" && beat_ <= "N010"                    ? ScanningPage
           : beat_ >= "N034"                                       ? SettingsPage
                                                                   : ProfilesPage;
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
    if (plug_bridge_ && beat_ == "N034" && reminder_.empty() && overlay_.empty())
        out.text = catalog().at("PB014").at("text").get<std::string>() + "\n\n" + out.text;
    if (beat_ == "PB006" || beat_ == "PB009")
        out.targets = {"plug_endpoint:" + (beat_ == "PB006" ? music_socket_ : game_socket_)};
    if (beat_ == "PB010" || beat_ == "PB011")
        out.targets = {"plug_app:" +
                           (beat_ == "PB010" ? plug_state_.game_app : plug_state_.music_app),
                       "plug_socket:" + (beat_ == "PB010" ? game_socket_ : music_socket_)};
    if (beat_ == "PB012")
        out.targets = {"plug_endpoint:" + music_socket_, "plug_endpoint:" + game_socket_};
    if (beat_ == "N018A")
        out.targets.push_back("profile:" + profile_id_);
    if (beat_ == "N018B")
        out.targets.push_back("hole:" + hole_id_);
    if (beat_ == "N017" && early_profiles_ && reminder_.empty() && overlay_.empty())
        out.text = "Ah, you've already started without me. Love that initiative. Unfortunately, "
                   "management still wants me to walk you through this bit, so humor me. " +
                   out.text;
    if (!complete_ && !recovery_.empty()) {
        out.text = "Hey! We weren't done with that yet, put it back! Now management says I have to "
                   "explain that part over again...";
        out.expression = "surprised";
        out.choices.clear();
    }
    if (!complete_ && !prerequisite_target_.empty()) {
        out.text = catalog().at("B009").at("text").get<std::string>();
        out.expression = catalog().at("B009").at("expression").get<std::string>();
        out.targets = {prerequisite_target_};
        out.choices.clear();
    }
    const int guide_page =
        !complete_ && current_page_ != expected_page() &&
                (!reminder_.empty() || !recovery_.empty() || !prerequisite_target_.empty())
            ? expected_page()
            : sightseeing_page_;
    if (guide_page >= 0) {
        const auto& reminder = catalog().at("B001");
        if (recovery_.empty() && reminder_ != "B009" && !reminder_.starts_with("PB-R")) {
            out.text = reminder.at("text").get<std::string>();
            out.expression = reminder.at("expression").get<std::string>();
        }
        out.choices.clear();
        out.targets = {guide_page == ProfilesPage    ? "profiles_tab"
                       : guide_page == PlugBoardPage ? "plug_board_tab"
                       : guide_page == DevicesPage   ? "devices_tab"
                       : guide_page == ScanningPage  ? "scan_tab"
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
    if ((beat_ == "PB012" || beat_ == "N034") && !recover_plug_board()) {
        return false;
    }
    const auto next = beat_ == "N028" ? (choice->label == "I said something" ? "N030-B" : "N030-A")
                                      : choices_next.at(beat_);
    if (beat_ == "N028")
        acknowledged_ = true;
    if (next == "N035") {
        if (!saved_ || !routed_ || !acknowledged_ || !settings_ || !plug_finished_ ||
            renamed_.size() < 2)
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
    recovery_.clear();
    prerequisite_target_.clear();
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
std::string Tutorial::next_plug_requirement() const {
    const auto find = [&](const std::string& id) -> const TutorialSocket* {
        const auto found =
            std::find_if(plug_state_.sockets.begin(), plug_state_.sockets.end(),
                         [&](const auto& socket) { return !id.empty() && socket.id == id; });
        return found == plug_state_.sockets.end() ? nullptr : &*found;
    };
    const auto* music = find(music_socket_);
    const auto* game = find(game_socket_);
    if (!music)
        return "PB004";
    if (!game)
        return "PB007";
    if (music->endpoint != "tutorial-music")
        return "PB006";
    if (game->endpoint != "tutorial-game")
        return "PB009";
    const auto assigned = [&](const std::string& app, const std::string& socket) {
        const auto found = plug_state_.assignments.find(app);
        return !app.empty() && !socket.empty() && found != plug_state_.assignments.end() &&
               found->second == socket;
    };
    if (!assigned(plug_state_.game_app, game_socket_))
        return "PB010";
    if (!assigned(plug_state_.music_app, music_socket_))
        return "PB011";
    return {};
}
bool Tutorial::recover_plug_board() {
    const auto required = next_plug_requirement();
    if (required.empty())
        return true;
    settings_ = plug_finished_ = plug_bridge_ = false;
    enter(required);
    if (required == "PB004" || required == "PB007") {
        // Only discard an identity whose socket actually disappeared. A valid
        // peer keeps its name, output and application assignment during recovery.
        const auto missing = [&](const std::string& id) {
            return std::none_of(plug_state_.sockets.begin(), plug_state_.sockets.end(),
                                [&](const auto& socket) { return socket.id == id; });
        };
        if (missing(music_socket_))
            music_socket_.clear();
        if (missing(game_socket_))
            game_socket_.clear();
        remind("PB-R09");
    } else if (required == "PB006" || required == "PB009") {
        remind("PB-R02");
    } else {
        const bool missing_app =
            (required == "PB010" ? plug_state_.game_app : plug_state_.music_app).empty();
        remind(missing_app ? "PB-R01" : "PB-R08");
    }
    return false;
}
void Tutorial::plug_board(const TutorialPlugBoard& state) {
    const auto previous = std::move(plug_state_);
    plug_state_ = state;
    if (complete_ || !beat_.starts_with("PB") || beat_ == "PB013")
        return;
    const auto find = [&](const std::string& id) -> const TutorialSocket* {
        const auto found = std::find_if(state.sockets.begin(), state.sockets.end(),
                                        [&](const auto& socket) { return socket.id == id; });
        return found == state.sockets.end() ? nullptr : &*found;
    };
    if (!music_socket_.empty() && !find(music_socket_)) {
        recover_plug_board();
        return;
    }
    if (!game_socket_.empty() && !find(game_socket_)) {
        recover_plug_board();
        return;
    }
    if (beat_ == "PB004" || beat_ == "PB007") {
        for (const auto& socket : state.sockets) {
            if (socket.id == "system" || plug_existing_.contains(socket.id))
                continue;
            const bool music = beat_ == "PB004";
            const auto* other = find(music ? game_socket_ : music_socket_);
            // The real creation handler publishes a socket only after its
            // naming popup commits. Blank or duplicate names cannot satisfy it.
            if (socket.name.find_first_not_of(" \t\r\n") == std::string::npos ||
                (other && socket.name == other->name))
                continue;
            (music ? music_socket_ : game_socket_) = socket.id;
            enter(music ? "PB006" : "PB009");
            return;
        }
    }
    if (beat_ == "PB006" || beat_ == "PB009") {
        const bool music = beat_ == "PB006";
        const auto* socket = find(music ? music_socket_ : game_socket_);
        if (socket && socket->endpoint == (music ? "tutorial-music" : "tutorial-game")) {
            const auto next = next_plug_requirement();
            enter(next.empty() ? "PB012" : next);
        }
    } else if (beat_ == "PB010" || beat_ == "PB011") {
        const bool game = beat_ == "PB010";
        const auto& app = game ? state.game_app : state.music_app;
        const auto assigned = state.assignments.find(app);
        if (assigned != state.assignments.end() &&
            assigned->second == (game ? game_socket_ : music_socket_)) {
            const auto next = next_plug_requirement();
            enter(next.empty() ? "PB012" : next);
        } else if (state.assignments != previous.assignments) {
            remind("PB-R04");
        }
    }
}
void Tutorial::event(std::string_view action, const Snapshot& s, const Profile& draft, int hole,
                     int page) {
    current_page_ = page;
    prerequisite_target_.clear();
    if (!complete_) {
        const bool lost_profile = !profile_id_.empty() && !profile(s, profile_id_);
        const bool lost_hole = action == "hole_deleted" && draft.id == profile_id_ &&
                               !hole_id_.empty() &&
                               std::none_of(draft.holes.begin(), draft.holes.end(),
                                            [this](const auto& h) { return h.id == hole_id_; });
        if (lost_profile || lost_hole) {
            saved_ = routed_ = acknowledged_ = settings_ = false;
            hole_id_.clear();
            if (lost_profile)
                profile_id_.clear();
            enter(lost_profile ? "N017" : "N018");
            recovery_ = lost_profile ? "profile" : "hole";
            sightseeing_page_ = page == ProfilesPage ? -1 : ProfilesPage;
            return;
        }
    }
    if (!complete_ && beat_ >= "N020" && beat_ <= "N026" && draft.id == profile_id_ && hole >= 0 &&
        static_cast<std::size_t>(hole) < draft.holes.size() && draft.holes[hole].id == hole_id_) {
        const auto& current = draft.holes[hole];
        std::string rewind;
        if (!current.duplex)
            rewind = "N019";
        else if (beat_ > "N020" && current.forward.source != "tutorial-mic")
            rewind = "N020";
        else if (beat_ > "N021" && current.forward.destination != "tutorial-mixer-out")
            rewind = "N021";
        else if (beat_ > "N021A" && !current.forward.exclusive_mode_preferred)
            rewind = "N021A";
        else if (beat_ > "N022" && current.reverse.source != "tutorial-mixer-in")
            rewind = "N022";
        else if (beat_ > "N023" && current.reverse.destination != "tutorial-speakers")
            rewind = "N023";
        if (!rewind.empty()) {
            saved_ = false;
            enter(rewind);
            remind("B009");
        }
    }
    if (!complete_ && beat_ >= "N018" && beat_ <= "N026" && !profile_id_.empty()) {
        if (draft.id != profile_id_)
            prerequisite_target_ = "profile:" + profile_id_;
        else if (!hole_id_.empty() &&
                 (hole < 0 || static_cast<std::size_t>(hole) >= draft.holes.size() ||
                  draft.holes[hole].id != hole_id_))
            prerequisite_target_ = "hole:" + hole_id_;
        else if (beat_ >= "N019" && !hole_id_.empty() && !draft.holes[hole].enabled)
            prerequisite_target_ = "hole_enabled";
        else if (saved_ && beat_ >= "N025") {
            const auto* stored = profile(s, profile_id_);
            if (action == "observe_dirty" || !stored || *stored != draft) {
                saved_ = false;
                enter("N024");
                remind("B009");
            }
        }
    }
    if (action == "observe" || action == "observe_dirty")
        return;
    if (action == "scan_options_invalid") {
        if (beat_ == "N008")
            enter("N007");
        remind("B009");
        return;
    }
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
        if (beat_ == "PB001" && page == PlugBoardPage)
            enter("PB002");
        else if (beat_ == "PB013" && page == SettingsPage) {
            if (!recover_plug_board())
                return;
            settings_ = plug_finished_ = plug_bridge_ = true;
            enter("N034");
        } else if (beat_ == "N004" && page == DevicesPage)
            enter("N005");
        else if (beat_ == "N005" && page == ScanningPage)
            enter("N006");
        else if (beat_ == "N011" && page == DevicesPage)
            enter("N012");
        else if (beat_ == "N016" && page == ProfilesPage) {
            early_profiles_ = !s.profiles.empty();
            enter("N017");
        } else {
            const int expected = expected_page();
            sightseeing_page_ = !complete_ && page != expected ? expected : -1;
            if (sightseeing_page_ >= 0 && !reminder_.starts_with("PB-R"))
                remind("B001");
            else {
                if (!reminder_.starts_with("PB-R"))
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
    if (action == "scan_completed" && beat_ == "N009" && !s.scanning &&
        s.endpoints.size() == tutorial_devices().size()) {
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
            if (original.id != "tutorial-mic" && original.id != "tutorial-speakers")
                continue;
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
        recovery_.clear();
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
        recovery_.clear();
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
