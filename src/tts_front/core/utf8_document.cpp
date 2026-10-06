#include "utf8_document.hpp"

#include <algorithm>

namespace tts_front::detail {

Utf8Document::Utf8Document(std::string_view input)
    : bytes(input), valid(decode_utf8(input, points)) {}

SourceSpan Utf8Document::span_from_bytes(std::size_t begin, std::size_t end) const {
    const auto first = std::lower_bound(
        points.begin(), points.end(), begin, [](const Utf8CodePoint& point, std::size_t offset) {
            return point.offset < offset;
        });
    const auto last = std::lower_bound(
        points.begin(), points.end(), end, [](const Utf8CodePoint& point, std::size_t offset) {
            return point.offset < offset;
        });
    return {begin,
            end,
            static_cast<std::size_t>(first - points.begin()),
            static_cast<std::size_t>(last - points.begin())};
}

SourceSpan Utf8Document::span_from_codepoints(std::size_t begin, std::size_t end) const {
    const auto byte_begin = begin < points.size() ? points[begin].offset : bytes.size();
    const auto byte_end =
        end == 0 ? byte_begin
                 : (end <= points.size() ? points[end - 1].offset + points[end - 1].length
                                         : bytes.size());
    return {byte_begin, byte_end, begin, end};
}

std::size_t codepoint_index_at_or_after(const Utf8Document& document, std::size_t byte_offset) {
    return static_cast<std::size_t>(
        std::lower_bound(
            document.points.begin(),
            document.points.end(),
            byte_offset,
            [](const Utf8CodePoint& point, std::size_t offset) { return point.offset < offset; }) -
        document.points.begin());
}

} // namespace tts_front::detail
