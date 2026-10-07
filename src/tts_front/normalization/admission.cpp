#include "tts_front/normalization/admission.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"

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
        if (text::is_digit(point->value)) {
            first_digit = point;
            break;
        }
    }
    if (first_digit == after)
        return true;

    auto last_digit = first_digit;
    for (auto point = first_digit; point != after; ++point) {
        if (text::is_digit(point->value))
            last_digit = point;
    }
    const auto first_index = static_cast<std::size_t>(first_digit - points.begin());
    const auto next_index = static_cast<std::size_t>(after - points.begin());
    const CodePoint* previous = first_index == 0 ? nullptr : &points[first_index - 1];
    const CodePoint* before_previous = first_index < 2 ? nullptr : &points[first_index - 2];
    const CodePoint* next = next_index == points.size() ? nullptr : &points[next_index];
    const CodePoint* after_next =
        next_index + 1 >= points.size() ? nullptr : &points[next_index + 1];

    if (previous && text::is_lexical_numeric_boundary(previous->value))
        return false;
    if (previous && text::is_numeric_separator(previous->value) && before_previous &&
        text::is_digit(before_previous->value))
        return false;
    if (previous && previous->value == '-' && before_previous &&
        text::is_lexical_numeric_boundary(before_previous->value))
        return false;
    bool suffix_consumed = false;
    for (auto point = last_digit + 1; point != after; ++point) {
        if (!text::is_digit(point->value)) {
            suffix_consumed = true;
            break;
        }
    }
    if (next && text::is_lexical_numeric_boundary(next->value) && !suffix_consumed)
        return false;
    if (next && text::is_numeric_separator(next->value) && after_next &&
        text::is_digit(after_next->value))
        return false;
    return true;
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
} // namespace tts_front::detail
