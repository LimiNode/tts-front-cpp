#include "tts_front/normalization/mixed_language.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/language/english/normalizer.hpp"
#include "tts_front/language/russian/normalizer.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"
#include "tts_front/normalization/patterns.hpp"
#include "tts_front/technical/patterns.hpp"

#include <array>
#include <regex>
#include <string>
#include <vector>

namespace tts_front::detail {
namespace {

const std::regex& foreign_english_candidate() {
    static const std::regex pattern{
        R"((^|[ \t\r\n(])((?:\$[0-9]+(?:\.[0-9]+)?)|-?[0-9]+(?:\.[0-9]+)?[ \t]*(?:kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm|GB|MB))(?=[^A-Za-z0-9_]|$))"};
    return pattern;
}

const std::regex& foreign_russian_candidate() {
    static const std::regex pattern{
        R"((^|[ \t\r\n(])(-?[0-9]+(?:,[0-9]+)?[ \t]*(?:рублей|рубля|рубль|руб\.?|километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м))(?=[^А-Яа-яЁёA-Za-z0-9_]|$))"};
    return pattern;
}

std::vector<SourceRange> technical_ranges(const std::string& text);

bool overlaps_technical(const std::string& text, std::size_t begin, std::size_t end) {
    const auto ranges = technical_ranges(text);
    for (const auto& range : ranges) {
        if (begin < range.offset + range.length && range.offset < end)
            return true;
    }
    return false;
}

std::vector<SourceRange> technical_ranges(const std::string& text) {
    std::vector<SourceRange> ranges;
    const auto& patterns = technical_patterns();
    const std::array<const std::regex*, 9> technical = {&patterns.technical_url,
                                                        &patterns.technical_email,
                                                        &patterns.technical_ipv4,
                                                        &patterns.technical_version,
                                                        &patterns.technical_http,
                                                        &patterns.technical_gpu,
                                                        &patterns.technical_identifier,
                                                        &patterns.technical_cpp,
                                                        &patterns.technical_csharp};
    for (const auto* pattern : technical) {
        for (std::sregex_iterator it(text.begin(), text.end(), *pattern), end_it; it != end_it;
             ++it) {
            ranges.push_back(
                {static_cast<std::size_t>(it->position()), static_cast<std::size_t>(it->length())});
        }
    }
    return ranges;
}

template <typename Normalizer>
void collect_candidates(const MappedText& input,
                        const std::regex& pattern,
                        Normalizer normalize,
                        WarningSink& warnings,
                        std::vector<SourceEdit>& edits) {
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), pattern), end; it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position(2));
        const auto finish = begin + static_cast<std::size_t>(it->length(2));
        if (overlaps_technical(input.text, begin, finish))
            continue;
        MappedText segment;
        segment.preserved_ranges = input.preserved_ranges;
        segment.append_copy(input, begin, finish);
        segment = normalize(std::move(segment), warnings);
        if (segment.text != input.text.substr(begin, finish - begin))
            edits.push_back({begin, finish, std::move(segment.text)});
    }
}

} // namespace

Language detect_mixed_language(std::string_view text, bool& has_cyrillic, bool& has_latin) {
    const std::string value(text);
    const auto ranges = technical_ranges(value);
    std::vector<Utf8CodePoint> points;
    decode_utf8(value, points);
    std::size_t cyrillic_count = 0;
    std::size_t latin_count = 0;
    for (const auto& point : points) {
        const bool technical =
            std::any_of(ranges.begin(), ranges.end(), [&point](const SourceRange& range) {
                return point.offset >= range.offset && point.offset < range.offset + range.length;
            });
        if (technical)
            continue;
        cyrillic_count += is_cyrillic(point.value) ? 1 : 0;
        latin_count += is_latin(point.value) ? 1 : 0;
    }
    has_cyrillic = cyrillic_count != 0;
    has_latin = latin_count != 0;
    if (has_cyrillic && has_latin && cyrillic_count >= latin_count)
        return Language::Russian;
    return has_cyrillic ? Language::Russian : Language::English;
}

MappedText
normalize_mixed_candidates(MappedText text, WarningSink& warnings, Language dominant_language) {
    std::vector<SourceEdit> edits;
    if (dominant_language == Language::Russian) {
        collect_candidates(
            text,
            foreign_english_candidate(),
            [](MappedText value, WarningSink& sink) {
                return english::normalize(std::move(value), sink);
            },
            warnings,
            edits);
    } else if (dominant_language == Language::English) {
        collect_candidates(
            text,
            foreign_russian_candidate(),
            [](MappedText value, WarningSink& sink) {
                return russian::normalize(std::move(value), sink);
            },
            warnings,
            edits);
    }
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail
