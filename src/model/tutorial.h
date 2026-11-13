#pragma once
#include "model/controller.h"
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
    TutorialView view() const;
    bool choose(const std::string& id);
    void event(std::string_view action, const Snapshot&, const Profile&, int hole, int page);
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
    std::string profile_id_, hole_id_;
    bool saved_ = false, routed_ = false, acknowledged_ = false, settings_ = false,
         complete_ = false;
    void enter(const std::string&);
};
// Exact stable sandbox identities. Display-name edits never affect matching.
std::vector<Endpoint> tutorial_devices();
bool tutorial_assignment(const Hole&);
} // namespace audiohole
