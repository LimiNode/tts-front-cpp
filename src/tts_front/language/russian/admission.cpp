#include "tts_front/language/russian/admission.hpp"

#include <array>
#include <string_view>

namespace tts_front::detail::russian {
namespace {

void classify_surface(std::string_view unit, bool& known_stem, bool& valid_surface) {
    const auto starts_with = [unit](std::string_view prefix) {
        return unit.size() >= prefix.size() && unit.compare(0, prefix.size(), prefix) == 0;
    };
    static constexpr std::array<std::string_view, 12> stems = {"руб",
                                                               "кг",
                                                               "килограмм",
                                                               "км",
                                                               "километр",
                                                               "см",
                                                               "сантиметр",
                                                               "мм",
                                                               "миллиметр",
                                                               "м",
                                                               "ГБ",
                                                               "МБ"};
    static constexpr std::array<std::string_view, 27> surfaces = {
        "руб",        "руб.",       "рубль",       "рубля",      "рублей",      "кг",
        "килограмм",  "килограмма", "килограммов", "км",         "километр",    "километра",
        "километров", "см",         "сантиметр",   "сантиметра", "сантиметров", "мм",
        "миллиметр",  "миллиметра", "миллиметров", "ГБ",         "МБ",          "м",
        "метр",       "метра",      "метров"};
    for (const auto stem : stems)
        known_stem = known_stem || (stem == "м" ? unit == "м" : starts_with(stem));
    for (const auto surface : surfaces)
        valid_surface = valid_surface || unit == surface;
}

} // namespace

const AdmissionRules& admission_rules() {
    static const AdmissionRules rules{nullptr, nullptr, false, true, &classify_surface};
    return rules;
}

} // namespace tts_front::detail::russian
