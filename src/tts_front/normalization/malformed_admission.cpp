#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/normalization/admission.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tts_front::detail {

using CodePoint = Utf8CodePoint;

bool valid_date(int day, int month, int year) {
    if (month < 1 || month > 12 || day < 1)
        return false;
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days[month];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        limit = 29;
    return day <= limit;
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
        auto currency_context_probe = index;
        while (currency_context_probe > 0 && (points[currency_context_probe - 1].value == ' ' ||
                                              points[currency_context_probe - 1].value == '\t'))
            --currency_context_probe;
        if (currency_context_probe > 0 && (points[currency_context_probe - 1].value == '+' ||
                                           points[currency_context_probe - 1].value == '-')) {
            --currency_context_probe;
            while (currency_context_probe > 0 && (points[currency_context_probe - 1].value == ' ' ||
                                                  points[currency_context_probe - 1].value == '\t'))
                --currency_context_probe;
        }
        const bool currency_context =
            currency_context_probe > 0 && (points[currency_context_probe - 1].value == '$' ||
                                           points[currency_context_probe - 1].value == 0x20ac ||
                                           points[currency_context_probe - 1].value == 0xa3 ||
                                           points[currency_context_probe - 1].value == 0xa5);
        const auto surface = scan_numeric_surface(document, index);
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
        std::size_t end_index = index + 1;
        std::size_t separators = 0;
        bool has_percent = false;
        bool percent_attached_to_numeric = false;
        bool has_range_connector = false;
        bool has_unsupported_numeric_connector = false;
        bool malformed_grouped = false;
        bool malformed_compound = false;
        bool grouped_seen = false;
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
            malformed_compound = true;
        if (starts_phone) {
            while (end_index < points.size()) {
                if (text::is_digit(points[end_index].value)) {
                    ++end_index;
                    continue;
                }
                if (points[end_index].value == '-' || points[end_index].value == '/') {
                    ++separators;
                    ++end_index;
                    continue;
                }
                if (points[end_index].value == '(' || points[end_index].value == ')') {
                    ++end_index;
                    continue;
                }
                if (points[end_index].value == ' ' || points[end_index].value == '\t') {
                    auto probe = end_index + 1;
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
                            end_index = probe;
                            continue;
                        }
                    }
                    if (probe < points.size() && text::is_digit(points[probe].value)) {
                        end_index = probe;
                        continue;
                    }
                }
                break;
            }
        } else {
            while (end_index < points.size()) {
                const auto value = points[end_index].value;
                if (value == ' ' || value == '\t' || value == '\n' || value == '\r') {
                    auto probe = end_index + 1;
                    while (probe < points.size() &&
                           (points[probe].value == ' ' || points[probe].value == '\t' ||
                            points[probe].value == '\n' || points[probe].value == '\r'))
                        ++probe;
                    if (probe < points.size() && (points[probe].value == '%' ||
                                                  text::is_range_connector(points[probe].value))) {
                        end_index = probe;
                        continue;
                    }
                    if (has_range_connector && probe < points.size() &&
                        text::is_digit(points[probe].value)) {
                        end_index = probe;
                        continue;
                    }
                    if (probe < points.size() && text::is_digit(points[probe].value)) {
                        auto group_end = probe;
                        while (group_end < points.size() && text::is_digit(points[group_end].value))
                            ++group_end;
                        const auto group_size = group_end - probe;
                        const bool short_following_group =
                            grouped_seen && group_size < 3 &&
                            (group_end == points.size() ||
                             text::is_lexical_numeric_boundary(points[group_end].value));
                        if (separators == 0 && initial_digit_count <= 3 &&
                            (group_size >= 3 || short_following_group)) {
                            grouped_seen = true;
                            malformed_grouped = malformed_grouped || group_size != 3;
                            if (group_end < points.size() &&
                                text::is_lexical_numeric_boundary(points[group_end].value))
                                malformed_grouped = true;
                            end_index = group_end;
                            continue;
                        }
                    }
                    break;
                }
                if ((value == '.' || value == ',' || value == ':') &&
                    (end_index + 1 >= points.size() ||
                     !text::is_digit(points[end_index + 1].value))) {
                    if (value == '.' && end_index + 1 < points.size() &&
                        points[end_index + 1].value == '.') {
                        auto probe = end_index;
                        while (probe < points.size() && points[probe].value == '.')
                            ++probe;
                        if (probe < points.size() && text::is_digit(points[probe].value)) {
                            malformed_compound = true;
                            end_index = probe;
                            continue;
                        }
                    }
                    if (separators != 0 && end_index + 1 < points.size() &&
                        (text::is_numeric_connector(points[end_index + 1].value) ||
                         text::is_digit(points[end_index + 1].value))) {
                        malformed_compound = true;
                        ++end_index;
                        while (end_index < points.size() &&
                               (text::is_numeric_connector(points[end_index].value) ||
                                text::is_digit(points[end_index].value)))
                            ++end_index;
                        continue;
                    }
                    break;
                }
                if (value == ')' && parenthesized_prefix)
                    break;
                if (text::is_numeric_connector(value)) {
                    if (grouped_seen && (value == '.' || value == ',' || value == ':'))
                        malformed_compound = true;
                    if (value == '%') {
                        has_percent = true;
                        const auto probe = end_index + 1;
                        if (probe < points.size() &&
                            (text::is_lexical_numeric_boundary(points[probe].value) ||
                             points[probe].value == '%'))
                            percent_attached_to_numeric = true;
                    } else
                        ++separators;
                    if (text::is_range_connector(value))
                        has_range_connector = true;
                    if (!text::is_supported_numeric_separator(value) &&
                        end_index + 1 < points.size() && points[end_index + 1].value != ' ' &&
                        points[end_index + 1].value != '\t' &&
                        points[end_index + 1].value != '\n' && points[end_index + 1].value != '\r')
                        has_unsupported_numeric_connector = true;
                    ++end_index;
                    continue;
                }
                if (text::is_digit(value)) {
                    ++end_index;
                    continue;
                }
                break;
            }
        }
        if ((positive_numeric || points[index].value == 0x2212) && end_index < points.size()) {
            if (surface.exponent_marker) {
                malformed_compound = true;
                end_index = std::max(end_index, surface.end);
            }
            auto suffix_begin = end_index;
            while (suffix_begin < points.size() &&
                   text::is_horizontal_space(points[suffix_begin].value))
                ++suffix_begin;
            auto suffix_end = suffix_begin;
            while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                ++suffix_end;
            if (suffix_end > suffix_begin) {
                malformed_compound = true;
                end_index = suffix_end;
            }
        }
        if (points[index].value == 0x2212 && end_index < points.size()) {
            auto suffix_begin = end_index;
            while (suffix_begin < points.size() &&
                   text::is_horizontal_space(points[suffix_begin].value))
                ++suffix_begin;
            auto suffix_end = suffix_begin;
            while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                ++suffix_end;
            if (suffix_end > suffix_begin) {
                malformed_compound = true;
                end_index = suffix_end;
            } else if (suffix_begin == end_index && suffix_begin < points.size() &&
                       text::is_letter(points[suffix_begin].value)) {
                while (suffix_end < points.size() && text::is_letter(points[suffix_end].value))
                    ++suffix_end;
                malformed_compound = true;
                end_index = suffix_end;
            }
        }
        if (initial_digit_count > 9 && end_index < points.size() &&
            text::is_lexical_numeric_boundary(points[end_index].value)) {
            while (end_index < points.size() &&
                   (text::is_lexical_numeric_boundary(points[end_index].value) ||
                    text::is_combining_mark(points[end_index].value)))
                ++end_index;
        }
        if (initial_digit_count > 9 && end_index < points.size() &&
            (points[end_index].value == ' ' || points[end_index].value == '\t')) {
            auto probe = end_index;
            while (probe < points.size() &&
                   (points[probe].value == ' ' || points[probe].value == '\t'))
                ++probe;
            while (probe < points.size() && text::is_letter(points[probe].value))
                ++probe;
            if (probe > end_index)
                end_index = probe;
        }
        if (currency_context && has_percent)
            end_index = scan_numeric_continuation_points(document, end_index, false);
        if (has_percent && end_index < points.size() &&
            text::is_lexical_numeric_boundary(points[end_index].value)) {
            while (end_index < points.size() &&
                   (text::is_lexical_numeric_boundary(points[end_index].value) ||
                    text::is_combining_mark(points[end_index].value)))
                ++end_index;
            percent_attached_to_numeric = true;
        }
        // A terminal currency/hash connector is not a supported postfix
        // notation. Preserve it atomically instead of letting the generic
        // number pass rewrite only the leading digits (`1$` -> `one$`).
        if (end_index < points.size() &&
            (points[end_index].value == '$' || points[end_index].value == '#')) {
            malformed_compound = true;
            ++end_index;
            while (end_index < points.size() && (text::is_digit(points[end_index].value) ||
                                                 text::is_letter(points[end_index].value) ||
                                                 text::is_combining_mark(points[end_index].value)))
                ++end_index;
        }
        // A clock-like expression followed by percent is not an English or
        // Russian percent candidate (`1:02%`). Keep the whole expression.
        if (has_percent && std::any_of(points.begin() + static_cast<std::ptrdiff_t>(index),
                                       points.begin() + static_cast<std::ptrdiff_t>(end_index),
                                       [](const CodePoint& point) { return point.value == ':'; }))
            malformed_compound = true;
        if (signed_time)
            malformed_compound = true;
        // A recognized unit/currency stem followed by an attached lexical
        // suffix is one malformed candidate.  Letting the generic number
        // pass see only the leading digits would otherwise produce partial
        // rewrites such as `one kgfoo` or `один руб.foo`.
        if (end_index < points.size()) {
            auto suffix_begin = end_index;
            while (suffix_begin < points.size() &&
                   (points[suffix_begin].value == ' ' || points[suffix_begin].value == '\t'))
                ++suffix_begin;
            auto word_end = suffix_begin;
            while (word_end < points.size() && text::is_letter(points[word_end].value))
                ++word_end;
            if (word_end != suffix_begin) {
                auto suffix_end = word_end;
                // Include a dotted continuation such as `руб.foo`; a final
                // period by itself remains a valid Russian abbreviation.
                if (suffix_end < points.size() && points[suffix_end].value == '.' &&
                    suffix_end + 1 < points.size() &&
                    text::is_letter(points[suffix_end + 1].value)) {
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
                    rules.classify_surface(unit, known_stem, valid_surface);
                if (known_stem && (!valid_surface || suffix_end != word_end)) {
                    malformed_compound = true;
                    end_index = suffix_end;
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
            std::size_t grouped_end = end_index;
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
            end_index = grouped_end;
        }
        if (malformed_grouped) {
            while (end_index < points.size() &&
                   (text::is_lexical_numeric_boundary(points[end_index].value) ||
                    text::is_combining_mark(points[end_index].value)))
                ++end_index;
        }
        const auto end =
            end_index == 0 ? begin : points[end_index - 1].offset + points[end_index - 1].length;
        const auto candidate = text.text.substr(begin, end - begin);
        std::size_t currency_prefix_begin = index;
        auto currency_probe = index;
        while (currency_probe > 0 && (points[currency_probe - 1].value == ' ' ||
                                      points[currency_probe - 1].value == '\t'))
            --currency_probe;
        if (currency_probe > 0 &&
            (points[currency_probe - 1].value == '+' || points[currency_probe - 1].value == '-'))
            --currency_probe;
        while (currency_probe > 0 && (points[currency_probe - 1].value == ' ' ||
                                      points[currency_probe - 1].value == '\t'))
            --currency_probe;
        const bool currency_prefix =
            currency_probe > 0 &&
            (points[currency_probe - 1].value == '$' ||
             points[currency_probe - 1].value == 0x20ac ||
             points[currency_probe - 1].value == 0xa3 || points[currency_probe - 1].value == 0xa5);
        bool signed_currency_prefix = false;
        if (currency_prefix)
            currency_prefix_begin = currency_probe - 1;
        if (currency_prefix) {
            auto sign_probe = currency_prefix_begin;
            while (sign_probe > 0 &&
                   (points[sign_probe - 1].value == ' ' || points[sign_probe - 1].value == '\t'))
                --sign_probe;
            if (sign_probe > 0 &&
                (points[sign_probe - 1].value == '+' || points[sign_probe - 1].value == '-' ||
                 points[sign_probe - 1].value == 0x2212)) {
                signed_currency_prefix = true;
                currency_prefix_begin = sign_probe - 1;
            }
        }
        const bool attached_lexical_suffix =
            separators != 0 && end_index < points.size() &&
            text::is_lexical_numeric_boundary(points[end_index].value);
        const bool valid_english_comma_group =
            rules.comma_grouping && rules.comma_grouped_value != nullptr &&
            rules.comma_grouped_percent != nullptr &&
            (std::regex_match(candidate, *rules.comma_grouped_value) ||
             std::regex_match(candidate, *rules.comma_grouped_percent));
        const bool valid_currency_group_surface =
            !currency_prefix ||
            (!signed_currency_prefix && points[index].value != '+' && points[index].value != '-' &&
             points[currency_probe - 1].value == '$');
        const bool malformed_english_comma_group =
            rules.comma_grouping && candidate.find(',') != std::string::npos;
        const auto digit_count = static_cast<std::size_t>(std::count_if(
            candidate.begin(), candidate.end(), [](unsigned char c) { return text::is_digit(c); }));
        if (digit_count > 9)
            malformed_compound = true;
        const bool invalid_percent =
            has_percent && (percent_attached_to_numeric ||
                            (end_index < points.size() &&
                             text::is_lexical_numeric_boundary(points[end_index].value)));
        const bool comma_group_followed_by_word = [&] {
            const auto comma = candidate.find(',');
            if (comma == std::string::npos || candidate.find('.', comma + 1) != std::string::npos ||
                candidate.find(':', comma + 1) != std::string::npos ||
                candidate.size() - comma - 1 != 3)
                return false;
            auto tail_index = end_index;
            while (tail_index < points.size() &&
                   (points[tail_index].value == ' ' || points[tail_index].value == '\t' ||
                    points[tail_index].value == '\n' || points[tail_index].value == '\r'))
                ++tail_index;
            if (tail_index == points.size())
                return false;
            return text::is_letter(points[tail_index].value);
        }();
        bool valid_russian_date = false;
        if (rules.dotted_dates && separators == 2) {
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
            candidate.find('/') != std::string::npos || has_range_connector ||
            has_unsupported_numeric_connector;
        const auto protected_begin = currency_prefix ? points[currency_prefix_begin].offset : begin;
        const bool already_preserved = [&] {
            if (text.preserved_ranges == nullptr)
                return false;
            const auto source = text.source_range(protected_begin, end);
            const auto& ranges = *text.preserved_ranges;
            const auto overlaps = [source](const SourceRange& range) {
                return source.offset < range.offset + range.length &&
                       range.offset < source.offset + source.length;
            };
            auto next = std::lower_bound(
                ranges.begin(),
                ranges.end(),
                source.offset,
                [](const SourceRange& range, std::size_t at) { return range.offset < at; });
            return (next != ranges.end() && overlaps(*next)) ||
                   (next != ranges.begin() && overlaps(*std::prev(next)));
        }();
        if ((separators >= 2 || embedded || attached_lexical_suffix ||
             (comma_group_followed_by_word && !valid_english_comma_group) ||
             unsupported_numeric_connector || invalid_percent || malformed_english_comma_group ||
             malformed_grouped || malformed_compound ||
             (currency_prefix &&
              (has_percent || currency_probe < index || points[index].value == '+' ||
               points[index].value == '-' || points[currency_probe - 1].value != '$' ||
               signed_currency_prefix))) &&
            !(valid_russian_date && !embedded) &&
            !(valid_english_comma_group && !embedded && !attached_lexical_suffix &&
              !(currency_prefix && has_percent) && !malformed_compound &&
              valid_currency_group_surface) &&
            !already_preserved) {
            NumericCandidate candidate{document.span_from_bytes(protected_begin, end),
                                       currency_prefix ? NumericCandidateKind::Currency
                                                       : (malformed_grouped || malformed_compound
                                                              ? NumericCandidateKind::Grouped
                                                              : NumericCandidateKind::Numeric),
                                       true,
                                       embedded,
                                       has_percent};
            add_protected_candidate(text, candidate, warnings, protected_spans, edits);
        }
        index = end_index;
    }
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail
