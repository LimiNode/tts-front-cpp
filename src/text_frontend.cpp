#include "detail/utf8.hpp"
#include "tts_front/tts_front.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace tts_front {
namespace {

using CodePoint = detail::Utf8CodePoint;
using detail::decode_utf8;

bool is_cyrillic(std::uint32_t cp) {
    return cp >= 0x0400 && cp <= 0x052f;
}
bool is_latin(std::uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= 0x00c0 && cp <= 0x024f);
}
bool is_digit(std::uint32_t cp) {
    return cp >= '0' && cp <= '9';
}
bool is_letter(std::uint32_t cp) {
    return is_cyrillic(cp) || is_latin(cp);
}
bool is_word_codepoint(std::uint32_t cp) {
    return is_letter(cp) || is_digit(cp) || cp == '+' || cp == '#' || cp == '_' || cp == '.';
}

std::string cleanup_text(std::string text) {
    std::string result;
    result.reserve(text.size());
    std::vector<CodePoint> points;
    if (!decode_utf8(text, points))
        return text;
    bool pending_space = false;
    for (const auto& point : points) {
        const bool space =
            point.value == ' ' || point.value == '\t' || point.value == '\n' || point.value == '\r';
        if (space) {
            pending_space = !result.empty();
            continue;
        }
        const bool punctuation = point.value == ',' || point.value == '.' || point.value == ';' ||
                                 point.value == ':' || point.value == '!' || point.value == '?';
        if (pending_space && !punctuation && !result.empty())
            result.push_back(' ');
        pending_space = false;
        result.append(text, point.offset, point.length);
    }
    while (!result.empty() && result.back() == ' ')
        result.pop_back();
    return result;
}

template <typename Formatter>
std::string replace_matches(std::string text, const std::regex& pattern, Formatter formatter) {
    std::string output;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        output.append(text, cursor, begin - cursor);
        output += formatter(*it);
        cursor = finish;
    }
    output += text.substr(cursor);
    return output;
}

struct RegexPatterns {
    const std::regex technical_url{R"(https?://[^\s]+)"};
    const std::regex technical_email{R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})"};
    const std::regex technical_ipv4{R"(\b\d{1,3}(?:\.\d{1,3}){3}\b)"};
    const std::regex technical_version{R"(\bv\d+(?:\.\d+)+\b)"};
    const std::regex technical_http{R"(\bHTTP/\d+(?:\.\d+)?\b)"};
    const std::regex technical_gpu{R"(\b(?:RTX|CUDA|GPU|API)\s+\d+(?:\.\d+)?\b)"};
    const std::regex technical_cpp{R"(C\+\+)"};
    const std::regex technical_csharp{R"(C#)"};
    const std::regex grouped_number{R"((^|[^0-9])-?\d{1,3}(?:\s+\d{3})+)"};
    const std::regex ru_date{R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)"};
    const std::regex ru_year{R"(\b(\d{4})\s*г\.)"};
    const std::regex ru_decimal_percent{R"((-?\d+),([0-9]+)\s*%([^0-9]|$))"};
    const std::regex ru_percent{R"((-?\d+)\s*%)"};
    const std::regex ru_currency{
        R"((-?\d+)\s*(рублей|рубля|рубль|руб\.?)([^А-Яа-яЁёA-Za-z0-9]|$))"};
    const std::regex ru_time{R"(\b(\d{1,2}):(\d{2})\b)"};
    const std::regex ru_decimal{R"((-?\d+)[,](\d+)([^0-9]|$))"};
    const std::regex ru_measurement{
        R"((-?\d+)\s*(километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м)([^А-Яа-яЁёA-Za-z0-9]|$))"};
    const std::regex generic_ru_number{R"((^|[^A-Za-z0-9_,.:])(-?\d+)(?![0-9]*[.,:][0-9]))"};
    const std::regex ru_abbreviation_td{R"((^|[^A-Za-zА-Яа-яЁё])т\.д\.)"};
    const std::regex ru_abbreviation_tp{R"((^|[^A-Za-zА-Яа-яЁё])т\.п\.)"};
    const std::regex en_currency_decimal{R"(\$([0-9]+)\.([0-9]{1,}))"};
    const std::regex en_currency_integer{R"(\$([0-9]+)(?![0-9]|\.[0-9]))"};
    const std::regex en_percent{R"((-?[0-9]+(?:\.[0-9]+)?)\s*%)"};
    const std::regex en_time{R"(\b(\d{1,2}):(\d{2})\b)"};
    const std::regex en_decimal{R"((^|[^$A-Za-z0-9])(-?\d+\.\d+))"};
    const std::regex en_measurement{
        R"((-?\d+)\s*(kilometers|kilometres|km|kilograms|kg|meters|metres|m|centimeters|centimetres|cm|millimeters|millimetres|mm|GB|MB)([^A-Za-z0-9]|$))"};
    const std::regex generic_en_number{R"((^|[^A-Za-z0-9_,.:])(-?\d+)(?![0-9]*[.,:][0-9]))"};
};

const RegexPatterns& regex_patterns() {
    static const RegexPatterns patterns;
    return patterns;
}

void add_warning(std::vector<TextWarning>& warnings,
                 WarningCode code,
                 std::string message,
                 const std::smatch& match) {
    (void)match;
    // Normalization operates on transformed/protected text. Until a source map is introduced,
    // exposing intermediate coordinates as source offsets would be misleading.
    warnings.push_back({code, std::move(message), 0, 0});
}

void add_warning(std::vector<TextWarning>& warnings,
                 WarningCode code,
                 std::string message,
                 std::size_t offset,
                 std::size_t length) {
    warnings.push_back({code, std::move(message), offset, length});
}

const char* const ru_ones[] = {"ноль",        "один",       "два",          "три",
                               "четыре",      "пять",       "шесть",        "семь",
                               "восемь",      "девять",     "десять",       "одиннадцать",
                               "двенадцать",  "тринадцать", "четырнадцать", "пятнадцать",
                               "шестнадцать", "семнадцать", "восемнадцать", "девятнадцать"};
const char* const ru_tens[] = {"",
                               "",
                               "двадцать",
                               "тридцать",
                               "сорок",
                               "пятьдесят",
                               "шестьдесят",
                               "семьдесят",
                               "восемьдесят",
                               "девяносто"};
const char* const ru_hundreds[] = {"",
                                   "сто",
                                   "двести",
                                   "триста",
                                   "четыреста",
                                   "пятьсот",
                                   "шестьсот",
                                   "семьсот",
                                   "восемьсот",
                                   "девятьсот"};
std::string ru_under_1000(int n) {
    std::string result;
    if (n >= 100) {
        result += ru_hundreds[n / 100];
        n %= 100;
        if (n)
            result += " ";
    }
    if (n < 20) {
        if (n)
            result += ru_ones[n];
    } else {
        result += ru_tens[n / 10];
        if (n % 10)
            result += " " + std::string(ru_ones[n % 10]);
    }
    return result.empty() ? "ноль" : result;
}
std::string ru_number(long long n) {
    if (n < 0)
        return "минус " + ru_number(-n);
    if (n < 1000)
        return ru_under_1000(static_cast<int>(n));
    if (n < 1000000) {
        const int thousands = static_cast<int>(n / 1000);
        const int rest = static_cast<int>(n % 1000);
        std::string thousands_word = thousands == 1   ? "одна"
                                     : thousands == 2 ? "две"
                                                      : ru_under_1000(thousands);
        const int last_two = thousands % 100;
        const int last_digit = thousands % 10;
        if (!(last_two >= 11 && last_two <= 14)) {
            if (last_digit == 1) {
                const auto at = thousands_word.rfind("один");
                if (at != std::string::npos)
                    thousands_word.replace(at, std::string("один").size(), "одна");
            } else if (last_digit == 2) {
                const auto at = thousands_word.rfind("два");
                if (at != std::string::npos)
                    thousands_word.replace(at, std::string("два").size(), "две");
            }
        }
        std::string result = thousands_word;
        result += (thousands % 10 == 1 && thousands % 100 != 11) ? " тысяча"
                  : (thousands % 10 >= 2 && thousands % 10 <= 4 &&
                     (thousands % 100 < 10 || thousands % 100 >= 20))
                      ? " тысячи"
                      : " тысяч";
        if (rest)
            result += " " + ru_under_1000(rest);
        return result;
    }
    if (n < 1000000000) {
        const int millions = static_cast<int>(n / 1000000);
        const int rest = static_cast<int>(n % 1000000);
        std::string result =
            ru_number(millions) +
            ((millions % 100 >= 11 && millions % 100 <= 14)
                 ? " миллионов"
                 : (millions % 10 == 1
                        ? " миллион"
                        : (millions % 10 >= 2 && millions % 10 <= 4 ? " миллиона" : " миллионов")));
        if (rest)
            result += " " + ru_number(rest);
        return result;
    }
    return std::to_string(n);
}
std::string en_number(long long n) {
    static const char* const ones[] = {"zero",    "one",     "two",       "three",    "four",
                                       "five",    "six",     "seven",     "eight",    "nine",
                                       "ten",     "eleven",  "twelve",    "thirteen", "fourteen",
                                       "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    static const char* const tens[] = {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    if (n < 0)
        return "minus " + en_number(-n);
    if (n < 20)
        return ones[n];
    if (n < 100)
        return std::string(tens[n / 10]) + (n % 10 ? " " + std::string(ones[n % 10]) : "");
    if (n < 1000)
        return std::string(ones[n / 100]) + " hundred" + (n % 100 ? " " + en_number(n % 100) : "");
    if (n < 1000000)
        return en_number(n / 1000) + " thousand" + (n % 1000 ? " " + en_number(n % 1000) : "");
    if (n < 1000000000)
        return en_number(n / 1000000) + " million" +
               (n % 1000000 ? " " + en_number(n % 1000000) : "");
    return std::to_string(n);
}
bool try_parse_long(std::string_view token, long long& value) {
    try {
        std::size_t consumed = 0;
        value = std::stoll(std::string(token), &consumed);
        return consumed == token.size() && value >= -999999999 && value <= 999999999;
    } catch (...) {
        return false;
    }
}
std::string
number_or_original(const std::string& token, bool russian, std::vector<TextWarning>& warnings) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        add_warning(warnings, WarningCode::UnresolvedNumber, "Unable to parse number", 0, 0);
        return token;
    }
    return russian ? ru_number(value) : en_number(value);
}
std::string ru_form(long long value, const char* one, const char* few, const char* many) {
    const auto n = std::llabs(value) % 100;
    const auto last = n % 10;
    if (n >= 11 && n <= 19)
        return many;
    if (last == 1)
        return one;
    if (last >= 2 && last <= 4)
        return few;
    return many;
}
std::string ru_feminine_number(long long value) {
    const auto absolute = std::llabs(value);
    const auto suffix = absolute % 100;
    if (suffix >= 11 && suffix <= 14)
        return ru_number(value);
    std::string result = ru_number(value);
    if (absolute % 10 == 1) {
        const auto at = result.rfind("один");
        if (at != std::string::npos)
            result.replace(at, std::string("один").size(), "одна");
    } else if (absolute % 10 == 2) {
        const auto at = result.rfind("два");
        if (at != std::string::npos)
            result.replace(at, std::string("два").size(), "две");
    }
    return result;
}
std::string ru_ordinal_day(int day) {
    static const char* const ordinal[] = {"",
                                          "первое",
                                          "второе",
                                          "третье",
                                          "четвертое",
                                          "пятое",
                                          "шестое",
                                          "седьмое",
                                          "восьмое",
                                          "девятое",
                                          "десятое",
                                          "одиннадцатое",
                                          "двенадцатое",
                                          "тринадцатое",
                                          "четырнадцатое",
                                          "пятнадцатое",
                                          "шестнадцатое",
                                          "семнадцатое",
                                          "восемнадцатое",
                                          "девятнадцатое",
                                          "двадцатое",
                                          "двадцать первое",
                                          "двадцать второе",
                                          "двадцать третье",
                                          "двадцать четвертое",
                                          "двадцать пятое",
                                          "двадцать шестое",
                                          "двадцать седьмое",
                                          "двадцать восьмое",
                                          "двадцать девятое",
                                          "тридцатое",
                                          "тридцать первое"};
    return day >= 1 && day <= 31 ? ordinal[day] : ru_number(day);
}
enum class RuYearCase { Locative, Genitive };
std::string ru_year_ordinal(int n, RuYearCase grammatical_case) {
    const bool genitive = grammatical_case == RuYearCase::Genitive;
    static const char* const loc[] = {"",
                                      "первом",
                                      "втором",
                                      "третьем",
                                      "четвертом",
                                      "пятом",
                                      "шестом",
                                      "седьмом",
                                      "восьмом",
                                      "девятом",
                                      "десятом",
                                      "одиннадцатом",
                                      "двенадцатом",
                                      "тринадцатом",
                                      "четырнадцатом",
                                      "пятнадцатом",
                                      "шестнадцатом",
                                      "семнадцатом",
                                      "восемнадцатом",
                                      "девятнадцатом",
                                      "двадцатом"};
    static const char* const gen[] = {"",
                                      "первого",
                                      "второго",
                                      "третьего",
                                      "четвертого",
                                      "пятого",
                                      "шестого",
                                      "седьмого",
                                      "восьмого",
                                      "девятого",
                                      "десятого",
                                      "одиннадцатого",
                                      "двенадцатого",
                                      "тринадцатого",
                                      "четырнадцатого",
                                      "пятнадцатого",
                                      "шестнадцатого",
                                      "семнадцатого",
                                      "восемнадцатого",
                                      "девятнадцатого",
                                      "двадцатого"};
    if (n <= 20)
        return std::string((genitive ? gen : loc)[n]);
    static const char* const tens_loc[] = {"",
                                           "",
                                           "двадцатом",
                                           "тридцатом",
                                           "сороковом",
                                           "пятидесятом",
                                           "шестидесятом",
                                           "семидесятом",
                                           "восьмидесятом",
                                           "девяностом"};
    static const char* const tens_gen[] = {"",
                                           "",
                                           "двадцатого",
                                           "тридцатого",
                                           "сорокового",
                                           "пятидесятого",
                                           "шестидесятого",
                                           "семидесятого",
                                           "восьмидесятого",
                                           "девяностого"};
    if (n < 100)
        return n % 10 == 0 ? std::string((genitive ? tens_gen : tens_loc)[n / 10])
                           : ru_number(n / 10 * 10) + " " + ((genitive ? gen : loc)[n % 10]);
    if (n % 100 == 0) {
        static const char* const hundreds_loc[] = {"",
                                                   "сотом",
                                                   "двухсотом",
                                                   "трехсотом",
                                                   "четырехсотом",
                                                   "пятисотом",
                                                   "шестисотом",
                                                   "семисотом",
                                                   "восьмисотом",
                                                   "девятисотом"};
        static const char* const hundreds_gen[] = {"",
                                                   "сотого",
                                                   "двухсотого",
                                                   "трехсотого",
                                                   "четырехсотого",
                                                   "пятисотого",
                                                   "шестисотого",
                                                   "семисотого",
                                                   "восьмисотого",
                                                   "девятисотого"};
        return std::string((genitive ? hundreds_gen : hundreds_loc)[n / 100]);
    }
    return ru_number(n / 100 * 100) + " " + ru_year_ordinal(n % 100, grammatical_case);
}
std::string ru_year_prefix(int thousands) {
    if (thousands == 1)
        return "тысяча";
    if (thousands == 2)
        return "две тысячи";
    return ru_number(thousands) + " " + ru_form(thousands, "тысяча", "тысячи", "тысяч");
}
std::string ru_year_locative(int year) {
    if (year < 1000 || year > 9999)
        return ru_number(year);
    const int thousands = year / 1000;
    const int rest = year % 1000;
    if (rest == 0) {
        static const char* const exact[] = {"",
                                            "тысячном",
                                            "двухтысячном",
                                            "трехтысячном",
                                            "четырехтысячном",
                                            "пятитысячном",
                                            "шеститысячном",
                                            "семитысячном",
                                            "восьмитысячном",
                                            "девятитысячном"};
        return exact[thousands];
    }
    return ru_year_prefix(thousands) + " " + ru_year_ordinal(rest, RuYearCase::Locative);
}
std::string ru_year_genitive(int year) {
    if (year < 1000 || year > 9999)
        return ru_number(year);
    const int thousands = year / 1000;
    const int rest = year % 1000;
    if (rest == 0) {
        static const char* const exact[] = {"",
                                            "тысячного",
                                            "двухтысячного",
                                            "трехтысячного",
                                            "четырехтысячного",
                                            "пятитысячного",
                                            "шеститысячного",
                                            "семитысячного",
                                            "восьмитысячного",
                                            "девятитысячного"};
        return exact[thousands];
    }
    return ru_year_prefix(thousands) + " " + ru_year_ordinal(rest, RuYearCase::Genitive);
}
std::string ru_decimal(long long integer, const std::string& fraction) {
    const auto denominator = fraction.size() == 1   ? "десятая"
                             : fraction.size() == 2 ? "сотая"
                                                    : "тысячная";
    const auto denominator_plural = fraction.size() == 1   ? "десятых"
                                    : fraction.size() == 2 ? "сотых"
                                                           : "тысячных";
    const auto fractional = std::stoll(fraction);
    const auto category = std::llabs(fractional) % 100;
    const auto denominator_word = category % 10 == 1 && !(category >= 11 && category <= 14)
                                      ? denominator
                                      : denominator_plural;
    return ru_feminine_number(integer) +
           (std::llabs(integer) % 10 == 1 && std::llabs(integer) % 100 != 11 ? " целая "
                                                                             : " целых ") +
           ru_feminine_number(fractional) + " " + denominator_word;
}
std::string en_digits(const std::string& digits) {
    static const char* const names[] = {
        "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
    std::string result;
    for (const char digit : digits) {
        if (!result.empty())
            result += ' ';
        result += names[digit - '0'];
    }
    return result;
}

struct ProtectedSpan {
    std::string marker;
    std::string value;
};
std::string marker_for(const std::string& text, std::size_t index) {
    std::string marker = "\x01tts_front_protected_" + std::to_string(index) + "\x02";
    while (text.find(marker) != std::string::npos)
        marker.insert(marker.size() - 1, "x");
    return marker;
}
std::string protect_technical(std::string text, std::vector<ProtectedSpan>& protected_spans) {
    const auto& patterns = regex_patterns();
    const std::array<const std::regex*, 8> technical_patterns = {&patterns.technical_url,
                                                                 &patterns.technical_email,
                                                                 &patterns.technical_ipv4,
                                                                 &patterns.technical_version,
                                                                 &patterns.technical_http,
                                                                 &patterns.technical_gpu,
                                                                 &patterns.technical_cpp,
                                                                 &patterns.technical_csharp};
    for (const auto* pattern : technical_patterns) {
        std::string output;
        std::size_t cursor = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), *pattern), end; it != end; ++it) {
            const auto begin = static_cast<std::size_t>(it->position());
            const auto finish = begin + static_cast<std::size_t>(it->length());
            output.append(text, cursor, begin - cursor);
            const auto marker = marker_for(text, protected_spans.size());
            protected_spans.push_back({marker, it->str()});
            output += marker;
            cursor = finish;
        }
        output += text.substr(cursor);
        text = std::move(output);
    }
    return text;
}
std::string restore_technical(std::string text, const std::vector<ProtectedSpan>& protected_spans) {
    for (const auto& span : protected_spans) {
        std::size_t at = text.find(span.marker);
        while (at != std::string::npos) {
            text.replace(at, span.marker.size(), span.value);
            at = text.find(span.marker, at + span.value.size());
        }
    }
    return text;
}

bool valid_date(int day, int month, int year) {
    if (month < 1 || month > 12 || day < 1)
        return false;
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days[month];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        limit = 29;
    return day <= limit;
}

std::string collapse_grouped_numbers(std::string text) {
    return replace_matches(
        std::move(text), regex_patterns().grouped_number, [](const std::smatch& match) {
            std::string result = match.str();
            const auto number_start = match[1].length();
            std::string prefix = result.substr(0, number_start);
            result.erase(0, number_start);
            result.erase(std::remove_if(result.begin(),
                                        result.end(),
                                        [](unsigned char c) { return std::isspace(c) != 0; }),
                         result.end());
            return prefix + result;
        });
}

std::string normalize_ru(std::string text, std::vector<TextWarning>& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_technical(std::move(text), protected_spans);
    text = collapse_grouped_numbers(std::move(text));
    text =
        replace_matches(std::move(text), regex_patterns().ru_date, [&](const std::smatch& match) {
            long long day = 0, month = 0, year = 0;
            if (!try_parse_long(match[1].str(), day) || !try_parse_long(match[2].str(), month) ||
                !try_parse_long(match[3].str(), year) ||
                !valid_date(
                    static_cast<int>(day), static_cast<int>(month), static_cast<int>(year))) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid Russian calendar date",
                            match);
                return match.str();
            }
            static const char* const months[] = {"",
                                                 "января",
                                                 "февраля",
                                                 "марта",
                                                 "апреля",
                                                 "мая",
                                                 "июня",
                                                 "июля",
                                                 "августа",
                                                 "сентября",
                                                 "октября",
                                                 "ноября",
                                                 "декабря"};
            return ru_ordinal_day(static_cast<int>(day)) + " " + months[month] + " " +
                   ru_year_genitive(static_cast<int>(year)) + " года";
        });
    text =
        replace_matches(std::move(text), regex_patterns().ru_year, [&](const std::smatch& match) {
            long long year = 0;
            if (!try_parse_long(match[1].str(), year)) {
                add_warning(
                    warnings, WarningCode::UnresolvedNumber, "Unable to parse Russian year", match);
                return match.str();
            }
            return ru_year_locative(static_cast<int>(year)) + " году";
        });
    text = replace_matches(
        std::move(text), regex_patterns().ru_decimal_percent, [&](const std::smatch& match) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian decimal percent",
                            match);
                return match.str();
            }
            return ru_decimal(integer, match[2].str()) + " процента" + match[3].str();
        });
    text = replace_matches(
        std::move(text), regex_patterns().ru_percent, [&](const std::smatch& match) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian percent",
                            match);
                return match.str();
            }
            return ru_number(n) + " " + ru_form(n, "процент", "процента", "процентов");
        });
    text = replace_matches(
        std::move(text), regex_patterns().ru_currency, [&](const std::smatch& match) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian currency",
                            match);
                return match.str();
            }
            return ru_number(n) + " " + ru_form(n, "рубль", "рубля", "рублей") + match[3].str();
        });
    text =
        replace_matches(std::move(text), regex_patterns().ru_time, [&](const std::smatch& match) {
            long long h = 0, m = 0;
            if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m) ||
                h > 23 || m > 59) {
                add_warning(
                    warnings, WarningCode::UnresolvedNumber, "Invalid Russian clock time", match);
                return match.str();
            }
            return ru_number(h) + " " + ru_form(h, "час", "часа", "часов") + " " +
                   ru_feminine_number(m) + " " + ru_form(m, "минута", "минуты", "минут");
        });
    text = replace_matches(
        std::move(text), regex_patterns().ru_decimal, [&](const std::smatch& match) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian decimal",
                            match);
                return match.str();
            }
            return ru_decimal(integer, match[2].str()) + match[3].str();
        });
    text = replace_matches(
        std::move(text), regex_patterns().ru_measurement, [&](const std::smatch& match) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian measurement",
                            match);
                return match.str();
            }
            const auto source = match[2].str();
            const std::string unit = source.find("кг") == 0 || source.find("килограмм") == 0
                                         ? ru_form(n, "килограмм", "килограмма", "килограммов")
                                     : source.find("км") == 0 || source.find("километр") == 0
                                         ? ru_form(n, "километр", "километра", "километров")
                                     : source.find("см") == 0 || source.find("сантиметр") == 0
                                         ? ru_form(n, "сантиметр", "сантиметра", "сантиметров")
                                     : source.find("мм") == 0 || source.find("миллиметр") == 0
                                         ? ru_form(n, "миллиметр", "миллиметра", "миллиметров")
                                     : source == "м" ? ru_form(n, "метр", "метра", "метров")
                                     : source == "ГБ"
                                         ? ru_form(n, "гигабайт", "гигабайта", "гигабайт")
                                         : ru_form(n, "мегабайт", "мегабайта", "мегабайт");
            return ru_number(n) + " " + unit + match[3].str();
        });
    text = replace_matches(
        std::move(text), regex_patterns().generic_ru_number, [&](const std::smatch& match) {
            return match[1].str() + number_or_original(match[2].str(), true, warnings);
        });
    text = replace_matches(std::move(text),
                           regex_patterns().ru_abbreviation_td,
                           [](const auto& match) { return match[1].str() + "так далее"; });
    text = replace_matches(std::move(text),
                           regex_patterns().ru_abbreviation_tp,
                           [](const auto& match) { return match[1].str() + "тому подобное"; });
    return restore_technical(std::move(text), protected_spans);
}

std::string normalize_en(std::string text, std::vector<TextWarning>& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_technical(std::move(text), protected_spans);
    text = collapse_grouped_numbers(std::move(text));
    text = replace_matches(
        std::move(text), regex_patterns().en_currency_decimal, [&](const std::smatch& match) {
            long long dollars = 0, cents = 0;
            const auto fraction = match[2].str();
            if (!try_parse_long(match[1].str(), dollars) || fraction.size() > 2 ||
                !try_parse_long(fraction, cents)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English currency",
                            match);
                return match.str();
            }
            if (fraction.size() == 1)
                cents *= 10;
            return en_number(dollars) + (dollars == 1 ? " dollar" : " dollars") + " " +
                   en_number(cents) + (cents == 1 ? " cent" : " cents");
        });
    text = replace_matches(
        std::move(text), regex_patterns().en_currency_integer, [&](const std::smatch& match) {
            long long dollars = 0;
            if (!try_parse_long(match[1].str(), dollars)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English currency",
                            match);
                return match.str();
            }
            return en_number(dollars) + (dollars == 1 ? " dollar" : " dollars");
        });
    text = replace_matches(
        std::move(text), regex_patterns().en_percent, [&](const std::smatch& match) {
            const auto value = match[1].str();
            const auto dot = value.find('.');
            long long integer = 0;
            if (!try_parse_long(dot == std::string::npos ? value : value.substr(0, dot), integer)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English percent",
                            match);
                return match.str();
            }
            return (dot == std::string::npos
                        ? en_number(integer)
                        : en_number(integer) + " point " + en_digits(value.substr(dot + 1))) +
                   " percent";
        });
    text =
        replace_matches(std::move(text), regex_patterns().en_time, [&](const std::smatch& match) {
            long long h = 0, m = 0;
            if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m) ||
                h > 23 || m > 59) {
                add_warning(
                    warnings, WarningCode::UnresolvedNumber, "Invalid English clock time", match);
                return match.str();
            }
            return en_number(h) + (h == 1 ? " hour " : " hours ") + en_number(m) +
                   (m == 1 ? " minute" : " minutes");
        });
    text = replace_matches(
        std::move(text), regex_patterns().en_decimal, [&](const std::smatch& match) {
            const auto value = match[2].str();
            const auto dot = value.find('.');
            long long integer = 0;
            if (!try_parse_long(value.substr(0, dot), integer)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English decimal",
                            match);
                return match.str();
            }
            return match[1].str() + en_number(integer) + " point " +
                   en_digits(value.substr(dot + 1));
        });
    text = replace_matches(
        std::move(text), regex_patterns().en_measurement, [&](const std::smatch& match) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English measurement",
                            match);
                return match.str();
            }
            const auto unit = match[2].str();
            const bool singular = n == 1;
            const std::string spoken = unit == "kg" || unit.find("kilogram") == 0
                                           ? (singular ? "kilogram" : "kilograms")
                                       : unit == "km" || unit.find("kilomet") == 0
                                           ? (singular ? "kilometer" : "kilometers")
                                       : unit == "m" || unit == "meters" || unit == "metres"
                                           ? (singular ? "meter" : "meters")
                                       : unit == "cm" || unit.find("centimet") == 0
                                           ? (singular ? "centimeter" : "centimeters")
                                       : unit == "mm" || unit.find("millimet") == 0
                                           ? (singular ? "millimeter" : "millimeters")
                                       : unit == "MB" ? (singular ? "megabyte" : "megabytes")
                                                      : (singular ? "gigabyte" : "gigabytes");
            return en_number(n) + " " + spoken + match[3].str();
        });
    text = replace_matches(
        std::move(text), regex_patterns().generic_en_number, [&](const std::smatch& match) {
            return match[1].str() + number_or_original(match[2].str(), false, warnings);
        });
    return restore_technical(std::move(text), protected_spans);
}

struct Span {
    std::size_t begin = 0;
    std::size_t end = 0;
};
std::vector<Span> token_spans(const std::string& text) {
    std::vector<CodePoint> points;
    decode_utf8(text, points);
    std::vector<Span> spans;
    std::size_t begin = std::string::npos;
    std::size_t end = 0;
    for (const auto& point : points) {
        if (is_word_codepoint(point.value)) {
            if (begin == std::string::npos)
                begin = point.offset;
            end = point.offset + point.length;
        } else if (begin != std::string::npos) {
            spans.push_back({begin, end});
            begin = std::string::npos;
        }
    }
    if (begin != std::string::npos)
        spans.push_back({begin, end});
    return spans;
}
Language detect_language(std::string_view text, bool& has_cyrillic, bool& has_latin) {
    std::vector<CodePoint> points;
    decode_utf8(text, points);
    has_cyrillic = std::any_of(
        points.begin(), points.end(), [](const CodePoint& p) { return is_cyrillic(p.value); });
    has_latin = std::any_of(
        points.begin(), points.end(), [](const CodePoint& p) { return is_latin(p.value); });
    return has_cyrillic ? Language::Russian : Language::English;
}

} // namespace

bool TextFrontendResult::has_uncertainty() const noexcept {
    return std::any_of(warnings.begin(), warnings.end(), [](const TextWarning& warning) {
        return warning.code == WarningCode::AmbiguousNormalization ||
               warning.code == WarningCode::UnresolvedNumber;
    });
}

TextFrontendResult TextFrontend::process(std::string_view input,
                                         const TextFrontendOptions& options) const {
    TextFrontendResult result;
    result.original_text = std::string(input);
    std::vector<CodePoint> points;
    if (!decode_utf8(input, points)) {
        add_warning(
            result.warnings, WarningCode::InvalidUtf8, "Input is not valid UTF-8", 0, input.size());
        return result;
    }
    std::string text(input);
    if (options.cleanup_spacing)
        text = cleanup_text(std::move(text));
    bool has_cyrillic = false;
    bool has_latin = false;
    Language language = options.language;
    if (language == Language::Auto)
        language = detect_language(text, has_cyrillic, has_latin);
    if (options.language == Language::Auto && has_cyrillic && has_latin)
        add_warning(result.warnings,
                    WarningCode::AmbiguousNormalization,
                    "Mixed Cyrillic/Latin input uses Russian normalization by policy",
                    0,
                    text.size());
    if (language != Language::Russian && language != Language::English) {
        result.warnings.push_back({WarningCode::UnsupportedLanguage, "Unsupported language", 0, 0});
        return result;
    }
    if (options.normalize) {
        switch (language) {
        case Language::Russian:
            text = normalize_ru(std::move(text), result.warnings);
            break;
        case Language::English:
            text = normalize_en(std::move(text), result.warnings);
            break;
        case Language::Auto:
            break;
        }
    }
    if (options.cleanup_spacing)
        text = cleanup_text(std::move(text));
    result.normalized_text = text;
    result.pronunciation_text = text;
    const bool stress_enabled =
        options.resolve_stress && options.stress_mode != StressMode::Disabled;

    // Dictionary phrases are matched on complete token sequences, longest first, without rescanning output.
    std::vector<const PronunciationDictionary::Entry*> matched_entries;
    if (options.apply_dictionary && options.dictionary) {
        const auto spans = token_spans(text);
        struct PhraseCandidate {
            const PronunciationDictionary::Entry* entry = nullptr;
            std::vector<Span> spans;
        };
        std::vector<PhraseCandidate> phrases;
        for (const auto& entry : options.dictionary->entries()) {
            if (entry.match == PronunciationDictionary::Match::ExactPhrase)
                phrases.push_back({&entry, token_spans(entry.pattern)});
        }
        matched_entries.assign(spans.size(), nullptr);
        std::string rendered;
        std::size_t cursor = 0;
        for (std::size_t i = 0; i < spans.size();) {
            const PronunciationDictionary::Entry* best = nullptr;
            std::size_t best_end = i;
            for (const auto& phrase : phrases) {
                const auto& entry = *phrase.entry;
                const auto& phrase_spans = phrase.spans;
                if (phrase_spans.empty() || i + phrase_spans.size() > spans.size())
                    continue;
                bool match = true;
                for (std::size_t j = 0; j < phrase_spans.size(); ++j) {
                    if (text.substr(spans[i + j].begin, spans[i + j].end - spans[i + j].begin) !=
                        entry.pattern.substr(phrase_spans[j].begin,
                                             phrase_spans[j].end - phrase_spans[j].begin)) {
                        match = false;
                        break;
                    }
                }
                if (match && (!best || phrase_spans.size() > best_end - i)) {
                    best = &entry;
                    best_end = i + phrase_spans.size();
                }
            }
            const auto token = text.substr(spans[i].begin, spans[i].end - spans[i].begin);
            if (best) {
                for (std::size_t j = i; j < best_end; ++j)
                    matched_entries[j] = best;
                rendered.append(text, cursor, spans[i].begin - cursor);
                rendered += best->pronunciation;
                const auto phrase_surface =
                    text.substr(spans[i].begin, spans[best_end - 1].end - spans[i].begin);
                result.dictionary_replacements.push_back(
                    {phrase_surface, best->pronunciation, spans[i].begin});
                if (options.resolve_stress && options.stress_mode != StressMode::Disabled &&
                    best->stressed_vowel)
                    result.stress_decisions.push_back({phrase_surface,
                                                       best->stressed_vowel,
                                                       true,
                                                       "pronunciation dictionary phrase"});
                cursor = spans[best_end - 1].end;
                i = best_end;
                continue;
            }
            rendered.append(text, cursor, spans[i].begin - cursor);
            if (const auto* entry = options.dictionary->find_token(token)) {
                matched_entries[i] = entry;
                rendered += entry->pronunciation;
                if (entry->pronunciation != token)
                    result.dictionary_replacements.push_back(
                        {token, entry->pronunciation, spans[i].begin});
            } else
                rendered += token;
            cursor = spans[i].end;
            ++i;
        }
        rendered += text.substr(cursor);
        result.pronunciation_text = std::move(rendered);
    }

    const auto normalized_spans = token_spans(result.normalized_text);
    for (std::size_t span_index = 0; span_index < normalized_spans.size(); ++span_index) {
        const auto& span = normalized_spans[span_index];
        const std::string token = result.normalized_text.substr(span.begin, span.end - span.begin);
        WordPronunciation word;
        word.surface = token;
        word.source_offset = span.begin;
        for (std::size_t replacement_index = 0;
             replacement_index < result.dictionary_replacements.size();
             ++replacement_index) {
            const auto& replacement = result.dictionary_replacements[replacement_index];
            const auto replacement_end = replacement.offset + replacement.input.size();
            if (span.begin >= replacement.offset && span.end <= replacement_end) {
                word.dictionary_replacement = replacement_index;
                word.from_dictionary = true;
                if (span.begin == replacement.offset)
                    word.pronunciation = replacement.output;
                break;
            }
        }
        const auto* matched_entry =
            options.dictionary && options.apply_dictionary && span_index < matched_entries.size()
                ? matched_entries[span_index]
                : nullptr;
        if (matched_entry) {
            if (matched_entry->match != PronunciationDictionary::Match::ExactPhrase) {
                word.from_dictionary = true;
                word.pronunciation = matched_entry->pronunciation;
                if (options.resolve_stress && options.stress_mode != StressMode::Disabled)
                    word.stressed_vowel = matched_entry->stressed_vowel;
            }
        }
        result.words.push_back(word);
        if (stress_enabled && matched_entry &&
            matched_entry->match == PronunciationDictionary::Match::ExactPhrase)
            continue;
        if (stress_enabled && (options.diagnostics || word.from_dictionary))
            result.stress_decisions.push_back({token,
                                               word.stressed_vowel,
                                               word.from_dictionary,
                                               word.from_dictionary
                                                   ? "pronunciation dictionary"
                                                   : "no deterministic stress rule"});
    }
    if (options.resolve_stress && options.stress_mode == StressMode::Automatic)
        result.warnings.push_back(
            {WarningCode::AutomaticStressUnavailable,
             "Automatic stress is not available until a parity-proven backend is added"});
    return result;
}

const char* to_string(Language language) noexcept {
    switch (language) {
    case Language::Auto:
        return "auto";
    case Language::Russian:
        return "russian";
    case Language::English:
        return "english";
    }
    return "unknown";
}
const char* to_string(WarningCode code) noexcept {
    switch (code) {
    case WarningCode::InvalidUtf8:
        return "invalid_utf8";
    case WarningCode::UnsupportedLanguage:
        return "unsupported_language";
    case WarningCode::AmbiguousNormalization:
        return "ambiguous_normalization";
    case WarningCode::UnresolvedNumber:
        return "unresolved_number";
    case WarningCode::AutomaticStressUnavailable:
        return "automatic_stress_unavailable";
    case WarningCode::DictionaryParseError:
        return "dictionary_parse_error";
    }
    return "unknown";
}
} // namespace tts_front
