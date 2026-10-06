#include "candidate_scanner.hpp"

#include <cctype>

namespace tts_front::detail {
namespace {

bool is_digit_cp(std::uint32_t cp) {
    return cp >= '0' && cp <= '9';
}

bool is_letter_cp(std::uint32_t cp) {
    const bool cyrillic = cp >= 0x0400 && cp <= 0x052f;
    const bool latin =
        (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= 0x00c0 && cp <= 0x024f);
    return cyrillic || latin;
}

bool is_combining_mark_cp(std::uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
           (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
           (cp >= 0xfe20 && cp <= 0xfe2f);
}

bool is_range_connector_cp(std::uint32_t cp) {
    return (cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212;
}

bool is_horizontal_space_cp(std::uint32_t cp) {
    return cp == ' ' || cp == '\t';
}

bool is_ascii_punctuation_cp(std::uint32_t cp) {
    return cp < 0x80 && std::ispunct(static_cast<unsigned char>(cp)) != 0;
}

} // namespace

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix) {
    const auto& points = document.points;
    std::size_t continuation_end = start;
    while (continuation_end < points.size()) {
        auto token_begin = continuation_end;
        while (token_begin < points.size() && is_horizontal_space_cp(points[token_begin].value))
            ++token_begin;
        if (token_begin >= points.size())
            break;

        auto token_end = token_begin;
        while (token_end < points.size()) {
            if (is_ascii_punctuation_cp(points[token_end].value)) {
                ++token_end;
            } else if (is_range_connector_cp(points[token_end].value)) {
                auto after_connector = token_end + 1;
                while (after_connector < points.size() &&
                       is_horizontal_space_cp(points[after_connector].value))
                    ++after_connector;
                if (after_connector >= points.size() || !is_digit_cp(points[after_connector].value))
                    break;
                token_end = after_connector;
                continue;
            } else {
                break;
            }
            while (token_end < points.size() && is_horizontal_space_cp(points[token_end].value))
                ++token_end;
        }
        if (token_end >= points.size() || !is_digit_cp(points[token_end].value))
            break;

        while (token_end < points.size()) {
            if (is_digit_cp(points[token_end].value) ||
                is_ascii_punctuation_cp(points[token_end].value)) {
                ++token_end;
                continue;
            }
            if (is_range_connector_cp(points[token_end].value)) {
                auto after_connector = token_end + 1;
                while (after_connector < points.size() &&
                       is_horizontal_space_cp(points[after_connector].value))
                    ++after_connector;
                if (after_connector >= points.size() || !is_digit_cp(points[after_connector].value))
                    break;
                token_end = after_connector;
                continue;
            }
            if (consume_lexical_suffix && (is_letter_cp(points[token_end].value) ||
                                           is_combining_mark_cp(points[token_end].value))) {
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
