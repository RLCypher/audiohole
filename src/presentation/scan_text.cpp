#include "presentation/scan_text.h"
#include <limits>
namespace audiohole::presentation {
std::optional<unsigned> milliseconds_from_seconds(std::string_view text) {
    if (text.empty())
        return {};
    unsigned seconds = 0, fraction = 0, digits = 0;
    bool decimal = false, integer = false;
    for (const char c : text) {
        if (c == '.' && !decimal && integer) {
            decimal = true;
            continue;
        }
        if (c < '0' || c > '9')
            return {};
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (decimal) {
            if (++digits > 3)
                return {};
            fraction = fraction * 10 + digit;
        } else {
            integer = true;
            if (seconds > (std::numeric_limits<unsigned>::max() / 1000 - digit) / 10)
                return {};
            seconds = seconds * 10 + digit;
        }
    }
    if (!integer || (decimal && !digits))
        return {};
    while (digits++ < 3)
        fraction *= 10;
    if (seconds > (std::numeric_limits<unsigned>::max() - fraction) / 1000)
        return {};
    return seconds * 1000 + fraction;
}
std::string seconds_from_milliseconds(unsigned milliseconds) {
    auto result = std::to_string(milliseconds / 1000);
    const auto remainder = milliseconds % 1000;
    if (remainder) {
        auto fraction = std::to_string(1000 + remainder).substr(1);
        while (fraction.back() == '0')
            fraction.pop_back();
        result += "." + fraction;
    }
    return result;
}
} // namespace audiohole::presentation
