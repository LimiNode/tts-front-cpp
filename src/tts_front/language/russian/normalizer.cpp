#include "tts_front/language/russian/normalizer.hpp"

#include "tts_front/core/utf8.hpp"
#include "tts_front/language/russian/admission.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/normalization/normalizer_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <cctype>
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
    text = replace_matches(
        text,
        russian::patterns().foreign_decimal_percent,
        [&](const std::smatch& match, const MappedText& source) {
            const auto begin = static_cast<std::size_t>(match.position());
            const bool preceded_by_unicode_minus =
                begin >= 3 && static_cast<unsigned char>(source.text[begin - 3]) == 0xE2 &&
                static_cast<unsigned char>(source.text[begin - 2]) == 0x88 &&
                static_cast<unsigned char>(source.text[begin - 1]) == 0x92;
            if (begin != 0 && (source.text[begin - 1] == '+' || source.text[begin - 1] == '-' ||
                               preceded_by_unicode_minus))
                return match.str();
            add_warning(warnings,
                        WarningCode::UnresolvedNumber,
                        "Unsupported dot-decimal Russian percentage",
                        source,
                        match);
            return match.str();
        });
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
            const auto suffix = match[4].str();
            const auto after = static_cast<std::size_t>(match.position()) +
                               static_cast<std::size_t>(match.length());
            const auto punctuation =
                suffix == "." && after < source.text.size() &&
                        (source.text[after] == ' ' || source.text[after] == '\t' ||
                         std::ispunct(static_cast<unsigned char>(source.text[after])))
                    ? std::string()
                    : suffix;
            return std::string(prefix) + decimal(integer, match[2].str()) + " рубля" + punctuation;
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
            const auto suffix = match[3].str();
            const auto after = static_cast<std::size_t>(match.position()) +
                               static_cast<std::size_t>(match.length());
            const auto punctuation =
                suffix == "." && after < source.text.size() &&
                        (source.text[after] == ' ' || source.text[after] == '\t' ||
                         std::ispunct(static_cast<unsigned char>(source.text[after])))
                    ? std::string()
                    : suffix;
            return number_for_token(match[1].str(), n) + " " +
                   plural_form(n, "рубль", "рубля", "рублей") + punctuation;
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
    text = replace_matches(
        text,
        russian::patterns().measurement,
        [&](const std::smatch& match, const MappedText& source) {
            long long n = 0;
            const auto fraction = match[2].str();
            if (!try_parse_long(match[1].str(), n) || fraction.size() > 3) {
                add_warning_without_suffix(warnings,
                                           WarningCode::UnresolvedNumber,
                                           "Unable to parse Russian measurement",
                                           source,
                                           match,
                                           4);
                return match.str();
            }
            const auto unit_source = match[3].str();
            const std::string unit =
                !fraction.empty() &&
                        (unit_source.find("кг") == 0 || unit_source.find("килограмм") == 0)
                    ? "килограмма"
                : !fraction.empty() &&
                        (unit_source.find("км") == 0 || unit_source.find("километр") == 0)
                    ? "километра"
                : !fraction.empty() &&
                        (unit_source.find("см") == 0 || unit_source.find("сантиметр") == 0)
                    ? "сантиметра"
                : !fraction.empty() &&
                        (unit_source.find("мм") == 0 || unit_source.find("миллиметр") == 0)
                    ? "миллиметра"
                : !fraction.empty() && unit_source == "м"  ? "метра"
                : !fraction.empty() && unit_source == "ГБ" ? "гигабайта"
                : !fraction.empty()                        ? "мегабайта"
                : unit_source.find("кг") == 0 || unit_source.find("килограмм") == 0
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
            if (!fraction.empty())
                return decimal(n, fraction) + " " + unit + match[4].str();
            return number_for_token(match[1].str(), n) + " " + unit + match[4].str();
        });
    text = replace_matches(
        text,
        russian::patterns().genitive_decimal_context,
        [&](const std::smatch& match, const MappedText&) {
            long long integer = 0;
            if (!try_parse_long(match[2].str(), integer) || match[3].str().size() > 3)
                return match.str();
            return match[1].str() + decimal_genitive(integer, match[3].str()) + match[4].str();
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
            const auto before = source.text.substr(0, static_cast<std::size_t>(match.position()));
            if (before.size() >= 3 && before.compare(before.size() - 3, 3, "до ") == 0)
                return decimal_genitive(integer, match[2].str()) + match[3].str();
            return std::string(prefix) + decimal(integer, match[2].str()) + match[3].str();
        });
    text = replace_matches(
        text, russian::patterns().feminine_count, [&](const std::smatch& match, const MappedText&) {
            long long value = 0;
            if (!try_parse_long(match[2].str(), value))
                return match.str();
            return match[1].str() + feminine_number(value) + " " +
                   plural_form(value, "ошибка", "ошибки", "ошибок") + match[4].str();
        });
    text = replace_matches(
        text,
        russian::patterns().genitive_context,
        [&](const std::smatch& match, const MappedText& source) {
            const auto token = match[3].str();
            if (token.find('.') != std::string::npos || token.find(',') != std::string::npos) {
                const auto begin = static_cast<std::size_t>(match.position(3));
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to inflect a genitive decimal",
                             source.source_range(begin, begin + match.length(3)));
                return match.str();
            }
            long long value = 0;
            if (!try_parse_long(token, value))
                return match.str();
            return match[1].str() + match[2].str() + genitive_number(value);
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
