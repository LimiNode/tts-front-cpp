#include "codepoint_classification.hpp"

#include <cctype>

namespace tts_front::detail::text {

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

} // namespace tts_front::detail::text
