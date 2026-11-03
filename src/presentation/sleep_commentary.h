#pragma once
#include <cstdint>
#include <functional>
#include <random>
#include <string>

namespace audiohole::presentation {
// GUI-thread, clock-injected schedule for cosmetic sleep text only. No controller,
// avatar requests, notifications or file access. Missed intervals never catch up.
class SleepCommentary {
  public:
    enum class Change { None, Rest, Dream, Awake };
    explicit SleepCommentary(std::uint32_t seed);
    explicit SleepCommentary(std::function<unsigned(unsigned)> choose);
    SleepCommentary(const SleepCommentary&) = delete;
    SleepCommentary& operator=(const SleepCommentary&) = delete;
    Change update(bool sleeping, const std::string& personality, bool has_dreams, std::int64_t now);

  private:
    std::mt19937 random_;
    std::function<unsigned(unsigned)> choose_;
    std::string personality_;
    bool asleep_ = false, dreaming_ = false, has_dreams_ = false;
    std::int64_t next_ms_ = 0;
    int interval();
};
} // namespace audiohole::presentation
