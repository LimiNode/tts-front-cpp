#include "tts_front/language/russian/mixed_candidates.hpp"

#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/russian/admission.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/language/russian/patterns.hpp"
#include "tts_front/normalization/admission.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail::russian {
namespace {

const std::regex& candidate_pattern() {
    static const std::regex pattern{
        R"((^|[ \t\r\n(])(-?[0-9]+(?:,[0-9]+)?[ \t]*(?:рублей|рубля|рубль|руб\.?|километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|GB|MB|м))(?=[^А-Яа-яЁёA-Za-z0-9_]|$))"};
    return pattern;
}

const std::regex& malformed_pattern() {
    static const std::regex pattern{
        R"((?:^|[^0-9.,])(-?[0-9]+(?:,[0-9]+)?[ \t]*(?:рублей|рубля|рубль|руб\.?(?![А-Яа-яЁё])|километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|GB|MB|м(?![А-Яа-яЁёA-Za-z]))[A-Za-zА-Яа-яЁё]*))"};
    return pattern;
}

const std::regex& measurement_pattern() {
    static const std::regex pattern{
        R"((-?[0-9]+)(?:,([0-9]+))?[ \t]*(рублей|рубля|рубль|руб\.?|километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|GB|MB|м))"};
    return pattern;
}

std::optional<std::string> format(std::string_view candidate) {
    std::smatch match;
    const std::string value(candidate);
    if (!std::regex_match(value, match, measurement_pattern()))
        return std::nullopt;
    long long integer = 0;
    if (!try_parse_long(match[1].str(), integer))
        return std::nullopt;
    const auto number_text = match[1].str();
    const auto fraction = match[2].str();
    const auto unit = match[3].str();
    const auto signed_zero_prefix =
        integer == 0 && !number_text.empty() && number_text.front() == '-' ? std::string("минус ")
                                                                           : std::string();
    if (!fraction.empty()) {
        if (fraction.size() > 2)
            return std::nullopt;
        if (unit.find("руб") == 0 || unit == "руб.")
            return monetary_decimal(number_text, integer, fraction);
        const auto spoken_unit =
            unit.find("кг") == 0 || unit.find("килограмм") == 0   ? "килограмма"
            : unit.find("км") == 0 || unit.find("километр") == 0  ? "километра"
            : unit.find("см") == 0 || unit.find("сантиметр") == 0 ? "сантиметра"
            : unit.find("мм") == 0 || unit.find("миллиметр") == 0 ? "миллиметра"
            : unit == "м"                                         ? "метра"
            : unit == "ГБ" || unit == "GB"                        ? "гигабайта"
                                                                  : "мегабайта";
        return signed_zero_prefix + decimal(integer, fraction) + " " + spoken_unit;
    }
    if (unit.find("руб") == 0 || unit == "руб.")
        return number_for_token(number_text, integer) + " " +
               plural_form(integer, "рубль", "рубля", "рублей");
    const bool kilogram = unit.find("кг") == 0 || unit.find("килограмм") == 0;
    const bool kilometer = unit.find("км") == 0 || unit.find("километр") == 0;
    const bool centimeter = unit.find("см") == 0 || unit.find("сантиметр") == 0;
    const bool millimeter = unit.find("мм") == 0 || unit.find("миллиметр") == 0;
    const char* one = kilogram                       ? "килограмм"
                      : kilometer                    ? "километр"
                      : centimeter                   ? "сантиметр"
                      : millimeter                   ? "миллиметр"
                      : unit == "м"                  ? "метр"
                      : unit == "ГБ" || unit == "GB" ? "гигабайт"
                                                     : "мегабайт";
    const char* few = kilogram                       ? "килограмма"
                      : kilometer                    ? "километра"
                      : centimeter                   ? "сантиметра"
                      : millimeter                   ? "миллиметра"
                      : unit == "м"                  ? "метра"
                      : unit == "ГБ" || unit == "GB" ? "гигабайта"
                                                     : "мегабайта";
    const char* many = kilogram                       ? "килограммов"
                       : kilometer                    ? "километров"
                       : centimeter                   ? "сантиметров"
                       : millimeter                   ? "миллиметров"
                       : unit == "м"                  ? "метров"
                       : unit == "ГБ" || unit == "GB" ? "гигабайт"
                                                      : "мегабайт";
    return number_for_token(number_text, integer) + " " + plural_form(integer, one, few, many);
}

std::optional<std::string> format_number(std::string_view candidate) {
    long long value = 0;
    if (!try_parse_long(candidate, value))
        return std::nullopt;
    return number_for_token(candidate, value);
}

std::vector<MixedCandidateMatch> scan_candidate_matches(std::string_view text) {
    std::vector<MixedCandidateMatch> matches;
    using Iterator = std::regex_iterator<std::string_view::const_iterator>;
    for (Iterator it(text.begin(), text.end(), candidate_pattern()), end; it != end; ++it) {
        const auto begin = static_cast<std::size_t>(it->position(2));
        matches.push_back({begin,
                           begin + static_cast<std::size_t>(it->length(2)),
                           text.substr(begin, static_cast<std::size_t>(it->length(2)))});
    }
    return matches;
}

std::vector<MixedCandidateMatch> scan_malformed_matches(std::string_view text) {
    std::vector<MixedCandidateMatch> matches;
    const Utf8Document document(text);
    using Iterator = std::regex_iterator<std::string_view::const_iterator>;
    for (Iterator it(text.begin(), text.end(), malformed_pattern()), end; it != end; ++it) {
        const auto begin = static_cast<std::size_t>(it->position(1));
        auto match_end = begin + static_cast<std::size_t>(it->length(1));
        if (document.valid) {
            const auto point = codepoint_index_at_or_after(document, begin);
            const auto surface =
                scan_candidate_surface(document, point, CandidateSurfaceKind::Contextual);
            if (surface.valid()) {
                const auto classified_end =
                    classify_genitive_surface(text, begin, surface.has_combining).end;
                const auto continuation_begin = codepoint_index_at_or_after(document, match_end);
                const auto continuation_end =
                    scan_admission(document, continuation_begin, CandidateSurfaceKind::Contextual)
                        .end;
                if (continuation_end > continuation_begin)
                    match_end = std::max(match_end, classified_end);
            }
        }
        matches.push_back({begin, match_end, text.substr(begin, match_end - begin)});
    }
    for (const auto& range : scan_unsupported_currency(document)) {
        matches.push_back(
            {range.offset, range.offset + range.length, text.substr(range.offset, range.length)});
    }
    // The regex admission above cannot see combining marks. Scan every full
    // numeric start once so signs and dot/comma decimals receive the same
    // short-unit boundary as integer candidates.
    if (document.valid) {
        for (std::size_t point = 0; point < document.points.size(); ++point) {
            const auto value = document.points[point].value;
            const auto signed_start = (value == '+' || value == '-' || value == 0x2212) &&
                                      point + 1 < document.points.size() &&
                                      text::is_digit(document.points[point + 1].value);
            const auto unsigned_start =
                text::is_digit(value) &&
                (point == 0 || !text::is_digit(document.points[point - 1].value));
            if (!signed_start && !unsigned_start)
                continue;
            const auto range = scan_combining_suffix(document, point);
            if (!range)
                continue;
            const auto range_end = range->offset + range->length;
            const auto present = std::any_of(matches.begin(), matches.end(), [&](auto& match) {
                const auto overlaps = match.begin < range_end && range->offset < match.end;
                if (overlaps) {
                    match.begin = std::min(match.begin, range->offset);
                    match.end = std::max(match.end, range_end);
                    match.value = text.substr(match.begin, match.end - match.begin);
                    return true;
                }
                return false;
            });
            if (!present)
                matches.push_back(
                    {range->offset, range_end, text.substr(range->offset, range->length)});
        }
    }
    const auto supported = scan_candidate_matches(text);
    matches.erase(
        std::remove_if(
            matches.begin(),
            matches.end(),
            [&](const MixedCandidateMatch& malformed) {
                return std::any_of(
                    supported.begin(), supported.end(), [&](const MixedCandidateMatch& candidate) {
                        if (candidate.begin != malformed.begin || candidate.end >= malformed.end)
                            return false;
                        const auto extra =
                            text.substr(candidate.end, malformed.end - candidate.end);
                        if (extra.empty() || !std::all_of(extra.begin(), extra.end(), [](char c) {
                                return c == '.';
                            }))
                            return false;
                        if (malformed.end == text.size())
                            return true;
                        const auto next = static_cast<unsigned char>(text[malformed.end]);
                        return std::isspace(next) != 0 || std::ispunct(next) != 0;
                    });
            }),
        matches.end());
    // Regex and typed scans may discover the same malformed surface from
    // different anchors (for example the currency tail and the full
    // combining-unit candidate).  Canonicalize overlaps before admission so
    // only the widest atomic span reaches the mixed pipeline.
    std::sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
        if (left.begin != right.begin)
            return left.begin < right.begin;
        return left.end > right.end;
    });
    std::vector<MixedCandidateMatch> merged;
    for (const auto& match : matches) {
        if (!merged.empty() && match.begin < merged.back().end) {
            merged.back().end = std::max(merged.back().end, match.end);
            merged.back().value =
                text.substr(merged.back().begin, merged.back().end - merged.back().begin);
        } else {
            merged.push_back(match);
        }
    }
    return merged;
}

} // namespace

const MixedLanguageRules& mixed_language_rules() {
    static const MixedLanguageRules rules{
        &scan_candidate_matches, &scan_malformed_matches, &format, &format_number};
    return rules;
}

} // namespace tts_front::detail::russian
