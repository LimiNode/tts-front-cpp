#pragma once

#include <cstdint>

namespace tts_front::detail {

namespace text {

bool is_cyrillic(std::uint32_t cp);
bool is_latin(std::uint32_t cp);
bool is_digit(std::uint32_t cp);
bool is_letter(std::uint32_t cp);
bool is_combining_mark(std::uint32_t cp);
bool is_word_codepoint(std::uint32_t cp);
bool is_numeric_separator(std::uint32_t cp);
bool is_range_connector(std::uint32_t cp);
bool is_numeric_connector(std::uint32_t cp);
bool is_supported_numeric_separator(std::uint32_t cp);
bool is_lexical_numeric_boundary(std::uint32_t cp);
bool is_horizontal_space(std::uint32_t cp);
bool is_ascii_punctuation(std::uint32_t cp);

} // namespace text

using text::is_ascii_punctuation;
using text::is_combining_mark;
using text::is_cyrillic;
using text::is_digit;
using text::is_horizontal_space;
using text::is_latin;
using text::is_letter;
using text::is_lexical_numeric_boundary;
using text::is_numeric_connector;
using text::is_numeric_separator;
using text::is_range_connector;
using text::is_supported_numeric_separator;
using text::is_word_codepoint;

} // namespace tts_front::detail
