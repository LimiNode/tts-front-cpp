#pragma once

#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

#include <algorithm>
#include <iterator>
#include <regex>
#include <utility>
#include <vector>

namespace tts_front::detail {

using CodePoint = Utf8CodePoint;

template <typename Formatter>
MappedText
replace_matches(const MappedText& input, const std::regex& pattern, Formatter formatter) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    std::size_t run_cursor = 0;
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), pattern), end; it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        output.append_copy_with_cursor(input, cursor, begin, run_cursor);
        const auto source = input.source_range(begin, finish);
        bool is_preserved = false;
        if (input.preserved_ranges) {
            const auto& ranges = *input.preserved_ranges;
            const auto overlaps = [source](const SourceRange& preserved) {
                return source.offset < preserved.offset + preserved.length &&
                       preserved.offset < source.offset + source.length;
            };
            const auto next = std::lower_bound(
                ranges.begin(),
                ranges.end(),
                source.offset,
                [](const SourceRange& range, std::size_t at) { return range.offset < at; });
            is_preserved = (next != ranges.end() && overlaps(*next)) ||
                           (next != ranges.begin() && overlaps(*std::prev(next)));
        }
        if (is_preserved) {
            output.append_copy_with_cursor(input, begin, finish, run_cursor);
            cursor = finish;
            continue;
        }
        output.append_generated(input, begin, finish, formatter(*it, input));
        cursor = finish;
    }
    output.append_copy_with_cursor(input, cursor, input.text.size(), run_cursor);
    return output;
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

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match);

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match,
                 std::size_t group);

void add_warning_span(WarningSink& warnings,
                      WarningCode code,
                      std::string message,
                      const MappedText& text,
                      const std::smatch& match,
                      std::size_t begin_group,
                      std::size_t end_group);

void add_warning_without_suffix(WarningSink& warnings,
                                WarningCode code,
                                std::string message,
                                const MappedText& text,
                                const std::smatch& match,
                                std::size_t suffix_group);

} // namespace tts_front::detail
