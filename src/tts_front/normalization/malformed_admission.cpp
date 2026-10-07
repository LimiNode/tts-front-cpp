#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/english/patterns.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/normalization/admission.hpp"
#include "tts_front/normalization/candidate_scanner.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"
#include "tts_front/normalization/patterns.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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
                                                AdmissionLanguage language) {
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
        const bool starts_phone = points[index].value == '+' && index + 1 < points.size() &&
                                  is_digit(points[index + 1].value) && !currency_context;
        const bool starts_number = is_digit(points[index].value) ||
                                   (points[index].value == '-' && index + 1 < points.size() &&
                                    is_digit(points[index + 1].value));
        if (!starts_number && !starts_phone) {
            ++index;
            continue;
        }
        const auto begin = points[index].offset;
        std::size_t end_index = index + 1;
        std::size_t separators = 0;
        bool has_percent = false;
        bool percent_attached_to_numeric = false;
        bool has_range_connector = false;
        bool has_unsupported_numeric_connector = false;
        bool malformed_grouped = false;
        bool malformed_compound = false;
        bool grouped_seen = false;
        auto initial_digit_index = index + (points[index].value == '-' ? 1 : 0);
        const auto initial_digit_begin = initial_digit_index;
        while (initial_digit_index < points.size() && is_digit(points[initial_digit_index].value))
            ++initial_digit_index;
        const auto initial_digit_count = initial_digit_index - initial_digit_begin;
        if (starts_phone) {
            while (end_index < points.size()) {
                if (is_digit(points[end_index].value)) {
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
                        if (digit_probe < points.size() && is_digit(points[digit_probe].value)) {
                            end_index = probe;
                            continue;
                        }
                    }
                    if (probe < points.size() && is_digit(points[probe].value)) {
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
                    if (probe < points.size() &&
                        (points[probe].value == '%' || is_range_connector(points[probe].value))) {
                        end_index = probe;
                        continue;
                    }
                    if (has_range_connector && probe < points.size() &&
                        is_digit(points[probe].value)) {
                        end_index = probe;
                        continue;
                    }
                    if (probe < points.size() && is_digit(points[probe].value)) {
                        auto group_end = probe;
                        while (group_end < points.size() && is_digit(points[group_end].value))
                            ++group_end;
                        const auto group_size = group_end - probe;
                        const bool short_following_group =
                            grouped_seen && group_size < 3 &&
                            (group_end == points.size() ||
                             is_lexical_numeric_boundary(points[group_end].value));
                        if (separators == 0 && initial_digit_count <= 3 &&
                            (group_size >= 3 || short_following_group)) {
                            grouped_seen = true;
                            malformed_grouped = malformed_grouped || group_size != 3;
                            if (group_end < points.size() &&
                                is_lexical_numeric_boundary(points[group_end].value))
                                malformed_grouped = true;
                            end_index = group_end;
                            continue;
                        }
                    }
                    break;
                }
                if ((value == '.' || value == ',' || value == ':') &&
                    (end_index + 1 >= points.size() || !is_digit(points[end_index + 1].value))) {
                    if (value == '.' && end_index + 1 < points.size() &&
                        points[end_index + 1].value == '.') {
                        auto probe = end_index;
                        while (probe < points.size() && points[probe].value == '.')
                            ++probe;
                        if (probe < points.size() && is_digit(points[probe].value)) {
                            malformed_compound = true;
                            end_index = probe;
                            continue;
                        }
                    }
                    if (separators != 0 && end_index + 1 < points.size() &&
                        (is_numeric_connector(points[end_index + 1].value) ||
                         is_digit(points[end_index + 1].value))) {
                        malformed_compound = true;
                        ++end_index;
                        while (end_index < points.size() &&
                               (is_numeric_connector(points[end_index].value) ||
                                is_digit(points[end_index].value)))
                            ++end_index;
                        continue;
                    }
                    break;
                }
                if (is_numeric_connector(value)) {
                    if (grouped_seen && (value == '.' || value == ',' || value == ':'))
                        malformed_compound = true;
                    if (value == '%') {
                        has_percent = true;
                        const auto probe = end_index + 1;
                        if (probe < points.size() &&
                            (is_lexical_numeric_boundary(points[probe].value) ||
                             points[probe].value == '%'))
                            percent_attached_to_numeric = true;
                    } else
                        ++separators;
                    if (is_range_connector(value))
                        has_range_connector = true;
                    if (!is_supported_numeric_separator(value) && end_index + 1 < points.size() &&
                        points[end_index + 1].value != ' ' && points[end_index + 1].value != '\t' &&
                        points[end_index + 1].value != '\n' && points[end_index + 1].value != '\r')
                        has_unsupported_numeric_connector = true;
                    ++end_index;
                    continue;
                }
                if (is_digit(value)) {
                    ++end_index;
                    continue;
                }
                break;
            }
        }
        if (initial_digit_count > 9 && end_index < points.size() &&
            is_lexical_numeric_boundary(points[end_index].value)) {
            while (end_index < points.size() &&
                   (is_lexical_numeric_boundary(points[end_index].value) ||
                    is_combining_mark(points[end_index].value)))
                ++end_index;
        }
        if (initial_digit_count > 9 && end_index < points.size() &&
            (points[end_index].value == ' ' || points[end_index].value == '\t')) {
            auto probe = end_index;
            while (probe < points.size() &&
                   (points[probe].value == ' ' || points[probe].value == '\t'))
                ++probe;
            while (probe < points.size() && is_letter(points[probe].value))
                ++probe;
            if (probe > end_index)
                end_index = probe;
        }
        if (currency_context && has_percent)
            end_index = scan_numeric_continuation_points(document, end_index, false);
        if (has_percent && end_index < points.size() &&
            is_lexical_numeric_boundary(points[end_index].value)) {
            while (end_index < points.size() &&
                   (is_lexical_numeric_boundary(points[end_index].value) ||
                    is_combining_mark(points[end_index].value)))
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
            while (end_index < points.size() &&
                   (is_digit(points[end_index].value) || is_letter(points[end_index].value) ||
                    is_combining_mark(points[end_index].value)))
                ++end_index;
        }
        // A clock-like expression followed by percent is not an English or
        // Russian percent candidate (`1:02%`). Keep the whole expression.
        if (has_percent && std::any_of(points.begin() + static_cast<std::ptrdiff_t>(index),
                                       points.begin() + static_cast<std::ptrdiff_t>(end_index),
                                       [](const CodePoint& point) { return point.value == ':'; }))
            malformed_compound = true;
        // Currency words with an attached lexical suffix are malformed. The
        // valid forms are handled by the language currency regexes; this guard
        // prevents `1 рублейfoo` from becoming `один рублейfoo`.
        if (language == AdmissionLanguage::Russian && end_index < points.size() &&
            (points[end_index].value == ' ' || points[end_index].value == '\t')) {
            auto word_begin = end_index;
            while (word_begin < points.size() &&
                   (points[word_begin].value == ' ' || points[word_begin].value == '\t'))
                ++word_begin;
            auto word_end = word_begin;
            while (word_end < points.size() && is_letter(points[word_end].value))
                ++word_end;
            if (word_end != word_begin) {
                const auto unit =
                    text.text.substr(points[word_begin].offset,
                                     points[word_end - 1].offset + points[word_end - 1].length -
                                         points[word_begin].offset);
                const std::string rub_prefix = "\xD1\x80\xD1\x83\xD0\xB1";
                const bool starts_ruble = unit.compare(0, rub_prefix.size(), rub_prefix) == 0;
                const bool known_ruble = unit == rub_prefix ||
                                         unit == rub_prefix + "\xD0\xBB\xD1\x8C" ||
                                         unit == rub_prefix + "\xD0\xBB\xD1\x8F" ||
                                         unit == rub_prefix + "\xD0\xBB\xD0\xB5\xD0\xB9";
                if (starts_ruble && !known_ruble) {
                    malformed_compound = true;
                    end_index = word_end;
                }
            }
        }
        const bool previous_is_connector =
            index > 0 && (points[index - 1].value == '-' || points[index - 1].value == '/' ||
                          points[index - 1].value == '+' || points[index - 1].value == '=');
        const bool embedded = index > 0 && (is_lexical_numeric_boundary(points[index - 1].value) ||
                                            (previous_is_connector && index > 1 &&
                                             is_lexical_numeric_boundary(points[index - 2].value)));
        if (embedded) {
            std::size_t grouped_end = end_index;
            while (grouped_end < points.size()) {
                auto probe = grouped_end;
                while (probe < points.size() &&
                       (points[probe].value == ' ' || points[probe].value == '\t' ||
                        points[probe].value == '\n' || points[probe].value == '\r'))
                    ++probe;
                const auto group_begin = probe;
                while (probe < points.size() && is_digit(points[probe].value))
                    ++probe;
                if (probe == group_begin)
                    break;
                while (probe < points.size()) {
                    const auto value = points[probe].value;
                    if ((value == '.' || value == ',' || value == ':') &&
                        probe + 1 < points.size() && is_digit(points[probe + 1].value)) {
                        probe += 1;
                        while (probe < points.size() && is_digit(points[probe].value))
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
                   (is_lexical_numeric_boundary(points[end_index].value) ||
                    is_combining_mark(points[end_index].value)))
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
        if (currency_prefix)
            currency_prefix_begin = currency_probe - 1;
        const bool attached_lexical_suffix = separators != 0 && end_index < points.size() &&
                                             is_lexical_numeric_boundary(points[end_index].value);
        const bool valid_english_comma_group =
            language == AdmissionLanguage::English &&
            (std::regex_match(candidate, english_patterns().en_comma_grouped_value) ||
             std::regex_match(candidate, english_patterns().en_comma_grouped_percent));
        const bool malformed_english_comma_group =
            language == AdmissionLanguage::English && candidate.find(',') != std::string::npos;
        const auto digit_count = static_cast<std::size_t>(std::count_if(
            candidate.begin(), candidate.end(), [](unsigned char c) { return is_digit(c); }));
        if (digit_count > 9)
            malformed_compound = true;
        const bool invalid_percent =
            has_percent &&
            (percent_attached_to_numeric ||
             (end_index < points.size() && is_lexical_numeric_boundary(points[end_index].value)));
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
            return is_letter(points[tail_index].value);
        }();
        bool valid_russian_date = false;
        if (language == AdmissionLanguage::Russian && separators == 2) {
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
        if ((separators >= 2 || embedded || attached_lexical_suffix ||
             (comma_group_followed_by_word && !valid_english_comma_group) ||
             unsupported_numeric_connector || invalid_percent || malformed_english_comma_group ||
             malformed_grouped || malformed_compound ||
             (currency_prefix &&
              (has_percent || currency_probe < index || points[index].value == '+' ||
               points[index].value == '-' || points[currency_probe - 1].value != '$'))) &&
            !(valid_russian_date && !embedded) &&
            !(valid_english_comma_group && !embedded && !attached_lexical_suffix &&
              !(currency_prefix && has_percent) && !malformed_compound)) {
            const auto protected_begin =
                currency_prefix ? points[currency_prefix_begin].offset : begin;
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
