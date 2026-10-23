#include "tts_front/language/russian/normalizer.hpp"

#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/russian/admission.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/normalization/normalizer_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <string>
#include <string_view>

namespace tts_front::detail::russian {
namespace {

std::uint32_t previous_codepoint(const std::string_view value, const std::size_t boundary) {
    if (boundary == 0)
        return 0;
    auto begin = boundary - 1;
    while (begin > 0 && (static_cast<unsigned char>(value[begin]) & 0xc0) == 0x80)
        --begin;
    const auto length = utf8_sequence_length(static_cast<unsigned char>(value[begin]));
    if (length == 0 || begin + length > boundary)
        return 0;
    auto codepoint = static_cast<std::uint32_t>(static_cast<unsigned char>(value[begin]) &
                                                (length == 2   ? 0x1f
                                                 : length == 3 ? 0x0f
                                                 : length == 4 ? 0x07
                                                               : 0x7f));
    for (std::size_t offset = 1; offset < length; ++offset)
        codepoint = (codepoint << 6) | (static_cast<unsigned char>(value[begin + offset]) & 0x3f);
    return codepoint;
}

bool has_context_boundary(const std::string_view value, const std::size_t begin) {
    const auto previous = previous_codepoint(value, begin);
    return !text::is_letter(previous) && !text::is_digit(previous) && previous != '_' &&
           !text::is_combining_mark(previous);
}

bool has_genitive_prefix(const MappedText& source, const std::size_t position) {
    const auto& text = source.text;
    auto end = position;
    while (end > 0 && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    const auto has_suffix = [&](const std::string_view suffix) {
        if (end < suffix.size() || text.compare(end - suffix.size(), suffix.size(), suffix) != 0)
            return false;
        auto boundary = end - suffix.size();
        if (boundary > 0 && text[boundary - 1] == '\x02') {
            const auto marker =
                std::find_if(source.runs.begin(), source.runs.end(), [&](const MappedRun& run) {
                    return run.output_end == boundary;
                });
            if (marker != source.runs.end() && !marker->direct_copy && marker->source.length != 0)
                return false;
        }
        auto previous = previous_codepoint(text, boundary);
        while (text::is_combining_mark(previous) && boundary > 0) {
            auto begin = boundary - 1;
            while (begin > 0 && (static_cast<unsigned char>(text[begin]) & 0xc0) == 0x80)
                --begin;
            boundary = begin;
            previous = previous_codepoint(text, boundary);
        }
        return !text::is_letter(previous) && !text::is_digit(previous) && previous != '_';
    };
    return has_suffix("после") || has_suffix("После") || has_suffix("из") || has_suffix("Из");
}

std::size_t unsupported_genitive_end(const MappedText& source,
                                     const Utf8Document& document,
                                     const std::size_t begin) {
    const auto point = codepoint_index_at_or_after(document, begin);
    if (!document.valid || point >= document.points.size())
        return 0;
    const auto candidate = scan_candidate_surface(document, point, CandidateSurfaceKind::Numeric);
    if (!candidate.valid())
        return 0;

    const auto combining = scan_combining_suffix(document, point);
    if (combining)
        return combining->offset + combining->length;

    const auto special =
        russian::classify_genitive_surface(source.text, begin, candidate.has_combining);
    const auto special_end = special.end;
    auto end = candidate.end;
    if (special_end > 0)
        end = std::max(end, codepoint_index_at_or_after(document, special_end));
    auto admission = candidate;
    admission.end = end;
    const auto full_end = scan_candidate_admission_end(document, admission, special.kind);

    bool has_cyrillic_suffix = false;
    bool has_foreign_suffix = false;
    for (auto suffix = candidate.numeric.end; suffix < candidate.end; ++suffix) {
        const auto value = document.points[suffix].value;
        if (text::is_horizontal_space(value) || text::is_combining_mark(value))
            continue;
        if (text::is_cyrillic(value))
            has_cyrillic_suffix = true;
        else
            has_foreign_suffix = true;
    }
    if (!candidate.numeric.malformed_combining && special_end == 0 && full_end == candidate.end &&
        has_cyrillic_suffix && !has_foreign_suffix)
        return 0;

    if (special_end == 0 && full_end == candidate.numeric.end &&
        candidate.end == candidate.numeric.end)
        return 0;
    return full_end > point ? document.span_from_codepoints(point, full_end).byte_end : 0;
}

MappedText protect_unsupported_genitive_candidates(MappedText text, WarningSink& warnings) {
    const Utf8Document document(text.text);
    if (!document.valid)
        return text;
    for (std::sregex_iterator
             it(text.text.begin(), text.text.end(), russian::patterns().genitive_context),
         end;
         it != end;
         ++it) {
        const auto number_begin = static_cast<std::size_t>(it->position(3));
        if (!has_genitive_prefix(text, number_begin))
            continue;
        const auto unsupported_end = unsupported_genitive_end(text, document, number_begin);
        if (unsupported_end > number_begin &&
            !overlaps_preserved_range(text, number_begin, unsupported_end))
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unable to inflect a compound numeric surface",
                         text.source_range(number_begin, unsupported_end));
    }
    return text;
}

bool genitive_requires_noun_gender(const long long value) {
    const auto absolute = std::llabs(value);
    return absolute == 1 || (absolute % 10 == 1 && absolute % 100 != 11);
}

std::string
number_or_original(const std::string& token, WarningSink& warnings, SourceRange source) {
    long long value = 0;
    if (!try_parse_long(token, value)) {
        warnings.add(WarningCode::UnresolvedNumber, "Unable to parse number", source);
        return token;
    }
    return number_for_token(token, value);
}

bool try_parse_context_year(const std::string& token, long long& year) {
    return token.size() == 4 &&
           std::all_of(token.begin(),
                       token.end(),
                       [](const unsigned char byte) { return byte >= '0' && byte <= '9'; }) &&
           try_parse_long(token, year) && year >= 1000 && year <= 9999;
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
                                     const int admission_begin_group = 0,
                                     const bool require_context_boundary = false,
                                     const bool preserve_core = false) {
        for (std::sregex_iterator it(text.text.begin(), text.text.end(), pattern), end; it != end;
             ++it) {
            const auto match_begin = static_cast<std::size_t>(it->position());
            if (require_context_boundary && !has_context_boundary(text.text, match_begin))
                continue;
            const auto admission_begin = admission_begin_group > 0
                                             ? static_cast<std::size_t>(it->position(
                                                   static_cast<std::size_t>(admission_begin_group)))
                                             : match_begin;
            const auto core_end = match_begin + static_cast<std::size_t>(it->length());
            const auto continuation_begin = codepoint_index_at_or_after(document, core_end);
            const auto continuation_end =
                scan_typed_admission_end(document, continuation_begin, kind);
            if (!preserve_core && continuation_end <= continuation_begin)
                continue;
            const auto finish_point = std::max(continuation_begin, continuation_end);
            const auto finish = document.span_from_codepoints(0, finish_point).byte_end;
            if (overlaps_preserved_range(text, admission_begin, finish))
                continue;
            add_protected_candidate(text,
                                    NumericCandidate{SourceSpan{admission_begin, finish},
                                                     NumericCandidateKind::Numeric,
                                                     true},
                                    warnings,
                                    protected_spans,
                                    edits);
        }
    };

    protect_matches(patterns().admission_measurement, CandidateSurfaceKind::Unit);
    protect_matches(patterns().admission_currency, CandidateSurfaceKind::Currency);
    protect_matches(patterns().year_locative_context, CandidateSurfaceKind::Contextual, 3, true);
    protect_matches(patterns().year_accusative_context, CandidateSurfaceKind::Contextual, 3, true);

    // Attached nouns and year ranges are intentionally unsupported. Preserve
    // each complete candidate before the ordinary number pass can rewrite a
    // fragment; the shared scanner still owns any continuation after the core.
    protect_matches(
        patterns().year_locative_attached, CandidateSurfaceKind::Contextual, 3, true, true);
    protect_matches(
        patterns().year_accusative_attached, CandidateSurfaceKind::Contextual, 3, true, true);
    protect_matches(patterns().year_range_context, CandidateSurfaceKind::Contextual, 3, true, true);
}

} // namespace

MappedText normalize(MappedText text, WarningSink& warnings) {
    std::vector<ProtectedSpan> protected_spans;
    std::vector<SourceEdit> typed_edits;
    protect_typed_continuations(text, warnings, protected_spans, typed_edits);
    text = apply_source_edits(text, std::move(typed_edits));
    text = protect_combining_suffix_candidates(std::move(text), warnings, protected_spans);
    text = protect_unsupported_genitive_candidates(std::move(text), warnings);
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
        russian::patterns().year_locative_context,
        [&](const std::smatch& match, const MappedText& source) {
            if (!has_context_boundary(source.text, static_cast<std::size_t>(match.position())))
                return match.str();
            long long year = 0;
            if (!try_parse_context_year(match[3].str(), year)) {
                const auto begin = static_cast<std::size_t>(match.position(3));
                const auto end = static_cast<std::size_t>(match.position() + match.length());
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to parse Russian locative year",
                             source.source_range(begin, end));
                return match.str();
            }
            return match[1].str() + match[2].str() + year_locative(static_cast<int>(year)) +
                   match[4].str();
        });
    text = replace_matches(
        text,
        russian::patterns().year_accusative_context,
        [&](const std::smatch& match, const MappedText& source) {
            if (!has_context_boundary(source.text, static_cast<std::size_t>(match.position())))
                return match.str();
            long long year = 0;
            if (!try_parse_context_year(match[3].str(), year)) {
                const auto begin = static_cast<std::size_t>(match.position(3));
                const auto end = static_cast<std::size_t>(match.position() + match.length());
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to parse Russian accusative year",
                             source.source_range(begin, end));
                return match.str();
            }
            return match[1].str() + match[2].str() + year_accusative(static_cast<int>(year)) +
                   match[4].str();
        });
    const Utf8Document genitive_document(text.text);
    text = replace_matches(
        text,
        russian::patterns().genitive_context,
        [&](const std::smatch& match, const MappedText& source) {
            const auto token = match[3].str();
            const auto number_begin = static_cast<std::size_t>(match.position(3));
            if (!has_genitive_prefix(source, number_begin))
                return match.str();
            const auto unsupported_end =
                unsupported_genitive_end(source, genitive_document, number_begin);
            if (unsupported_end > number_begin) {
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to inflect a compound numeric surface",
                             source.source_range(number_begin, unsupported_end));
                return match.str();
            }
            if (token.find('.') != std::string::npos || token.find(',') != std::string::npos) {
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to inflect a genitive decimal",
                             source.source_range(number_begin, number_begin + match.length(3)));
                return match.str();
            }
            long long value = 0;
            if (!try_parse_long(token, value))
                return match.str();
            // Agreement of compound genitives depends on the governed noun
            // ("двадцати одного узла", "двадцати одной попытки", ...).
            // Values ending in one require noun gender ("одного" versus
            // "одной"), except for the fixed teens form "одиннадцати".
            // Preserve the genuinely ambiguous forms until a noun-aware
            // inflector exists; the remaining compound genitives are
            // gender-neutral enough for the supported plural contexts.
            const auto absolute = std::llabs(value);
            if (genitive_requires_noun_gender(value) || absolute > 999) {
                const auto begin = static_cast<std::size_t>(match.position(3));
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to inflect compound genitive",
                             source.source_range(begin, begin + match.length(3)));
                return match.str();
            }
            return match[1].str() + match[2].str() + genitive_number(value);
        });
    text = replace_matches(
        text,
        russian::patterns().decimal_percent,
        [&](const std::smatch& match, const MappedText& source) {
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position())))
                return match.str();
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
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position())))
                return match.str();
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
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position())))
                return match.str();
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
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position())))
                return match.str();
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
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position()))) {
                add_warning(warnings,
                            WarningCode::UnresolvedNumber,
                            "Unable to inflect time in a genitive context",
                            source,
                            match);
                return match.str();
            }
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
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position())))
                return match.str();
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
                : !fraction.empty() && unit_source == "м" ? "метра"
                : !fraction.empty() && (unit_source == "ГБ" || unit_source == "GB") ? "гигабайта"
                : !fraction.empty() ? "мегабайта"
                : unit_source.find("кг") == 0 || unit_source.find("килограмм") == 0
                    ? plural_form(n, "килограмм", "килограмма", "килограммов")
                : unit_source.find("км") == 0 || unit_source.find("километр") == 0
                    ? plural_form(n, "километр", "километра", "километров")
                : unit_source.find("см") == 0 || unit_source.find("сантиметр") == 0
                    ? plural_form(n, "сантиметр", "сантиметра", "сантиметров")
                : unit_source.find("мм") == 0 || unit_source.find("миллиметр") == 0
                    ? plural_form(n, "миллиметр", "миллиметра", "миллиметров")
                : unit_source == "м" ? plural_form(n, "метр", "метра", "метров")
                : (unit_source == "ГБ" || unit_source == "GB")
                    ? plural_form(n, "гигабайт", "гигабайта", "гигабайт")
                    : plural_form(n, "мегабайт", "мегабайта", "мегабайт");
            if (!fraction.empty())
                return decimal(n, fraction) + " " + unit + match[4].str();
            return number_for_token(match[1].str(), n) + " " + unit + match[4].str();
        });
    text = replace_matches(
        text,
        russian::patterns().genitive_decimal_context,
        [&](const std::smatch& match, const MappedText& source) {
            long long integer = 0;
            if (!try_parse_long(match[2].str(), integer) || match[3].str().size() > 3) {
                return match.str();
            }
            const auto begin = static_cast<std::size_t>(match.position(2));
            const auto end = begin + static_cast<std::size_t>(match.length(2)) +
                             static_cast<std::size_t>(match.length(3)) + 1;
            auto after = static_cast<std::size_t>(match.position(4)) +
                         static_cast<std::size_t>(match.length(4));
            while (after < source.text.size() &&
                   (source.text[after] == ' ' || source.text[after] == '\t'))
                ++after;
            const Utf8Document source_document(source.text);
            const auto after_point = codepoint_index_at_or_after(source_document, after);
            const bool noun_follows =
                source_document.valid && after_point < source_document.points.size() &&
                tts_front::detail::text::is_letter(source_document.points[after_point].value);
            if (noun_follows) {
                warnings.add(WarningCode::UnresolvedNumber,
                             "Unable to inflect a genitive decimal",
                             source.source_range(begin, end));
                return match.str();
            }
            return match[1].str() + decimal_genitive(integer, match[3].str()) + match[4].str();
        });
    text = replace_numeric_matches(
        text, russian::patterns().decimal, [&](const std::smatch& match, const MappedText& source) {
            if (has_genitive_prefix(source, static_cast<std::size_t>(match.position())))
                return match.str();
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
    const Utf8Document generic_document(text.text);
    text = replace_numeric_matches(
        text,
        russian::patterns().generic_number,
        [&](const std::smatch& match, const MappedText& source) {
            const auto number_begin = static_cast<std::size_t>(match.position(2));
            const auto number_end = number_begin + static_cast<std::size_t>(match.length(2));
            long long parsed = 0;
            if (has_genitive_prefix(source, number_begin) &&
                try_parse_long(match[2].str(), parsed) &&
                (genitive_requires_noun_gender(parsed) ||
                 unsupported_genitive_end(source, generic_document, number_begin) > number_begin)) {
                return match[1].str() + match[2].str();
            }
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
