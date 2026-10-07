#include "tts_front.hpp"
#include "tts_front/backend/silero/silero_stress_backend.hpp"
#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/source_span.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/english/numbers.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/normalization/candidate_scanner.hpp"
#include "tts_front/normalization/patterns.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tts_front {
namespace {

using CodePoint = detail::Utf8CodePoint;
using detail::apply_source_edits;
using detail::codepoint_index_at_or_after;
using detail::decode_utf8;
using detail::MappedText;
using detail::NumericCandidate;
using detail::NumericCandidateKind;
using detail::regex_patterns;
using detail::scan_numeric_continuation_points;
using detail::SourceEdit;
using detail::SourceRange;
using detail::SourceSpan;
using detail::Utf8Document;
using detail::english::digits;
using detail::english::number;
using detail::english::ordinal;
using detail::english::ordinal_suffix;
using detail::russian::ru_decimal;
using detail::russian::ru_feminine_number;
using detail::russian::ru_form;
using detail::russian::ru_number;
using detail::russian::ru_ordinal_day;
using detail::russian::ru_year_genitive;
using detail::russian::ru_year_locative;

bool is_cyrillic(std::uint32_t cp) {
    return cp >= 0x0400 && cp <= 0x052f;
}
bool is_latin(std::uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= 0x00c0 && cp <= 0x024f);
}
bool is_digit(std::uint32_t cp) {
    return cp >= '0' && cp <= '9';
}
bool is_letter(std::uint32_t cp) {
    return is_cyrillic(cp) || is_latin(cp);
}
bool is_combining_mark(std::uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
           (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
           (cp >= 0xfe20 && cp <= 0xfe2f);
}
bool is_word_codepoint(std::uint32_t cp) {
    return is_letter(cp) || is_digit(cp) || cp == '+' || cp == '#' || cp == '_';
}

bool is_numeric_separator(std::uint32_t cp) {
    return cp == '.' || cp == ',' || cp == ':' || cp == '%' || cp == '-' || cp == '/' ||
           cp == '+' || cp == '=' || (cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212;
}

bool is_range_connector(std::uint32_t cp) {
    return (cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212;
}

bool is_numeric_connector(std::uint32_t cp) {
    return is_numeric_separator(cp) ||
           (cp < 0x80 && std::ispunct(static_cast<unsigned char>(cp)) != 0);
}

bool is_supported_numeric_separator(std::uint32_t cp) {
    return cp == '.' || cp == ',' || cp == ':' || cp == '%';
}

bool is_lexical_numeric_boundary(std::uint32_t cp) {
    return is_letter(cp) || is_combining_mark(cp) || is_digit(cp) || cp == '_';
}

bool is_horizontal_space(std::uint32_t cp) {
    return cp == ' ' || cp == '\t';
}

bool is_ascii_punctuation(std::uint32_t cp) {
    return cp < 0x80 && std::ispunct(static_cast<unsigned char>(cp)) != 0;
}

MappedText cleanup_text(const MappedText& input) {
    MappedText result;
    result.preserved_ranges = input.preserved_ranges;
    result.text.reserve(input.text.size());
    std::vector<CodePoint> points;
    if (!decode_utf8(input.text, points))
        return input;
    bool pending_space = false;
    std::size_t space_begin = 0;
    for (const auto& point : points) {
        const bool space = point.value == ' ' || point.value == '\t' || point.value == '\n' ||
                           point.value == '\r' || point.value == '\v' || point.value == '\f' ||
                           point.value == 0x00a0 || point.value == 0x202f;
        if (space) {
            if (!pending_space && !result.text.empty())
                space_begin = point.offset;
            pending_space = !result.text.empty();
            continue;
        }
        const bool punctuation = point.value == ',' || point.value == '.' || point.value == ';' ||
                                 point.value == ':' || point.value == '!' || point.value == '?';
        if (pending_space && !punctuation && !result.text.empty())
            result.append_generated(input, space_begin, point.offset, " ");
        pending_space = false;
        result.append_copy(input, point.offset, point.offset + point.length);
    }
    return result;
}

template <typename Formatter>
MappedText
replace_matches(const MappedText& input, const std::regex& pattern, Formatter formatter) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), pattern), end; it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        output.append_copy(input, cursor, begin);
        const auto source = input.source_range(begin, finish);
        if (input.preserved_ranges &&
            std::any_of(input.preserved_ranges->begin(),
                        input.preserved_ranges->end(),
                        [source](const SourceRange& preserved) {
                            return source.offset < preserved.offset + preserved.length &&
                                   preserved.offset < source.offset + source.length;
                        })) {
            output.append_copy(input, begin, finish);
            cursor = finish;
            continue;
        }
        output.append_generated(input, begin, finish, formatter(*it, input));
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

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

template <typename Formatter>
MappedText
replace_numeric_matches(const MappedText& input, const std::regex& pattern, Formatter formatter) {
    std::vector<CodePoint> points;
    if (!decode_utf8(input.text, points))
        return input;
    return replace_matches(input, pattern, [&](const std::smatch& match, const MappedText& source) {
        const auto begin = static_cast<std::size_t>(match.position());
        const auto end = begin + static_cast<std::size_t>(match.length());
        if (!numeric_match_has_valid_boundaries(points, begin, end))
            return match.str();
        return formatter(match, source);
    });
}

struct WarningSink {
    std::vector<TextWarning>& warnings;
    std::vector<SourceRange> preserved_ranges;

    void add_range(WarningCode code, std::string message, std::size_t offset, std::size_t length) {
        warnings.push_back({code, std::move(message), offset, length});
    }

    void add(WarningCode code, std::string message, SourceRange source) {
        if (source.length != 0)
            preserved_ranges.push_back(source);
        add_range(code, std::move(message), source.offset, source.length);
    }
};

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match) {
    const auto begin = static_cast<std::size_t>(match.position());
    warnings.add(code, std::move(message), text.source_range(begin, begin + match.length()));
}

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match,
                 std::size_t group) {
    const auto begin = static_cast<std::size_t>(match.position(group));
    warnings.add(code, std::move(message), text.source_range(begin, begin + match.length(group)));
}

void add_warning_span(WarningSink& warnings,
                      WarningCode code,
                      std::string message,
                      const MappedText& text,
                      const std::smatch& match,
                      std::size_t begin_group,
                      std::size_t end_group) {
    const auto begin = static_cast<std::size_t>(match.position(begin_group));
    const auto end = static_cast<std::size_t>(match.position(end_group)) +
                     static_cast<std::size_t>(match.length(end_group));
    warnings.add(code, std::move(message), text.source_range(begin, end));
}

void add_warning_without_suffix(WarningSink& warnings,
                                WarningCode code,
                                std::string message,
                                const MappedText& text,
                                const std::smatch& match,
                                std::size_t suffix_group) {
    const auto begin = static_cast<std::size_t>(match.position());
    const auto length = static_cast<std::size_t>(match.length() - match.length(suffix_group));
    warnings.add(code, std::move(message), text.source_range(begin, begin + length));
}

void add_warning(std::vector<TextWarning>& warnings,
                 WarningCode code,
                 std::string message,
                 std::size_t offset,
                 std::size_t length) {
    warnings.push_back({code, std::move(message), offset, length});
}

std::string en_number(long long n) {
    return number(n);
}
const char* en_ordinal_suffix(long long n) {
    return ordinal_suffix(n);
}
std::string en_ordinal(long long n) {
    return ordinal(n);
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
std::string number_or_original(const std::string& token,
                               bool russian,
                               WarningSink& warnings,
                               SourceRange source) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        warnings.add(WarningCode::UnresolvedNumber, "Unable to parse number", source);
        return token;
    }
    return russian ? ru_number(value) : en_number(value);
}
std::string en_digits(const std::string& digits) {
    return detail::english::digits(digits);
}

struct ProtectedSpan {
    std::string marker;
    std::string value;
};

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
                    break;
                }
                if (is_numeric_connector(value)) {
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
        if (currency_context && has_percent)
            end_index = scan_numeric_continuation_points(document, end_index, false);
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
             (currency_prefix && (has_percent || currency_probe < index ||
                                  points[currency_probe - 1].value != '$'))) &&
            !(valid_russian_date && !embedded) &&
            !(valid_english_comma_group && !embedded && !attached_lexical_suffix &&
              !(currency_prefix && has_percent))) {
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

MappedText normalize_ru(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_numeric_technical_candidates(std::move(text), warnings, protected_spans);
    text = protect_technical(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(std::move(text), warnings, protected_spans, true);
    text = collapse_grouped_numbers(std::move(text));
    text = replace_numeric_matches(
        text, regex_patterns().ru_date, [&](const std::smatch& match, const MappedText& source) {
            long long day = 0, month = 0, year = 0;
            if (!try_parse_long(match[1].str(), day) || !try_parse_long(match[2].str(), month) ||
                !try_parse_long(match[3].str(), year) ||
                !valid_date(
                    static_cast<int>(day), static_cast<int>(month), static_cast<int>(year))) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid Russian calendar date",
                            source,
                            match);
                return match.str();
            }
            static const char* const months[] = {"",
                                                 "января",
                                                 "февраля",
                                                 "марта",
                                                 "апреля",
                                                 "мая",
                                                 "июня",
                                                 "июля",
                                                 "августа",
                                                 "сентября",
                                                 "октября",
                                                 "ноября",
                                                 "декабря"};
            return ru_ordinal_day(static_cast<int>(day)) + " " + months[month] + " " +
                   ru_year_genitive(static_cast<int>(year)) + " года";
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_year, [&](const std::smatch& match, const MappedText& source) {
            long long year = 0;
            if (!try_parse_long(match[1].str(), year)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian year",
                            source,
                            match);
                return match.str();
            }
            return ru_year_locative(static_cast<int>(year)) + " году";
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().ru_decimal_percent,
        [&](const std::smatch& match, const MappedText& source) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian decimal percent",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            return ru_decimal(integer, match[2].str()) + " процента" + match[3].str();
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_percent, [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian percent",
                            source,
                            match);
                return match.str();
            }
            return ru_number(n) + " " + ru_form(n, "процент", "процента", "процентов");
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().ru_currency,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian currency",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            return ru_number(n) + " " + ru_form(n, "рубль", "рубля", "рублей") + match[3].str();
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_time, [&](const std::smatch& match, const MappedText& source) {
            long long h = 0, m = 0;
            if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m) ||
                h > 23 || m > 59) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid Russian clock time",
                            source,
                            match);
                return match.str();
            }
            return ru_number(h) + " " + ru_form(h, "час", "часа", "часов") + " " +
                   ru_feminine_number(m) + " " + ru_form(m, "минута", "минуты", "минут");
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_decimal, [&](const std::smatch& match, const MappedText& source) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian decimal",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            return ru_decimal(integer, match[2].str()) + match[3].str();
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().ru_measurement,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian measurement",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            const auto unit_source = match[2].str();
            const std::string unit =
                unit_source.find("кг") == 0 || unit_source.find("килограмм") == 0
                    ? ru_form(n, "килограмм", "килограмма", "килограммов")
                : unit_source.find("км") == 0 || unit_source.find("километр") == 0
                    ? ru_form(n, "километр", "километра", "километров")
                : unit_source.find("см") == 0 || unit_source.find("сантиметр") == 0
                    ? ru_form(n, "сантиметр", "сантиметра", "сантиметров")
                : unit_source.find("мм") == 0 || unit_source.find("миллиметр") == 0
                    ? ru_form(n, "миллиметр", "миллиметра", "миллиметров")
                : unit_source == "м" ? ru_form(n, "метр", "метра", "метров")
                : unit_source == "ГБ" ? ru_form(n, "гигабайт", "гигабайта", "гигабайт")
                                      : ru_form(n, "мегабайт", "мегабайта", "мегабайт");
            return ru_number(n) + " " + unit + match[3].str();
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().generic_ru_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(match[2].str(),
                                      true,
                                      warnings,
                                      source.source_range(number_begin, number_end));
        });
    text = replace_matches(
        std::move(text),
        regex_patterns().ru_abbreviation_td,
        [](const auto& match, const MappedText&) { return match[1].str() + "так далее"; });
    text = replace_matches(
        std::move(text),
        regex_patterns().ru_abbreviation_tp,
        [](const auto& match, const MappedText&) { return match[1].str() + "тому подобное"; });
    return restore_technical(std::move(text), protected_spans);
}

MappedText normalize_en(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_numeric_technical_candidates(std::move(text), warnings, protected_spans);
    text = protect_technical(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(std::move(text), warnings, protected_spans, false);
    text = collapse_english_comma_grouped_numbers(std::move(text));
    text = collapse_grouped_numbers(std::move(text));
    text =
        replace_numeric_matches(text,
                                regex_patterns().en_currency_decimal,
                                [&](const std::smatch& match, const MappedText& source) {
                                    long long dollars = 0, cents = 0;
                                    const auto fraction = match[2].str();
                                    if (!try_parse_long(match[1].str(), dollars) ||
                                        fraction.size() > 2 || !try_parse_long(fraction, cents)) {
                                        add_warning(warnings,
                                                    WarningCode::UnresolvedNumber,
                                                    "Unable to parse English currency",
                                                    source,
                                                    match);
                                        return match.str();
                                    }
                                    if (fraction.size() == 1)
                                        cents *= 10;
                                    return en_number(dollars) +
                                           (dollars == 1 ? " dollar" : " dollars") + " " +
                                           en_number(cents) + (cents == 1 ? " cent" : " cents");
                                });
    text = replace_numeric_matches(text,
                                   regex_patterns().en_currency_integer,
                                   [&](const std::smatch& match, const MappedText& source) {
                                       long long dollars = 0;
                                       if (!try_parse_long(match[1].str(), dollars)) {
                                           add_warning(warnings,
                                                       WarningCode::UnresolvedNumber,
                                                       "Unable to parse English currency",
                                                       source,
                                                       match);
                                           return match.str();
                                       }
                                       return en_number(dollars) +
                                              (dollars == 1 ? " dollar" : " dollars");
                                   });
    text = replace_numeric_matches(
        text, regex_patterns().en_ordinal, [&](const std::smatch& match, const MappedText& source) {
            long long value = 0;
            const auto suffix = match[3].str();
            if (!try_parse_long(match[2].str(), value) || suffix != en_ordinal_suffix(value)) {
                add_warning_span(warnings,
                                 WarningCode::UnresolvedNumber,
                                 "Unable to parse English ordinal",
                                 source,
                                 match,
                                 2,
                                 3);
                return match.str();
            }
            return match[1].str() + en_ordinal(value);
        });
    text = replace_numeric_matches(
        text, regex_patterns().en_percent, [&](const std::smatch& match, const MappedText& source) {
            const auto value = match[1].str();
            const auto dot = value.find('.');
            long long integer = 0;
            if (!try_parse_long(dot == std::string::npos ? value : value.substr(0, dot), integer)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English percent",
                            source,
                            match);
                return match.str();
            }
            return (dot == std::string::npos
                        ? en_number(integer)
                        : en_number(integer) + " point " + en_digits(value.substr(dot + 1))) +
                   " percent";
        });
    text = replace_numeric_matches(
        text, regex_patterns().en_time, [&](const std::smatch& match, const MappedText& source) {
            long long h = 0, m = 0;
            if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m) ||
                h > 23 || m > 59) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid English clock time",
                            source,
                            match);
                return match.str();
            }
            return en_number(h) + (h == 1 ? " hour " : " hours ") + en_number(m) +
                   (m == 1 ? " minute" : " minutes");
        });
    text = replace_numeric_matches(
        text, regex_patterns().en_decimal, [&](const std::smatch& match, const MappedText& source) {
            const auto value = match[2].str();
            const auto dot = value.find('.');
            long long integer = 0;
            if (!try_parse_long(value.substr(0, dot), integer)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English decimal",
                            source,
                            match,
                            2);
                return match.str();
            }
            return match[1].str() + en_number(integer) + " point " +
                   en_digits(value.substr(dot + 1));
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().en_measurement,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse English measurement",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            const auto unit = match[2].str();
            const bool singular = n == 1 || n == -1;
            const std::string spoken = unit == "kg" || unit.find("kilogram") == 0
                                           ? (singular ? "kilogram" : "kilograms")
                                       : unit == "km" || unit.find("kilomet") == 0
                                           ? (singular ? "kilometer" : "kilometers")
                                       : unit == "m" || unit == "meters" || unit == "metres"
                                           ? (singular ? "meter" : "meters")
                                       : unit == "cm" || unit.find("centimet") == 0
                                           ? (singular ? "centimeter" : "centimeters")
                                       : unit == "mm" || unit.find("millimet") == 0
                                           ? (singular ? "millimeter" : "millimeters")
                                       : unit == "MB" ? (singular ? "megabyte" : "megabytes")
                                                      : (singular ? "gigabyte" : "gigabytes");
            return en_number(n) + " " + spoken + match[3].str();
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().generic_en_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(match[2].str(),
                                      false,
                                      warnings,
                                      source.source_range(number_begin, number_end));
        });
    return restore_technical(std::move(text), protected_spans);
}

struct Span {
    std::size_t begin = 0;
    std::size_t end = 0;
};
std::vector<Span> token_spans(const std::string& text) {
    std::vector<CodePoint> points;
    if (!decode_utf8(text, points))
        return {};
    std::vector<Span> spans;
    std::size_t begin = std::string::npos;
    std::size_t end = 0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& point = points[index];
        const bool embedded_dot = point.value == '.' && index > 0 && index + 1 < points.size() &&
                                  is_digit(points[index - 1].value) &&
                                  is_digit(points[index + 1].value);
        if (is_word_codepoint(point.value) || embedded_dot) {
            if (begin == std::string::npos)
                begin = point.offset;
            end = point.offset + point.length;
        } else if (begin != std::string::npos) {
            spans.push_back({begin, end});
            begin = std::string::npos;
        }
    }
    if (begin != std::string::npos)
        spans.push_back({begin, end});
    return spans;
}

// Automatic expansion is deliberately an allowlist.  Uppercase-looking text
// is not enough evidence that a token should be spelled out letter by letter:
// lexical acronyms such as НАТО, МИД and ЗАГС must remain unchanged unless a
// caller supplies an explicit dictionary entry.
std::optional<std::string> safe_russian_initialism(std::string_view token) {
    if (token == "ВК")
        return "вэ ка";
    if (token == "ООО")
        return "о о о";
    if (token == "РФ")
        return "эр эф";
    if (token == "МГУ")
        return "эм гэ у";
    if (token == "ФСБ")
        return "эф эс бэ";
    if (token == "МФЦ")
        return "эм эф цэ";
    if (token == "ИП")
        return "и пэ";
    return std::nullopt;
}
Language detect_language(std::string_view text, bool& has_cyrillic, bool& has_latin) {
    std::vector<CodePoint> points;
    decode_utf8(text, points);
    has_cyrillic = std::any_of(
        points.begin(), points.end(), [](const CodePoint& p) { return is_cyrillic(p.value); });
    has_latin = std::any_of(
        points.begin(), points.end(), [](const CodePoint& p) { return is_latin(p.value); });
    return has_cyrillic ? Language::Russian : Language::English;
}

[[maybe_unused]] std::optional<std::filesystem::path>
resolve_silero_bundle(const TextFrontendOptions& options) {
    if (!options.silero_bundle_path.empty())
        return std::filesystem::path(options.silero_bundle_path);
    if (const auto* environment = std::getenv("TTS_FRONT_SILERO_BUNDLE");
        environment != nullptr && *environment != '\0')
        return std::filesystem::path(environment);
    return std::nullopt;
}

} // namespace

struct TextFrontend::Impl {
    std::mutex silero_mutex;
    std::unordered_map<std::string, std::shared_ptr<detail::SileroStressBackend>> silero_backends;

    std::shared_ptr<detail::SileroStressBackend>
    backend_for(const std::filesystem::path& bundle_root) {
        std::error_code error;
        const auto resolved_root = std::filesystem::absolute(bundle_root, error);
        const auto stable_root = (error ? bundle_root : resolved_root).lexically_normal();
        const auto key = stable_root.string();
        std::lock_guard lock(silero_mutex);
        if (const auto found = silero_backends.find(key); found != silero_backends.end())
            return found->second;
        auto backend = std::make_shared<detail::SileroStressBackend>(
            detail::SileroStressBackendConfig{stable_root});
        silero_backends.emplace(key, backend);
        return backend;
    }
};

TextFrontend::TextFrontend() : impl_(std::make_shared<Impl>()) {}
TextFrontend::~TextFrontend() = default;

bool TextFrontendResult::has_uncertainty() const noexcept {
    return !warnings.empty();
}

TextFrontendResult TextFrontend::process(std::string_view input,
                                         const TextFrontendOptions& options) const {
    TextFrontendResult result;
    result.original_text = std::string(input);
    std::vector<CodePoint> points;
    if (!decode_utf8(input, points)) {
        add_warning(
            result.warnings, WarningCode::InvalidUtf8, "Input is not valid UTF-8", 0, input.size());
        return result;
    }
    WarningSink warning_sink{result.warnings, {}};
    MappedText text = MappedText::from_original(input, &warning_sink.preserved_ranges);
    if (options.cleanup_spacing)
        text = cleanup_text(text);
    bool has_cyrillic = false;
    bool has_latin = false;
    Language language = options.language;
    if (language == Language::Auto)
        language = detect_language(text.text, has_cyrillic, has_latin);
    if (options.language == Language::Auto && has_cyrillic && has_latin)
        warning_sink.add_range(WarningCode::AmbiguousNormalization,
                               "Mixed Cyrillic/Latin input uses Russian normalization by policy",
                               0,
                               input.size());
    if (language != Language::Russian && language != Language::English) {
        result.warnings.push_back({WarningCode::UnsupportedLanguage, "Unsupported language", 0, 0});
        return result;
    }
    if (options.normalize) {
        switch (language) {
        case Language::Russian:
            text = normalize_ru(std::move(text), warning_sink);
            break;
        case Language::English:
            text = normalize_en(std::move(text), warning_sink);
            break;
        case Language::Auto:
            break;
        }
    }
    if (options.cleanup_spacing)
        text = cleanup_text(text);
    result.normalized_text = text.text;
    result.pronunciation_text = text.text;
    const bool stress_enabled =
        options.resolve_stress && options.stress_mode != StressMode::Disabled;

    // Dictionary phrases are matched on complete token sequences, longest first, without rescanning output.
    std::vector<const PronunciationDictionary::Entry*> matched_entries;
    const auto spans = token_spans(text.text);
    matched_entries.assign(spans.size(), nullptr);
    std::vector<std::string> automatic_replacements(spans.size());
    if ((options.apply_dictionary && options.dictionary) || options.expand_initialisms) {
        struct PhraseCandidate {
            const PronunciationDictionary::Entry* entry = nullptr;
            std::vector<Span> spans;
        };
        std::vector<PhraseCandidate> phrases;
        if (options.apply_dictionary && options.dictionary) {
            for (const auto& entry : options.dictionary->entries()) {
                if (entry.match == PronunciationDictionary::Match::ExactPhrase)
                    phrases.push_back({&entry, token_spans(entry.pattern)});
            }
        }
        std::string rendered;
        std::size_t cursor = 0;
        for (std::size_t i = 0; i < spans.size();) {
            const PronunciationDictionary::Entry* best = nullptr;
            std::size_t best_end = i;
            for (const auto& phrase : phrases) {
                const auto& entry = *phrase.entry;
                const auto& phrase_spans = phrase.spans;
                if (phrase_spans.empty() || i + phrase_spans.size() > spans.size())
                    continue;
                const auto input_phrase = text.text.substr(
                    spans[i].begin, spans[i + phrase_spans.size() - 1].end - spans[i].begin);
                const auto pattern_phrase =
                    entry.pattern.substr(phrase_spans.front().begin,
                                         phrase_spans.back().end - phrase_spans.front().begin);
                const bool match = input_phrase == pattern_phrase;
                if (match && (!best || phrase_spans.size() > best_end - i)) {
                    best = &entry;
                    best_end = i + phrase_spans.size();
                }
            }
            const auto token = text.text.substr(spans[i].begin, spans[i].end - spans[i].begin);
            if (best) {
                for (std::size_t j = i; j < best_end; ++j)
                    matched_entries[j] = best;
                rendered.append(text.text, cursor, spans[i].begin - cursor);
                rendered += best->pronunciation;
                const auto phrase_surface =
                    text.text.substr(spans[i].begin, spans[best_end - 1].end - spans[i].begin);
                result.dictionary_replacements.push_back(
                    {phrase_surface, best->pronunciation, spans[i].begin});
                if (options.resolve_stress && options.stress_mode != StressMode::Disabled &&
                    best->stressed_vowel)
                    result.stress_decisions.push_back({phrase_surface,
                                                       best->stressed_vowel,
                                                       true,
                                                       "pronunciation dictionary phrase"});
                cursor = spans[best_end - 1].end;
                i = best_end;
                continue;
            }
            rendered.append(text.text, cursor, spans[i].begin - cursor);
            const auto* entry = options.apply_dictionary && options.dictionary
                                    ? options.dictionary->find_token(token)
                                    : nullptr;
            if (entry) {
                matched_entries[i] = entry;
                rendered += entry->pronunciation;
                if (entry->pronunciation != token)
                    result.dictionary_replacements.push_back(
                        {token, entry->pronunciation, spans[i].begin});
            } else if (options.expand_initialisms && language == Language::Russian) {
                if (const auto replacement = safe_russian_initialism(token)) {
                    automatic_replacements[i] = *replacement;
                    rendered += *replacement;
                    result.automatic_rewrites.push_back(
                        {token, *replacement, spans[i].begin, "safe Russian initialism", true});
                } else {
                    rendered += token;
                }
            } else {
                rendered += token;
            }
            cursor = spans[i].end;
            ++i;
        }
        rendered += text.text.substr(cursor);
        result.pronunciation_text = std::move(rendered);
    }

    const auto normalized_spans = token_spans(result.normalized_text);
    for (std::size_t span_index = 0; span_index < normalized_spans.size(); ++span_index) {
        const auto& span = normalized_spans[span_index];
        const std::string token = result.normalized_text.substr(span.begin, span.end - span.begin);
        WordPronunciation word;
        word.surface = token;
        word.source_offset = span.begin;
        for (std::size_t replacement_index = 0;
             replacement_index < result.dictionary_replacements.size();
             ++replacement_index) {
            const auto& replacement = result.dictionary_replacements[replacement_index];
            const auto replacement_end = replacement.offset + replacement.input.size();
            if (span.begin >= replacement.offset && span.end <= replacement_end) {
                word.dictionary_replacement = replacement_index;
                word.from_dictionary = true;
                if (span.begin == replacement.offset)
                    word.pronunciation = replacement.output;
                break;
            }
        }
        const auto* matched_entry =
            options.dictionary && options.apply_dictionary && span_index < matched_entries.size()
                ? matched_entries[span_index]
                : nullptr;
        if (matched_entry) {
            if (matched_entry->match != PronunciationDictionary::Match::ExactPhrase) {
                word.from_dictionary = true;
                word.pronunciation = matched_entry->pronunciation;
                if (options.resolve_stress && options.stress_mode != StressMode::Disabled)
                    word.stressed_vowel = matched_entry->stressed_vowel;
            }
        }
        if (!matched_entry && span_index < automatic_replacements.size() &&
            !automatic_replacements[span_index].empty()) {
            word.pronunciation = automatic_replacements[span_index];
            word.from_automatic_rewrite = true;
        }
        result.words.push_back(word);
        if (stress_enabled && matched_entry &&
            matched_entry->match == PronunciationDictionary::Match::ExactPhrase)
            continue;
        if (stress_enabled && (options.diagnostics || word.from_dictionary))
            result.stress_decisions.push_back({token,
                                               word.stressed_vowel,
                                               word.from_dictionary,
                                               word.from_dictionary
                                                   ? "pronunciation dictionary"
                                                   : "no deterministic stress rule"});
    }
    if (options.resolve_stress && options.stress_mode == StressMode::Automatic) {
        const auto add_unavailable_warning = [&](std::string message) {
            result.warnings.push_back(
                {WarningCode::AutomaticStressUnavailable, std::move(message), 0, 0});
        };
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
        const auto bundle_root = resolve_silero_bundle(options);
        if (!bundle_root) {
            add_unavailable_warning(
                "Automatic stress bundle is not configured (set TTS_FRONT_SILERO_BUNDLE or "
                "TextFrontendOptions::silero_bundle_path)");
        } else {
            const auto deterministic_pronunciation = result.pronunciation_text;
            const auto deterministic_words = result.words;
            const auto deterministic_stress_decisions = result.stress_decisions;
            try {
                struct ProtectedRewrite {
                    std::size_t begin;
                    std::size_t end;
                    std::string output;
                };
                std::vector<ProtectedRewrite> protected_rewrites;
                for (std::size_t index = 0; index < normalized_spans.size();) {
                    const auto* entry =
                        index < matched_entries.size() ? matched_entries[index] : nullptr;
                    if (entry) {
                        std::size_t end = index + 1;
                        if (entry->match == PronunciationDictionary::Match::ExactPhrase) {
                            while (end < matched_entries.size() && matched_entries[end] == entry)
                                ++end;
                        }
                        protected_rewrites.push_back({normalized_spans[index].begin,
                                                      normalized_spans[end - 1].end,
                                                      entry->pronunciation});
                        index = end;
                    } else if (index < automatic_replacements.size() &&
                               !automatic_replacements[index].empty()) {
                        protected_rewrites.push_back({normalized_spans[index].begin,
                                                      normalized_spans[index].end,
                                                      automatic_replacements[index]});
                        ++index;
                    } else {
                        ++index;
                    }
                }

                std::string protected_input;
                std::size_t cursor = 0;
                for (const auto& rewrite : protected_rewrites) {
                    protected_input.append(result.normalized_text, cursor, rewrite.begin - cursor);
                    protected_input.append(rewrite.end - rewrite.begin, '\x01');
                    cursor = rewrite.end;
                }
                protected_input.append(
                    result.normalized_text, cursor, result.normalized_text.size() - cursor);

                const auto backend = impl_->backend_for(*bundle_root);
                const auto semantic = backend->process(protected_input);
                auto automatic_words = result.words;
                auto automatic_stress_decisions = result.stress_decisions;
                for (const auto& word : semantic.words) {
                    const auto span_index = std::find_if(
                        normalized_spans.begin(), normalized_spans.end(), [&](const Span span) {
                            return span.begin == word.source_offset;
                        });
                    if (span_index == normalized_spans.end())
                        continue;
                    const auto index =
                        static_cast<std::size_t>(span_index - normalized_spans.begin());
                    if (index >= automatic_words.size() || automatic_words[index].from_dictionary ||
                        automatic_words[index].from_automatic_rewrite)
                        continue;
                    automatic_words[index].pronunciation = word.pronunciation;
                    automatic_words[index].stressed_vowel = word.stressed_vowel;
                    automatic_stress_decisions.push_back({automatic_words[index].surface,
                                                          word.stressed_vowel,
                                                          false,
                                                          "silero " + word.reason});
                }
                auto automatic_pronunciation = semantic.pronunciation_text;
                std::size_t marker_cursor = 0;
                for (const auto& rewrite : protected_rewrites) {
                    const auto marker = std::string(rewrite.end - rewrite.begin, '\x01');
                    const auto marker_position =
                        automatic_pronunciation.find(marker, marker_cursor);
                    if (marker_position == std::string::npos)
                        throw std::runtime_error("Silero backend lost a protected rewrite span");
                    automatic_pronunciation.replace(marker_position, marker.size(), rewrite.output);
                    marker_cursor = marker_position + rewrite.output.size();
                }
                result.words = std::move(automatic_words);
                result.stress_decisions = std::move(automatic_stress_decisions);
                result.pronunciation_text = std::move(automatic_pronunciation);
            } catch (const std::exception& error) {
                result.words = deterministic_words;
                result.stress_decisions = deterministic_stress_decisions;
                result.pronunciation_text = deterministic_pronunciation;
                add_unavailable_warning(std::string("Automatic stress backend unavailable: ") +
                                        error.what());
            }
        }
#else
        add_unavailable_warning(
            "Automatic stress requires a build with TTS_FRONT_ENABLE_ONNX_STRESS=ON");
#endif
    }
    return result;
}

const char* to_string(Language language) noexcept {
    switch (language) {
    case Language::Auto:
        return "auto";
    case Language::Russian:
        return "russian";
    case Language::English:
        return "english";
    }
    return "unknown";
}
const char* to_string(WarningCode code) noexcept {
    switch (code) {
    case WarningCode::InvalidUtf8:
        return "invalid_utf8";
    case WarningCode::UnsupportedLanguage:
        return "unsupported_language";
    case WarningCode::AmbiguousNormalization:
        return "ambiguous_normalization";
    case WarningCode::UnresolvedNumber:
        return "unresolved_number";
    case WarningCode::AutomaticStressUnavailable:
        return "automatic_stress_unavailable";
    case WarningCode::DictionaryParseError:
        return "dictionary_parse_error";
    }
    return "unknown";
}
} // namespace tts_front
