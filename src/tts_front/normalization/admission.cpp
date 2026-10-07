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

} // namespace tts_front::detail
