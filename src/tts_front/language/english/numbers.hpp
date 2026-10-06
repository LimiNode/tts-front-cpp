#pragma once

#include <string>

namespace tts_front::detail::english {

std::string number(long long value);
const char* ordinal_suffix(long long value);
std::string ordinal(long long value);
std::string digits(const std::string& value);

} // namespace tts_front::detail::english
