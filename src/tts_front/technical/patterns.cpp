#include "tts_front/technical/patterns.hpp"

namespace tts_front::detail::technical {

const Patterns& patterns() {
    static const Patterns value;
    return value;
}

} // namespace tts_front::detail::technical
