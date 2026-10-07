#include "tts_front/language/english/normalizer.hpp"

#include "tts_front/language/english/numbers.hpp"
#include "tts_front/language/english/patterns.hpp"
#include "tts_front/normalization/normalizer_support.hpp"
#include "tts_front/normalization/patterns.hpp"

#include <string>

namespace tts_front::detail::english {
namespace {

std::string en_number(long long n) {
    return number(n);
}
const char* en_ordinal_suffix(long long n) {
    return ordinal_suffix(n);
}
std::string en_ordinal(long long n) {
    return ordinal(n);
}
std::string en_digits(const std::string& value) {
    return digits(value);
}

std::string
number_or_original(const std::string& token, WarningSink& warnings, SourceRange source) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        warnings.add(WarningCode::UnresolvedNumber, "Unable to parse number", source);
        return token;
    }
    return en_number(value);
}

} // namespace

MappedText normalize(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = protect_numeric_technical_candidates(std::move(text), warnings, protected_spans);
    text = protect_technical(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(
        std::move(text), warnings, protected_spans, AdmissionLanguage::English);
    text = collapse_english_comma_grouped_numbers(std::move(text));
    text = collapse_grouped_numbers(std::move(text));
    text =
        replace_numeric_matches(text,
                                english_patterns().en_currency_decimal,
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
                                   english_patterns().en_currency_integer,
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
    text = replace_numeric_matches(text,
                                   english_patterns().en_ordinal,
                                   [&](const std::smatch& match, const MappedText& source) {
                                       long long value = 0;
                                       const auto suffix = match[3].str();
                                       if (!try_parse_long(match[2].str(), value) ||
                                           suffix != en_ordinal_suffix(value)) {
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
        text,
        english_patterns().en_percent,
        [&](const std::smatch& match, const MappedText& source) {
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
        text, english_patterns().en_time, [&](const std::smatch& match, const MappedText& source) {
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
    text = replace_numeric_matches(text,
                                   english_patterns().en_decimal,
                                   [&](const std::smatch& match, const MappedText& source) {
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
    text = replace_matches(text,
                           english_patterns().en_measurement,
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
                               const bool singular = n == 1 || n == -1;
                               const std::string spoken =
                                   unit == "kg" || unit.find("kilogram") == 0
                                       ? (singular ? "kilogram" : "kilograms")
                                   : unit == "km" || unit.find("kilomet") == 0
                                       ? (singular ? "kilometer" : "kilometers")
                                   : unit == "m" || unit == "meter" || unit == "meters" ||
                                           unit == "metres"
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
        english_patterns().generic_en_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(
                       match[2].str(), warnings, source.source_range(number_begin, number_end));
        });
    return restore_technical(std::move(text), protected_spans);
}

} // namespace tts_front::detail::english
