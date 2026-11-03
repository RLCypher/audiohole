#include "presentation/sleep_commentary.h"
#include <stdexcept>
#include <utility>

namespace audiohole::presentation {
SleepCommentary::SleepCommentary(std::uint32_t seed) : random_(seed) {
    choose_ = [this](unsigned upper) {
        return std::uniform_int_distribution<unsigned>(0, upper - 1)(random_);
    };
}
SleepCommentary::SleepCommentary(std::function<unsigned(unsigned)> choose)
    : choose_(std::move(choose)) {}
int SleepCommentary::interval() {
    const auto offset = choose_(45001);
    if (offset >= 45001)
        throw std::out_of_range("Sleep commentary selector exceeded its bound");
    return 45000 + static_cast<int>(offset);
}
SleepCommentary::Change SleepCommentary::update(bool sleeping, const std::string& personality,
                                                bool has_dreams, std::int64_t now) {
    if (!sleeping) {
        const bool was_asleep = asleep_;
        asleep_ = dreaming_ = false;
        personality_.clear();
        return was_asleep ? Change::Awake : Change::None;
    }
    if (!asleep_ || personality != personality_ || has_dreams != has_dreams_) {
        asleep_ = true;
        dreaming_ = false;
        personality_ = personality;
        has_dreams_ = has_dreams;
        next_ms_ = has_dreams ? now + interval() : 0;
        return Change::Rest;
    }
    if (!has_dreams_ || now < next_ms_)
        return Change::None;
    dreaming_ = !dreaming_;
    next_ms_ = now + (dreaming_ ? 6000 : interval());
    return dreaming_ ? Change::Dream : Change::Rest;
}
} // namespace audiohole::presentation
