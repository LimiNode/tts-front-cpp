#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tts_front::detail::russian {

enum class YearCase { Locative, Genitive, Accusative };

std::string plural_form(long long value, const char* one, const char* few, const char* many);
std::string feminine_number(long long value);
std::string genitive_number(long long value);
std::string ordinal_day(int day);
// Calendar-year ordinals. Locative and accusative are masculine to agree
// with "год"; the contextual formatters support years 1000 through 9999.
std::string year_ordinal(int value, YearCase grammatical_case);
std::string year_locative(int year);
std::string year_genitive(int year);
std::string year_accusative(int year);
std::string decimal(long long integer, const std::string& fraction);
std::string decimal_genitive(long long integer, const std::string& fraction);
// Formats a supported Russian monetary decimal as major/minor units. The
// fraction must contain one or two decimal digits; unsupported precision is
// reported by returning nullopt so callers can preserve the source surface.
std::optional<std::string>
monetary_decimal(std::string_view integer_token, long long integer, std::string_view fraction);

} // namespace tts_front::detail::russian
