#pragma once

#include <string>

namespace tts_front::detail::russian {

enum class RuYearCase { Locative, Genitive };

std::string plural_form(long long value, const char* one, const char* few, const char* many);
std::string feminine_number(long long value);
std::string ordinal_day(int day);
std::string year_ordinal(int value, RuYearCase grammatical_case);
std::string year_locative(int year);
std::string year_genitive(int year);
std::string decimal(long long integer, const std::string& fraction);

} // namespace tts_front::detail::russian
