#include "tts_front/core/tokenization.hpp"

#include "tts_front/core/utf8.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"

#include <string>

namespace tts_front::detail {

std::vector<TokenSpan> token_spans(std::string_view text) {
    std::vector<Utf8CodePoint> points;
    if (!decode_utf8(text, points))
        return {};
    std::vector<TokenSpan> spans;
    std::size_t begin = std::string::npos;
    std::size_t end = 0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& point = points[index];
        const bool embedded_dot = point.value == '.' && index > 0 && index + 1 < points.size() &&
                                  is_digit(points[index - 1].value) &&
                                  is_digit(points[index + 1].value);
        if (is_word_codepoint(point.value) || embedded_dot) {
            if (begin == std::string::npos)
                begin = point.offset;
            end = point.offset + point.length;
        } else if (begin != std::string::npos) {
            spans.push_back({begin, end});
            begin = std::string::npos;
        }
    }
    if (begin != std::string::npos)
        spans.push_back({begin, end});
    return spans;
}

} // namespace tts_front::detail
