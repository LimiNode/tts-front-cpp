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
    auto suffix_begin = surface.continuation_end;
    if (suffix_begin < document.points.size() &&
        text::is_horizontal_space(document.points[suffix_begin].value)) {
        while (suffix_begin < document.points.size() &&
               text::is_horizontal_space(document.points[suffix_begin].value))
            ++suffix_begin;
    }
    auto suffix_end = suffix_begin;
    while (suffix_end < document.points.size() &&
           (text::is_letter(document.points[suffix_end].value) ||
            text::is_combining_mark(document.points[suffix_end].value)))
        ++suffix_end;
    if (suffix_end > suffix_begin)
        surface.continuation_end = suffix_end;
    else if (surface.continuation_end == surface.end && surface.end < document.points.size() &&
             (document.points[surface.end].value == '/' ||
              document.points[surface.end].value == '%')) {
        auto connector_end = surface.end + 1;
        while (connector_end < document.points.size() &&
               (text::is_letter(document.points[connector_end].value) ||
                text::is_combining_mark(document.points[connector_end].value) ||
                text::is_digit(document.points[connector_end].value)))
            ++connector_end;
        if (connector_end > surface.end + 1)
            surface.continuation_end = connector_end;
    }
    return surface;
}

CandidateSurface
scan_candidate_surface(const Utf8Document& document, std::size_t begin, CandidateSurfaceKind kind) {
    CandidateSurface candidate;
    candidate.kind = kind;
    candidate.begin = begin;
    candidate.numeric = scan_numeric_surface(document, begin);
    candidate.core_end = candidate.numeric.end;
    candidate.end = candidate.numeric.continuation_end;
    if (!candidate.valid())
        return candidate;

    // Scientific and encoded numeric surfaces may continue through a
    // connector and another numeric token.  Compute that continuation once
    // here so technical and malformed admission share the same boundary.
    if (candidate.numeric.exponent != ExponentState::None)
        candidate.end = scan_numeric_continuation_points(document, candidate.end, false);
    return candidate;
}

std::size_t scan_candidate_continuation(const Utf8Document& document,
                                        std::size_t begin,
                                        CandidateSurfaceKind kind) {
    const auto& points = document.points;
    auto end = begin;
    const auto lexical = [&](std::size_t point) {
        return text::is_letter(points[point].value) || text::is_digit(points[point].value) ||
               text::is_combining_mark(points[point].value) || points[point].value == '_';
    };
    while (end < points.size()) {
        while (end < points.size() && lexical(end))
            ++end;
        if (end >= points.size())
            break;
        const auto value = points[end].value;
        const bool connector =
            value == '-' || value == '+' || value == '.' || value == ',' || value == ':' ||
            value == '%' || text::is_range_connector(value) ||
            (kind != CandidateSurfaceKind::Numeric &&
             (value == '/' || value == '#' || value == '$')) ||
            (kind == CandidateSurfaceKind::Technical && (value == '=' || value == '*'));
        if (!connector)
            break;
        const bool punctuation_connector = value == '.' || value == ',' || value == ':';
        const bool technical_word_stem =
            end > begin || (begin > 0 && (text::is_letter(points[begin - 1].value) ||
                                          points[begin - 1].value == '_'));
        if (punctuation_connector &&
            (end + 1 >= points.size() || text::is_horizontal_space(points[end + 1].value)) &&
            !(kind == CandidateSurfaceKind::Technical && technical_word_stem))
            break;
        const bool has_lexical_stem = end > begin || (begin > 0 && lexical(begin - 1));
        if (end + 1 >= points.size()) {
            if (kind == CandidateSurfaceKind::Technical && has_lexical_stem)
                ++end;
            break;
        }
        const auto connector_point = end;
        ++end;
        const auto continuation_begin = end;
        while (end < points.size() && lexical(end))
            ++end;
        if (end == continuation_begin && kind == CandidateSurfaceKind::Technical &&
            has_lexical_stem) {
            // Technical tails may use a connector followed by horizontal
            // space and another numeric token (for example `RTX- 4090`).
            // Treat that as one unsupported surface, but only when the
            // numeric continuation is actually present.
            auto numeric_begin = end;
            while (numeric_begin < points.size() &&
                   text::is_horizontal_space(points[numeric_begin].value))
                ++numeric_begin;
            if (numeric_begin < points.size() && text::is_digit(points[numeric_begin].value)) {
                end = numeric_begin;
                while (end < points.size() && lexical(end))
                    ++end;
            } else if (numeric_begin > continuation_begin) {
                // Horizontal space ends a lexical continuation.  Keep the
                // connector attached to the malformed technical token while
                // leaving the following word as an independent token.
                end = continuation_begin;
                break;
            }
        }
        if (end == continuation_begin) {
            end = connector_point;
            break;
        }
    }
    return end;
}

std::size_t scan_spaced_numeric_continuation(const Utf8Document& document,
                                             std::size_t begin,
                                             CandidateSurfaceKind kind) {
    const auto& points = document.points;
    auto connector = begin;
    const auto connector_start = connector;
    while (connector < points.size() && text::is_horizontal_space(points[connector].value))
        ++connector;
    if (connector >= points.size())
        return begin;

    const auto value = points[connector].value;
    const bool compound_connector =
        kind != CandidateSurfaceKind::Numeric &&
        (value == '/' || value == '#' || value == '$' || value == '=' || value == '*' ||
         value == '+' || value == '-' || text::is_range_connector(value));
    if (!compound_connector)
        return begin;

    auto numeric_begin = connector + 1;
    const auto numeric_start = numeric_begin;
    while (numeric_begin < points.size() && text::is_horizontal_space(points[numeric_begin].value))
        ++numeric_begin;
    if (connector == connector_start && numeric_begin == numeric_start)
        return begin;
    if (numeric_begin >= points.size())
        return begin;
    const auto numeric = scan_numeric_surface(document, numeric_begin);
    return numeric.valid() ? numeric.continuation_end : begin;
}

bool NumericSurfaceIndex::contains(std::size_t point) const {
    const auto found = std::lower_bound(
        ranges.begin(),
        ranges.end(),
        point,
        [](const NumericSurfaceRange& range, std::size_t value) { return range.end <= value; });
    return found != ranges.end() && found->continuation_begin <= point && point < found->end;
}

bool NumericSurfaceIndex::overlaps(std::size_t begin, std::size_t end) const {
    const auto found = std::lower_bound(
        ranges.begin(),
        ranges.end(),
        begin,
        [](const NumericSurfaceRange& range, std::size_t value) { return range.end <= value; });
    return found != ranges.end() && found->surface_begin < end && begin < found->end;
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
            text::is_digit(value) && (point == 0 || !text::is_digit(points[point - 1].value));
        if (!signed_start && !unsigned_start)
            continue;
        const auto candidate =
            scan_candidate_surface(document, point, CandidateSurfaceKind::Technical);
        if (!candidate.valid() || candidate.numeric.exponent == ExponentState::None)
            continue;
        index.ranges.push_back({point, point + 1, candidate.end});
        point = std::max(point, candidate.end - 1);
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
