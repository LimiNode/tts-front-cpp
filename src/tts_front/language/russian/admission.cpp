#include "tts_front/language/russian/admission.hpp"

#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/russian/patterns.hpp"

#include <algorithm>
#include <array>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail::russian {
namespace {

void classify_surface(std::string_view unit, bool& known_stem, bool& valid_surface) {
    const auto starts_with = [unit](std::string_view prefix) {
        return unit.size() >= prefix.size() && unit.compare(0, prefix.size(), prefix) == 0;
    };
    static constexpr std::array<std::string_view, 14> stems = {"руб",
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
                                                               "МБ",
                                                               "GB",
                                                               "MB"};
    static constexpr std::array<std::string_view, 29> surfaces = {
        "руб",        "руб.",       "рубль",       "рубля",      "рублей",      "кг",
        "килограмм",  "килограмма", "килограммов", "км",         "километр",    "километра",
        "километров", "см",         "сантиметр",   "сантиметра", "сантиметров", "мм",
        "миллиметр",  "миллиметра", "миллиметров", "ГБ",         "МБ",          "м",
        "метр",       "метра",      "метров",      "GB",         "MB"};
    for (const auto stem : stems)
        known_stem = known_stem || (stem == "м" ? unit == "м" : starts_with(stem));
    for (const auto surface : surfaces)
        valid_surface = valid_surface || unit == surface;
}

} // namespace

GenitiveSurfaceAdmission classify_genitive_surface(const std::string_view text,
                                                   const std::size_t begin,
                                                   const bool malformed_combining) {
    const auto end_at = [text, begin](const std::regex& pattern) {
        if (begin >= text.size())
            return std::size_t{0};
        std::cmatch match;
        const auto* first = text.data() + begin;
        const auto* last = text.data() + text.size();
        return std::regex_search(
                   first, last, match, pattern, std::regex_constants::match_continuous)
                   ? begin + static_cast<std::size_t>(match.length())
                   : std::size_t{0};
    };
    std::string compact;
    std::vector<std::size_t> original_end;
    std::size_t projection_limit = 0;
    bool projection_ready = false;
    const auto project_without_combining = [&] {
        if (projection_ready || !malformed_combining || begin >= text.size())
            return;
        projection_ready = true;
        // Admission patterns inspect only the immediate suffix. Build one
        // bounded projection and reuse it for all language-owned patterns.
        projection_limit = std::min(text.size(), begin + std::size_t{256});
        const Utf8Document document(text.substr(begin, projection_limit - begin));
        if (!document.valid)
            return;
        original_end.assign(1, begin);
        for (const auto& point : document.points) {
            const auto absolute_end = begin + point.offset + point.length;
            if (text::is_combining_mark(point.value)) {
                original_end.back() = absolute_end;
                continue;
            }
            const auto bytes = text.substr(begin + point.offset, point.length);
            compact.append(bytes);
            for (std::size_t index = 0; index < bytes.size(); ++index)
                original_end.push_back(absolute_end);
        }
    };
    const auto end_at_projected = [&](const std::regex& pattern) {
        project_without_combining();
        if (compact.empty())
            return std::size_t{0};
        std::cmatch match;
        if (!std::regex_search(
                compact.c_str(), match, pattern, std::regex_constants::match_continuous))
            return std::size_t{0};
        const auto compact_end = static_cast<std::size_t>(match.length());
        return compact_end < original_end.size() ? original_end[compact_end] : projection_limit;
    };
    const auto classify_end = [&](const std::regex& pattern) {
        const auto direct = end_at(pattern);
        return direct != 0 ? direct : end_at_projected(pattern);
    };
    const auto& russian_patterns = patterns();
    const auto percent_end = classify_end(russian_patterns.admission_percent);
    const auto measurement_end = classify_end(russian_patterns.admission_measurement);
    const auto currency_end = classify_end(russian_patterns.admission_currency);
    const auto special_end = std::max({percent_end, measurement_end, currency_end});
    return {special_end,
            percent_end != 0 && percent_end == special_end ? CandidateSurfaceKind::Percent
                                                           : CandidateSurfaceKind::Contextual};
}

const AdmissionRules& admission_rules() {
    static const AdmissionRules rules{nullptr, nullptr, false, true, &classify_surface};
    return rules;
}

} // namespace tts_front::detail::russian
