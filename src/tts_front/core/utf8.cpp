#include "utf8.hpp"

namespace tts_front::detail {

std::size_t utf8_sequence_length(const unsigned char lead) noexcept {
    if (lead <= 0x7f)
        return 1;
    if ((lead & 0xe0) == 0xc0)
        return 2;
    if ((lead & 0xf0) == 0xe0)
        return 3;
    if ((lead & 0xf8) == 0xf0)
        return 4;
    return 0;
}

bool decode_utf8(const std::string_view text, std::vector<Utf8CodePoint>& output) {
    output.clear();
    for (std::size_t i = 0; i < text.size();) {
        const std::size_t start = i;
        const auto lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = utf8_sequence_length(lead);
        if (length == 0 || i + length > text.size())
            return false;

        std::uint32_t value = lead & (length == 2   ? 0x1f
                                      : length == 3 ? 0x0f
                                      : length == 4 ? 0x07
                                                    : 0x7f);
        for (std::size_t j = 1; j < length; ++j) {
            const auto continuation = static_cast<unsigned char>(text[i + j]);
            if ((continuation & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (continuation & 0x3f);
        }
        if ((length == 2 && value < 0x80) || (length == 3 && value < 0x800) ||
            (length == 4 && value < 0x10000) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            return false;
        output.push_back({value, start, length});
        i += length;
    }
    return true;
}

bool is_valid_utf8(const std::string_view text) {
    std::vector<Utf8CodePoint> points;
    return decode_utf8(text, points);
}

} // namespace tts_front::detail
