#include "tts_front/language/english/mixed_candidates.hpp"

#include "tts_front/language/english/numbers.hpp"
#include "tts_front/language/english/patterns.hpp"
#include "tts_front/normalization/admission.hpp"

#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail::english {
namespace {

const std::regex& candidate_pattern() {
    static const std::regex pattern{
        R"((^|[ \t\r\n(])((?:\$[0-9]+(?:\.[0-9]+)?)|-?[0-9]+(?:\.[0-9]+)?[ \t]*(?:kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm|GB|MB))(?=[^A-Za-z0-9_]|$))"};
    return pattern;
}

const std::regex& malformed_pattern() {
    static const std::regex pattern{
        R"(((?:\$[0-9]+(?:\.[0-9]+)?|[+-]?[0-9]+(?:\.[0-9]+)?[ \t]*(?:kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm))[A-Za-zА-Яа-яЁё]*))"};
    return pattern;
}

const std::regex& currency_pattern() {
    static const std::regex pattern{R"(\$([0-9]+)(?:\.([0-9]+))?)"};
    return pattern;
}

const std::regex& measurement_pattern() {
    static const std::regex pattern{
        R"((-?[0-9]+(?:\.([0-9]+))?)[ \t]*(kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm|GB|MB))"};
    return pattern;
}

std::optional<std::string> format(std::string_view candidate) {
    std::smatch match;
    const std::string value(candidate);
    long long integer = 0;
    if (std::regex_match(value, match, currency_pattern())) {
        if (!try_parse_long(match[1].str(), integer))
            return std::nullopt;
        if (match[2].matched) {
            const auto fraction = match[2].str();
            long long cents = 0;
            if (fraction.size() > 2 || !try_parse_long(fraction, cents))
                return std::nullopt;
            if (fraction.size() == 1)
                cents *= 10;
            return number(integer) + (integer == 1 ? " dollar " : " dollars ") + number(cents) +
                   (cents == 1 ? " cent" : " cents");
        }
        return number(integer) + (integer == 1 ? " dollar" : " dollars");
    }
    if (!std::regex_match(value, match, measurement_pattern()))
        return std::nullopt;
    const auto number_text = match[1].str();
    const auto fraction = match[2].str();
    const auto unit = match[3].str();
    const auto dot = number_text.find('.');
    if (!try_parse_long(dot == std::string::npos ? number_text : number_text.substr(0, dot),
                        integer))
        return std::nullopt;
    std::string spoken = number(integer);
    if (!fraction.empty())
        spoken += " point " + digits(fraction);
    if (fraction.empty()) {
        const bool singular = integer == 1 || integer == -1;
        const auto unit_name =
            unit == "kg" || unit.find("kilogram") == 0  ? (singular ? "kilogram" : "kilograms")
            : unit == "km" || unit.find("kilomet") == 0 ? (singular ? "kilometer" : "kilometers")
            : unit == "m" || unit == "meter" || unit == "meters" || unit == "metres"
                ? (singular ? "meter" : "meters")
            : unit == "cm" || unit.find("centimet") == 0 ? (singular ? "centimeter" : "centimeters")
            : unit == "mm" || unit.find("millimet") == 0 ? (singular ? "millimeter" : "millimeters")
            : unit == "MB"                               ? (singular ? "megabyte" : "megabytes")
                                                         : (singular ? "gigabyte" : "gigabytes");
        spoken += std::string(" ") + unit_name;
    } else {
        spoken += " " + unit;
    }
    return spoken;
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
    using Iterator = std::regex_iterator<std::string_view::const_iterator>;
    for (Iterator it(text.begin(), text.end(), malformed_pattern()), end; it != end; ++it) {
        const auto begin = static_cast<std::size_t>(it->position(1));
        matches.push_back({begin,
                           begin + static_cast<std::size_t>(it->length(1)),
                           text.substr(begin, static_cast<std::size_t>(it->length(1)))});
    }
    return matches;
}

} // namespace

const MixedLanguageRules& mixed_language_rules() {
    static const MixedLanguageRules rules{
        &scan_candidate_matches, &scan_malformed_matches, &format};
    return rules;
}

} // namespace tts_front::detail::english
