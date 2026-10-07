#include "tts_front/normalization/language_detection.hpp"

#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"

#include <vector>

namespace tts_front::detail {

Language detect_language(std::string_view text, bool& has_cyrillic, bool& has_latin) {
    std::vector<Utf8CodePoint> points;
    decode_utf8(text, points);
    std::size_t cyrillic_count = 0;
    std::size_t latin_count = 0;
    for (const auto& point : points) {
        cyrillic_count += text::is_cyrillic(point.value) ? 1 : 0;
        latin_count += text::is_latin(point.value) ? 1 : 0;
    }
    has_cyrillic = cyrillic_count != 0;
    has_latin = latin_count != 0;
    return has_cyrillic ? Language::Russian : Language::English;
}

} // namespace tts_front::detail
