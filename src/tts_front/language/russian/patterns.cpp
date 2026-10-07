#include "tts_front/language/russian/patterns.hpp"

namespace tts_front::detail::russian {

const Patterns& patterns() {
    static const Patterns value;
    return value;
}

} // namespace tts_front::detail::russian
