#include "tts_front/core/numeric_scanner.hpp"

#include "tts_front/core/text/codepoint_classification.hpp"

#include <algorithm>

namespace tts_front::detail {

NumericSurface scan_numeric_surface(const Utf8Document& document, std::size_t begin) {
    NumericSurface surface;
    surface.digits_begin = begin;
    surface.digits_end = begin;
    surface.end = begin;
    surface.continuation_end = begin;
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
    surface.continuation_end = surface.end;
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
    surface.continuation_end = surface.end;
    if (surface.exponent != ExponentState::None && surface.end < document.points.size() &&
        (document.points[surface.end].value == '#' || document.points[surface.end].value == '$')) {
        auto suffix_end = surface.end + 1;
        while (suffix_end < document.points.size() &&
               (text::is_digit(document.points[suffix_end].value) ||
                text::is_letter(document.points[suffix_end].value) ||
                text::is_combining_mark(document.points[suffix_end].value) ||
                document.points[suffix_end].value == '_'))
            ++suffix_end;
        if (suffix_end > surface.end + 1)
            surface.continuation_end = suffix_end;
    }
    if (surface.continuation_end == surface.end && surface.end < document.points.size() &&
        text::is_horizontal_space(document.points[surface.end].value)) {
        auto suffix_begin = surface.end;
        while (suffix_begin < document.points.size() &&
               text::is_horizontal_space(document.points[suffix_begin].value))
            ++suffix_begin;
        auto suffix_end = suffix_begin;
        while (suffix_end < document.points.size() &&
               (text::is_letter(document.points[suffix_end].value) ||
                text::is_combining_mark(document.points[suffix_end].value)))
            ++suffix_end;
        if (suffix_end > suffix_begin)
            surface.continuation_end = suffix_end;
    }
    return surface;
}

bool NumericSurfaceIndex::contains(std::size_t point) const {
    const auto found = std::lower_bound(
        ranges.begin(),
        ranges.end(),
        point,
        [](const NumericSurfaceRange& range, std::size_t value) { return range.end <= value; });
    return found != ranges.end() && found->begin <= point && point < found->end;
}

NumericSurfaceIndex build_numeric_surface_index(const Utf8Document& document) {
    NumericSurfaceIndex index;
    const auto& points = document.points;
    for (std::size_t point = 0; point < points.size(); ++point) {
        const auto value = points[point].value;
        const bool signed_start = (value == '+' || value == '-' || value == 0x2212) &&
                                  point + 1 < points.size() &&
                                  text::is_digit(points[point + 1].value);
        const bool unsigned_start =
            text::is_digit(value) &&
            (point == 0 || !text::is_lexical_numeric_boundary(points[point - 1].value));
        if (!signed_start && !unsigned_start)
            continue;
        const auto surface = scan_numeric_surface(document, point);
        if (!surface.valid() || surface.exponent == ExponentState::None)
            continue;
        index.ranges.push_back({point, surface.continuation_end});
        point = std::max(point, surface.continuation_end - 1);
    }
    return index;
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
