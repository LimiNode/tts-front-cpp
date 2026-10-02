#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace silero_native {

inline bool continuation(unsigned char byte) {
    return (byte & 0xc0) == 0x80;
}

inline std::vector<std::uint32_t> decode_utf8(const std::string& text) {
    std::vector<std::uint32_t> result;
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80) {
            result.push_back(lead);
            ++index;
            continue;
        }
        if (lead >= 0xc2 && lead <= 0xdf) {
            if (index + 1 >= text.size()) {
                throw std::runtime_error("truncated UTF-8 sequence");
            }
            const auto next = static_cast<unsigned char>(text[index + 1]);
            if (!continuation(next)) {
                throw std::runtime_error("invalid UTF-8 continuation");
            }
            result.push_back(((lead & 0x1f) << 6) | (next & 0x3f));
            index += 2;
            continue;
        }
        if (lead >= 0xe0 && lead <= 0xef) {
            if (index + 2 >= text.size()) {
                throw std::runtime_error("truncated UTF-8 sequence");
            }
            const auto next = static_cast<unsigned char>(text[index + 1]);
            const auto last = static_cast<unsigned char>(text[index + 2]);
            const bool valid_second = lead == 0xe0   ? next >= 0xa0 && next <= 0xbf
                                      : lead == 0xed ? next >= 0x80 && next <= 0x9f
                                                     : next >= 0x80 && next <= 0xbf;
            if (!valid_second || !continuation(last)) {
                throw std::runtime_error("invalid UTF-8 sequence");
            }
            result.push_back(((lead & 0x0f) << 12) | ((next & 0x3f) << 6) | (last & 0x3f));
            index += 3;
            continue;
        }
        if (lead >= 0xf0 && lead <= 0xf4) {
            if (index + 3 >= text.size()) {
                throw std::runtime_error("truncated UTF-8 sequence");
            }
            const auto next = static_cast<unsigned char>(text[index + 1]);
            const auto middle = static_cast<unsigned char>(text[index + 2]);
            const auto last = static_cast<unsigned char>(text[index + 3]);
            const bool valid_second = lead == 0xf0   ? next >= 0x90 && next <= 0xbf
                                      : lead == 0xf4 ? next >= 0x80 && next <= 0x8f
                                                     : next >= 0x80 && next <= 0xbf;
            if (!valid_second || !continuation(middle) || !continuation(last)) {
                throw std::runtime_error("invalid UTF-8 sequence");
            }
            result.push_back(((lead & 0x07) << 18) | ((next & 0x3f) << 12) |
                             ((middle & 0x3f) << 6) | (last & 0x3f));
            index += 4;
            continue;
        }
        throw std::runtime_error("invalid UTF-8 lead byte");
    }
    return result;
}

inline void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff &&
               !(codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint >= 0x10000 && codepoint <= 0x10ffff) {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        throw std::runtime_error("code point is not a Unicode scalar value");
    }
}

} // namespace silero_native
