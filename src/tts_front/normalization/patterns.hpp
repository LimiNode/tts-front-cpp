#pragma once

#include "tts_front/language/english/patterns.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/technical/patterns.hpp"

#include <regex>

namespace tts_front::detail {

struct RegexPatterns : technical::Patterns, english::Patterns, russian::Patterns {
    const std::regex grouped_number{R"((^|[^0-9])-?\d{1,3}(?:\s+\d{3})+(?![0-9]))"};
};

const RegexPatterns& regex_patterns();

} // namespace tts_front::detail
