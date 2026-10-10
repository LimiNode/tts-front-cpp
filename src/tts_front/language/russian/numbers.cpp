#include "numbers.hpp"

namespace tts_front::detail::russian {

const char* const ones[] = {"ноль",        "один",       "два",          "три",
                            "четыре",      "пять",       "шесть",        "семь",
                            "восемь",      "девять",     "десять",       "одиннадцать",
                            "двенадцать",  "тринадцать", "четырнадцать", "пятнадцать",
                            "шестнадцать", "семнадцать", "восемнадцать", "девятнадцать"};
const char* const tens[] = {"",
                            "",
                            "двадцать",
                            "тридцать",
                            "сорок",
                            "пятьдесят",
                            "шестьдесят",
                            "семьдесят",
                            "восемьдесят",
                            "девяносто"};
const char* const hundreds[] = {"",
                                "сто",
                                "двести",
                                "триста",
                                "четыреста",
                                "пятьсот",
                                "шестьсот",
                                "семьсот",
                                "восемьсот",
                                "девятьсот"};
std::string under_1000(int n) {
    std::string result;
    if (n >= 100) {
        result += hundreds[n / 100];
        n %= 100;
        if (n)
            result += " ";
    }
    if (n < 20) {
        if (n)
            result += ones[n];
    } else {
        result += tens[n / 10];
        if (n % 10)
            result += " " + std::string(ones[n % 10]);
    }
    return result.empty() ? "ноль" : result;
}
std::string number(long long n) {
    if (n < 0)
        return "минус " + number(-n);
    if (n < 1000)
        return under_1000(static_cast<int>(n));
    if (n < 1000000) {
        const int thousands = static_cast<int>(n / 1000);
        const int rest = static_cast<int>(n % 1000);
        std::string thousands_word = thousands == 1   ? "одна"
                                     : thousands == 2 ? "две"
                                                      : under_1000(thousands);
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
            result += " " + under_1000(rest);
        return result;
    }
    if (n < 1000000000) {
        const int millions = static_cast<int>(n / 1000000);
        const int rest = static_cast<int>(n % 1000000);
        std::string result =
            number(millions) +
            ((millions % 100 >= 11 && millions % 100 <= 14)
                 ? " миллионов"
                 : (millions % 10 == 1
                        ? " миллион"
                        : (millions % 10 >= 2 && millions % 10 <= 4 ? " миллиона" : " миллионов")));
        if (rest)
            result += " " + number(rest);
        return result;
    }
    return std::to_string(n);
}

std::string number_for_token(std::string_view token, long long value) {
    if (value == 0 && !token.empty() && token.front() == '-')
        return "минус ноль";
    return number(value);
}

} // namespace tts_front::detail::russian
