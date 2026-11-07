#include "tts_front/language/russian.hpp"

namespace tts_front::detail::russian {

std::optional<std::string> technical_pronunciation(std::string_view token) {
    if (token == "RTX 4090")
        return "эр ти икс сорок девяносто";
    if (token == "C++")
        return "си плюс плюс";
    if (token == "HTTP/2")
        return "аш ти ти пи два";
    return std::nullopt;
}

} // namespace tts_front::detail::russian
