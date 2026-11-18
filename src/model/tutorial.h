#pragma once
#include "model/app_page.h"
#include "model/controller.h"
#include "model/first_run.h"
#include <map>
#include <set>
#include <string_view>
namespace audiohole {
std::string_view tutorial_catalog();
struct TutorialChoice {
    std::string id, label;
};
struct TutorialView {
    std::string beat, text, expression;
    std::vector<TutorialChoice> choices;
    std::vector<std::string> targets;
    bool complete = false;
    bool sightseeing = false;
};
// Value-only view of the isolated practice board; no module/native API enters
// the lesson state machine. Stable socket and application IDs survive renaming.
struct TutorialSocket {
    std::string id, name, endpoint;
};
struct TutorialPlugBoard {
    std::vector<TutorialSocket> sockets;
    std::map<std::string, std::string> assignments;
    std::string game_app, music_app;
};
// In-memory control-thread state machine. It observes confirmed UI/controller
// events; it cannot scan hardware, open streams, or read/write configuration.
// Time affects only the avatar-click overlay, never lesson progression.
class Tutorial {
  public:
    explicit Tutorial(bool replay = false);
    const std::string& beat() const {
        return beat_;
    }
    bool complete() const {
        return complete_;
    }
    TutorialView view(std::string_view selected_device = {}, bool device_dirty = false) const;
    bool choose(const std::string& id);
    void event(std::string_view action, const Snapshot&, const Profile&, int hole, int page);
    void plug_board(const TutorialPlugBoard&);
    void click(std::int64_t now);
    void tick(std::int64_t now);
    void remind(const std::string& id);
    void repeat();
    void dismiss_reminder() {
        reminder_.clear();
    }
    void skip();
    bool ready_to_route(const Profile&, int hole) const;

  private:
    std::string beat_ = "N001", reminder_, overlay_;
    std::int64_t overlay_until_ = 0, click_after_ = 0;
    std::size_t clicks_ = 0;
    std::map<std::string, std::pair<std::string, std::string>> incoming_;
    std::set<std::string> renamed_;
    std::string profile_id_, hole_id_, initial_profile_name_, initial_hole_name_;
    int sightseeing_page_ = -1, current_page_ = 0;
    bool early_profiles_ = false;
    std::string recovery_, prerequisite_target_;
    TutorialPlugBoard plug_state_;
    std::set<std::string> plug_existing_;
    std::string music_socket_, game_socket_, plug_initial_name_;
    bool plug_finished_ = false, plug_bridge_ = false, plug_recovery_ = false;
    bool plug_assignments_ready() const;
    int expected_page() const;
    bool saved_ = false, routed_ = false, acknowledged_ = false, settings_ = false,
         complete_ = false;
    void enter(const std::string&);
};
// Exact stable sandbox identities. Display-name edits never affect matching.
std::vector<Endpoint> tutorial_devices();
bool tutorial_assignment(const Hole&);
struct OnboardingGreeting {
    std::string text, expression;
};
// No environment/personality switch. Missing matching authored greeting stays pending.
std::optional<OnboardingGreeting> onboarding_greeting(OnboardingOutcome,
                                                      std::string_view personality);
} // namespace audiohole
