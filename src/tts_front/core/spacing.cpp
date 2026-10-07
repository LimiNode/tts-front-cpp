#include "tts_front/core/spacing.hpp"

#include "tts_front/core/utf8.hpp"

#include <vector>

namespace tts_front::detail {

MappedText cleanup_spacing(const MappedText& input) {
    MappedText result;
    result.preserved_ranges = input.preserved_ranges;
    result.text.reserve(input.text.size());
    std::vector<Utf8CodePoint> points;
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

} // namespace tts_front::detail
