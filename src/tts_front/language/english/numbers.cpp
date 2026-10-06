#include "numbers.hpp"

namespace tts_front::detail::english {

std::string number(long long value) {
    static const char* const ones[] = {"zero",    "one",     "two",       "three",    "four",
                                       "five",    "six",     "seven",     "eight",    "nine",
                                       "ten",     "eleven",  "twelve",    "thirteen", "fourteen",
                                       "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    static const char* const tens[] = {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    if (value < 0)
        return "minus " + number(-value);
    if (value < 20)
        return ones[value];
    if (value < 100)
        return std::string(tens[value / 10]) +
               (value % 10 ? " " + std::string(ones[value % 10]) : "");
    if (value < 1000)
        return std::string(ones[value / 100]) + " hundred" +
               (value % 100 ? " " + number(value % 100) : "");
    if (value < 1000000)
        return number(value / 1000) + " thousand" +
               (value % 1000 ? " " + number(value % 1000) : "");
    if (value < 1000000000)
        return number(value / 1000000) + " million" +
               (value % 1000000 ? " " + number(value % 1000000) : "");
    return std::to_string(value);
}

const char* ordinal_suffix(long long value) {
    const auto last_two = value % 100;
    if (last_two >= 11 && last_two <= 13)
        return "th";
    switch (value % 10) {
    case 1:
        return "st";
    case 2:
        return "nd";
    case 3:
        return "rd";
    default:
        return "th";
    }
}

std::string ordinal(long long value) {
    if (value < 0)
        return "minus " + ordinal(-value);
    if (value == 0)
        return "zeroth";
    static const char* const under_twenty[] = {
        "",          "first",     "second",      "third",      "fourth",
        "fifth",     "sixth",     "seventh",     "eighth",     "ninth",
        "tenth",     "eleventh",  "twelfth",     "thirteenth", "fourteenth",
        "fifteenth", "sixteenth", "seventeenth", "eighteenth", "nineteenth"};
    static const char* const tens[] = {"",
                                       "",
                                       "twentieth",
                                       "thirtieth",
                                       "fortieth",
                                       "fiftieth",
                                       "sixtieth",
                                       "seventieth",
                                       "eightieth",
                                       "ninetieth"};
    if (value < 20)
        return under_twenty[value];
    if (value < 100)
        return value % 10 == 0 ? tens[value / 10]
                               : number(value / 10 * 10) + " " + under_twenty[value % 10];
    if (value < 1000)
        return value % 100 == 0 ? number(value / 100) + " hundredth"
                                : number(value / 100) + " hundred " + ordinal(value % 100);
    if (value < 1000000)
        return value % 1000 == 0 ? number(value / 1000) + " thousandth"
                                 : number(value / 1000) + " thousand " + ordinal(value % 1000);
    if (value < 1000000000)
        return value % 1000000 == 0
                   ? number(value / 1000000) + " millionth"
                   : number(value / 1000000) + " million " + ordinal(value % 1000000);
    return std::to_string(value);
}

std::string digits(const std::string& value) {
    static const char* const names[] = {
        "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
    std::string result;
    for (const char digit : value) {
        if (!result.empty())
            result += ' ';
        result += names[digit - '0'];
    }
    return result;
}

} // namespace tts_front::detail::english
