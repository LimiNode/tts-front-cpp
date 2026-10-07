#include "tts_front/language/russian/initialisms.hpp"

namespace tts_front::detail::russian {

std::optional<std::string> safe_initialism(std::string_view token) {
    if (token == "ВК")
        return "вэ ка";
    if (token == "ООО")
        return "о о о";
    if (token == "РФ")
        return "эр эф";
    if (token == "МГУ")
        return "эм гэ у";
    if (token == "ФСБ")
        return "эф эс бэ";
    if (token == "МФЦ")
        return "эм эф цэ";
    if (token == "ИП")
        return "и пэ";
    return std::nullopt;
}

} // namespace tts_front::detail::russian
