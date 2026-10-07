#include "tts_front/language/english/admission.hpp"

#include "tts_front/language/english/patterns.hpp"

#include <array>
#include <string_view>

namespace tts_front::detail::english {
namespace {

void classify_surface(std::string_view unit, bool& known_stem, bool& valid_surface) {
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

} // namespace

const AdmissionRules& admission_rules() {
    const auto& language_patterns = patterns();
    static const AdmissionRules rules{&language_patterns.comma_grouped_value,
                                      &language_patterns.comma_grouped_percent,
                                      true,
                                      false,
                                      &classify_surface};
    return rules;
}

} // namespace tts_front::detail::english
