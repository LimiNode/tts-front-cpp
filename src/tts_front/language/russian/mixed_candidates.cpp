#include "tts_front/language/russian/mixed_candidates.hpp"

#include "tts_front/core/numeric_scanner.hpp"
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
            if (surface.valid())
                match_end = std::max(
                    match_end, classify_genitive_surface(text, begin, surface.has_combining).end);
        }
        matches.push_back({begin, match_end, text.substr(begin, match_end - begin)});
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
