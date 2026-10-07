#include "candidate_scanner.hpp"

#include "codepoint_classification.hpp"

namespace tts_front::detail {

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix) {
    const auto& points = document.points;
    std::size_t continuation_end = start;
    while (continuation_end < points.size()) {
        auto token_begin = continuation_end;
        while (token_begin < points.size() && is_horizontal_space(points[token_begin].value))
            ++token_begin;
        if (token_begin >= points.size())
            break;

        auto token_end = token_begin;
        while (token_end < points.size()) {
            if (is_ascii_punctuation(points[token_end].value)) {
                ++token_end;
            } else if (is_range_connector(points[token_end].value)) {
                auto after_connector = token_end + 1;
                while (after_connector < points.size() &&
                       is_horizontal_space(points[after_connector].value))
                    ++after_connector;
                if (after_connector >= points.size() || !is_digit(points[after_connector].value))
                    break;
                token_end = after_connector;
                continue;
            } else {
                break;
            }
            while (token_end < points.size() && is_horizontal_space(points[token_end].value))
                ++token_end;
        }
        if (token_end >= points.size() || !is_digit(points[token_end].value))
            break;

        while (token_end < points.size()) {
            if (is_digit(points[token_end].value) ||
                is_ascii_punctuation(points[token_end].value)) {
                ++token_end;
                continue;
            }
            if (is_range_connector(points[token_end].value)) {
                auto after_connector = token_end + 1;
                while (after_connector < points.size() &&
                       is_horizontal_space(points[after_connector].value))
                    ++after_connector;
                if (after_connector >= points.size() || !is_digit(points[after_connector].value))
                    break;
                token_end = after_connector;
                continue;
            }
            if (consume_lexical_suffix && (is_letter(points[token_end].value) ||
                                           is_combining_mark(points[token_end].value))) {
                ++token_end;
                continue;
            }
            break;
        }
        continuation_end = token_end;
    }
    return continuation_end;
}

} // namespace tts_front::detail
