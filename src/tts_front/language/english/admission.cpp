#include "tts_front/language/english/admission.hpp"

#include "tts_front/language/english/patterns.hpp"

#include <array>
#include <string>
#include <string_view>

namespace tts_front::detail::english {
namespace {

void classify_surface(std::string_view unit,
                      const Utf8Document&,
                      std::size_t,
                      bool& known_stem,
                      bool& valid_surface) {
    const auto starts_with = [unit](std::string_view prefix) {
        return unit.size() >= prefix.size() && unit.compare(0, prefix.size(), prefix) == 0;
    };
    static constexpr std::array<std::string_view, 22> stems = {
        "kg",  "kilogram", "kilomet", "km",    "meter",  "metre",  "m",    "centimet",
        "cm",  "millimet", "mm",      "MB",    "GB",     "dollar", "euro", "pound",
        "yen", "руб",      "рубль",   "рубля", "рублей", "кг"};
    static constexpr std::array<std::string_view, 18> surfaces = {"kg",
                                                                  "kilogram",
                                                                  "kilograms",
                                                                  "km",
                                                                  "kilometer",
                                                                  "kilometers",
                                                                  "kilometre",
                                                                  "kilometres",
                                                                  "m",
                                                                  "meter",
                                                                  "meters",
                                                                  "metre",
                                                                  "metres",
                                                                  "cm",
                                                                  "mm",
                                                                  "MB",
                                                                  "GB",
                                                                  "dollar"};
    for (const auto stem : stems)
        known_stem = known_stem || starts_with(stem);
    for (const auto surface : surfaces)
        valid_surface = valid_surface || unit == surface;
}

CandidateValidity classify_candidate(std::string_view candidate, const CandidateStructure&) {
    if (candidate.find(',') == std::string_view::npos)
        return {CandidateValidityState::NotApplicable, CandidateSurfaceClass::Unknown};
    const auto& language_patterns = patterns();
    const std::string candidate_text(candidate);
    if (std::regex_match(candidate_text, language_patterns.comma_grouped_value) ||
        std::regex_match(candidate_text, language_patterns.comma_grouped_percent))
        return {CandidateValidityState::Supported, CandidateSurfaceClass::CommaGrouped};
    return {CandidateValidityState::Malformed, CandidateSurfaceClass::CommaGrouped};
}

} // namespace

const AdmissionRules& admission_rules() {
    static const AdmissionRules rules{&classify_surface, &classify_candidate};
    return rules;
}

} // namespace tts_front::detail::english
