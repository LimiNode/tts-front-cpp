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
        if (fraction.size() > 3)
            return std::nullopt;
        if (unit.find("руб") == 0 || unit == "руб.")
            return signed_zero_prefix + decimal(integer, fraction) + " рубля";
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

struct ShortUnitCombiningRange {
    std::size_t begin = 0;
    std::size_t end = 0;
};

constexpr std::uint32_t kShortMeterCodePoint = 0x043c;

std::optional<ShortUnitCombiningRange> scan_short_unit_combining(const Utf8Document& document,
                                                                 const std::size_t begin) {
    const auto surface = scan_candidate_surface(document, begin, CandidateSurfaceKind::Contextual);
    if (!surface.valid())
        return std::nullopt;

    // Start from the complete numeric continuation boundary. In particular,
    // this consumes dot/comma decimals before looking for the lexical suffix,
    // so a mixed-language match can never begin at the fractional digit.
    // Keep the lexical suffix itself for the short-unit check below.
    const auto numeric_end = scan_candidate_continuation(
        document, surface.numeric.end, CandidateSurfaceKind::Contextual);
    const auto suffix = scan_lexical_suffix(document, numeric_end);
    if (suffix.end <= suffix.continuation_begin ||
        document.points[suffix.continuation_begin].value != kShortMeterCodePoint ||
        suffix.continuation_begin + 1 >= suffix.end ||
        !text::is_combining_mark(document.points[suffix.continuation_begin + 1].value))
        return std::nullopt;

    auto after_mark = suffix.continuation_begin + 1;
    while (after_mark < suffix.end && text::is_combining_mark(document.points[after_mark].value))
        ++after_mark;
    if (after_mark != suffix.end && text::is_cyrillic(document.points[after_mark].value))
        return std::nullopt;

    const auto range_begin = document.points[begin].offset;
    const auto range_end =
        document.points[suffix.end - 1].offset + document.points[suffix.end - 1].length;
    return ShortUnitCombiningRange{range_begin, range_end};
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
                match_end = std::max(
                    match_end, classify_genitive_surface(text, begin, surface.has_combining).end);
            }
        }
        matches.push_back({begin, match_end, text.substr(begin, match_end - begin)});
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
            const auto range = scan_short_unit_combining(document, point);
            if (!range)
                continue;
            const auto present = std::any_of(matches.begin(), matches.end(), [&](auto& match) {
                const auto overlaps = match.begin < range->end && range->begin < match.end;
                if (overlaps) {
                    match.begin = std::min(match.begin, range->begin);
                    match.end = std::max(match.end, range->end);
                    match.value = text.substr(match.begin, match.end - match.begin);
                    return true;
                }
                return false;
            });
            if (!present)
                matches.push_back({range->begin,
                                   range->end,
                                   text.substr(range->begin, range->end - range->begin)});
        }
    }
    return matches;
}

} // namespace

const MixedLanguageRules& mixed_language_rules() {
    static const MixedLanguageRules rules{
        &scan_candidate_matches, &scan_malformed_matches, &format};
    return rules;
}

} // namespace tts_front::detail::russian
