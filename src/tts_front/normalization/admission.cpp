#include "tts_front/normalization/admission.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
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

bool numeric_match_has_valid_boundaries(const std::vector<CodePoint>& points,
                                        std::size_t begin,
                                        std::size_t end) {
    const auto first = std::lower_bound(
        points.begin(), points.end(), begin, [](const CodePoint& point, std::size_t offset) {
            return point.offset < offset;
        });
    const auto after = std::lower_bound(
        points.begin(), points.end(), end, [](const CodePoint& point, std::size_t offset) {
            return point.offset < offset;
        });
    auto first_digit = after;
    for (auto point = first; point != after; ++point) {
        if (is_digit(point->value)) {
            first_digit = point;
            break;
        }
    }
    if (first_digit == after)
        return true;

    auto last_digit = first_digit;
    for (auto point = first_digit; point != after; ++point) {
        if (is_digit(point->value))
            last_digit = point;
    }
    const auto first_index = static_cast<std::size_t>(first_digit - points.begin());
    const auto next_index = static_cast<std::size_t>(after - points.begin());
    const CodePoint* previous = first_index == 0 ? nullptr : &points[first_index - 1];
    const CodePoint* before_previous = first_index < 2 ? nullptr : &points[first_index - 2];
    const CodePoint* next = next_index == points.size() ? nullptr : &points[next_index];
    const CodePoint* after_next =
        next_index + 1 >= points.size() ? nullptr : &points[next_index + 1];

    if (previous && is_lexical_numeric_boundary(previous->value))
        return false;
    if (previous && is_numeric_separator(previous->value) && before_previous &&
        is_digit(before_previous->value))
        return false;
    if (previous && previous->value == '-' && before_previous &&
        is_lexical_numeric_boundary(before_previous->value))
        return false;
    bool suffix_consumed = false;
    for (auto point = last_digit + 1; point != after; ++point) {
        if (!is_digit(point->value)) {
            suffix_consumed = true;
            break;
        }
    }
    if (next && is_lexical_numeric_boundary(next->value) && !suffix_consumed)
        return false;
    if (next && is_numeric_separator(next->value) && after_next && is_digit(after_next->value))
        return false;
    return true;
}

void WarningSink::add_range(WarningCode code,
                            std::string message,
                            std::size_t offset,
                            std::size_t length) {
    warnings.push_back({code, std::move(message), offset, length});
}

void WarningSink::add(WarningCode code, std::string message, SourceRange source) {
    if (source.length != 0)
        preserved_ranges.push_back(source);
    add_range(code, std::move(message), source.offset, source.length);
}

bool try_parse_long(std::string_view token, long long& value) {
    try {
        std::size_t consumed = 0;
        value = std::stoll(std::string(token), &consumed);
        return consumed == token.size() && value >= -999999999 && value <= 999999999;
    } catch (...) {
        return false;
    }
}
std::string marker_for(std::string_view text, std::size_t index);

void add_protected_candidate(const MappedText& text,
                             const NumericCandidate& candidate,
                             WarningSink& warnings,
                             std::vector<ProtectedSpan>& protected_spans,
                             std::vector<SourceEdit>& edits) {
    const auto begin = candidate.span.byte_begin;
    const auto end = candidate.span.byte_end;
    if (begin >= end)
        return;
    if (!edits.empty() && begin < edits.back().end)
        return;
    const auto marker = marker_for(text.text, protected_spans.size());
    warnings.add(WarningCode::UnresolvedNumber,
                 "Unsupported numeric-like candidate preserved verbatim",
                 text.source_range(begin, end));
    protected_spans.push_back({marker, text.text.substr(begin, end - begin)});
    edits.push_back({begin, end, marker});
}

std::string marker_for(std::string_view text, std::size_t index) {
    std::string suffix;
    do {
        suffix.push_back(static_cast<char>('a' + (index % 26)));
        index = index / 26;
    } while (index != 0);
    std::string marker = "\x01tts_front_protected_" + suffix + "\x02";
    while (text.find(marker) != std::string::npos)
        marker.insert(marker.size() - 1, "x");
    return marker;
}
MappedText protect_technical(MappedText text, std::vector<ProtectedSpan>& protected_spans) {
    const auto& patterns = regex_patterns();
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
        MappedText output;
        output.preserved_ranges = text.preserved_ranges;
        std::size_t cursor = 0;
        for (std::sregex_iterator it(text.text.begin(), text.text.end(), *pattern), end; it != end;
             ++it) {
            const auto begin = static_cast<std::size_t>(it->position());
            const auto finish = begin + static_cast<std::size_t>(it->length());
            output.append_copy(text, cursor, begin);
            const auto marker = marker_for(text.text, protected_spans.size());
            protected_spans.push_back({marker, it->str()});
            output.append_generated(text, begin, finish, marker);
            cursor = finish;
        }
        output.append_copy(text, cursor, text.text.size());
        text = std::move(output);
    }
    return text;
}
MappedText restore_technical(MappedText text, const std::vector<ProtectedSpan>& protected_spans) {
    for (auto span = protected_spans.rbegin(); span != protected_spans.rend(); ++span) {
        for (std::size_t at = text.text.find(span->marker); at != std::string::npos;
             at = text.text.find(span->marker, at + span->value.size())) {
            MappedText output;
            output.preserved_ranges = text.preserved_ranges;
            output.append_copy(text, 0, at);
            output.append_generated(text, at, at + span->marker.size(), span->value);
            output.append_copy(text, at + span->marker.size(), text.text.size());
            text = std::move(output);
        }
    }
    return text;
}

// Technical shielding intentionally runs before the general numeric pass, but
// numeric-leading scientific/encoded candidates must be admitted first.  If
// they are hidden as (for example) `e+3` or `$3`, the generic normalizer can
// rewrite only the leading numeric fragment.
MappedText protect_numeric_technical_candidates(MappedText text,
                                                WarningSink& warnings,
                                                std::vector<ProtectedSpan>& protected_spans) {
    // Admit grouped percentage tails attached to a technical identifier before
    // technical shielding hides the identifier itself.  Otherwise a token such
    // as `x$+1,234%` would leave `,234%` visible to the generic normalizer.
    {
        std::vector<SourceEdit> edits;
        std::size_t cursor = 0;
        const Utf8Document document(text.text);
        for (std::sregex_iterator
                 it(text.text.begin(), text.text.end(), regex_patterns().technical_numeric_percent),
             end;
             it != end;
             ++it) {
            const auto begin = static_cast<std::size_t>(it->position());
            if (begin < cursor)
                continue;
            const auto base_finish = begin + static_cast<std::size_t>(it->length());
            const auto base = it->str(1);
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
                               is_horizontal_space(text.text[after_connector]))
                            ++after_connector;
                        if (after_connector >= text.text.size() ||
                            !is_ascii_digit(text.text[after_connector]))
                            break;
                        probe += unicode_length;
                    } else if (value != '%' &&
                               (is_ascii_punctuation(value) || is_horizontal_space(value))) {
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
                while (connector < text.text.size() && is_horizontal_space(text.text[connector]))
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
                           is_horizontal_space(text.text[after_connector]))
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

    std::vector<SourceEdit> edits;
    for (std::size_t index = 0; index < points.size();) {
        if (index > 0 && is_digit(points[index].value) &&
            (points[index - 1].value == 0x20ac || points[index - 1].value == 0xa3 ||
             points[index - 1].value == 0xa5)) {
            auto numeric_end = index + 1;
            while (numeric_end < points.size() && is_digit(points[numeric_end].value))
                ++numeric_end;
            NumericCandidate candidate{document.span_from_codepoints(index - 1, numeric_end),
                                       NumericCandidateKind::Currency};
            add_protected_candidate(text, candidate, warnings, protected_spans, edits);
            index = numeric_end;
            continue;
        }
        if (points[index].value == '$' && index + 1 < points.size() &&
            is_digit(points[index + 1].value)) {
            auto numeric_end = index + 1;
            while (numeric_end < points.size() && is_digit(points[numeric_end].value))
                ++numeric_end;
            while (numeric_end < points.size() &&
                   (points[numeric_end].value == ',' || points[numeric_end].value == '.') &&
                   numeric_end + 1 < points.size() && is_digit(points[numeric_end + 1].value)) {
                numeric_end += 1;
                while (numeric_end < points.size() && is_digit(points[numeric_end].value))
                    ++numeric_end;
            }
            if (numeric_end < points.size() && points[numeric_end].value == '%') {
                const auto protected_end =
                    scan_numeric_continuation_points(document, numeric_end + 1, false);
                NumericCandidate candidate{document.span_from_codepoints(index, protected_end),
                                           NumericCandidateKind::Currency,
                                           true,
                                           false,
                                           true};
                add_protected_candidate(text, candidate, warnings, protected_spans, edits);
                index = protected_end;
                continue;
            }
        }
        const bool starts_number = is_digit(points[index].value) ||
                                   (points[index].value == '-' && index + 1 < points.size() &&
                                    is_digit(points[index + 1].value));
        if (!starts_number || (index > 0 && is_lexical_numeric_boundary(points[index - 1].value))) {
            ++index;
            continue;
        }

        std::size_t end_index = index + (points[index].value == '-' ? 1 : 0);
        while (end_index < points.size() && is_digit(points[end_index].value))
            ++end_index;
        if (end_index < points.size() &&
            (points[end_index].value == '.' || points[end_index].value == ',' ||
             points[end_index].value == ':') &&
            end_index + 1 < points.size() && is_digit(points[end_index + 1].value)) {
            ++end_index;
            while (end_index < points.size() && is_digit(points[end_index].value))
                ++end_index;
        }

        std::size_t suffix_end = end_index;
        bool encoded_suffix = false;
        if (end_index < points.size() &&
            (points[end_index].value == 'e' || points[end_index].value == 'E')) {
            auto probe = end_index + 1;
            if (probe < points.size() && (points[probe].value == '+' || points[probe].value == '-'))
                ++probe;
            const auto exponent_begin = probe;
            while (probe < points.size() && is_digit(points[probe].value))
                ++probe;
            if (probe != exponent_begin) {
                suffix_end = probe;
                encoded_suffix = true;
            }
        } else if (end_index < points.size() &&
                   (points[end_index].value == '#' || points[end_index].value == '$')) {
            auto probe = end_index + 1;
            const auto suffix_begin = probe;
            while (probe < points.size() &&
                   (is_digit(points[probe].value) || is_letter(points[probe].value) ||
                    is_combining_mark(points[probe].value) || points[probe].value == '_'))
                ++probe;
            if (probe != suffix_begin) {
                suffix_end = probe;
                encoded_suffix = true;
            }
        }

        if (!encoded_suffix) {
            ++index;
            continue;
        }

        NumericCandidate candidate{
            document.span_from_codepoints(index, suffix_end), NumericCandidateKind::Numeric, true};
        add_protected_candidate(text, candidate, warnings, protected_spans, edits);
        index = suffix_end;
    }
    return apply_source_edits(text, std::move(edits));
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

MappedText protect_malformed_numeric_candidates(MappedText text,
                                                WarningSink& warnings,
                                                std::vector<ProtectedSpan>& protected_spans,
                                                bool russian) {
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
        if (end_index < points.size() &&
            (points[end_index].value == ' ' || points[end_index].value == '\t')) {
            auto probe = end_index;
            while (probe < points.size() &&
                   (points[probe].value == ' ' || points[probe].value == '\t'))
                ++probe;
            const auto unit_begin = probe;
            while (probe < points.size() && is_letter(points[probe].value))
                ++probe;
            if (probe != unit_begin) {
                while (probe < points.size() &&
                       (points[probe].value == ' ' || points[probe].value == '\t'))
                    ++probe;
                if (probe < points.size() &&
                    (points[probe].value == '+' || points[probe].value == '-') &&
                    probe + 1 < points.size() && is_digit(points[probe + 1].value)) {
                    malformed_compound = true;
                    end_index = probe + 1;
                    while (end_index < points.size() &&
                           (is_digit(points[end_index].value) ||
                            is_numeric_connector(points[end_index].value) ||
                            is_letter(points[end_index].value) || points[end_index].value == ' ' ||
                            points[end_index].value == '\t'))
                        ++end_index;
                }
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
            !russian && (std::regex_match(candidate, regex_patterns().en_comma_grouped_value) ||
                         std::regex_match(candidate, regex_patterns().en_comma_grouped_percent));
        const bool malformed_english_comma_group =
            !russian && candidate.find(',') != std::string::npos;
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
        if (russian && separators == 2) {
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

MappedText collapse_grouped_numbers(const MappedText& input) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    for (std::sregex_iterator
             it(input.text.begin(), input.text.end(), regex_patterns().grouped_number),
         end;
         it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        const auto number_begin = begin + it->length(1);
        output.append_copy(input, cursor, number_begin);
        std::string number = it->str().substr(it->length(1));
        number.erase(std::remove_if(number.begin(),
                                    number.end(),
                                    [](unsigned char c) { return std::isspace(c) != 0; }),
                     number.end());
        output.append_generated(input, number_begin, finish, number);
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

MappedText collapse_english_comma_grouped_numbers(const MappedText& input) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    std::vector<CodePoint> points;
    if (!decode_utf8(input.text, points))
        return input;
    for (std::sregex_iterator
             it(input.text.begin(), input.text.end(), regex_patterns().en_comma_grouped_number),
         end;
         it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        const auto number_begin = begin + static_cast<std::size_t>(it->length(1));
        if (!numeric_match_has_valid_boundaries(points, begin, finish))
            continue;
        output.append_copy(input, cursor, number_begin);
        std::string number = it->str(2);
        number.erase(std::remove(number.begin(), number.end(), ','), number.end());
        output.append_generated(input, number_begin, finish, number);
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

} // namespace tts_front::detail
