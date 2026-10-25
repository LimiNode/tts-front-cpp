#include "tts_front/core/numeric_scanner.hpp"

#include "tts_front/core/text/codepoint_classification.hpp"

#include <algorithm>

namespace tts_front::detail {

namespace {

bool is_continuation_connector(const std::uint32_t value, const CandidateSurfaceKind kind) {
    return value == '-' || value == '+' || value == '.' || value == ',' || value == ':' ||
           value == '%' || text::is_range_connector(value) ||
           (kind != CandidateSurfaceKind::Numeric &&
            (value == '/' || value == '#' || value == '$' || value == '=' || value == '*'));
}

bool is_spaced_connector(const std::uint32_t value, const CandidateSurfaceKind kind) {
    return kind != CandidateSurfaceKind::Numeric &&
           (value == '/' || value == '#' || value == '$' || value == '%' || value == '=' ||
            value == '*' || value == '+' || value == '-' || text::is_range_connector(value));
}

bool has_dollar_prefix(const Utf8Document& document, std::size_t begin) {
    const auto& points = document.points;
    while (begin > 0 && text::is_horizontal_space(points[begin - 1].value))
        --begin;
    while (begin > 0 && (text::is_digit(points[begin - 1].value) ||
                         points[begin - 1].value == '.' || points[begin - 1].value == ','))
        --begin;
    if (begin > 0 && (points[begin - 1].value == '+' || points[begin - 1].value == '-' ||
                      points[begin - 1].value == 0x2212)) {
        --begin;
        while (begin > 0 && text::is_horizontal_space(points[begin - 1].value))
            --begin;
    }
    return begin > 0 && points[begin - 1].value == '$';
}

AdmissionOutcome merge_admission_outcome(const AdmissionOutcome current,
                                         const AdmissionOutcome step) {
    if (step == AdmissionOutcome::None)
        return current;
    if (current == AdmissionOutcome::None || current == step)
        return step;
    return AdmissionOutcome::Compound;
}

AdmissionOutcome classify_admission_step(const std::size_t attached,
                                         const std::size_t spaced,
                                         const std::size_t punctuated,
                                         const std::size_t next) {
    if (next == punctuated && punctuated > std::max(attached, spaced))
        return AdmissionOutcome::Punctuated;
    if (next == spaced && spaced > attached)
        return AdmissionOutcome::Spaced;
    if (next == attached && attached > spaced)
        return AdmissionOutcome::Attached;
    return next > 0 ? AdmissionOutcome::Compound : AdmissionOutcome::None;
}

std::size_t scan_punctuated_continuation(const Utf8Document& document,
                                         const std::size_t begin,
                                         const CandidateSurfaceKind kind) {
    const auto& points = document.points;
    auto punctuation = begin;
    while (punctuation < points.size() &&
           (points[punctuation].value == '.' || points[punctuation].value == ',' ||
            points[punctuation].value == ':'))
        ++punctuation;
    if (punctuation == begin || punctuation >= points.size())
        return begin;

    const auto connector = points[punctuation].value;
    const auto connector_allowed = connector == '/' || connector == '#' || connector == '$' ||
                                   connector == '%' || connector == '=' || connector == '*' ||
                                   connector == '+' || connector == '-' ||
                                   text::is_range_connector(connector);
    if (!connector_allowed || kind == CandidateSurfaceKind::Numeric)
        return begin;

    auto next = punctuation + 1;
    while (next < points.size() && text::is_horizontal_space(points[next].value))
        ++next;
    if (next >= points.size())
        return begin;
    const auto numeric = scan_numeric_surface(document, next);
    if (numeric.valid())
        return numeric.continuation_end;
    const auto lexical = scan_candidate_continuation(document, punctuation, kind);
    return lexical > punctuation ? lexical : begin;
}

} // namespace

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
    // Combining marks belong to the numeric surface even though they are not
    // part of the digit grammar.  Keeping them in continuation_end lets all
    // admission paths apply the same byte-accurate boundary policy.
    if (surface.continuation_end < document.points.size() &&
        text::is_combining_mark(document.points[surface.continuation_end].value))
        surface.malformed_combining = true;
    while (surface.continuation_end < document.points.size() &&
           text::is_combining_mark(document.points[surface.continuation_end].value))
        ++surface.continuation_end;
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
    else if (suffix_begin < document.points.size() &&
             (document.points[suffix_begin].value == '/' ||
              document.points[suffix_begin].value == '%')) {
        auto connector_end = suffix_begin + 1;
        while (connector_end < document.points.size() &&
               (text::is_letter(document.points[connector_end].value) ||
                text::is_combining_mark(document.points[connector_end].value) ||
                text::is_digit(document.points[connector_end].value)))
            ++connector_end;
        if (connector_end > suffix_begin + 1)
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

    const auto combining_probe =
        std::max(candidate.end, scan_candidate_continuation(document, candidate.numeric.end, kind));
    candidate.has_combining = std::any_of(
        document.points.begin() + static_cast<std::ptrdiff_t>(candidate.numeric.end),
        document.points.begin() + static_cast<std::ptrdiff_t>(combining_probe),
        [](const Utf8CodePoint& point) { return text::is_combining_mark(point.value); });
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
        const bool connector = is_continuation_connector(value, kind);
        if (!connector)
            break;
        const bool punctuation_connector = value == '.' || value == ',' || value == ':';
        const bool technical_word_stem =
            kind == CandidateSurfaceKind::Technical &&
            (end > begin || (begin > 0 && (text::is_letter(points[begin - 1].value) ||
                                           points[begin - 1].value == '_')));
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
        // A typed candidate may continue through a connector followed by
        // horizontal space (for example `2024% 4` or `kg# foo`).  Keep the
        // whitespace inside the admitted source span; it is part of the
        // unsupported surface and must not be normalized independently.
        if (points[connector_point].value == '%')
            while (end < points.size() && text::is_horizontal_space(points[end].value))
                ++end;
        const auto continuation_begin = end;
        while (end < points.size() && lexical(end))
            ++end;
        // Once a compound connector has been admitted, a numeric token after
        // horizontal space remains part of the same unsupported surface.  Do
        // this in the shared scanner so typed language paths do not split
        // forms such as `$1,234%123 456` into a protected head and a rewritten
        // tail.  A plain space without a connector is intentionally handled
        // by scan_spaced_numeric_continuation and remains an independent
        // token (`$1 2`).
        const bool compound_connector =
            kind != CandidateSurfaceKind::Date && (value == '/' || value == '#' || value == '$' ||
                                                   value == '%' || value == '=' || value == '*');
        if (compound_connector && end < points.size() &&
            text::is_horizontal_space(points[end].value)) {
            auto spaced_numeric = end;
            while (spaced_numeric < points.size() &&
                   text::is_horizontal_space(points[spaced_numeric].value))
                ++spaced_numeric;
            if (spaced_numeric < points.size() && text::is_digit(points[spaced_numeric].value)) {
                const auto tail = scan_numeric_surface(document, spaced_numeric);
                if (tail.valid())
                    end = tail.continuation_end;
            }
        }
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
            if (kind != CandidateSurfaceKind::Numeric && end < points.size() &&
                is_continuation_connector(points[end].value, kind))
                continue;
            end = connector_point;
            break;
        }
    }
    return end;
}

NumericSurfaceRange scan_lexical_suffix(const Utf8Document& document, const std::size_t begin) {
    NumericSurfaceRange result{begin, begin, begin};
    while (result.continuation_begin < document.points.size() &&
           text::is_horizontal_space(document.points[result.continuation_begin].value))
        ++result.continuation_begin;
    result.end = result.continuation_begin;
    while (result.end < document.points.size() &&
           (text::is_letter(document.points[result.end].value) ||
            text::is_combining_mark(document.points[result.end].value)))
        ++result.end;
    return result;
}

std::size_t scan_currency_prefix_begin(const Utf8Document& document,
                                       const std::size_t numeric_begin) {
    const auto& points = document.points;
    if (numeric_begin > points.size())
        return numeric_begin;

    auto probe = numeric_begin;
    while (probe > 0 && text::is_horizontal_space(points[probe - 1].value))
        --probe;
    if (probe > 0 && (points[probe - 1].value == '+' || points[probe - 1].value == '-' ||
                      points[probe - 1].value == 0x2212)) {
        --probe;
        while (probe > 0 && text::is_horizontal_space(points[probe - 1].value))
            --probe;
    }
    if (probe == 0 || !text::is_currency_symbol(points[probe - 1].value))
        return numeric_begin;

    auto prefix = probe - 1;
    auto sign = prefix;
    while (sign > 0 && text::is_horizontal_space(points[sign - 1].value))
        --sign;
    if (sign > 0 && (points[sign - 1].value == '+' || points[sign - 1].value == '-' ||
                     points[sign - 1].value == 0x2212))
        prefix = sign - 1;
    return prefix;
}

std::size_t scan_currency_numeric_end(const Utf8Document& document, std::size_t numeric_begin) {
    const auto& points = document.points;
    if (numeric_begin < points.size() &&
        (points[numeric_begin].value == '+' || points[numeric_begin].value == '-' ||
         points[numeric_begin].value == 0x2212))
        ++numeric_begin;
    const auto digits_begin = numeric_begin;
    while (numeric_begin < points.size() && text::is_digit(points[numeric_begin].value))
        ++numeric_begin;
    if (numeric_begin == digits_begin)
        return digits_begin;
    while (numeric_begin + 1 < points.size() &&
           (points[numeric_begin].value == ',' || points[numeric_begin].value == '.') &&
           text::is_digit(points[numeric_begin + 1].value)) {
        ++numeric_begin;
        while (numeric_begin < points.size() && text::is_digit(points[numeric_begin].value))
            ++numeric_begin;
    }
    return numeric_begin;
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
    if (!is_spaced_connector(value, kind))
        return begin;
    if (kind == CandidateSurfaceKind::Currency && value == '$' &&
        has_dollar_prefix(document, begin))
        return begin;

    auto numeric_begin = connector + 1;
    const auto numeric_start = numeric_begin;
    while (numeric_begin < points.size() && text::is_horizontal_space(points[numeric_begin].value))
        ++numeric_begin;
    if (kind == CandidateSurfaceKind::Date && numeric_begin != numeric_start && value != '%')
        return begin;
    if (connector == connector_start && numeric_begin == numeric_start)
        return begin;
    if (numeric_begin >= points.size())
        return begin;
    // A combining mark immediately after a connector is part of the same
    // unsupported compound. If a numeric token follows it, consume that token
    // atomically; otherwise retain the connector and mark as the boundary.
    auto combining_end = numeric_begin;
    while (combining_end < points.size() && text::is_combining_mark(points[combining_end].value))
        ++combining_end;
    if (combining_end > numeric_begin) {
        if (combining_end < points.size() && text::is_digit(points[combining_end].value)) {
            const auto tail = scan_numeric_surface(document, combining_end);
            return tail.valid() ? tail.continuation_end : combining_end;
        }
        return combining_end;
    }
    const auto numeric = scan_numeric_surface(document, numeric_begin);
    if (numeric.valid())
        return numeric.continuation_end;
    if (kind != CandidateSurfaceKind::Numeric) {
        const auto lexical = scan_candidate_continuation(document, connector, kind);
        if (lexical > connector)
            return lexical;
    }
    return begin;
}

AdmissionResult scan_admission(const Utf8Document& document,
                               const std::size_t begin,
                               const CandidateSurfaceKind kind) {
    auto end = begin;
    AdmissionResult result{kind, begin, begin, begin, AdmissionOutcome::None};
    const bool dollar_prefixed_currency =
        kind == CandidateSurfaceKind::Currency && has_dollar_prefix(document, begin);
    for (;;) {
        if (kind != CandidateSurfaceKind::Date && end < document.points.size() &&
            text::is_horizontal_space(document.points[end].value)) {
            auto connector = end;
            while (connector < document.points.size() &&
                   text::is_horizontal_space(document.points[connector].value))
                ++connector;
            if (connector >= document.points.size() ||
                !is_spaced_connector(document.points[connector].value, kind))
                break;
            if (dollar_prefixed_currency && document.points[connector].value == '$')
                break;
            // A spaced sign/range connector introduces a new signed token
            // (`1 kg -2 kg`), unlike a compound postfix such as `kg #3`.
            // Keep the two supported measurements independent.
            if (document.points[connector].value == '+' ||
                document.points[connector].value == '-' ||
                text::is_range_connector(document.points[connector].value))
                break;
        }
        const auto attached = scan_candidate_continuation(document, end, kind);
        const auto spaced = scan_spaced_numeric_continuation(document, end, kind);
        const auto next = std::max(attached, spaced);
        if (next <= end)
            break;
        const auto step = classify_admission_step(attached, spaced, 0, next);
        result.outcome = merge_admission_outcome(result.outcome, step);
        end = next;
    }
    result.end = end;
    return result;
}

AdmissionResult scan_admission(const Utf8Document& document,
                               const CandidateSurface& candidate,
                               CandidateSurfaceKind kind) {
    if (!candidate.valid())
        return {kind, candidate.begin, candidate.begin, candidate.begin, AdmissionOutcome::None};

    auto end = candidate.end;
    AdmissionResult result{
        kind, candidate.begin, candidate.end, candidate.end, AdmissionOutcome::None};
    if (candidate.numeric.malformed_combining) {
        auto suffix = end;
        while (suffix < document.points.size() &&
               text::is_horizontal_space(document.points[suffix].value))
            ++suffix;
        if (suffix < document.points.size() &&
            is_spaced_connector(document.points[suffix].value, kind))
            end = suffix + 1;
    }
    for (;;) {
        const auto attached = scan_candidate_continuation(document, end, kind);
        const auto spaced = scan_spaced_numeric_continuation(document, end, kind);
        const auto punctuated = scan_punctuated_continuation(document, end, kind);
        auto next = std::max({attached, spaced, punctuated});
        if (end > candidate.begin) {
            const auto previous = document.points[end - 1].value;
            if (is_spaced_connector(previous, kind)) {
                next = std::max(next, scan_candidate_continuation(document, end - 1, kind));
                next = std::max(next, scan_spaced_numeric_continuation(document, end - 1, kind));
            }
        }
        if (kind == CandidateSurfaceKind::Percent) {
            auto numeric_begin = next > end ? next : end;
            while (numeric_begin < document.points.size() &&
                   text::is_horizontal_space(document.points[numeric_begin].value))
                ++numeric_begin;
            if (numeric_begin < document.points.size() &&
                text::is_digit(document.points[numeric_begin].value)) {
                const auto tail = scan_numeric_surface(document, numeric_begin);
                if (tail.valid())
                    next = std::max(next, tail.continuation_end);
            }
        }
        if (next <= end)
            break;
        const auto step = classify_admission_step(attached, spaced, punctuated, next);
        result.outcome = merge_admission_outcome(result.outcome, step);
        end = next;
    }
    result.end = end;
    return result;
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
