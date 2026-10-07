#include "tts_front/language/russian/normalizer.hpp"

#include "tts_front/core/utf8.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/normalization/normalizer_support.hpp"
#include "tts_front/normalization/patterns.hpp"

#include <string>

namespace tts_front::detail::russian {
namespace {

std::string
number_or_original(const std::string& token, WarningSink& warnings, SourceRange source) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        warnings.add(WarningCode::UnresolvedNumber, "Unable to parse number", source);
        return token;
    }
    return ru_number(value);
}

} // namespace

MappedText normalize(MappedText text, WarningSink& warnings) {
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
                   number_or_original(
                       match[2].str(), warnings, source.source_range(number_begin, number_end));
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

} // namespace tts_front::detail::russian
