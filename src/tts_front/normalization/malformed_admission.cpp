#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/normalization/admission.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tts_front::detail {

using CodePoint = Utf8CodePoint;

// Intermediate mixed per-candidate state used while the admission phases are
// still being separated. It includes language-conditioned observations from
// AdmissionRules and is not a neutral/core scanner result. Keeping boundary
// movement and structural facts together prevents a later refactor from
// silently updating one without the other.
struct CandidateAnalysis {
    std::size_t end_index = 0;
    std::size_t separators = 0;
    bool has_percent = false;
    bool has_dot_decimal = false;
    bool percent_attached_to_numeric = false;
    bool has_range_connector = false;
    bool has_unsupported_numeric_connector = false;
    bool malformed_grouped = false;
    bool malformed_compound = false;
    bool grouped_seen = false;
};

struct PreservationFacts {
    bool embedded = false;
    bool attached_lexical_suffix = false;
    bool comma_group_followed_by_word = false;
    bool valid_english_comma_group = false;
    bool unsupported_numeric_connector = false;
    bool invalid_percent = false;
    bool malformed_english_comma_group = false;
    bool currency_prefix = false;
    bool invalid_currency_prefix = false;
    bool valid_russian_date = false;
    bool valid_currency_group_surface = false;
    bool already_preserved = false;
};

bool requires_preservation(const CandidateAnalysis& analysis, const PreservationFacts& facts) {
    const bool malformed_surface =
        analysis.separators >= 2 || facts.embedded || facts.attached_lexical_suffix ||
        (facts.comma_group_followed_by_word && !facts.valid_english_comma_group) ||
        facts.unsupported_numeric_connector || facts.invalid_percent ||
        facts.malformed_english_comma_group || analysis.malformed_grouped ||
        analysis.malformed_compound || (facts.currency_prefix && facts.invalid_currency_prefix);
    const bool supported_russian_date = facts.valid_russian_date && !facts.embedded;
    const bool supported_english_group =
        facts.valid_english_comma_group && !facts.embedded && !facts.attached_lexical_suffix &&
        !(facts.currency_prefix && analysis.has_percent) && !analysis.malformed_compound &&
        facts.valid_currency_group_surface;
    return malformed_surface && !supported_russian_date && !supported_english_group &&
           !facts.already_preserved;
}

bool valid_date(int day, int month, int year) {
    if (month < 1 || month > 12 || day < 1)
        return false;
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days[month];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        limit = 29;
    return day <= limit;
}

std::size_t scan_nonterminal_ellipsis(const std::vector<CodePoint>& points, std::size_t begin) {
    if (begin + 1 >= points.size() || points[begin].value != '.' || points[begin + 1].value != '.')
        return begin;
    auto continuation = begin;
    while (continuation < points.size() && points[continuation].value == '.')
        ++continuation;
    if (continuation >= points.size() ||
        (!text::is_letter(points[continuation].value) &&
         !text::is_digit(points[continuation].value) && points[continuation].value != '/' &&
         points[continuation].value != '#' && points[continuation].value != '$'))
        return begin;
    while (continuation < points.size() &&
           (text::is_letter(points[continuation].value) ||
            text::is_digit(points[continuation].value) || points[continuation].value == '/' ||
            points[continuation].value == '#' || points[continuation].value == '$'))
        ++continuation;
    return continuation;
}

MappedText protect_malformed_numeric_candidates(MappedText text,
                                                WarningSink& warnings,
                                                std::vector<ProtectedSpan>& protected_spans,
                                                const AdmissionRules& rules) {
    const Utf8Document document(text.text);
    if (!document.valid)
        return text;
    const auto& points = document.points;
    std::vector<SourceEdit> edits;
    // Protected spans are emitted as well-formed control-byte markers. Carry
    // one forward state instead of searching backwards through the text for
    // every codepoint; this keeps the candidate admission pass linear.
    bool inside_marker = false;
    for (std::size_t index = 0; index < points.size();) {
        if (points[index].value == 0x01) {
            inside_marker = true;
            ++index;
            continue;
        }
        if (inside_marker) {
            if (points[index].value == 0x02)
                inside_marker = false;
            ++index;
            continue;
        }
        if (overlaps_preserved_range(text, points[index].offset, points[index].offset + 1)) {
            ++index;
            continue;
        }
        const auto currency_context_begin = scan_currency_prefix_begin(document, index);
        const bool currency_context = currency_context_begin != index;
        const auto candidate_surface = scan_candidate_surface(document, index);
        const auto& surface = candidate_surface.numeric;
        const auto spaced_continuation =
            scan_spaced_numeric_continuation(document, surface.end, CandidateSurfaceKind::Numeric);
        const bool positive_numeric = [&] {
            if (points[index].value != '+' || index + 1 >= points.size() ||
                !text::is_digit(points[index + 1].value))
                return false;
            auto probe = index + 1;
            while (probe < points.size() && text::is_digit(points[probe].value))
                ++probe;
            if (probe >= points.size())
                return false;
            const auto continuation = points[probe].value;
            // An incomplete scientific marker is still a signed numeric
            // candidate.  Admit it atomically so `+1e`, `+1e+` and
            // `+1e+x` cannot fall through to a partial rewrite of `1`.
            if (continuation == 'e' || continuation == 'E')
                return true;
            if (text::is_numeric_connector(continuation) && continuation != '-' &&
                continuation != '/')
                return true;
            if (text::is_horizontal_space(points[probe].value)) {
                while (probe < points.size() && text::is_horizontal_space(points[probe].value))
                    ++probe;
                // A second digit group is characteristic of a phone number;
                // leave it to the dedicated phone scanner.  A lexical unit
                // after the sign remains a malformed numeric candidate.
                return probe < points.size() && text::is_letter(points[probe].value);
            }
            return false;
        }();
        const bool starts_phone = points[index].value == '+' && index + 1 < points.size() &&
                                  text::is_digit(points[index + 1].value) && !currency_context &&
                                  !positive_numeric;
        const bool signed_number = points[index].value == '-' || points[index].value == 0x2212;
        const bool starts_number =
            text::is_digit(points[index].value) || positive_numeric ||
            (signed_number && index + 1 < points.size() && text::is_digit(points[index + 1].value));
        if (!starts_number && !starts_phone) {
            ++index;
            continue;
        }
        const auto begin = points[index].offset;
        // Parentheses are ordinary sentence punctuation when they enclose a
        // numeric expression.  Treat a closing parenthesis as a boundary only
        // when the candidate was introduced by an opening parenthesis (or by
        // an opening parenthesis immediately followed by a currency sign).
        // An unmatched suffix such as `1.2)` remains fail-closed below.
        const bool parenthesized_prefix =
            (index > 0 && points[index - 1].value == '(') ||
            (index > 1 && points[index - 2].value == '(' &&
             (points[index - 1].value == '$' || points[index - 1].value == 0x20ac ||
              points[index - 1].value == 0xa3 || points[index - 1].value == 0xa5));
        CandidateAnalysis analysis;
        analysis.end_index = index + 1;
        if (surface.exponent != ExponentState::None && candidate_surface.end > analysis.end_index)
            analysis.end_index = candidate_surface.end;
        if (!currency_context && spaced_continuation > surface.end) {
            analysis.malformed_compound = true;
            analysis.end_index = spaced_continuation;
        }
        if (surface.exponent != ExponentState::None)
            analysis.malformed_compound = true;
        auto initial_digit_index = index + (signed_number || positive_numeric ? 1 : 0);
        const auto initial_digit_begin = initial_digit_index;
        while (initial_digit_index < points.size() &&
               text::is_digit(points[initial_digit_index].value))
            ++initial_digit_index;
        const auto initial_digit_count = initial_digit_index - initial_digit_begin;
        // A leading sign is not part of the supported clock-time grammar. Keep
        // the complete signed expression atomic so the time normalizer cannot
        // match the unsigned suffix after this admission pass.
        const bool signed_time = signed_number && initial_digit_count > 0 &&
                                 initial_digit_index + 1 < points.size() &&
                                 points[initial_digit_index].value == ':' &&
                                 text::is_digit(points[initial_digit_index + 1].value);
        if (points[index].value == 0x2212)
            analysis.malformed_compound = true;
        if (starts_phone) {
            while (analysis.end_index < points.size()) {
                if (text::is_digit(points[analysis.end_index].value)) {
                    ++analysis.end_index;
                    continue;
                }
                if (points[analysis.end_index].value == '-' ||
                    points[analysis.end_index].value == '/') {
                    ++analysis.separators;
                    ++analysis.end_index;
                    continue;
                }
                if (points[analysis.end_index].value == '(' ||
                    points[analysis.end_index].value == ')') {
                    ++analysis.end_index;
                    continue;
                }
                if (points[analysis.end_index].value == ' ' ||
                    points[analysis.end_index].value == '\t') {
                    auto probe = analysis.end_index + 1;
                    while (probe < points.size() &&
                           (points[probe].value == ' ' || points[probe].value == '\t'))
                        ++probe;
                    if (probe < points.size() && points[probe].value == '(') {
                        auto digit_probe = probe + 1;
                        while (digit_probe < points.size() && (points[digit_probe].value == ' ' ||
                                                               points[digit_probe].value == '\t'))
                            ++digit_probe;
                        if (digit_probe < points.size() &&
                            text::is_digit(points[digit_probe].value)) {
                            analysis.end_index = probe;
                            continue;
                        }
                    }
                    if (probe < points.size() && text::is_digit(points[probe].value)) {
                        analysis.end_index = probe;
                        continue;
                    }
                }
                break;
            }
        } else {
            while (analysis.end_index < points.size()) {
                const auto value = points[analysis.end_index].value;
                if (value == ' ' || value == '\t' || value == '\n' || value == '\r') {
                    auto probe = analysis.end_index + 1;
                    while (probe < points.size() &&
                           (points[probe].value == ' ' || points[probe].value == '\t' ||
                            points[probe].value == '\n' || points[probe].value == '\r'))
                        ++probe;
                    if (probe < points.size() && (points[probe].value == '%' ||
                                                  text::is_range_connector(points[probe].value))) {
                        analysis.end_index = probe;
                        continue;
                    }
                    if (analysis.has_range_connector && probe < points.size() &&
                        text::is_digit(points[probe].value)) {
                        analysis.end_index = probe;
                        continue;
                    }
                    if (probe < points.size() && text::is_digit(points[probe].value)) {
                        auto group_end = probe;
                        while (group_end < points.size() && text::is_digit(points[group_end].value))
                            ++group_end;
                        const auto group_size = group_end - probe;
                        const bool short_following_group =
                            analysis.grouped_seen && group_size < 3 &&
                            (group_end == points.size() ||
                             text::is_lexical_numeric_boundary(points[group_end].value));
                        if (analysis.separators == 0 && initial_digit_count <= 3 &&
                            (group_size >= 3 || short_following_group)) {
                            analysis.grouped_seen = true;
                            analysis.malformed_grouped =
                                analysis.malformed_grouped || group_size != 3;
                            if (group_end < points.size() &&
                                text::is_lexical_numeric_boundary(points[group_end].value))
                                analysis.malformed_grouped = true;
                            analysis.end_index = group_end;
                            continue;
                        }
                    }
                    break;
                }
                if ((value == '.' || value == ',' || value == ':') &&
                    (analysis.end_index + 1 >= points.size() ||
                     !text::is_digit(points[analysis.end_index + 1].value))) {
                    if (value == '.' && analysis.end_index + 1 < points.size() &&
                        points[analysis.end_index + 1].value == '.') {
                        auto probe = analysis.end_index;
                        while (probe < points.size() && points[probe].value == '.')
                            ++probe;
                        if (probe < points.size() && text::is_digit(points[probe].value)) {
                            analysis.malformed_compound = true;
                            analysis.end_index = probe;
                            continue;
                        }
                    }
                    if (analysis.separators != 0 && analysis.end_index + 1 < points.size() &&
                        (text::is_numeric_connector(points[analysis.end_index + 1].value) ||
                         text::is_digit(points[analysis.end_index + 1].value))) {
                        analysis.malformed_compound = true;
                        ++analysis.end_index;
                        while (analysis.end_index < points.size() &&
                               (text::is_numeric_connector(points[analysis.end_index].value) ||
                                text::is_digit(points[analysis.end_index].value)))
                            ++analysis.end_index;
                        continue;
                    }
                    break;
                }
                if (value == ')' && parenthesized_prefix)
                    break;
                if (value == '[' || value == ']')
                    break;
                if (text::is_numeric_connector(value)) {
                    if (rules.dotted_dates && value == '.' &&
                        analysis.end_index + 1 < points.size() &&
                        text::is_digit(points[analysis.end_index + 1].value))
                        analysis.has_dot_decimal = true;
                    if (analysis.grouped_seen && (value == '.' || value == ',' || value == ':'))
                        analysis.malformed_compound = true;
                    if (value == '%') {
                        analysis.has_percent = true;
                        const auto probe = analysis.end_index + 1;
                        if (probe < points.size() &&
                            (text::is_lexical_numeric_boundary(points[probe].value) ||
                             points[probe].value == '%'))
                            analysis.percent_attached_to_numeric = true;
                    } else
                        ++analysis.separators;
                    if (text::is_range_connector(value))
                        analysis.has_range_connector = true;
                    if (!text::is_supported_numeric_separator(value) &&
                        analysis.end_index + 1 < points.size() &&
                        points[analysis.end_index + 1].value != ' ' &&
                        points[analysis.end_index + 1].value != '\t' &&
                        points[analysis.end_index + 1].value != '\n' &&
                        points[analysis.end_index + 1].value != '\r')
                        analysis.has_unsupported_numeric_connector = true;
                    ++analysis.end_index;
                    continue;
                }
                if (text::is_digit(value)) {
                    ++analysis.end_index;
                    continue;
                }
                break;
            }
        }
        // Currency-prefixed malformed forms (`$ 1`, `$-1`, `$−1`) enter this
        // pass at their numeric component.  Once that context is known, use
        // the same typed endpoint as supported currency cores so an attached
        // or spaced connector tail cannot be rewritten independently.
        if (currency_context) {
            const auto currency_end =
                scan_admission(document, analysis.end_index, CandidateSurfaceKind::Currency).end;
            if (currency_end > analysis.end_index) {
                analysis.malformed_compound = true;
                analysis.end_index = currency_end;
            }
        }
        // Use the shared contextual scanner to catch a combining-mark tail
        // after a connector or lexical short-unit surface. This keeps
        // `2 м́1`, `2 м́/1`, and `2 /́3` atomic without affecting ordinary
        // decomposed words such as `2 café`.
        const auto contextual_end =
            scan_admission(document, candidate_surface, CandidateSurfaceKind::Contextual).end;
        const auto combining_connector_surface = [&] {
            const auto first =
                points.begin() + static_cast<std::ptrdiff_t>(candidate_surface.numeric.end);
            const auto last = points.begin() + static_cast<std::ptrdiff_t>(candidate_surface.end);
            const auto has_combining = std::any_of(first, last, [](const auto& point) {
                return text::is_combining_mark(point.value);
            });
            const auto has_connector = std::any_of(first, last, [](const auto& point) {
                return point.value == '/' || point.value == '#' || point.value == '$' ||
                       point.value == '%' || point.value == '=' || point.value == '*' ||
                       text::is_range_connector(point.value);
            });
            return has_combining && has_connector;
        }();
        if (combining_connector_surface && candidate_surface.end > analysis.end_index) {
            analysis.malformed_compound = true;
            analysis.end_index = candidate_surface.end;
        }
        if (contextual_end > candidate_surface.end) {
            const auto has_combining = std::any_of(
                points.begin() + static_cast<std::ptrdiff_t>(candidate_surface.numeric.end),
                points.begin() + static_cast<std::ptrdiff_t>(contextual_end),
                [](const auto& point) { return text::is_combining_mark(point.value); });
            if (has_combining) {
                analysis.malformed_compound = true;
                analysis.end_index = contextual_end;
            }
        }
        // Signed numeric candidates are already fail-closed when their
        // lexical continuation contains a combining mark. Extend the same
        // atomic span across a spaced suffix so a mixed path cannot leave
        // the mark outside the diagnostic (for example `+2.5 м́`).
        if ((positive_numeric || signed_number) && analysis.end_index < points.size()) {
            const auto suffix = scan_lexical_suffix(document, analysis.end_index);
            const auto has_combining =
                std::any_of(points.begin() + static_cast<std::ptrdiff_t>(suffix.continuation_begin),
                            points.begin() + static_cast<std::ptrdiff_t>(suffix.end),
                            [](const auto& point) { return text::is_combining_mark(point.value); });
            if (has_combining) {
                analysis.malformed_compound = true;
                analysis.end_index = suffix.end;
                const auto continuation = scan_candidate_continuation(
                    document, analysis.end_index, CandidateSurfaceKind::Contextual);
                analysis.end_index = std::max(analysis.end_index, continuation);
            }
        }
        const auto numeric_continuation = scan_candidate_continuation(
            document, candidate_surface.numeric.end, CandidateSurfaceKind::Numeric);
        const auto numeric_has_combining =
            std::any_of(points.begin() + static_cast<std::ptrdiff_t>(candidate_surface.numeric.end),
                        points.begin() + static_cast<std::ptrdiff_t>(numeric_continuation),
                        [](const auto& point) { return text::is_combining_mark(point.value); });
        if (numeric_has_combining) {
            analysis.malformed_compound = true;
            analysis.end_index = std::max(analysis.end_index, numeric_continuation);
            const auto suffix = scan_lexical_suffix(document, analysis.end_index);
            if (suffix.end > suffix.continuation_begin)
                analysis.end_index = suffix.end;
            analysis.end_index = std::max(
                analysis.end_index,
                scan_admission(document, analysis.end_index, CandidateSurfaceKind::Contextual).end);
        }
        if (surface.malformed_combining ||
            (candidate_surface.has_combining && analysis.end_index < points.size() &&
             text::is_combining_mark(points[analysis.end_index].value))) {
            analysis.malformed_compound = true;
            auto combining_end = analysis.end_index;
            while (combining_end < points.size() &&
                   text::is_combining_mark(points[combining_end].value))
                ++combining_end;
            auto suffix_begin = combining_end;
            while (suffix_begin < points.size() &&
                   text::is_horizontal_space(points[suffix_begin].value))
                ++suffix_begin;
            auto suffix_end = suffix_begin;
            while (suffix_end < points.size() &&
                   (text::is_letter(points[suffix_end].value) ||
                    text::is_combining_mark(points[suffix_end].value)))
                ++suffix_end;
            analysis.end_index = std::max(analysis.end_index, suffix_end);
        }
        if (numeric_has_combining || surface.malformed_combining ||
            candidate_surface.has_combining) {
            while (analysis.end_index < points.size() &&
                   text::is_combining_mark(points[analysis.end_index].value))
                ++analysis.end_index;
            analysis.end_index = std::max(
                analysis.end_index,
                scan_admission(document, analysis.end_index, CandidateSurfaceKind::Contextual).end);
        }
        if ((positive_numeric || points[index].value == 0x2212) &&
            analysis.end_index < points.size()) {
            if (surface.exponent != ExponentState::None) {
                analysis.malformed_compound = true;
                analysis.end_index = std::max(analysis.end_index, surface.end);
                if (surface.exponent == ExponentState::Complete) {
                    analysis.end_index =
                        scan_numeric_continuation_points(document, analysis.end_index, false);
                    while (analysis.end_index < points.size()) {
                        const auto value = points[analysis.end_index].value;
                        if (text::is_horizontal_space(value)) {
                            auto probe = analysis.end_index;
                            while (probe < points.size() &&
                                   text::is_horizontal_space(points[probe].value))
                                ++probe;
                            auto word_end = probe;
                            while (word_end < points.size() &&
                                   text::is_letter(points[word_end].value))
                                ++word_end;
                            if (word_end == probe)
                                break;
                            analysis.end_index = word_end;
                            continue;
                        }
                        if (text::is_digit(value) || text::is_letter(value) ||
                            text::is_combining_mark(value) || value == '%' || value == '#' ||
                            value == '$' || value == '/' || value == '+' || value == '-' ||
                            text::is_range_connector(value)) {
                            ++analysis.end_index;
                            continue;
                        }
                        break;
                    }
                }
            }
            auto suffix_begin = analysis.end_index;
            while (suffix_begin < points.size() &&
                   text::is_horizontal_space(points[suffix_begin].value))
                ++suffix_begin;
            auto suffix_end = suffix_begin;
            while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                ++suffix_end;
            if (suffix_end > suffix_begin) {
                analysis.malformed_compound = true;
                analysis.end_index = suffix_end;
            }
        }
        if (points[index].value == 0x2212 && analysis.end_index < points.size()) {
            auto suffix_begin = analysis.end_index;
            while (suffix_begin < points.size() &&
                   text::is_horizontal_space(points[suffix_begin].value))
                ++suffix_begin;
            auto suffix_end = suffix_begin;
            while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                ++suffix_end;
            if (suffix_end > suffix_begin) {
                analysis.malformed_compound = true;
                analysis.end_index = suffix_end;
            } else if (suffix_begin == analysis.end_index && suffix_begin < points.size() &&
                       text::is_letter(points[suffix_begin].value)) {
                while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                    ++suffix_end;
                analysis.malformed_compound = true;
                analysis.end_index = suffix_end;
            }
        }
        if (initial_digit_count > 9 && analysis.end_index < points.size() &&
            text::is_lexical_numeric_boundary(points[analysis.end_index].value)) {
            while (analysis.end_index < points.size() &&
                   (text::is_lexical_numeric_boundary(points[analysis.end_index].value) ||
                    text::is_combining_mark(points[analysis.end_index].value)))
                ++analysis.end_index;
        }
        if (initial_digit_count > 9 && analysis.end_index < points.size() &&
            (points[analysis.end_index].value == ' ' || points[analysis.end_index].value == '\t')) {
            auto probe = analysis.end_index;
            while (probe < points.size() &&
                   (points[probe].value == ' ' || points[probe].value == '\t'))
                ++probe;
            while (probe < points.size() && text::is_letter(points[probe].value))
                ++probe;
            if (probe > analysis.end_index)
                analysis.end_index = probe;
        }
        if (currency_context && analysis.has_percent)
            analysis.end_index =
                scan_numeric_continuation_points(document, analysis.end_index, false);
        if (rules.dotted_dates && analysis.has_dot_decimal && analysis.has_percent) {
            analysis.malformed_compound = true;
            const auto continuation =
                scan_numeric_continuation_points(document, analysis.end_index, false);
            if (continuation != analysis.end_index)
                analysis.end_index = continuation;
            auto suffix_end = analysis.end_index;
            while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                ++suffix_end;
            if (suffix_end > analysis.end_index) {
                analysis.malformed_compound = true;
                analysis.end_index = suffix_end;
            }
            if (analysis.end_index < points.size() && (points[analysis.end_index].value == '/' ||
                                                       points[analysis.end_index].value == '#' ||
                                                       points[analysis.end_index].value == '$')) {
                ++analysis.end_index;
                while (analysis.end_index < points.size() &&
                       (text::is_digit(points[analysis.end_index].value) ||
                        text::is_letter(points[analysis.end_index].value) ||
                        text::is_combining_mark(points[analysis.end_index].value)))
                    ++analysis.end_index;
            }
        }
        if (analysis.has_percent && analysis.end_index < points.size() &&
            text::is_lexical_numeric_boundary(points[analysis.end_index].value)) {
            while (analysis.end_index < points.size() &&
                   (text::is_lexical_numeric_boundary(points[analysis.end_index].value) ||
                    text::is_combining_mark(points[analysis.end_index].value)))
                ++analysis.end_index;
            analysis.percent_attached_to_numeric = true;
        }
        // A dotted foreign percentage immediately followed by a technical
        // identifier is one unsupported candidate (for example
        // `1.5%RTX-4090`).  The technical pass deliberately leaves that
        // identifier visible so the warning can cover the complete source
        // span; consume its lexical tail here instead of emitting two
        // diagnostics.
        if (analysis.has_percent && analysis.end_index < points.size()) {
            const auto continuation = scan_candidate_continuation(
                document, analysis.end_index, CandidateSurfaceKind::Technical);
            if (continuation != analysis.end_index) {
                analysis.malformed_compound = true;
                analysis.end_index = continuation;
            }
        }
        // A terminal currency/hash connector is not a supported postfix
        // notation. Preserve it atomically instead of letting the generic
        // number pass rewrite only the leading digits (`1$` -> `one$`).
        if (analysis.end_index < points.size() &&
            (points[analysis.end_index].value == '$' || points[analysis.end_index].value == '#')) {
            analysis.malformed_compound = true;
            ++analysis.end_index;
            while (analysis.end_index < points.size() &&
                   (text::is_digit(points[analysis.end_index].value) ||
                    text::is_letter(points[analysis.end_index].value) ||
                    text::is_combining_mark(points[analysis.end_index].value)))
                ++analysis.end_index;
        }
        // A clock-like expression followed by percent is not an English or
        // Russian percent candidate (`1:02%`). Keep the whole expression.
        if (analysis.has_percent &&
            std::any_of(points.begin() + static_cast<std::ptrdiff_t>(index),
                        points.begin() + static_cast<std::ptrdiff_t>(analysis.end_index),
                        [](const CodePoint& point) { return point.value == ':'; }))
            analysis.malformed_compound = true;
        if (signed_time)
            analysis.malformed_compound = true;
        // A recognized unit/currency stem followed by an attached lexical
        // suffix is one malformed candidate.  Letting the generic number
        // pass see only the leading digits would otherwise produce partial
        // rewrites such as `one kgfoo` or `один руб.foo`.
        const auto lexical_suffix = scan_lexical_suffix(document, analysis.end_index);
        if (lexical_suffix.end > lexical_suffix.continuation_begin) {
            const auto suffix_begin = lexical_suffix.continuation_begin;
            const auto word_end = lexical_suffix.end;
            auto suffix_end = word_end;
            // Include a dotted continuation such as `руб.foo`; a final
            // period by itself remains a valid Russian abbreviation.
            if (suffix_end < points.size() && points[suffix_end].value == '.' &&
                suffix_end + 1 < points.size() && text::is_letter(points[suffix_end + 1].value)) {
                ++suffix_end;
                while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                    ++suffix_end;
            }
            const auto unit =
                text.text.substr(points[suffix_begin].offset,
                                 points[suffix_end - 1].offset + points[suffix_end - 1].length -
                                     points[suffix_begin].offset);
            bool known_stem = false;
            bool valid_surface = false;
            if (rules.classify_surface != nullptr)
                rules.classify_surface(unit, document, suffix_begin, known_stem, valid_surface);
            if (known_stem && (!valid_surface || suffix_end != word_end)) {
                analysis.malformed_compound = true;
                analysis.end_index = suffix_end;
                // Once a language-owned suffix is rejected, consume any
                // attached or spaced continuation through the shared scanner
                // before the next normalizer can see a trailing number.
                analysis.end_index =
                    std::max(analysis.end_index,
                             scan_candidate_continuation(
                                 document, analysis.end_index, CandidateSurfaceKind::Contextual));
                analysis.end_index =
                    std::max(analysis.end_index,
                             scan_spaced_numeric_continuation(
                                 document, analysis.end_index, CandidateSurfaceKind::Contextual));
            }
            // An abbreviation ellipsis is terminal only when it is
            // followed by ordinary punctuation or the end of input.
            // `руб...foo` and `руб.../2` are unsupported continuations
            // and must remain one atomic candidate.
            if (known_stem && suffix_end == word_end) {
                const auto continuation = scan_nonterminal_ellipsis(points, suffix_end);
                if (continuation != suffix_end) {
                    analysis.malformed_compound = true;
                    analysis.end_index = continuation;
                }
            }
        }
        const bool previous_is_connector =
            index > 0 && (points[index - 1].value == '-' || points[index - 1].value == '/' ||
                          points[index - 1].value == '+' || points[index - 1].value == '=');
        const bool embedded =
            index > 0 && (text::is_lexical_numeric_boundary(points[index - 1].value) ||
                          (previous_is_connector && index > 1 &&
                           text::is_lexical_numeric_boundary(points[index - 2].value)));
        if (embedded) {
            std::size_t grouped_end = analysis.end_index;
            while (grouped_end < points.size()) {
                auto probe = grouped_end;
                while (probe < points.size() &&
                       (points[probe].value == ' ' || points[probe].value == '\t' ||
                        points[probe].value == '\n' || points[probe].value == '\r'))
                    ++probe;
                const auto group_begin = probe;
                while (probe < points.size() && text::is_digit(points[probe].value))
                    ++probe;
                if (probe == group_begin)
                    break;
                while (probe < points.size()) {
                    const auto value = points[probe].value;
                    if ((value == '.' || value == ',' || value == ':') &&
                        probe + 1 < points.size() && text::is_digit(points[probe + 1].value)) {
                        probe += 1;
                        while (probe < points.size() && text::is_digit(points[probe].value))
                            ++probe;
                        continue;
                    }
                    if (value == '%') {
                        ++probe;
                        continue;
                    }
                    break;
                }
                grouped_end = probe;
            }
            analysis.end_index = grouped_end;
        }
        if (analysis.malformed_grouped) {
            while (analysis.end_index < points.size() &&
                   (text::is_lexical_numeric_boundary(points[analysis.end_index].value) ||
                    text::is_combining_mark(points[analysis.end_index].value)))
                ++analysis.end_index;
        }
        const auto end = analysis.end_index == 0 ? begin
                                                 : points[analysis.end_index - 1].offset +
                                                       points[analysis.end_index - 1].length;
        const auto candidate = text.text.substr(begin, end - begin);
        const auto currency_prefix_begin = scan_currency_prefix_begin(document, index);
        const bool currency_prefix = currency_prefix_begin != index;
        const bool signed_currency_prefix =
            currency_prefix && (points[currency_prefix_begin].value == '+' ||
                                points[currency_prefix_begin].value == '-' ||
                                points[currency_prefix_begin].value == 0x2212);
        auto currency_symbol = currency_prefix_begin + (signed_currency_prefix ? 1 : 0);
        while (currency_symbol < index && text::is_horizontal_space(points[currency_symbol].value))
            ++currency_symbol;
        const auto currency_probe = currency_prefix ? currency_symbol + 1 : index;
        const bool attached_lexical_suffix =
            analysis.separators != 0 && analysis.end_index < points.size() &&
            text::is_lexical_numeric_boundary(points[analysis.end_index].value);
        const bool valid_english_comma_group =
            rules.comma_grouping && rules.comma_grouped_value != nullptr &&
            rules.comma_grouped_percent != nullptr &&
            (std::regex_match(candidate, *rules.comma_grouped_value) ||
             std::regex_match(candidate, *rules.comma_grouped_percent));
        const bool valid_currency_group_surface =
            !currency_prefix ||
            (!signed_currency_prefix && currency_probe == index && points[index].value != '+' &&
             points[index].value != '-' && points[currency_symbol].value == '$');
        const bool malformed_english_comma_group =
            rules.comma_grouping && candidate.find(',') != std::string::npos;
        const auto digit_count = static_cast<std::size_t>(std::count_if(
            candidate.begin(), candidate.end(), [](unsigned char c) { return text::is_digit(c); }));
        if (digit_count > 9)
            analysis.malformed_compound = true;
        const bool invalid_percent =
            analysis.has_percent &&
            (analysis.percent_attached_to_numeric ||
             (analysis.end_index < points.size() &&
              text::is_lexical_numeric_boundary(points[analysis.end_index].value)));
        const bool comma_group_followed_by_word = [&] {
            const auto comma = candidate.find(',');
            if (comma == std::string::npos || candidate.find('.', comma + 1) != std::string::npos ||
                candidate.find(':', comma + 1) != std::string::npos ||
                candidate.size() - comma - 1 != 3)
                return false;
            auto tail_index = analysis.end_index;
            while (tail_index < points.size() &&
                   (points[tail_index].value == ' ' || points[tail_index].value == '\t' ||
                    points[tail_index].value == '\n' || points[tail_index].value == '\r'))
                ++tail_index;
            if (tail_index == points.size())
                return false;
            return text::is_letter(points[tail_index].value);
        }();
        bool valid_russian_date = false;
        if (rules.dotted_dates && analysis.separators == 2) {
            const auto first_dot = candidate.find('.');
            const auto second_dot = candidate.find('.', first_dot + 1);
            if (first_dot != std::string::npos && second_dot != std::string::npos &&
                candidate.find('.', second_dot + 1) == std::string::npos) {
                long long day = 0, month = 0, year = 0;
                valid_russian_date =
                    try_parse_long(candidate.substr(0, first_dot), day) &&
                    try_parse_long(candidate.substr(first_dot + 1, second_dot - first_dot - 1),
                                   month) &&
                    try_parse_long(candidate.substr(second_dot + 1), year) &&
                    candidate.substr(0, first_dot).size() <= 2 &&
                    candidate.substr(first_dot + 1, second_dot - first_dot - 1).size() <= 2 &&
                    candidate.substr(second_dot + 1).size() == 4 &&
                    valid_date(
                        static_cast<int>(day), static_cast<int>(month), static_cast<int>(year)) &&
                    !attached_lexical_suffix;
            }
        }
        const auto hyphen_search_start = candidate.size() > 0 && candidate.front() == '-' ? 1 : 0;
        const bool unsupported_numeric_connector =
            starts_phone || candidate.find('+') != std::string::npos ||
            candidate.find('=') != std::string::npos ||
            candidate.find('-', hyphen_search_start) != std::string::npos ||
            candidate.find('/') != std::string::npos || analysis.has_range_connector ||
            analysis.has_unsupported_numeric_connector;
        const auto protected_begin = currency_prefix ? points[currency_prefix_begin].offset : begin;
        const bool already_preserved = [&] {
            if (text.preserved_ranges == nullptr)
                return false;
            const auto source = text.source_range(protected_begin, end);
            return text.preserved_ranges->overlaps(source);
        }();
        const bool invalid_currency_prefix =
            analysis.has_percent || currency_probe < index || points[index].value == '+' ||
            points[index].value == '-' || points[currency_symbol].value != '$' ||
            signed_currency_prefix;
        PreservationFacts preservation;
        preservation.embedded = embedded;
        preservation.attached_lexical_suffix = attached_lexical_suffix;
        preservation.comma_group_followed_by_word = comma_group_followed_by_word;
        preservation.valid_english_comma_group = valid_english_comma_group;
        preservation.unsupported_numeric_connector = unsupported_numeric_connector;
        preservation.invalid_percent = invalid_percent;
        preservation.malformed_english_comma_group = malformed_english_comma_group;
        preservation.currency_prefix = currency_prefix;
        preservation.invalid_currency_prefix = invalid_currency_prefix;
        preservation.valid_russian_date = valid_russian_date;
        preservation.valid_currency_group_surface = valid_currency_group_surface;
        preservation.already_preserved = already_preserved;
        if (requires_preservation(analysis, preservation)) {
            NumericCandidate candidate{
                document.span_from_bytes(protected_begin, end),
                currency_prefix ? NumericCandidateKind::Currency
                                : (analysis.malformed_grouped || analysis.malformed_compound
                                       ? NumericCandidateKind::Grouped
                                       : NumericCandidateKind::Numeric),
                true,
                embedded,
                analysis.has_percent};
            add_protected_candidate(text, candidate, warnings, protected_spans, edits);
        }
        index = analysis.end_index;
    }
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail
