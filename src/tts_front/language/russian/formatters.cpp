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
                           : number(n / 10 * 10) + " " + ((genitive ? gen : loc)[n % 10]);
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

} // namespace tts_front::detail::russian
