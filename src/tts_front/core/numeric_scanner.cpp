#include "tts_front/core/numeric_scanner.hpp"

#include "tts_front/core/text/codepoint_classification.hpp"

namespace tts_front::detail {

NumericSurface scan_numeric_surface(const Utf8Document& document, std::size_t begin) {
    NumericSurface surface;
    surface.digits_begin = begin;
    surface.digits_end = begin;
    surface.end = begin;
    if (begin >= document.points.size())
        return surface;
    const auto first = document.points[begin].value;
    if (first == '+' || first == '-' || first == 0x2212) {
        surface.sign = first == '+'
                           ? NumericSign::Plus
                           : (first == '-' ? NumericSign::Minus : NumericSign::UnicodeMinus);
        surface.digits_begin = begin + 1;
    }
    surface.digits_end = surface.digits_begin;
    while (surface.digits_end < document.points.size() &&
           text::is_digit(document.points[surface.digits_end].value))
        ++surface.digits_end;
    if (!surface.valid())
        return surface;
    surface.end = surface.digits_end;
    if (surface.end < document.points.size() &&
        (document.points[surface.end].value == 'e' || document.points[surface.end].value == 'E')) {
        surface.exponent = ExponentState::Incomplete;
        ++surface.end;
        if (surface.end < document.points.size() && (document.points[surface.end].value == '+' ||
                                                     document.points[surface.end].value == '-'))
            ++surface.end;
        const auto exponent_begin = surface.end;
        while (surface.end < document.points.size() &&
               text::is_digit(document.points[surface.end].value))
            ++surface.end;
        if (surface.end > exponent_begin) {
            surface.exponent = ExponentState::Complete;
        } else if (surface.end < document.points.size()) {
            surface.exponent = ExponentState::InvalidContinuation;
            while (surface.end < document.points.size() &&
                   (text::is_digit(document.points[surface.end].value) ||
                    text::is_letter(document.points[surface.end].value) ||
                    text::is_combining_mark(document.points[surface.end].value) ||
                    text::is_ascii_punctuation(document.points[surface.end].value) ||
                    text::is_range_connector(document.points[surface.end].value)))
                ++surface.end;
        }
    }
    return surface;
}

bool is_scientific_continuation(const Utf8Document& document, std::size_t index) {
    if (index == 0 || index >= document.points.size())
        return false;
    // This predicate is queried while sweeping every codepoint in technical
    // admission.  Do not rescan a complete numeric surface here: on a long
    // contiguous digit run that turns the otherwise linear pass into O(n^2).
    // Scientific continuation and an attached encoded suffix are identified
    // from a bounded local window.  Never rescan the complete numeric surface
    // here: this predicate runs for every codepoint in technical admission.
    const auto& points = document.points;
    const auto previous = points[index - 1].value;
    if ((previous == 'e' || previous == 'E') && index > 1 &&
        text::is_digit(points[index - 2].value))
        return true;
    if (index > 2 && text::is_digit(previous) &&
        (points[index - 2].value == 'e' || points[index - 2].value == 'E') &&
        text::is_digit(points[index - 3].value))
        return true;
    if (index > 4 && text::is_digit(previous) &&
        (points[index - 2].value == '+' || points[index - 2].value == '-' ||
         text::is_range_connector(points[index - 2].value)) &&
        (points[index - 3].value == 'e' || points[index - 3].value == 'E') &&
        text::is_digit(points[index - 4].value))
        return true;
    return index > 2 &&
           (previous == '+' || previous == '-' || text::is_range_connector(previous)) &&
           (points[index - 2].value == 'e' || points[index - 2].value == 'E') &&
           text::is_digit(points[index - 3].value);
}

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix) {
    const auto& points = document.points;
    std::size_t continuation_end = start;
    while (continuation_end < points.size()) {
        auto token_begin = continuation_end;
        while (token_begin < points.size() && text::is_horizontal_space(points[token_begin].value))
            ++token_begin;
        if (token_begin >= points.size())
            break;
        auto token_end = token_begin;
        while (token_end < points.size()) {
            if (text::is_ascii_punctuation(points[token_end].value)) {
                ++token_end;
            } else if (text::is_range_connector(points[token_end].value)) {
                auto after_connector = token_end + 1;
                while (after_connector < points.size() &&
                       text::is_horizontal_space(points[after_connector].value))
                    ++after_connector;
                if (after_connector >= points.size() ||
                    !text::is_digit(points[after_connector].value))
                    break;
                token_end = after_connector;
                continue;
            } else {
                break;
            }
            while (token_end < points.size() && text::is_horizontal_space(points[token_end].value))
                ++token_end;
        }
        if (token_end >= points.size() || !text::is_digit(points[token_end].value))
            break;
        while (token_end < points.size()) {
            if (text::is_digit(points[token_end].value) ||
                text::is_ascii_punctuation(points[token_end].value)) {
                ++token_end;
                continue;
            }
            if (text::is_range_connector(points[token_end].value)) {
                auto after_connector = token_end + 1;
                while (after_connector < points.size() &&
                       text::is_horizontal_space(points[after_connector].value))
                    ++after_connector;
                if (after_connector >= points.size() ||
                    !text::is_digit(points[after_connector].value))
                    break;
                token_end = after_connector;
                continue;
            }
            if (consume_lexical_suffix && (text::is_letter(points[token_end].value) ||
                                           text::is_combining_mark(points[token_end].value))) {
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
