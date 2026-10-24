#include "formatters.hpp"

#include "tts_front/language/russian/numbers.hpp"

#include <cstdlib>

namespace tts_front::detail::russian {

std::string plural_form(long long value, const char* one, const char* few, const char* many) {
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
std::string feminine_number(long long value) {
    const auto absolute = std::llabs(value);
    const auto suffix = absolute % 100;
    if (suffix >= 11 && suffix <= 14)
        return number(value);
    std::string result = number(value);
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
std::string genitive_number(long long value) {
    const auto absolute = std::llabs(value);
    if (absolute <= 20) {
        static const char* const forms[] = {
            "ноля",       "одного",      "двух",       "трёх",         "четырёх",
            "пяти",       "шести",       "семи",       "восьми",       "девяти",
            "десяти",     "одиннадцати", "двенадцати", "тринадцати",   "четырнадцати",
            "пятнадцати", "шестнадцати", "семнадцати", "восемнадцати", "девятнадцати",
            "двадцати"};
        const auto result = std::string(forms[absolute]);
        return value < 0 ? "минус " + result : result;
    }
    const auto remainder = absolute % 10;
    static const char* const tens_forms[] = {"",
                                             "",
                                             "двадцати",
                                             "тридцати",
                                             "сорока",
                                             "пятидесяти",
                                             "шестидесяти",
                                             "семидесяти",
                                             "восьмидесяти",
                                             "девяноста"};
    std::string result;
    if (absolute < 100) {
        result = tens_forms[absolute / 10];
        if (remainder != 0)
            result +=
                result.empty() ? genitive_number(remainder) : " " + genitive_number(remainder);
    } else if (absolute < 1000) {
        static const char* const hundreds_forms[] = {"",
                                                     "ста",
                                                     "двухсот",
                                                     "трёхсот",
                                                     "четырёхсот",
                                                     "пятисот",
                                                     "шестисот",
                                                     "семисот",
                                                     "восьмисот",
                                                     "девятисот"};
        result = hundreds_forms[absolute / 100];
        const auto hundreds_remainder = absolute % 100;
        if (hundreds_remainder != 0)
            result += " " + genitive_number(hundreds_remainder);
    } else {
        result = number(value);
    }
    if (value < 0)
        result = "минус " + result;
    return result;
}
std::string ordinal_day(int day) {
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
    return day >= 1 && day <= 31 ? ordinal[day] : number(day);
}
std::string year_ordinal(int n, YearCase grammatical_case) {
    const bool genitive = grammatical_case == YearCase::Genitive;
    const bool accusative = grammatical_case == YearCase::Accusative;
    static const char* const loc[] = {"",
                                      "первом",
                                      "втором",
                                      "третьем",
                                      "четвёртом",
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
                                      "четвёртого",
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
    static const char* const acc[] = {"",
                                      "первый",
                                      "второй",
                                      "третий",
                                      "четвёртый",
                                      "пятый",
                                      "шестой",
                                      "седьмой",
                                      "восьмой",
                                      "девятый",
                                      "десятый",
                                      "одиннадцатый",
                                      "двенадцатый",
                                      "тринадцатый",
                                      "четырнадцатый",
                                      "пятнадцатый",
                                      "шестнадцатый",
                                      "семнадцатый",
                                      "восемнадцатый",
                                      "девятнадцатый",
                                      "двадцатый"};
    if (n <= 20)
        return std::string((genitive ? gen : accusative ? acc : loc)[n]);
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
    static const char* const tens_acc[] = {"",
                                           "",
                                           "двадцатый",
                                           "тридцатый",
                                           "сороковой",
                                           "пятидесятый",
                                           "шестидесятый",
                                           "семидесятый",
                                           "восьмидесятый",
                                           "девяностый"};
    if (n < 100)
        return n % 10 == 0 ? std::string((genitive     ? tens_gen
                                          : accusative ? tens_acc
                                                       : tens_loc)[n / 10])
                           : number(n / 10 * 10) + " " +
                                 ((genitive     ? gen
                                   : accusative ? acc
                                                : loc)[n % 10]);
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
        static const char* const hundreds_acc[] = {"",
                                                   "сотый",
                                                   "двухсотый",
                                                   "трехсотый",
                                                   "четырехсотый",
                                                   "пятисотый",
                                                   "шестисотый",
                                                   "семисотый",
                                                   "восьмисотый",
                                                   "девятисотый"};
        return std::string((genitive     ? hundreds_gen
                            : accusative ? hundreds_acc
                                         : hundreds_loc)[n / 100]);
    }
    return number(n / 100 * 100) + " " + year_ordinal(n % 100, grammatical_case);
}
std::string year_prefix(int thousands) {
    if (thousands == 1)
        return "тысяча";
    if (thousands == 2)
        return "две тысячи";
    return number(thousands) + " " + plural_form(thousands, "тысяча", "тысячи", "тысяч");
}
std::string year_locative(int year) {
    if (year < 1000 || year > 9999)
        return number(year);
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
    return year_prefix(thousands) + " " + year_ordinal(rest, YearCase::Locative);
}
std::string year_genitive(int year) {
    if (year < 1000 || year > 9999)
        return number(year);
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
    return year_prefix(thousands) + " " + year_ordinal(rest, YearCase::Genitive);
}
std::string year_accusative(int year) {
    if (year < 1000 || year > 9999)
        return number(year);
    const int thousands = year / 1000;
    const int rest = year % 1000;
    if (rest == 0) {
        static const char* const exact[] = {"",
                                            "тысячный",
                                            "двухтысячный",
                                            "трехтысячный",
                                            "четырехтысячный",
                                            "пятитысячный",
                                            "шеститысячный",
                                            "семитысячный",
                                            "восьмитысячный",
                                            "девятитысячный"};
        return exact[thousands];
    }
    return year_prefix(thousands) + " " + year_ordinal(rest, YearCase::Accusative);
}
std::string decimal(long long integer, const std::string& fraction) {
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
    return feminine_number(integer) +
           (std::llabs(integer) % 10 == 1 && std::llabs(integer) % 100 != 11 ? " целая "
                                                                             : " целых ") +
           feminine_number(fractional) + " " + denominator_word;
}

std::string decimal_genitive(long long integer, const std::string& fraction) {
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
    return genitive_number(integer) + " целых " + genitive_number(fractional) + " " +
           denominator_word;
}

std::optional<std::string> monetary_decimal(const std::string_view integer_token,
                                            const long long integer,
                                            const std::string_view fraction) {
    if (fraction.empty() || fraction.size() > 2)
        return std::nullopt;

    auto minor = std::stoll(std::string(fraction));
    if (fraction.size() == 1)
        minor *= 10;

    std::string result = number_for_token(integer_token, integer) + " " +
                         plural_form(integer, "рубль", "рубля", "рублей");
    if (minor != 0)
        result += " " + number(minor) + " " +
                  plural_form(minor, "копейка", "копейки", "копеек");
    return result;
}

} // namespace tts_front::detail::russian
