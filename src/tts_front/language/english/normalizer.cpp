#include "tts_front/language/english/normalizer.hpp"

#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/english/admission.hpp"
#include "tts_front/language/english/grouped_numbers.hpp"
#include "tts_front/language/english/numbers.hpp"
#include "tts_front/language/english/patterns.hpp"
#include "tts_front/normalization/normalizer_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <regex>
#include <string>
#include <string_view>

namespace tts_front::detail::english {
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

int month_number(std::string_view month) {
    static constexpr std::string_view names[] = {"January",
                                                 "February",
                                                 "March",
                                                 "April",
                                                 "May",
                                                 "June",
                                                 "July",
                                                 "August",
                                                 "September",
                                                 "October",
                                                 "November",
                                                 "December"};
    for (int index = 0; index < 12; ++index)
        if (names[index] == month)
            return index + 1;
    return 0;
}

bool valid_calendar_date(int month, int day, int year) {
    static constexpr int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12 || day < 1)
        return false;
    int limit = days[month];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        limit = 29;
    return day <= limit;
}

void protect_typed_continuations(MappedText& text,
                                 WarningSink& warnings,
                                 std::vector<ProtectedSpan>& protected_spans,
                                 std::vector<SourceEdit>& edits) {
    const Utf8Document document(text.text);
    if (!document.valid)
        return;

    const auto protect_matches = [&](const std::regex& pattern,
                                     const CandidateSurfaceKind kind,
                                     const int core_end_group) {
        for (std::sregex_iterator it(text.text.begin(), text.text.end(), pattern), end; it != end;
             ++it) {
            const auto match_begin = static_cast<std::size_t>(it->position());
            const auto core_end = core_end_group >= 0
                                      ? static_cast<std::size_t>(it->position(core_end_group))
                                      : match_begin + static_cast<std::size_t>(it->length());
            const auto continuation_begin = codepoint_index_at_or_after(document, core_end);
            if (kind == CandidateSurfaceKind::Currency &&
                continuation_begin < document.points.size() && text.text[match_begin] == '$' &&
                tts_front::detail::text::is_horizontal_space(
                    document.points[continuation_begin].value))
                continue;
            if (kind == CandidateSurfaceKind::Currency &&
                continuation_begin < document.points.size() &&
                (document.points[continuation_begin].value == ',' ||
                 document.points[continuation_begin].value == '.' ||
                 document.points[continuation_begin].value == ':'))
                continue;
            const auto continuation_end =
                scan_typed_admission_end(document, continuation_begin, kind);
            if (continuation_end <= continuation_begin)
                continue;
            const auto finish = document.span_from_codepoints(0, continuation_end).byte_end;
            if (overlaps_preserved_range(text, match_begin, finish))
                continue;
            add_protected_candidate(text,
                                    NumericCandidate{SourceSpan{match_begin, finish},
                                                     NumericCandidateKind::Numeric,
                                                     true},
                                    warnings,
                                    protected_spans,
                                    edits);
        }
    };

    // The regexes intentionally recognize only the supported core.  Any
    // connector or lexical tail is admitted by the shared scanner before the
    // formatter gets a chance to rewrite the head.
    protect_matches(patterns().measurement, CandidateSurfaceKind::Unit, 4);
    protect_matches(patterns().currency_decimal, CandidateSurfaceKind::Currency, -1);
    protect_matches(patterns().currency_integer, CandidateSurfaceKind::Currency, -1);
}

} // namespace

MappedText normalize(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    // Admit month-name dates before any numeric or technical shielding.  A
    // date continuation such as `2024#4` is one unsupported candidate and
    // must not be split by the generic scanners.
    if (std::regex_search(text.text, english::patterns().date)) {
        const Utf8Document document(text.text);
        std::vector<SourceEdit> date_edits;
        for (std::sregex_iterator it(text.text.begin(), text.text.end(), english::patterns().date),
             end;
             it != end;
             ++it) {
            long long day = 0, year_value = 0;
            const auto month = month_number((*it)[1].str());
            const bool valid =
                try_parse_long((*it)[2].str(), day) && try_parse_long((*it)[3].str(), year_value) &&
                valid_calendar_date(month, static_cast<int>(day), static_cast<int>(year_value));
            const auto match_begin = static_cast<std::size_t>(it->position());
            const auto match_end = match_begin + static_cast<std::size_t>(it->length());
            const auto continuation_begin = codepoint_index_at_or_after(document, match_end);
            const auto continuation_end =
                scan_typed_admission_end(document, continuation_begin, CandidateSurfaceKind::Date);
            const bool has_continuation = continuation_end != continuation_begin;
            if (valid && !has_continuation)
                continue;
            const auto begin = match_begin;
            const auto finish = continuation_end != continuation_begin
                                    ? document.span_from_codepoints(0, continuation_end).byte_end
                                    : match_end;
            add_protected_candidate(
                text,
                NumericCandidate{SourceSpan{begin, finish}, NumericCandidateKind::Numeric, true},
                warnings,
                protected_spans,
                date_edits);
        }
        text = apply_source_edits(text, std::move(date_edits));
        text = replace_matches(
            text,
            english::patterns().date,
            [&](const std::smatch& match, const MappedText& source) {
                long long day = 0, year_value = 0;
                const auto month = month_number(match[1].str());
                if (!try_parse_long(match[2].str(), day) ||
                    !try_parse_long(match[3].str(), year_value) ||
                    !valid_calendar_date(
                        month, static_cast<int>(day), static_cast<int>(year_value))) {
                    add_warning(warnings,
                                WarningCode::UnresolvedNumber,
                                "Unable to parse English calendar date",
                                source,
                                match);
                    return match.str();
                }
                return match[1].str() + " " + ordinal(day) + ", " + year(year_value);
            });
    }
    std::vector<SourceEdit> typed_edits;
    protect_typed_continuations(text, warnings, protected_spans, typed_edits);
    text = apply_source_edits(text, std::move(typed_edits));
    text = technical::protect_numeric_candidates(std::move(text), warnings, protected_spans);
    text = technical::protect(std::move(text), protected_spans);
    text = protect_malformed_numeric_candidates(
        std::move(text), warnings, protected_spans, admission_rules());
    text = collapse_comma_grouped_numbers(text);
    text = collapse_grouped_numbers(std::move(text));
    text =
        replace_numeric_matches(text,
                                english::patterns().currency_decimal,
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
                                    return number(dollars) +
                                           (dollars == 1 ? " dollar" : " dollars") + " " +
                                           number(cents) + (cents == 1 ? " cent" : " cents");
                                });
    text = replace_numeric_matches(text,
                                   english::patterns().currency_integer,
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
                                       return number(dollars) +
                                              (dollars == 1 ? " dollar" : " dollars");
                                   });
    text = replace_numeric_matches(
        text, english::patterns().ordinal, [&](const std::smatch& match, const MappedText& source) {
            long long value = 0;
            const auto suffix = match[3].str();
            const bool signed_ordinal = !match[2].str().empty() && match[2].str().front() == '-';
            if (signed_ordinal || !try_parse_long(match[2].str(), value) ||
                suffix != ordinal_suffix(value)) {
                add_warning_span(warnings,
                                 WarningCode::UnresolvedNumber,
                                 "Unable to parse English ordinal",
                                 source,
                                 match,
                                 2,
                                 3);
                return match.str();
            }
            return match[1].str() + ordinal(value);
        });
    text = replace_numeric_matches(
        text, english::patterns().percent, [&](const std::smatch& match, const MappedText& source) {
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
            return (dot == std::string::npos ? number_for_token(value, integer)
                                             : number_for_token(value.substr(0, dot), integer) +
                                                   " point " + digits(value.substr(dot + 1))) +
                   " percent";
        });
    text = replace_numeric_matches(
        text, english::patterns().time, [&](const std::smatch& match, const MappedText& source) {
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
            return number(h) + (h == 1 ? " hour " : " hours ") + number(m) +
                   (m == 1 ? " minute" : " minutes");
        });
    text = replace_matches(text,
                           english::patterns().measurement,
                           [&](const std::smatch& match, const MappedText& source) {
                               long long n = 0;
                               if (!try_parse_long(match[1].str(), n)) {
                                   add_warning_without_suffix(warnings,
                                                              WarningCode::UnresolvedNumber,
                                                              "Unable to parse English measurement",
                                                              source,
                                                              match,
                                                              4);
                                   return match.str();
                               }
                               const auto fraction = match[2].str();
                               const auto unit = match[3].str();
                               const bool singular = n == 1 || n == -1;
                               const auto unit_name = [&](bool singular_form) {
                                   return unit == "kg" || unit.find("kilogram") == 0
                                              ? (singular_form ? "kilogram" : "kilograms")
                                          : unit == "km" || unit.find("kilomet") == 0
                                              ? (singular_form ? "kilometer" : "kilometers")
                                          : unit == "m" || unit == "meter" || unit == "meters" ||
                                                  unit == "metres"
                                              ? (singular_form ? "meter" : "meters")
                                          : unit == "cm" || unit.find("centimet") == 0
                                              ? (singular_form ? "centimeter" : "centimeters")
                                          : unit == "mm" || unit.find("millimet") == 0
                                              ? (singular_form ? "millimeter" : "millimeters")
                                          : unit == "MB"
                                              ? (singular_form ? "megabyte" : "megabytes")
                                              : (singular_form ? "gigabyte" : "gigabytes");
                               };
                               const auto spoken_unit = unit_name(fraction.empty() && singular);
                               auto spoken = number_for_token(match[1].str(), n);
                               if (!fraction.empty())
                                   spoken += " point " + digits(fraction);
                               return spoken + " " + spoken_unit + match[4].str();
                           });
    text = replace_numeric_matches(
        text, english::patterns().decimal, [&](const std::smatch& match, const MappedText& source) {
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
            return match[1].str() + number_for_token(value.substr(0, dot), integer) + " point " +
                   digits(value.substr(dot + 1));
        });
    text = replace_numeric_matches(
        text,
        english::patterns().generic_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            return match[1].str() +
                   number_or_original(
                       match[2].str(), warnings, source.source_range(number_begin, number_end));
        });
    return technical::restore(std::move(text), protected_spans);
}

} // namespace tts_front::detail::english
