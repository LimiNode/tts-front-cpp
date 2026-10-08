#pragma once

#include <string>
#include <string_view>

namespace tts_front::detail::russian {

std::string number(long long value);
std::string number_for_token(std::string_view token, long long value);

} // namespace tts_front::detail::russian
