#include "presentation/text_layout.h"
#include <algorithm>

namespace audiohole::presentation {
namespace {
bool face_like(std::string_view body) {
    if (body.empty() || body.size() > 128 || body.find_first_of("\r\n()") != std::string_view::npos)
        return false;
    bool graphic = false;
    std::size_t word = 0;
    for (unsigned char character : body) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')) {
            if (++word >= 3)
                return false; // Ordinary words, e.g. "simulated".
        } else {
            word = 0;
            if (character != ' ' && character != '\t')
                graphic = true;
        }
    }
    return graphic;
}
} // namespace
std::string keep_kaomoji_together(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t position = 0; position < text.size();) {
        if (text[position] == '(') {
            const auto end = text.find(')', position + 1);
            if (end != std::string_view::npos &&
                face_like(text.substr(position + 1, end - position - 1))) {
                for (std::size_t index = position; index <= end; ++index) {
                    if (text[index] == ' ')
                        result += "\xC2\xA0";
                    else
                        result += text[index];
                }
                position = end + 1;
                continue;
            }
        }
        result += text[position++];
    }
    return result;
}
} // namespace audiohole::presentation
