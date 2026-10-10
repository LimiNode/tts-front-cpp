#include "tts_front/technical/admission.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/technical/patterns.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tts_front::detail::technical {

using CodePoint = Utf8CodePoint;

MappedText protect(MappedText text, std::vector<ProtectedSpan>& protected_spans) {
    const auto& patterns = technical::patterns();
    const std::array<const std::regex*, 9> technical_patterns = {&patterns.technical_url,
                                                                 &patterns.technical_email,
                                                                 &patterns.technical_ipv4,
                                                                 &patterns.technical_version,
                                                                 &patterns.technical_http,
                                                                 &patterns.technical_gpu,
                                                                 &patterns.technical_identifier,
                                                                 &patterns.technical_cpp,
                                                                 &patterns.technical_csharp};
    for (const auto* pattern : technical_patterns) {
        const Utf8Document document(text.text);
        const auto scientific_index = build_numeric_surface_index(document);
        MappedText output;
        output.preserved_ranges = text.preserved_ranges;
        std::size_t cursor = 0;
        std::size_t run_cursor = 0;
        for (std::sregex_iterator it(text.text.begin(), text.text.end(), *pattern), end; it != end;
             ++it) {
            const auto begin = static_cast<std::size_t>(it->position());
            const auto finish = begin + static_cast<std::size_t>(it->length());
            const auto match_point = codepoint_index_at_or_after(document, begin);
            const auto match_end_point = codepoint_index_at_or_after(document, finish);
            const auto& value = it->str();
            const bool generic_identifier = pattern == &patterns.technical_identifier;
            if (generic_identifier && !is_known_identifier(value) &&
                scientific_index.overlaps(match_point, match_end_point))
                continue;
            const bool scientific_suffix = match_point < document.points.size() && !value.empty() &&
                                           (value.front() == '#' || value.front() == '$') &&
                                           scientific_index.contains(match_point);
            if (scientific_suffix)
                continue;
            // Do not shield an incomplete scientific suffix (`e+x`) that is
            // attached to a numeric candidate.  The language admission pass
            // must see the complete `+1e+x` expression and preserve it
            // atomically instead of leaving only `+1` visible.
            if (pattern == &patterns.technical_identifier && begin > 0 &&
                (text.text[begin - 1] >= '0' && text.text[begin - 1] <= '9') &&
                !it->str().empty() && (it->str().front() == 'e' || it->str().front() == 'E'))
                continue;
            output.append_copy_with_cursor(text, cursor, begin, run_cursor);
            const auto marker = marker_for(text.text, protected_spans.size());
            protected_spans.push_back({marker, it->str()});
            output.append_generated(text, begin, finish, marker);
            cursor = finish;
        }
        output.append_copy_with_cursor(text, cursor, text.text.size(), run_cursor);
        text = std::move(output);
    }
    return text;
}
MappedText restore(MappedText text, const std::vector<ProtectedSpan>& protected_spans) {
    if (protected_spans.empty() || text.text.empty())
        return text;

    std::unordered_map<std::string, std::string> values;
    values.reserve(protected_spans.size());
    for (const auto& span : protected_spans)
        values.emplace(span.marker, span.value);

    // Expand nested markers in memory first.  Technical shielding can nest a
    // currency/number marker inside an URL marker; resolving that nesting here
    // lets us restore every marker with one source edit sweep.
    std::function<std::string(const std::string&, std::vector<std::string>&)> expand =
        [&](const std::string& value, std::vector<std::string>& stack) {
            std::string result;
            std::size_t cursor = 0;
            while (cursor < value.size()) {
                const auto marker_begin = value.find('\x01', cursor);
                if (marker_begin == std::string::npos) {
                    result.append(value, cursor, std::string::npos);
                    break;
                }
                result.append(value, cursor, marker_begin - cursor);
                const auto marker_end = value.find('\x02', marker_begin + 1);
                if (marker_end == std::string::npos) {
                    result.append(value, marker_begin, std::string::npos);
                    break;
                }
                const std::string marker =
                    value.substr(marker_begin, marker_end - marker_begin + 1);
                const auto found = values.find(marker);
                if (found == values.end() ||
                    std::find(stack.begin(), stack.end(), marker) != stack.end()) {
                    result.append(marker);
                } else {
                    stack.push_back(marker);
                    result += expand(found->second, stack);
                    stack.pop_back();
                }
                cursor = marker_end + 1;
            }
            return result;
        };

    std::unordered_map<std::string, std::string> expanded;
    expanded.reserve(values.size());
    for (const auto& span : protected_spans) {
        std::vector<std::string> stack{span.marker};
        expanded.emplace(span.marker, expand(span.value, stack));
    }

    const MappedText& input = text;
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    std::size_t copied_until = 0;
    std::size_t run_cursor = 0;
    while (cursor < input.text.size()) {
        if (input.text[cursor] != '\x01') {
            ++cursor;
            continue;
        }
        const auto marker_end = input.text.find('\x02', cursor + 1);
        if (marker_end == std::string::npos)
            break;
        const std::string marker = input.text.substr(cursor, marker_end - cursor + 1);
        const auto found = expanded.find(marker);
        if (found == expanded.end()) {
            cursor = marker_end + 1;
            continue;
        }
        output.append_copy_with_cursor(input, copied_until, cursor, run_cursor);
        output.append_generated(input, cursor, marker_end + 1, found->second);
        copied_until = marker_end + 1;
        cursor = copied_until;
    }
    output.append_copy_with_cursor(input, copied_until, input.text.size(), run_cursor);
    return output;
}

// Technical shielding intentionally runs before the general numeric pass, but
// numeric-leading scientific/encoded candidates must be admitted first.  If
// they are hidden as (for example) `e+3` or `$3`, the generic normalizer can
// rewrite only the leading numeric fragment.
MappedText protect_numeric_candidates(MappedText text,
                                      WarningSink& warnings,
                                      std::vector<ProtectedSpan>& protected_spans) {
    // Admit grouped percentage tails attached to a technical identifier before
    // technical shielding hides the identifier itself.  Otherwise a token such
    // as `x$+1,234%` would leave `,234%` visible to the generic normalizer.
    {
        std::vector<SourceEdit> edits;
        std::size_t cursor = 0;
        const Utf8Document document(text.text);
        const auto scientific_index = build_numeric_surface_index(document);
        for (std::sregex_iterator it(text.text.begin(),
                                     text.text.end(),
                                     technical::patterns().technical_numeric_percent),
             end;
             it != end;
             ++it) {
            const auto begin = static_cast<std::size_t>(it->position());
            if (begin < cursor)
                continue;
            const auto base_finish = begin + static_cast<std::size_t>(it->length());
            const auto base = it->str(1);
            const auto base_point = codepoint_index_at_or_after(document, begin);
            const auto base_end_point = codepoint_index_at_or_after(document, base_finish);
            if (scientific_index.overlaps(base_point, base_end_point))
                continue;
            if (begin > 0 && !base.empty() && (base.front() == 'e' || base.front() == 'E') &&
                text::is_digit(static_cast<unsigned char>(text.text[begin - 1])))
                continue;
            const auto is_ascii_digit = [](char value) { return value >= '0' && value <= '9'; };
            const auto unicode_connector_length = [&](std::size_t offset) {
                if (offset + 2 >= text.text.size())
                    return std::size_t{0};
                const auto first = static_cast<unsigned char>(text.text[offset]);
                const auto second = static_cast<unsigned char>(text.text[offset + 1]);
                const auto third = static_cast<unsigned char>(text.text[offset + 2]);
                if (first == 0xe2 && second == 0x80 && third >= 0x90 && third <= 0x95)
                    return std::size_t{3};
                if (first == 0xe2 && second == 0x88 && third == 0x92)
                    return std::size_t{3};
                return std::size_t{0};
            };
            const auto scan_numeric_continuation = [&](std::size_t start) {
                const auto start_index = codepoint_index_at_or_after(document, start);
                const auto end_index =
                    scan_numeric_continuation_points(document, start_index, true);
                return document.span_from_codepoints(start_index, end_index).byte_end;
            };
            std::size_t finish = base_finish;
            if (base_finish < text.text.size() &&
                (text.text[base_finish] == ',' || text.text[base_finish] == '%')) {
                auto probe = base_finish;
                bool saw_digit = false;
                while (probe < text.text.size()) {
                    const char value = text.text[probe];
                    const auto unicode_length = unicode_connector_length(probe);
                    if (is_ascii_digit(value)) {
                        ++probe;
                        saw_digit = true;
                    } else if (unicode_length != 0) {
                        auto after_connector = probe + unicode_length;
                        while (after_connector < text.text.size() &&
                               text::is_horizontal_space(text.text[after_connector]))
                            ++after_connector;
                        if (after_connector >= text.text.size() ||
                            !is_ascii_digit(text.text[after_connector]))
                            break;
                        probe += unicode_length;
                    } else if (value != '%' && (text::is_ascii_punctuation(value) ||
                                                text::is_horizontal_space(value))) {
                        ++probe;
                    } else {
                        break;
                    }
                }
                if (probe < text.text.size() && text.text[probe] == '%') {
                    const auto after_percent = probe + 1;
                    const auto continuation_end = scan_numeric_continuation(after_percent);
                    if (continuation_end != after_percent || (base_finish != probe && saw_digit))
                        finish = continuation_end;
                }
                if (finish == base_finish && probe > base_finish && saw_digit)
                    finish = probe;
            }
            if (finish == base_finish) {
                auto connector = base_finish;
                while (connector < text.text.size() &&
                       text::is_horizontal_space(text.text[connector]))
                    ++connector;
                const auto connector_length =
                    connector < text.text.size() &&
                            (text.text[connector] == '/' || text.text[connector] == '=' ||
                             text.text[connector] == '*' || text.text[connector] == '-' ||
                             unicode_connector_length(connector) != 0)
                        ? (unicode_connector_length(connector) != 0
                               ? unicode_connector_length(connector)
                               : std::size_t{1})
                        : std::size_t{0};
                if (connector_length != 0) {
                    auto after_connector = connector + connector_length;
                    while (after_connector < text.text.size() &&
                           text::is_horizontal_space(text.text[after_connector]))
                        ++after_connector;
                    if (after_connector < text.text.size() &&
                        is_ascii_digit(text.text[after_connector]))
                        finish = scan_numeric_continuation(after_connector);
                }
            }
            if (finish == base_finish)
                continue;
            std::size_t protected_begin = base_finish;
            if (const auto dollar = base.find('$'); dollar != std::string::npos) {
                protected_begin = begin + dollar;
            } else if (!base.empty() && base.front() == '#') {
                protected_begin = begin;
            } else if (base.rfind("C++", 0) != 0 && base.rfind("V", 0) != 0) {
                if (const auto sign = base.find_first_of("+-"); sign != std::string::npos)
                    protected_begin = begin + sign;
            }
            // Technical regexes can begin immediately after a leading sign
            // (for example the `1e+3` suffix in `+1e+3`).  Extend the source
            // span over that sign so admission remains atomic and warning
            // offsets never expose a partial candidate.
            const auto protected_point = codepoint_index_at_or_after(document, protected_begin);
            if (protected_point > 0) {
                const auto previous = document.points[protected_point - 1].value;
                if (previous == '+' || previous == '-' || previous == 0x2212)
                    protected_begin = document.points[protected_point - 1].offset;
            }
            NumericCandidate candidate{document.span_from_bytes(protected_begin, finish),
                                       NumericCandidateKind::Technical};
            add_protected_candidate(text, candidate, warnings, protected_spans, edits);
            cursor = finish;
        }
        text = apply_source_edits(text, std::move(edits));
    }

    const Utf8Document document(text.text);
    if (!document.valid)
        return text;
    const auto& points = document.points;
    const auto scientific_index = build_numeric_surface_index(document);

    std::vector<SourceEdit> edits;
    for (std::size_t index = 0; index < points.size();) {
        if (index > 0 && text::is_digit(points[index].value) &&
            (points[index - 1].value == 0x20ac || points[index - 1].value == 0xa3 ||
             points[index - 1].value == 0xa5)) {
            auto currency_begin = index - 1;
            if (currency_begin > 0 && (points[currency_begin - 1].value == '+' ||
                                       points[currency_begin - 1].value == '-' ||
                                       points[currency_begin - 1].value == 0x2212))
                --currency_begin;
            auto numeric_end = index + 1;
            while (numeric_end < points.size() && text::is_digit(points[numeric_end].value))
                ++numeric_end;
            while (numeric_end < points.size() &&
                   (points[numeric_end].value == ',' || points[numeric_end].value == '.') &&
                   numeric_end + 1 < points.size() &&
                   text::is_digit(points[numeric_end + 1].value)) {
                ++numeric_end;
                while (numeric_end < points.size() && text::is_digit(points[numeric_end].value))
                    ++numeric_end;
            }
            NumericCandidate candidate{document.span_from_codepoints(currency_begin, numeric_end),
                                       NumericCandidateKind::Currency};
            add_protected_candidate(text, candidate, warnings, protected_spans, edits);
            index = numeric_end;
            continue;
        }
        if (points[index].value == '$' && index + 1 < points.size() &&
            text::is_digit(points[index + 1].value)) {
            auto currency_begin = index;
            if (currency_begin > 0 && (points[currency_begin - 1].value == '+' ||
                                       points[currency_begin - 1].value == '-' ||
                                       points[currency_begin - 1].value == 0x2212))
                --currency_begin;
            auto numeric_end = index + 1;
            while (numeric_end < points.size() && text::is_digit(points[numeric_end].value))
                ++numeric_end;
            while (numeric_end < points.size() &&
                   (points[numeric_end].value == ',' || points[numeric_end].value == '.') &&
                   numeric_end + 1 < points.size() &&
                   text::is_digit(points[numeric_end + 1].value)) {
                numeric_end += 1;
                while (numeric_end < points.size() && text::is_digit(points[numeric_end].value))
                    ++numeric_end;
            }
            if (numeric_end < points.size() && points[numeric_end].value == '%') {
                const auto protected_end =
                    scan_numeric_continuation_points(document, numeric_end + 1, false);
                NumericCandidate candidate{
                    document.span_from_codepoints(currency_begin, protected_end),
                    NumericCandidateKind::Currency,
                    true,
                    false,
                    true};
                add_protected_candidate(text, candidate, warnings, protected_spans, edits);
                index = protected_end;
                continue;
            }
        }
        const bool starts_number =
            text::is_digit(points[index].value) ||
            ((points[index].value == '-' || points[index].value == '+' ||
              points[index].value == 0x2212) &&
             index + 1 < points.size() && text::is_digit(points[index + 1].value));
        const bool scientific_suffix_start = scientific_index.contains(index);
        if (!starts_number || scientific_suffix_start ||
            (index > 0 && text::is_lexical_numeric_boundary(points[index - 1].value))) {
            ++index;
            continue;
        }

        std::size_t end_index =
            index + ((points[index].value == '-' || points[index].value == '+' ||
                      points[index].value == 0x2212)
                         ? 1
                         : 0);
        while (end_index < points.size() && text::is_digit(points[end_index].value))
            ++end_index;
        if (end_index < points.size() &&
            (points[end_index].value == '.' || points[end_index].value == ',' ||
             points[end_index].value == ':') &&
            end_index + 1 < points.size() && text::is_digit(points[end_index + 1].value)) {
            ++end_index;
            while (end_index < points.size() && text::is_digit(points[end_index].value))
                ++end_index;
        }

        std::size_t suffix_end = end_index;
        bool encoded_suffix = false;
        if (end_index < points.size() &&
            (points[end_index].value == '#' || points[end_index].value == '$')) {
            auto probe = end_index + 1;
            const auto suffix_begin = probe;
            while (probe < points.size() &&
                   (text::is_digit(points[probe].value) || text::is_letter(points[probe].value) ||
                    text::is_combining_mark(points[probe].value) || points[probe].value == '_'))
                ++probe;
            if (probe != suffix_begin) {
                suffix_end = probe;
                encoded_suffix = true;
            }
        }

        const bool terminal_connector =
            !encoded_suffix && end_index < points.size() &&
            (points[end_index].value == '$' || points[end_index].value == '#');
        if (!encoded_suffix && !terminal_connector) {
            ++index;
            continue;
        }
        if (terminal_connector) {
            suffix_end = end_index + 1;
        }

        NumericCandidate candidate{
            document.span_from_codepoints(index, suffix_end), NumericCandidateKind::Numeric, true};
        add_protected_candidate(text, candidate, warnings, protected_spans, edits);
        index = suffix_end;
    }
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail::technical
