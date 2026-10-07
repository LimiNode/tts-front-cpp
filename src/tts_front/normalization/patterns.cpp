#include "patterns.hpp"

#include "tts_front/language/english/patterns.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/technical/patterns.hpp"

namespace tts_front::detail {

const technical::Patterns& technical_patterns() {
    static const technical::Patterns patterns;
    return patterns;
}

const english::Patterns& english_patterns() {
    static const english::Patterns patterns;
    return patterns;
}

const russian::Patterns& russian_patterns() {
    static const russian::Patterns patterns;
    return patterns;
}

const std::regex& grouped_number_pattern() {
    static const std::regex pattern{R"((^|[^0-9])-?\d{1,3}(?:\s+\d{3})+(?![0-9]))"};
    return pattern;
}

} // namespace tts_front::detail
