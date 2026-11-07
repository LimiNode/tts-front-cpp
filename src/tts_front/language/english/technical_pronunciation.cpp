#include "tts_front/language/english.hpp"

namespace tts_front::detail::english {

std::optional<std::string> technical_pronunciation(std::string_view token) {
    if (token == "RTX 4090")
        return "R-T-X forty ninety";
    if (token == "C++")
        return "C plus plus";
    if (token == "HTTP/2")
        return "H-T-T-P two";
    return std::nullopt;
}

} // namespace tts_front::detail::english
