#include "detail/silero_stress_backend.hpp"
#include "detail/utf8.hpp"
#include "tts_front.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
bool is_combining_mark(std::uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
           (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
           (cp >= 0xfe20 && cp <= 0xfe2f);
}
bool is_word_codepoint(std::uint32_t cp) {
    return is_letter(cp) || is_digit(cp) || cp == '+' || cp == '#' || cp == '_';
}

bool is_numeric_separator(std::uint32_t cp) {
    return cp == '.' || cp == ',' || cp == ':' || cp == '%' || cp == '-' || cp == '/' ||
           cp == '+' || cp == '=' || (cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212;
}

bool is_range_connector(std::uint32_t cp) {
    return (cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212;
}

bool is_numeric_connector(std::uint32_t cp) {
    return is_numeric_separator(cp) ||
           (cp < 0x80 && std::ispunct(static_cast<unsigned char>(cp)) != 0);
}

bool is_supported_numeric_separator(std::uint32_t cp) {
    return cp == '.' || cp == ',' || cp == ':' || cp == '%';
}

bool is_lexical_numeric_boundary(std::uint32_t cp) {
    return is_letter(cp) || is_combining_mark(cp) || is_digit(cp) || cp == '_';
}

struct SourceRange {
    std::size_t offset = 0;
    std::size_t length = 0;
};

struct MappedRun {
    std::size_t output_begin = 0;
    std::size_t output_end = 0;
    SourceRange source;
    bool direct_copy = false;
};

struct MappedText {
    std::string text;
    std::vector<MappedRun> runs;
    const std::vector<SourceRange>* preserved_ranges = nullptr;

    static MappedText from_original(std::string_view original,
                                    const std::vector<SourceRange>* preserved_ranges) {
        MappedText result;
        result.text = std::string(original);
        result.preserved_ranges = preserved_ranges;
        if (!original.empty())
            result.runs.push_back({0, original.size(), {0, original.size()}, true});
        return result;
    }

    SourceRange source_range(std::size_t begin, std::size_t end) const {
        SourceRange result;
        bool has_source = false;
        auto run = std::lower_bound(
            runs.begin(), runs.end(), begin, [](const MappedRun& candidate, std::size_t position) {
                return candidate.output_end <= position;
            });
        for (; run != runs.end() && run->output_begin < end; ++run) {
            const auto overlap_begin = std::max(begin, run->output_begin);
            const auto overlap_end = std::min(end, run->output_end);
            if (overlap_begin >= overlap_end)
                continue;
            SourceRange source = run->source;
            if (run->direct_copy) {
                source.offset += overlap_begin - run->output_begin;
                source.length = overlap_end - overlap_begin;
            }
            if (!has_source) {
                result = source;
                has_source = true;
            } else {
                const auto source_end =
                    std::max(result.offset + result.length, source.offset + source.length);
                result.offset = std::min(result.offset, source.offset);
                result.length = source_end - result.offset;
            }
        }
        return result;
    }

    void append_copy(const MappedText& source, std::size_t begin, std::size_t end) {
        auto run = std::lower_bound(source.runs.begin(),
                                    source.runs.end(),
                                    begin,
                                    [](const MappedRun& candidate, std::size_t position) {
                                        return candidate.output_end <= position;
                                    });
        for (; run != source.runs.end() && run->output_begin < end; ++run) {
            const auto overlap_begin = std::max(begin, run->output_begin);
            const auto overlap_end = std::min(end, run->output_end);
            if (overlap_begin >= overlap_end)
                continue;
            SourceRange origin = run->source;
            if (run->direct_copy) {
                origin.offset += overlap_begin - run->output_begin;
                origin.length = overlap_end - overlap_begin;
            }
            append(source.text.substr(overlap_begin, overlap_end - overlap_begin),
                   origin,
                   run->direct_copy);
        }
    }

    void append_generated(const MappedText& source,
                          std::size_t begin,
                          std::size_t end,
                          std::string_view replacement) {
        append(replacement, source.source_range(begin, end), false);
    }

  private:
    void append(std::string_view value, SourceRange source, bool direct_copy) {
        if (value.empty())
            return;
        const auto output_begin = text.size();
        text += value;
        const auto output_end = text.size();
        if (direct_copy && !runs.empty() && runs.back().direct_copy &&
            runs.back().output_end == output_begin &&
            runs.back().source.offset + runs.back().source.length == source.offset) {
            runs.back().output_end = output_end;
            runs.back().source.length += source.length;
            return;
        }
        runs.push_back({output_begin, output_end, source, direct_copy});
    }
};

MappedText cleanup_text(const MappedText& input) {
    MappedText result;
    result.preserved_ranges = input.preserved_ranges;
    result.text.reserve(input.text.size());
    std::vector<CodePoint> points;
    if (!decode_utf8(input.text, points))
        return input;
    bool pending_space = false;
    std::size_t space_begin = 0;
    for (const auto& point : points) {
        const bool space = point.value == ' ' || point.value == '\t' || point.value == '\n' ||
                           point.value == '\r' || point.value == '\v' || point.value == '\f' ||
                           point.value == 0x00a0 || point.value == 0x202f;
        if (space) {
            if (!pending_space && !result.text.empty())
                space_begin = point.offset;
            pending_space = !result.text.empty();
            continue;
        }
        const bool punctuation = point.value == ',' || point.value == '.' || point.value == ';' ||
                                 point.value == ':' || point.value == '!' || point.value == '?';
        if (pending_space && !punctuation && !result.text.empty())
            result.append_generated(input, space_begin, point.offset, " ");
        pending_space = false;
        result.append_copy(input, point.offset, point.offset + point.length);
    }
    return result;
}

template <typename Formatter>
MappedText
replace_matches(const MappedText& input, const std::regex& pattern, Formatter formatter) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), pattern), end; it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        output.append_copy(input, cursor, begin);
        const auto source = input.source_range(begin, finish);
        if (input.preserved_ranges &&
            std::any_of(input.preserved_ranges->begin(),
                        input.preserved_ranges->end(),
                        [source](const SourceRange& preserved) {
                            return source.offset < preserved.offset + preserved.length &&
                                   preserved.offset < source.offset + source.length;
                        })) {
            output.append_copy(input, begin, finish);
            cursor = finish;
            continue;
        }
        output.append_generated(input, begin, finish, formatter(*it, input));
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

bool numeric_match_has_valid_boundaries(const std::vector<CodePoint>& points,
                                        std::size_t begin,
                                        std::size_t end) {
    const auto first = std::lower_bound(
        points.begin(), points.end(), begin, [](const CodePoint& point, std::size_t offset) {
            return point.offset < offset;
        });
    const auto after = std::lower_bound(
        points.begin(), points.end(), end, [](const CodePoint& point, std::size_t offset) {
            return point.offset < offset;
        });
    auto first_digit = after;
    for (auto point = first; point != after; ++point) {
        if (is_digit(point->value)) {
            first_digit = point;
            break;
        }
    }
    if (first_digit == after)
        return true;

    auto last_digit = first_digit;
    for (auto point = first_digit; point != after; ++point) {
        if (is_digit(point->value))
            last_digit = point;
    }
    const auto first_index = static_cast<std::size_t>(first_digit - points.begin());
    const auto next_index = static_cast<std::size_t>(after - points.begin());
    const CodePoint* previous = first_index == 0 ? nullptr : &points[first_index - 1];
    const CodePoint* before_previous = first_index < 2 ? nullptr : &points[first_index - 2];
    const CodePoint* next = next_index == points.size() ? nullptr : &points[next_index];
    const CodePoint* after_next =
        next_index + 1 >= points.size() ? nullptr : &points[next_index + 1];

    if (previous && is_lexical_numeric_boundary(previous->value))
        return false;
    if (previous && is_numeric_separator(previous->value) && before_previous &&
        is_digit(before_previous->value))
        return false;
    if (previous && previous->value == '-' && before_previous &&
        is_lexical_numeric_boundary(before_previous->value))
        return false;
    bool suffix_consumed = false;
    for (auto point = last_digit + 1; point != after; ++point) {
        if (!is_digit(point->value)) {
            suffix_consumed = true;
            break;
        }
    }
    if (next && is_lexical_numeric_boundary(next->value) && !suffix_consumed)
        return false;
    if (next && is_numeric_separator(next->value) && after_next && is_digit(after_next->value))
        return false;
    return true;
}

template <typename Formatter>
MappedText
replace_numeric_matches(const MappedText& input, const std::regex& pattern, Formatter formatter) {
    std::vector<CodePoint> points;
    if (!decode_utf8(input.text, points))
        return input;
    return replace_matches(input, pattern, [&](const std::smatch& match, const MappedText& source) {
        const auto begin = static_cast<std::size_t>(match.position());
        const auto end = begin + static_cast<std::size_t>(match.length());
        if (!numeric_match_has_valid_boundaries(points, begin, end))
            return match.str();
        return formatter(match, source);
    });
}

struct RegexPatterns {
    const std::regex technical_url{R"(https?://[^\s]+)"};
    const std::regex technical_email{R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})"};
    const std::regex technical_ipv4{R"(\b\d{1,3}(?:\.\d{1,3}){3}\b)"};
    const std::regex technical_version{R"(\b[vV]\d+(?:\.\d+)+\b)"};
    const std::regex technical_http{R"(\bHTTP/\d+(?:\.\d+)?\b)"};
    const std::regex technical_gpu{R"(\b(?:RTX|CUDA|GPU|API)\s+\d+(?:\.\d+)?\b)"};
    const std::regex technical_cpp{R"(C\+\+)"};
    const std::regex technical_csharp{R"(C#)"};
    const std::regex technical_identifier{
        R"((?:#[0-9]+)|(?:[A-Za-z][A-Za-z0-9+._$#-]*[-+$][A-Za-z0-9._$#-]+))"};
    const std::regex grouped_number{R"((^|[^0-9])-?\d{1,3}(?:\s+\d{3})+)"};
    const std::regex en_comma_grouped_number{R"((^|[^0-9])(-?\d{1,3}(?:,\d{3})+(?:\.\d+)?))"};
    const std::regex en_comma_grouped_value{R"(-?\d{1,3}(?:,\d{3})+(?:\.\d+)?)"};
    const std::regex en_comma_grouped_percent{R"(-?\d{1,3}(?:,\d{3})+(?:\.\d+)?%)"};
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
    const std::regex en_ordinal{R"((^|[^A-Za-z0-9-])(-?[0-9]+)(st|nd|rd|th)\b)"};
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

struct WarningSink {
    std::vector<TextWarning>& warnings;
    std::vector<SourceRange> preserved_ranges;

    void add_range(WarningCode code, std::string message, std::size_t offset, std::size_t length) {
        warnings.push_back({code, std::move(message), offset, length});
    }

    void add(WarningCode code, std::string message, SourceRange source) {
        if (source.length != 0)
            preserved_ranges.push_back(source);
        add_range(code, std::move(message), source.offset, source.length);
    }
};

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match) {
    const auto begin = static_cast<std::size_t>(match.position());
    warnings.add(code, std::move(message), text.source_range(begin, begin + match.length()));
}

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match,
                 std::size_t group) {
    const auto begin = static_cast<std::size_t>(match.position(group));
    warnings.add(code, std::move(message), text.source_range(begin, begin + match.length(group)));
}

void add_warning_span(WarningSink& warnings,
                      WarningCode code,
                      std::string message,
                      const MappedText& text,
                      const std::smatch& match,
                      std::size_t begin_group,
                      std::size_t end_group) {
    const auto begin = static_cast<std::size_t>(match.position(begin_group));
    const auto end = static_cast<std::size_t>(match.position(end_group)) +
                     static_cast<std::size_t>(match.length(end_group));
    warnings.add(code, std::move(message), text.source_range(begin, end));
}

void add_warning_without_suffix(WarningSink& warnings,
                                WarningCode code,
                                std::string message,
                                const MappedText& text,
                                const std::smatch& match,
                                std::size_t suffix_group) {
    const auto begin = static_cast<std::size_t>(match.position());
    const auto length = static_cast<std::size_t>(match.length() - match.length(suffix_group));
    warnings.add(code, std::move(message), text.source_range(begin, begin + length));
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
const char* en_ordinal_suffix(long long n) {
    const auto last_two = n % 100;
    if (last_two >= 11 && last_two <= 13)
        return "th";
    switch (n % 10) {
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
std::string en_ordinal(long long n) {
    if (n < 0)
        return "minus " + en_ordinal(-n);
    if (n == 0)
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
    if (n < 20)
        return under_twenty[n];
    if (n < 100)
        return n % 10 == 0 ? tens[n / 10] : en_number(n / 10 * 10) + " " + under_twenty[n % 10];
    if (n < 1000)
        return n % 100 == 0 ? en_number(n / 100) + " hundredth"
                            : en_number(n / 100) + " hundred " + en_ordinal(n % 100);
    if (n < 1000000)
        return n % 1000 == 0 ? en_number(n / 1000) + " thousandth"
                             : en_number(n / 1000) + " thousand " + en_ordinal(n % 1000);
    if (n < 1000000000)
        return n % 1000000 == 0 ? en_number(n / 1000000) + " millionth"
                                : en_number(n / 1000000) + " million " + en_ordinal(n % 1000000);
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
std::string number_or_original(const std::string& token,
                               bool russian,
                               WarningSink& warnings,
                               SourceRange source) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        warnings.add(WarningCode::UnresolvedNumber, "Unable to parse number", source);
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
std::string marker_for(std::string_view text, std::size_t index) {
    std::string suffix;
    do {
        suffix.push_back(static_cast<char>('a' + (index % 26)));
        index = index / 26;
    } while (index != 0);
    std::string marker = "\x01tts_front_protected_" + suffix + "\x02";
    while (text.find(marker) != std::string::npos)
        marker.insert(marker.size() - 1, "x");
    return marker;
}
MappedText protect_technical(MappedText text, std::vector<ProtectedSpan>& protected_spans) {
    const auto& patterns = regex_patterns();
    const std::array<const std::regex*, 9> technical_patterns = {&patterns.technical_url,
                                                                 &patterns.technical_email,
                                                                 &patterns.technical_ipv4,
                                                                 &patterns.technical_version,
                                                                 &patterns.technical_http,
                                                                 &patterns.technical_gpu,
                                                                 &patterns.technical_identifier,
                                                                 &patterns.technical_cpp,
                                                                 &patterns.technical_csharp};
    for (const auto* pattern : technical_patterns) {
        MappedText output;
        output.preserved_ranges = text.preserved_ranges;
        std::size_t cursor = 0;
        for (std::sregex_iterator it(text.text.begin(), text.text.end(), *pattern), end; it != end;
             ++it) {
            const auto begin = static_cast<std::size_t>(it->position());
            const auto finish = begin + static_cast<std::size_t>(it->length());
            output.append_copy(text, cursor, begin);
            const auto marker = marker_for(text.text, protected_spans.size());
            protected_spans.push_back({marker, it->str()});
            output.append_generated(text, begin, finish, marker);
            cursor = finish;
        }
        output.append_copy(text, cursor, text.text.size());
        text = std::move(output);
    }
    return text;
}
MappedText restore_technical(MappedText text, const std::vector<ProtectedSpan>& protected_spans) {
    for (auto span = protected_spans.rbegin(); span != protected_spans.rend(); ++span) {
        for (std::size_t at = text.text.find(span->marker); at != std::string::npos;
             at = text.text.find(span->marker, at + span->value.size())) {
            MappedText output;
            output.preserved_ranges = text.preserved_ranges;
            output.append_copy(text, 0, at);
            output.append_generated(text, at, at + span->marker.size(), span->value);
            output.append_copy(text, at + span->marker.size(), text.text.size());
            text = std::move(output);
        }
    }
    return text;
}

// Technical shielding intentionally runs before the general numeric pass, but
// numeric-leading scientific/encoded candidates must be admitted first.  If
// they are hidden as (for example) `e+3` or `$3`, the generic normalizer can
// rewrite only the leading numeric fragment.
MappedText protect_numeric_technical_candidates(MappedText text,
                                                WarningSink& warnings,
                                                std::vector<ProtectedSpan>& protected_spans) {
    std::vector<CodePoint> points;
    if (!decode_utf8(text.text, points))
        return text;

    MappedText output;
    output.preserved_ranges = text.preserved_ranges;
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < points.size();) {
        if (points[index].value == '$' && index + 1 < points.size() &&
            is_digit(points[index + 1].value)) {
            auto numeric_end = index + 1;
            while (numeric_end < points.size() && is_digit(points[numeric_end].value))
                ++numeric_end;
            while (numeric_end < points.size() &&
                   (points[numeric_end].value == ',' || points[numeric_end].value == '.') &&
                   numeric_end + 1 < points.size() && is_digit(points[numeric_end + 1].value)) {
                numeric_end += 1;
                while (numeric_end < points.size() && is_digit(points[numeric_end].value))
                    ++numeric_end;
            }
            if (numeric_end < points.size() && points[numeric_end].value == '%') {
                const auto begin = points[index].offset;
                const auto end = points[numeric_end].offset + points[numeric_end].length;
                output.append_copy(text, cursor, begin);
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unsupported numeric-like candidate preserved verbatim",
                             text.source_range(begin, end));
                const auto marker = marker_for(text.text, protected_spans.size());
                protected_spans.push_back({marker, text.text.substr(begin, end - begin)});
                output.append_generated(text, begin, end, marker);
                cursor = end;
                index = numeric_end + 1;
                continue;
            }
        }
        const bool starts_number = is_digit(points[index].value) ||
                                   (points[index].value == '-' && index + 1 < points.size() &&
                                    is_digit(points[index + 1].value));
        if (!starts_number || (index > 0 && is_lexical_numeric_boundary(points[index - 1].value))) {
            ++index;
            continue;
        }

        std::size_t end_index = index + (points[index].value == '-' ? 1 : 0);
        while (end_index < points.size() && is_digit(points[end_index].value))
            ++end_index;
        if (end_index < points.size() &&
            (points[end_index].value == '.' || points[end_index].value == ',') &&
            end_index + 1 < points.size() && is_digit(points[end_index + 1].value)) {
            ++end_index;
            while (end_index < points.size() && is_digit(points[end_index].value))
                ++end_index;
        }

        std::size_t suffix_end = end_index;
        bool encoded_suffix = false;
        if (end_index < points.size() &&
            (points[end_index].value == 'e' || points[end_index].value == 'E')) {
            auto probe = end_index + 1;
            if (probe < points.size() && (points[probe].value == '+' || points[probe].value == '-'))
                ++probe;
            const auto exponent_begin = probe;
            while (probe < points.size() && is_digit(points[probe].value))
                ++probe;
            if (probe != exponent_begin) {
                suffix_end = probe;
                encoded_suffix = true;
            }
        } else if (end_index < points.size() &&
                   (points[end_index].value == '#' || points[end_index].value == '$')) {
            auto probe = end_index + 1;
            const auto suffix_begin = probe;
            while (probe < points.size() &&
                   (is_digit(points[probe].value) || is_letter(points[probe].value) ||
                    is_combining_mark(points[probe].value) || points[probe].value == '_'))
                ++probe;
            if (probe != suffix_begin) {
                suffix_end = probe;
                encoded_suffix = true;
            }
        }

        if (!encoded_suffix) {
            ++index;
            continue;
        }

        const auto begin = points[index].offset;
        const auto end = points[suffix_end - 1].offset + points[suffix_end - 1].length;
        output.append_copy(text, cursor, begin);
        warnings.add(WarningCode::UnresolvedNumber,
                     "Unsupported numeric-like candidate preserved verbatim",
                     text.source_range(begin, end));
        const auto marker = marker_for(text.text, protected_spans.size());
        protected_spans.push_back({marker, text.text.substr(begin, end - begin)});
        output.append_generated(text, begin, end, marker);
        cursor = end;
        index = suffix_end;
    }
    output.append_copy(text, cursor, text.text.size());
    return output;
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

MappedText protect_malformed_numeric_candidates(MappedText text,
                                                WarningSink& warnings,
                                                std::vector<ProtectedSpan>& protected_spans,
                                                bool russian) {
    std::vector<CodePoint> points;
    if (!decode_utf8(text.text, points))
        return text;

    MappedText output;
    output.preserved_ranges = text.preserved_ranges;
    std::size_t cursor = 0;
    // Protected spans are emitted as well-formed control-byte markers. Carry
    // one forward state instead of searching backwards through the text for
    // every codepoint; this keeps the candidate admission pass linear.
    bool inside_marker = false;
    for (std::size_t index = 0; index < points.size();) {
        if (points[index].value == 0x01) {
            inside_marker = true;
            ++index;
            continue;
        }
        if (inside_marker) {
            if (points[index].value == 0x02)
                inside_marker = false;
            ++index;
            continue;
        }
        auto currency_context_probe = index;
        while (currency_context_probe > 0 && (points[currency_context_probe - 1].value == ' ' ||
                                              points[currency_context_probe - 1].value == '\t'))
            --currency_context_probe;
        if (currency_context_probe > 0 && points[currency_context_probe - 1].value == '+') {
            --currency_context_probe;
            while (currency_context_probe > 0 && (points[currency_context_probe - 1].value == ' ' ||
                                                  points[currency_context_probe - 1].value == '\t'))
                --currency_context_probe;
        }
        const bool currency_context =
            currency_context_probe > 0 && points[currency_context_probe - 1].value == '$';
        const bool starts_phone = points[index].value == '+' && index + 1 < points.size() &&
                                  is_digit(points[index + 1].value) && !currency_context;
        const bool starts_number = is_digit(points[index].value) ||
                                   (points[index].value == '-' && index + 1 < points.size() &&
                                    is_digit(points[index + 1].value));
        if (!starts_number && !starts_phone) {
            ++index;
            continue;
        }
        const auto begin = points[index].offset;
        std::size_t end_index = index + 1;
        std::size_t separators = 0;
        bool has_percent = false;
        bool percent_attached_to_numeric = false;
        bool has_range_connector = false;
        bool has_unsupported_numeric_connector = false;
        if (starts_phone) {
            while (end_index < points.size()) {
                if (is_digit(points[end_index].value)) {
                    ++end_index;
                    continue;
                }
                if (points[end_index].value == '-' || points[end_index].value == '/') {
                    ++separators;
                    ++end_index;
                    continue;
                }
                if (points[end_index].value == '(' || points[end_index].value == ')') {
                    ++end_index;
                    continue;
                }
                if (points[end_index].value == ' ' || points[end_index].value == '\t') {
                    auto probe = end_index + 1;
                    while (probe < points.size() &&
                           (points[probe].value == ' ' || points[probe].value == '\t'))
                        ++probe;
                    if (probe < points.size() && points[probe].value == '(') {
                        auto digit_probe = probe + 1;
                        while (digit_probe < points.size() && (points[digit_probe].value == ' ' ||
                                                               points[digit_probe].value == '\t'))
                            ++digit_probe;
                        if (digit_probe < points.size() && is_digit(points[digit_probe].value)) {
                            end_index = probe;
                            continue;
                        }
                    }
                    if (probe < points.size() && is_digit(points[probe].value)) {
                        end_index = probe;
                        continue;
                    }
                }
                break;
            }
        } else {
            while (end_index < points.size()) {
                const auto value = points[end_index].value;
                if (value == ' ' || value == '\t' || value == '\n' || value == '\r') {
                    auto probe = end_index + 1;
                    while (probe < points.size() &&
                           (points[probe].value == ' ' || points[probe].value == '\t' ||
                            points[probe].value == '\n' || points[probe].value == '\r'))
                        ++probe;
                    if (probe < points.size() &&
                        (points[probe].value == '%' || is_range_connector(points[probe].value))) {
                        end_index = probe;
                        continue;
                    }
                    if (has_range_connector && probe < points.size() &&
                        is_digit(points[probe].value)) {
                        end_index = probe;
                        continue;
                    }
                    break;
                }
                if ((value == '.' || value == ',' || value == ':') &&
                    (end_index + 1 >= points.size() || !is_digit(points[end_index + 1].value)))
                    break;
                if (is_numeric_connector(value)) {
                    if (value == '%') {
                        has_percent = true;
                        const auto probe = end_index + 1;
                        if (probe < points.size() &&
                            (is_lexical_numeric_boundary(points[probe].value) ||
                             points[probe].value == '%'))
                            percent_attached_to_numeric = true;
                    } else
                        ++separators;
                    if (is_range_connector(value))
                        has_range_connector = true;
                    if (!is_supported_numeric_separator(value) && end_index + 1 < points.size() &&
                        points[end_index + 1].value != ' ' && points[end_index + 1].value != '\t' &&
                        points[end_index + 1].value != '\n' && points[end_index + 1].value != '\r')
                        has_unsupported_numeric_connector = true;
                    ++end_index;
                    continue;
                }
                if (is_digit(value)) {
                    ++end_index;
                    continue;
                }
                break;
            }
        }
        const bool previous_is_connector =
            index > 0 && (points[index - 1].value == '-' || points[index - 1].value == '/' ||
                          points[index - 1].value == '+' || points[index - 1].value == '=');
        const bool embedded = index > 0 && (is_lexical_numeric_boundary(points[index - 1].value) ||
                                            (previous_is_connector && index > 1 &&
                                             is_lexical_numeric_boundary(points[index - 2].value)));
        if (embedded) {
            std::size_t grouped_end = end_index;
            while (grouped_end < points.size()) {
                auto probe = grouped_end;
                while (probe < points.size() &&
                       (points[probe].value == ' ' || points[probe].value == '\t' ||
                        points[probe].value == '\n' || points[probe].value == '\r'))
                    ++probe;
                const auto group_begin = probe;
                while (probe < points.size() && is_digit(points[probe].value))
                    ++probe;
                if (probe == group_begin)
                    break;
                while (probe < points.size()) {
                    const auto value = points[probe].value;
                    if ((value == '.' || value == ',' || value == ':') &&
                        probe + 1 < points.size() && is_digit(points[probe + 1].value)) {
                        probe += 1;
                        while (probe < points.size() && is_digit(points[probe].value))
                            ++probe;
                        continue;
                    }
                    if (value == '%') {
                        ++probe;
                        continue;
                    }
                    break;
                }
                grouped_end = probe;
            }
            end_index = grouped_end;
        }
        const auto end =
            end_index == 0 ? begin : points[end_index - 1].offset + points[end_index - 1].length;
        const auto candidate = text.text.substr(begin, end - begin);
        std::size_t currency_prefix_begin = index;
        auto currency_probe = index;
        while (currency_probe > 0 && (points[currency_probe - 1].value == ' ' ||
                                      points[currency_probe - 1].value == '\t'))
            --currency_probe;
        if (currency_probe > 0 &&
            (points[currency_probe - 1].value == '+' || points[currency_probe - 1].value == '-'))
            --currency_probe;
        while (currency_probe > 0 && (points[currency_probe - 1].value == ' ' ||
                                      points[currency_probe - 1].value == '\t'))
            --currency_probe;
        const bool currency_prefix = currency_probe > 0 && points[currency_probe - 1].value == '$';
        if (currency_prefix)
            currency_prefix_begin = currency_probe - 1;
        const bool attached_lexical_suffix = separators != 0 && end_index < points.size() &&
                                             is_lexical_numeric_boundary(points[end_index].value);
        const bool valid_english_comma_group =
            !russian && (std::regex_match(candidate, regex_patterns().en_comma_grouped_value) ||
                         std::regex_match(candidate, regex_patterns().en_comma_grouped_percent));
        const bool malformed_english_comma_group =
            !russian && candidate.find(',') != std::string::npos;
        const bool invalid_percent =
            has_percent &&
            (percent_attached_to_numeric ||
             (end_index < points.size() && is_lexical_numeric_boundary(points[end_index].value)));
        const bool comma_group_followed_by_word = [&] {
            const auto comma = candidate.find(',');
            if (comma == std::string::npos || candidate.find('.', comma + 1) != std::string::npos ||
                candidate.find(':', comma + 1) != std::string::npos ||
                candidate.size() - comma - 1 != 3)
                return false;
            auto tail_index = end_index;
            while (tail_index < points.size() &&
                   (points[tail_index].value == ' ' || points[tail_index].value == '\t' ||
                    points[tail_index].value == '\n' || points[tail_index].value == '\r'))
                ++tail_index;
            if (tail_index == points.size())
                return false;
            return is_letter(points[tail_index].value);
        }();
        bool valid_russian_date = false;
        if (russian && separators == 2) {
            const auto first_dot = candidate.find('.');
            const auto second_dot = candidate.find('.', first_dot + 1);
            if (first_dot != std::string::npos && second_dot != std::string::npos &&
                candidate.find('.', second_dot + 1) == std::string::npos) {
                long long day = 0, month = 0, year = 0;
                valid_russian_date =
                    try_parse_long(candidate.substr(0, first_dot), day) &&
                    try_parse_long(candidate.substr(first_dot + 1, second_dot - first_dot - 1),
                                   month) &&
                    try_parse_long(candidate.substr(second_dot + 1), year) &&
                    candidate.substr(0, first_dot).size() <= 2 &&
                    candidate.substr(first_dot + 1, second_dot - first_dot - 1).size() <= 2 &&
                    candidate.substr(second_dot + 1).size() == 4 &&
                    valid_date(
                        static_cast<int>(day), static_cast<int>(month), static_cast<int>(year)) &&
                    !attached_lexical_suffix;
            }
        }
        const auto hyphen_search_start = candidate.size() > 0 && candidate.front() == '-' ? 1 : 0;
        const bool unsupported_numeric_connector =
            starts_phone || candidate.find('+') != std::string::npos ||
            candidate.find('=') != std::string::npos ||
            candidate.find('-', hyphen_search_start) != std::string::npos ||
            candidate.find('/') != std::string::npos || has_range_connector ||
            has_unsupported_numeric_connector;
        if ((separators >= 2 || embedded || attached_lexical_suffix ||
             (comma_group_followed_by_word && !valid_english_comma_group) ||
             unsupported_numeric_connector || invalid_percent || malformed_english_comma_group ||
             (currency_prefix && has_percent)) &&
            !(valid_russian_date && !embedded) &&
            !(valid_english_comma_group && !embedded && !attached_lexical_suffix &&
              !(currency_prefix && has_percent))) {
            const auto protected_begin =
                currency_prefix && has_percent ? points[currency_prefix_begin].offset : begin;
            output.append_copy(text, cursor, protected_begin);
            const auto source = text.source_range(protected_begin, end);
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unsupported numeric-like candidate preserved verbatim",
                         source);
            const auto marker = marker_for(text.text, protected_spans.size());
            protected_spans.push_back(
                {marker, text.text.substr(protected_begin, end - protected_begin)});
            output.append_generated(text, protected_begin, end, marker);
            cursor = end;
        }
        index = end_index;
    }
    output.append_copy(text, cursor, text.text.size());
    return output;
}

MappedText collapse_grouped_numbers(const MappedText& input) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    for (std::sregex_iterator
             it(input.text.begin(), input.text.end(), regex_patterns().grouped_number),
         end;
         it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        const auto number_begin = begin + it->length(1);
        output.append_copy(input, cursor, number_begin);
        std::string number = it->str().substr(it->length(1));
        number.erase(std::remove_if(number.begin(),
                                    number.end(),
                                    [](unsigned char c) { return std::isspace(c) != 0; }),
                     number.end());
        output.append_generated(input, number_begin, finish, number);
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

MappedText collapse_english_comma_grouped_numbers(const MappedText& input) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    std::vector<CodePoint> points;
    if (!decode_utf8(input.text, points))
        return input;
    for (std::sregex_iterator
             it(input.text.begin(), input.text.end(), regex_patterns().en_comma_grouped_number),
         end;
         it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        const auto number_begin = begin + static_cast<std::size_t>(it->length(1));
        if (!numeric_match_has_valid_boundaries(points, begin, finish))
            continue;
        output.append_copy(input, cursor, number_begin);
        std::string number = it->str(2);
        number.erase(std::remove(number.begin(), number.end(), ','), number.end());
        output.append_generated(input, number_begin, finish, number);
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

MappedText normalize_ru(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_numeric_technical_candidates(std::move(text), warnings, protected_spans);
    text = protect_technical(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(std::move(text), warnings, protected_spans, true);
    text = collapse_grouped_numbers(std::move(text));
    text = replace_numeric_matches(
        text, regex_patterns().ru_date, [&](const std::smatch& match, const MappedText& source) {
            long long day = 0, month = 0, year = 0;
            if (!try_parse_long(match[1].str(), day) || !try_parse_long(match[2].str(), month) ||
                !try_parse_long(match[3].str(), year) ||
                !valid_date(
                    static_cast<int>(day), static_cast<int>(month), static_cast<int>(year))) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid Russian calendar date",
                            source,
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
    text = replace_numeric_matches(
        text, regex_patterns().ru_year, [&](const std::smatch& match, const MappedText& source) {
            long long year = 0;
            if (!try_parse_long(match[1].str(), year)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian year",
                            source,
                            match);
                return match.str();
            }
            return ru_year_locative(static_cast<int>(year)) + " году";
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().ru_decimal_percent,
        [&](const std::smatch& match, const MappedText& source) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian decimal percent",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            return ru_decimal(integer, match[2].str()) + " процента" + match[3].str();
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_percent, [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian percent",
                            source,
                            match);
                return match.str();
            }
            return ru_number(n) + " " + ru_form(n, "процент", "процента", "процентов");
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().ru_currency,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian currency",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            return ru_number(n) + " " + ru_form(n, "рубль", "рубля", "рублей") + match[3].str();
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_time, [&](const std::smatch& match, const MappedText& source) {
            long long h = 0, m = 0;
            if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m) ||
                h > 23 || m > 59) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid Russian clock time",
                            source,
                            match);
                return match.str();
            }
            return ru_number(h) + " " + ru_form(h, "час", "часа", "часов") + " " +
                   ru_feminine_number(m) + " " + ru_form(m, "минута", "минуты", "минут");
        });
    text = replace_numeric_matches(
        text, regex_patterns().ru_decimal, [&](const std::smatch& match, const MappedText& source) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian decimal",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            return ru_decimal(integer, match[2].str()) + match[3].str();
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().ru_measurement,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian measurement",
                                           source,
                                           match,
                                           3);
                return match.str();
            }
            const auto unit_source = match[2].str();
            const std::string unit =
                unit_source.find("кг") == 0 || unit_source.find("килограмм") == 0
                    ? ru_form(n, "килограмм", "килограмма", "килограммов")
                : unit_source.find("км") == 0 || unit_source.find("километр") == 0
                    ? ru_form(n, "километр", "километра", "километров")
                : unit_source.find("см") == 0 || unit_source.find("сантиметр") == 0
                    ? ru_form(n, "сантиметр", "сантиметра", "сантиметров")
                : unit_source.find("мм") == 0 || unit_source.find("миллиметр") == 0
                    ? ru_form(n, "миллиметр", "миллиметра", "миллиметров")
                : unit_source == "м" ? ru_form(n, "метр", "метра", "метров")
                : unit_source == "ГБ" ? ru_form(n, "гигабайт", "гигабайта", "гигабайт")
                                      : ru_form(n, "мегабайт", "мегабайта", "мегабайт");
            return ru_number(n) + " " + unit + match[3].str();
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().generic_ru_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(match[2].str(),
                                      true,
                                      warnings,
                                      source.source_range(number_begin, number_end));
        });
    text = replace_matches(
        std::move(text),
        regex_patterns().ru_abbreviation_td,
        [](const auto& match, const MappedText&) { return match[1].str() + "так далее"; });
    text = replace_matches(
        std::move(text),
        regex_patterns().ru_abbreviation_tp,
        [](const auto& match, const MappedText&) { return match[1].str() + "тому подобное"; });
    return restore_technical(std::move(text), protected_spans);
}

MappedText normalize_en(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_numeric_technical_candidates(std::move(text), warnings, protected_spans);
    text = protect_technical(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(std::move(text), warnings, protected_spans, false);
    text = collapse_english_comma_grouped_numbers(std::move(text));
    text = collapse_grouped_numbers(std::move(text));
    text =
        replace_numeric_matches(text,
                                regex_patterns().en_currency_decimal,
                                [&](const std::smatch& match, const MappedText& source) {
                                    long long dollars = 0, cents = 0;
                                    const auto fraction = match[2].str();
                                    if (!try_parse_long(match[1].str(), dollars) ||
                                        fraction.size() > 2 || !try_parse_long(fraction, cents)) {
                                        add_warning(warnings,
                                                    WarningCode::UnresolvedNumber,
                                                    "Unable to parse English currency",
                                                    source,
                                                    match);
                                        return match.str();
                                    }
                                    if (fraction.size() == 1)
                                        cents *= 10;
                                    return en_number(dollars) +
                                           (dollars == 1 ? " dollar" : " dollars") + " " +
                                           en_number(cents) + (cents == 1 ? " cent" : " cents");
                                });
    text = replace_numeric_matches(text,
                                   regex_patterns().en_currency_integer,
                                   [&](const std::smatch& match, const MappedText& source) {
                                       long long dollars = 0;
                                       if (!try_parse_long(match[1].str(), dollars)) {
                                           add_warning(warnings,
                                                       WarningCode::UnresolvedNumber,
                                                       "Unable to parse English currency",
                                                       source,
                                                       match);
                                           return match.str();
                                       }
                                       return en_number(dollars) +
                                              (dollars == 1 ? " dollar" : " dollars");
                                   });
    text = replace_numeric_matches(
        text, regex_patterns().en_ordinal, [&](const std::smatch& match, const MappedText& source) {
            long long value = 0;
            const auto suffix = match[3].str();
            if (!try_parse_long(match[2].str(), value) || suffix != en_ordinal_suffix(value)) {
                add_warning_span(warnings,
                                 WarningCode::UnresolvedNumber,
                                 "Unable to parse English ordinal",
                                 source,
                                 match,
                                 2,
                                 3);
                return match.str();
            }
            return match[1].str() + en_ordinal(value);
        });
    text = replace_numeric_matches(
        text, regex_patterns().en_percent, [&](const std::smatch& match, const MappedText& source) {
            const auto value = match[1].str();
            const auto dot = value.find('.');
            long long integer = 0;
            if (!try_parse_long(dot == std::string::npos ? value : value.substr(0, dot), integer)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English percent",
                            source,
                            match);
                return match.str();
            }
            return (dot == std::string::npos
                        ? en_number(integer)
                        : en_number(integer) + " point " + en_digits(value.substr(dot + 1))) +
                   " percent";
        });
    text = replace_numeric_matches(
        text, regex_patterns().en_time, [&](const std::smatch& match, const MappedText& source) {
            long long h = 0, m = 0;
            if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m) ||
                h > 23 || m > 59) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Invalid English clock time",
                            source,
                            match);
                return match.str();
            }
            return en_number(h) + (h == 1 ? " hour " : " hours ") + en_number(m) +
                   (m == 1 ? " minute" : " minutes");
        });
    text = replace_numeric_matches(
        text, regex_patterns().en_decimal, [&](const std::smatch& match, const MappedText& source) {
            const auto value = match[2].str();
            const auto dot = value.find('.');
            long long integer = 0;
            if (!try_parse_long(value.substr(0, dot), integer)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse English decimal",
                            source,
                            match,
                            2);
                return match.str();
            }
            return match[1].str() + en_number(integer) + " point " +
                   en_digits(value.substr(dot + 1));
        });
    text = replace_numeric_matches(
        text,
        regex_patterns().en_measurement,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse English measurement",
                                           source,
                                           match,
                                           3);
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
    text = replace_numeric_matches(
        text,
        regex_patterns().generic_en_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(match[2].str(),
                                      false,
                                      warnings,
                                      source.source_range(number_begin, number_end));
        });
    return restore_technical(std::move(text), protected_spans);
}

struct Span {
    std::size_t begin = 0;
    std::size_t end = 0;
};
std::vector<Span> token_spans(const std::string& text) {
    std::vector<CodePoint> points;
    if (!decode_utf8(text, points))
        return {};
    std::vector<Span> spans;
    std::size_t begin = std::string::npos;
    std::size_t end = 0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& point = points[index];
        const bool embedded_dot = point.value == '.' && index > 0 && index + 1 < points.size() &&
                                  is_digit(points[index - 1].value) &&
                                  is_digit(points[index + 1].value);
        if (is_word_codepoint(point.value) || embedded_dot) {
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

// Automatic expansion is deliberately an allowlist.  Uppercase-looking text
// is not enough evidence that a token should be spelled out letter by letter:
// lexical acronyms such as НАТО, МИД and ЗАГС must remain unchanged unless a
// caller supplies an explicit dictionary entry.
std::optional<std::string> safe_russian_initialism(std::string_view token) {
    if (token == "ВК")
        return "вэ ка";
    if (token == "ООО")
        return "о о о";
    if (token == "РФ")
        return "эр эф";
    if (token == "МГУ")
        return "эм гэ у";
    if (token == "ФСБ")
        return "эф эс бэ";
    if (token == "МФЦ")
        return "эм эф цэ";
    if (token == "ИП")
        return "и пэ";
    return std::nullopt;
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

[[maybe_unused]] std::optional<std::filesystem::path>
resolve_silero_bundle(const TextFrontendOptions& options) {
    if (!options.silero_bundle_path.empty())
        return std::filesystem::path(options.silero_bundle_path);
    if (const auto* environment = std::getenv("TTS_FRONT_SILERO_BUNDLE");
        environment != nullptr && *environment != '\0')
        return std::filesystem::path(environment);
    return std::nullopt;
}

} // namespace

struct TextFrontend::Impl {
    std::mutex silero_mutex;
    std::unordered_map<std::string, std::shared_ptr<detail::SileroStressBackend>> silero_backends;

    std::shared_ptr<detail::SileroStressBackend>
    backend_for(const std::filesystem::path& bundle_root) {
        std::error_code error;
        const auto resolved_root = std::filesystem::absolute(bundle_root, error);
        const auto stable_root = (error ? bundle_root : resolved_root).lexically_normal();
        const auto key = stable_root.string();
        std::lock_guard lock(silero_mutex);
        if (const auto found = silero_backends.find(key); found != silero_backends.end())
            return found->second;
        auto backend = std::make_shared<detail::SileroStressBackend>(
            detail::SileroStressBackendConfig{stable_root});
        silero_backends.emplace(key, backend);
        return backend;
    }
};

TextFrontend::TextFrontend() : impl_(std::make_shared<Impl>()) {}
TextFrontend::~TextFrontend() = default;

bool TextFrontendResult::has_uncertainty() const noexcept {
    return !warnings.empty();
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
    WarningSink warning_sink{result.warnings, {}};
    MappedText text = MappedText::from_original(input, &warning_sink.preserved_ranges);
    if (options.cleanup_spacing)
        text = cleanup_text(text);
    bool has_cyrillic = false;
    bool has_latin = false;
    Language language = options.language;
    if (language == Language::Auto)
        language = detect_language(text.text, has_cyrillic, has_latin);
    if (options.language == Language::Auto && has_cyrillic && has_latin)
        warning_sink.add_range(WarningCode::AmbiguousNormalization,
                               "Mixed Cyrillic/Latin input uses Russian normalization by policy",
                               0,
                               input.size());
    if (language != Language::Russian && language != Language::English) {
        result.warnings.push_back({WarningCode::UnsupportedLanguage, "Unsupported language", 0, 0});
        return result;
    }
    if (options.normalize) {
        switch (language) {
        case Language::Russian:
            text = normalize_ru(std::move(text), warning_sink);
            break;
        case Language::English:
            text = normalize_en(std::move(text), warning_sink);
            break;
        case Language::Auto:
            break;
        }
    }
    if (options.cleanup_spacing)
        text = cleanup_text(text);
    result.normalized_text = text.text;
    result.pronunciation_text = text.text;
    const bool stress_enabled =
        options.resolve_stress && options.stress_mode != StressMode::Disabled;

    // Dictionary phrases are matched on complete token sequences, longest first, without rescanning output.
    std::vector<const PronunciationDictionary::Entry*> matched_entries;
    const auto spans = token_spans(text.text);
    matched_entries.assign(spans.size(), nullptr);
    std::vector<std::string> automatic_replacements(spans.size());
    if ((options.apply_dictionary && options.dictionary) || options.expand_initialisms) {
        struct PhraseCandidate {
            const PronunciationDictionary::Entry* entry = nullptr;
            std::vector<Span> spans;
        };
        std::vector<PhraseCandidate> phrases;
        if (options.apply_dictionary && options.dictionary) {
            for (const auto& entry : options.dictionary->entries()) {
                if (entry.match == PronunciationDictionary::Match::ExactPhrase)
                    phrases.push_back({&entry, token_spans(entry.pattern)});
            }
        }
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
                const auto input_phrase = text.text.substr(
                    spans[i].begin, spans[i + phrase_spans.size() - 1].end - spans[i].begin);
                const auto pattern_phrase =
                    entry.pattern.substr(phrase_spans.front().begin,
                                         phrase_spans.back().end - phrase_spans.front().begin);
                const bool match = input_phrase == pattern_phrase;
                if (match && (!best || phrase_spans.size() > best_end - i)) {
                    best = &entry;
                    best_end = i + phrase_spans.size();
                }
            }
            const auto token = text.text.substr(spans[i].begin, spans[i].end - spans[i].begin);
            if (best) {
                for (std::size_t j = i; j < best_end; ++j)
                    matched_entries[j] = best;
                rendered.append(text.text, cursor, spans[i].begin - cursor);
                rendered += best->pronunciation;
                const auto phrase_surface =
                    text.text.substr(spans[i].begin, spans[best_end - 1].end - spans[i].begin);
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
            rendered.append(text.text, cursor, spans[i].begin - cursor);
            const auto* entry = options.apply_dictionary && options.dictionary
                                    ? options.dictionary->find_token(token)
                                    : nullptr;
            if (entry) {
                matched_entries[i] = entry;
                rendered += entry->pronunciation;
                if (entry->pronunciation != token)
                    result.dictionary_replacements.push_back(
                        {token, entry->pronunciation, spans[i].begin});
            } else if (options.expand_initialisms && language == Language::Russian) {
                if (const auto replacement = safe_russian_initialism(token)) {
                    automatic_replacements[i] = *replacement;
                    rendered += *replacement;
                    result.automatic_rewrites.push_back(
                        {token, *replacement, spans[i].begin, "safe Russian initialism", true});
                } else {
                    rendered += token;
                }
            } else {
                rendered += token;
            }
            cursor = spans[i].end;
            ++i;
        }
        rendered += text.text.substr(cursor);
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
        if (!matched_entry && span_index < automatic_replacements.size() &&
            !automatic_replacements[span_index].empty()) {
            word.pronunciation = automatic_replacements[span_index];
            word.from_automatic_rewrite = true;
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
    if (options.resolve_stress && options.stress_mode == StressMode::Automatic) {
        const auto add_unavailable_warning = [&](std::string message) {
            result.warnings.push_back(
                {WarningCode::AutomaticStressUnavailable, std::move(message), 0, 0});
        };
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
        const auto bundle_root = resolve_silero_bundle(options);
        if (!bundle_root) {
            add_unavailable_warning(
                "Automatic stress bundle is not configured (set TTS_FRONT_SILERO_BUNDLE or "
                "TextFrontendOptions::silero_bundle_path)");
        } else {
            const auto deterministic_pronunciation = result.pronunciation_text;
            const auto deterministic_words = result.words;
            const auto deterministic_stress_decisions = result.stress_decisions;
            try {
                struct ProtectedRewrite {
                    std::size_t begin;
                    std::size_t end;
                    std::string output;
                };
                std::vector<ProtectedRewrite> protected_rewrites;
                for (std::size_t index = 0; index < normalized_spans.size();) {
                    const auto* entry =
                        index < matched_entries.size() ? matched_entries[index] : nullptr;
                    if (entry) {
                        std::size_t end = index + 1;
                        if (entry->match == PronunciationDictionary::Match::ExactPhrase) {
                            while (end < matched_entries.size() && matched_entries[end] == entry)
                                ++end;
                        }
                        protected_rewrites.push_back({normalized_spans[index].begin,
                                                      normalized_spans[end - 1].end,
                                                      entry->pronunciation});
                        index = end;
                    } else if (index < automatic_replacements.size() &&
                               !automatic_replacements[index].empty()) {
                        protected_rewrites.push_back({normalized_spans[index].begin,
                                                      normalized_spans[index].end,
                                                      automatic_replacements[index]});
                        ++index;
                    } else {
                        ++index;
                    }
                }

                std::string protected_input;
                std::size_t cursor = 0;
                for (const auto& rewrite : protected_rewrites) {
                    protected_input.append(result.normalized_text, cursor, rewrite.begin - cursor);
                    protected_input.append(rewrite.end - rewrite.begin, '\x01');
                    cursor = rewrite.end;
                }
                protected_input.append(
                    result.normalized_text, cursor, result.normalized_text.size() - cursor);

                const auto backend = impl_->backend_for(*bundle_root);
                const auto semantic = backend->process(protected_input);
                auto automatic_words = result.words;
                auto automatic_stress_decisions = result.stress_decisions;
                for (const auto& word : semantic.words) {
                    const auto span_index = std::find_if(
                        normalized_spans.begin(), normalized_spans.end(), [&](const Span span) {
                            return span.begin == word.source_offset;
                        });
                    if (span_index == normalized_spans.end())
                        continue;
                    const auto index =
                        static_cast<std::size_t>(span_index - normalized_spans.begin());
                    if (index >= automatic_words.size() || automatic_words[index].from_dictionary ||
                        automatic_words[index].from_automatic_rewrite)
                        continue;
                    automatic_words[index].pronunciation = word.pronunciation;
                    automatic_words[index].stressed_vowel = word.stressed_vowel;
                    automatic_stress_decisions.push_back({automatic_words[index].surface,
                                                          word.stressed_vowel,
                                                          false,
                                                          "silero " + word.reason});
                }
                auto automatic_pronunciation = semantic.pronunciation_text;
                std::size_t marker_cursor = 0;
                for (const auto& rewrite : protected_rewrites) {
                    const auto marker = std::string(rewrite.end - rewrite.begin, '\x01');
                    const auto marker_position =
                        automatic_pronunciation.find(marker, marker_cursor);
                    if (marker_position == std::string::npos)
                        throw std::runtime_error("Silero backend lost a protected rewrite span");
                    automatic_pronunciation.replace(marker_position, marker.size(), rewrite.output);
                    marker_cursor = marker_position + rewrite.output.size();
                }
                result.words = std::move(automatic_words);
                result.stress_decisions = std::move(automatic_stress_decisions);
                result.pronunciation_text = std::move(automatic_pronunciation);
            } catch (const std::exception& error) {
                result.words = deterministic_words;
                result.stress_decisions = deterministic_stress_decisions;
                result.pronunciation_text = deterministic_pronunciation;
                add_unavailable_warning(std::string("Automatic stress backend unavailable: ") +
                                        error.what());
            }
        }
#else
        add_unavailable_warning(
            "Automatic stress requires a build with TTS_FRONT_ENABLE_ONNX_STRESS=ON");
#endif
    }
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
