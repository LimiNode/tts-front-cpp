#include "tts_front/language/russian/normalizer.hpp"

#include "tts_front/core/utf8.hpp"
#include "tts_front/language/russian/admission.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/normalization/normalizer_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <string>
#include <string_view>

namespace tts_front::detail::russian {
namespace {

std::string
number_or_original(const std::string& token, WarningSink& warnings, SourceRange source) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        warnings.add(WarningCode::UnresolvedNumber, "Unable to parse number", source);
        return token;
    }
    return number_for_token(token, value);
}

} // namespace

MappedText normalize(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    text = technical::protect_numeric_candidates(std::move(text), warnings, protected_spans);
    text = technical::protect(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(
        std::move(text), warnings, protected_spans, admission_rules());
    text = collapse_grouped_numbers(std::move(text));
    text = replace_numeric_matches(
        text, russian::patterns().date, [&](const std::smatch& match, const MappedText& source) {
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
            return ordinal_day(static_cast<int>(day)) + " " + months[month] + " " +
                   year_genitive(static_cast<int>(year)) + " года";
        });
    text = replace_numeric_matches(
        text, russian::patterns().year, [&](const std::smatch& match, const MappedText& source) {
            long long year = 0;
            if (!try_parse_long(match[1].str(), year)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian year",
                            source,
                            match);
                return match.str();
            }
            return year_locative(static_cast<int>(year)) + " году";
        });
    text = replace_matches(
        text,
        russian::patterns().decimal_percent,
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
            const auto prefix = integer == 0 && match[1].str().front() == '-' ? "минус " : "";
            return std::string(prefix) + decimal(integer, match[2].str()) + " процента" +
                   match[3].str();
        });
    text = replace_matches(
        text,
        russian::patterns().currency_decimal,
        [&](const std::smatch& match, const MappedText& source) {
            long long integer = 0, fraction = 0;
            if (!try_parse_long(match[1].str(), integer) ||
                !try_parse_long(match[2].str(), fraction) || match[2].str().size() > 3) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian decimal currency",
                                           source,
                                           match,
                                           4);
                return match.str();
            }
            const auto prefix = integer == 0 && match[1].str().front() == '-' ? "минус " : "";
            return std::string(prefix) + decimal(integer, match[2].str()) + " рубля" +
                   match[4].str();
        });
    text = replace_numeric_matches(
        text, russian::patterns().percent, [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            if (!try_parse_long(match[1].str(), n)) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to parse Russian percent",
                            source,
                            match);
                return match.str();
            }
            return number_for_token(match[1].str(), n) + " " +
                   plural_form(n, "процент", "процента", "процентов");
        });
    text = replace_numeric_matches(
        text,
        russian::patterns().currency,
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
            return number_for_token(match[1].str(), n) + " " +
                   plural_form(n, "рубль", "рубля", "рублей") + match[3].str();
        });
    text = replace_numeric_matches(
        text, russian::patterns().time, [&](const std::smatch& match, const MappedText& source) {
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
            return number(h) + " " + plural_form(h, "час", "часа", "часов") + " " +
                   feminine_number(m) + " " + plural_form(m, "минута", "минуты", "минут");
        });
    text = replace_numeric_matches(
        text, russian::patterns().decimal, [&](const std::smatch& match, const MappedText& source) {
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
            const auto prefix = integer == 0 && match[1].str().front() == '-' ? "минус " : "";
            return std::string(prefix) + decimal(integer, match[2].str()) + match[3].str();
        });
    text = replace_matches(
        text,
        russian::patterns().measurement,
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
                    ? plural_form(n, "килограмм", "килограмма", "килограммов")
                : unit_source.find("км") == 0 || unit_source.find("километр") == 0
                    ? plural_form(n, "километр", "километра", "километров")
                : unit_source.find("см") == 0 || unit_source.find("сантиметр") == 0
                    ? plural_form(n, "сантиметр", "сантиметра", "сантиметров")
                : unit_source.find("мм") == 0 || unit_source.find("миллиметр") == 0
                    ? plural_form(n, "миллиметр", "миллиметра", "миллиметров")
                : unit_source == "м" ? plural_form(n, "метр", "метра", "метров")
                : unit_source == "ГБ" ? plural_form(n, "гигабайт", "гигабайта", "гигабайт")
                                      : plural_form(n, "мегабайт", "мегабайта", "мегабайт");
            return number_for_token(match[1].str(), n) + " " + unit + match[3].str();
        });
    text = replace_numeric_matches(
        text,
        russian::patterns().generic_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(
                       match[2].str(), warnings, source.source_range(number_begin, number_end));
        });
    text = replace_matches(
        std::move(text),
        russian::patterns().abbreviation_td,
        [](const auto& match, const MappedText&) { return match[1].str() + "так далее"; });
    text = replace_matches(
        std::move(text),
        russian::patterns().abbreviation_tp,
        [](const auto& match, const MappedText&) { return match[1].str() + "тому подобное"; });
    return technical::restore(std::move(text), protected_spans);
}

} // namespace tts_front::detail::russian
