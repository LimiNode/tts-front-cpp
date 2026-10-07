#pragma once

#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

#include <algorithm>
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
