#include "tts_front/technical/patterns.hpp"

#include <array>

namespace tts_front::detail::technical {

const Patterns& patterns() {
    static const Patterns value;
    return value;
}

bool is_known_identifier(std::string_view value) {
    constexpr std::array<std::string_view, 5> prefixes = {"RTX", "CUDA", "GPU", "API", "C++"};
    for (const auto prefix : prefixes) {
        if (value.rfind(prefix, 0) == 0)
            return true;
    }
    return false;
}

} // namespace tts_front::detail::technical
