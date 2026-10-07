#include "tts_front/language/english/patterns.hpp"

namespace tts_front::detail::english {

const Patterns& patterns() {
    static const Patterns value;
    return value;
}

} // namespace tts_front::detail::english
