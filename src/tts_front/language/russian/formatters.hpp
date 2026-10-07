#pragma once

#include <string>

namespace tts_front::detail::russian {

enum class RuYearCase { Locative, Genitive };

std::string ru_form(long long value, const char* one, const char* few, const char* many);
std::string ru_feminine_number(long long value);
std::string ru_ordinal_day(int day);
std::string ru_year_ordinal(int value, RuYearCase grammatical_case);
std::string ru_year_locative(int year);
std::string ru_year_genitive(int year);
std::string ru_decimal(long long integer, const std::string& fraction);

} // namespace tts_front::detail::russian
