#include "patterns.hpp"

namespace tts_front::detail {

const RegexPatterns& regex_patterns() {
    static const RegexPatterns patterns;
    return patterns;
}

} // namespace tts_front::detail
