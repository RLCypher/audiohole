#pragma once
#include "audiohole/avatar/player.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace audiohole::avatar {
// GUI-host facade. One cancellable worker and at most one pending request of each
// kind; the host polls completions. Worker code never calls a window or callback.
// All public calls except destruction belong to one host thread. Never call from
// an audio callback. Destruction cancels jobs and joins the owned worker.
class Session {
  public:
    Session(std::filesystem::path root, Decoder decoder, LoadOptions options = {});
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void select(std::string personality, std::string style, std::int64_t now);
    Decision baseline(std::string state, std::uint64_t revision, int priority, std::int64_t now,
                      bool invalidate_reaction = false);
    Decision react(Reaction event, std::int64_t now);
    void inspect(std::string state, bool static_only, std::int64_t now);
    Presentation tick(std::int64_t now);
    Player& player() {
        return player_;
    }
    Decision last_decision() const {
        return last_decision_;
    }
    bool loading() const;
    std::size_t decoded_bytes() const {
        return repository_.resident_bytes();
    }
    std::vector<std::string> states() const {
        return states_;
    }
    std::vector<std::string> styles() const {
        return styles_;
    }
    void shutdown();

  private:
    enum class Kind { Baseline, Reaction, Inspect };
    struct Job {
        Kind kind;
        std::uint64_t generation, serial, revision;
        std::string personality, style, state;
        bool static_only = false;
        Reaction event;
    };
    struct Completion {
        Job job;
        ClipPtr clip;
    };
    Repository repository_;
    LoadOptions options_;
    Player player_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<Job> baseline_job_, reaction_job_, inspect_job_;
    std::deque<Completion> completed_;
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint64_t> generation_{0}, baseline_serial_{0}, reaction_serial_{0},
        inspect_serial_{0};
    std::atomic<unsigned> in_flight_{0};
    std::vector<std::string> states_, styles_, diagnostics_;
    Decision last_decision_{true, "initialized"};
    Image last_valid_;
    int pending_priority_ = -1;
    std::deque<std::string> submitted_events_;
    void queue(Job job);
    bool current(const Job& job) const;
    void work();
};
} // namespace audiohole::avatar
