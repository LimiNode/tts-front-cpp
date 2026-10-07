#include "tts_front/normalization/mixed_language.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/language/english/numbers.hpp"
#include "tts_front/language/russian/formatters.hpp"
#include "tts_front/language/russian/numbers.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"
#include "tts_front/normalization/patterns.hpp"
#include "tts_front/technical/patterns.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
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

const std::regex& english_currency() {
    static const std::regex pattern{R"(\$([0-9]+)(?:\.([0-9]+))?)"};
    return pattern;
}

const std::regex& english_measurement() {
    static const std::regex pattern{
        R"((-?[0-9]+(?:\.([0-9]+))?)[ \t]*(kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm|GB|MB))"};
    return pattern;
}

const std::regex& russian_measurement() {
    static const std::regex pattern{
        R"((-?[0-9]+)(?:,([0-9]+))?[ \t]*(рублей|рубля|рубль|руб\.?|километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м))"};
    return pattern;
}

const std::regex& malformed_english_candidate() {
    static const std::regex pattern{
        R"(((?:\$[0-9]+(?:\.[0-9]+)?|[+-]?[0-9]+(?:\.[0-9]+)?[ \t]*(?:kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm))[A-Za-zА-Яа-яЁё]*))"};
    return pattern;
}

const std::regex& malformed_russian_candidate() {
    static const std::regex pattern{
        R"((-?[0-9]+(?:,[0-9]+)?[ \t]*(?:рублей|рубля|рубль|руб\.?|километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м)[A-Za-zА-Яа-яЁё]*))"};
    return pattern;
}

std::vector<SourceRange> scan_technical_ranges(const std::string& text) {
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
    std::array<std::vector<SourceRange>, 9> streams;
    for (std::size_t stream = 0; stream < technical.size(); ++stream) {
        const auto* pattern = technical[stream];
        for (std::sregex_iterator it(text.begin(), text.end(), *pattern), end; it != end; ++it)
            streams[stream].push_back(
                {static_cast<std::size_t>(it->position()), static_cast<std::size_t>(it->length())});
    }

    // Each regex iterator is ordered by source offset.  Merge the fixed set
    // of streams directly instead of sorting the combined result, keeping
    // technical-index construction linear in the number of matches.
    std::vector<SourceRange> merged;
    std::array<std::size_t, 9> cursors{};
    while (true) {
        std::size_t selected = streams.size();
        for (std::size_t stream = 0; stream < streams.size(); ++stream) {
            if (cursors[stream] == streams[stream].size())
                continue;
            if (selected == streams.size() || streams[stream][cursors[stream]].offset <
                                                  streams[selected][cursors[selected]].offset)
                selected = stream;
        }
        if (selected == streams.size())
            break;
        const auto range = streams[selected][cursors[selected]++];
        if (range.length == 0)
            continue;
        if (!merged.empty() && range.offset <= merged.back().offset + merged.back().length) {
            const auto end =
                std::max(merged.back().offset + merged.back().length, range.offset + range.length);
            merged.back().length = end - merged.back().offset;
        } else {
            merged.push_back(range);
        }
    }
    return merged;
}

bool overlaps(const std::vector<SourceRange>& ranges,
              std::size_t begin,
              std::size_t end,
              std::size_t& cursor) {
    while (cursor < ranges.size() && ranges[cursor].offset + ranges[cursor].length <= begin)
        ++cursor;
    return cursor < ranges.size() && ranges[cursor].offset < end;
}

bool preserved(const MappedText& input, std::size_t begin, std::size_t end) {
    if (input.preserved_ranges == nullptr)
        return false;
    const auto source = input.source_range(begin, end);
    const auto& ranges = *input.preserved_ranges;
    const auto overlaps = [source](const SourceRange& range) {
        return source.offset < range.offset + range.length &&
               range.offset < source.offset + source.length;
    };
    auto next = std::lower_bound(
        ranges.begin(), ranges.end(), source.offset, [](const SourceRange& range, std::size_t at) {
            return range.offset < at;
        });
    if (next != ranges.end() && overlaps(*next))
        return true;
    return next != ranges.begin() && overlaps(*std::prev(next));
}

std::optional<std::string> format_english(std::string_view candidate) {
    std::smatch match;
    const std::string value(candidate);
    long long integer = 0;
    if (std::regex_match(value, match, english_currency())) {
        if (!try_parse_long(match[1].str(), integer))
            return std::nullopt;
        if (match[2].matched) {
            const auto fraction = match[2].str();
            long long cents = 0;
            if (fraction.size() > 2 || !try_parse_long(fraction, cents))
                return std::nullopt;
            if (fraction.size() == 1)
                cents *= 10;
            return english::number(integer) + (integer == 1 ? " dollar " : " dollars ") +
                   english::number(cents) + (cents == 1 ? " cent" : " cents");
        }
        return english::number(integer) + (integer == 1 ? " dollar" : " dollars");
    }
    if (!std::regex_match(value, match, english_measurement()))
        return std::nullopt;
    const auto number_text = match[1].str();
    const auto fraction = match[2].str();
    const auto unit = match[3].str();
    const auto dot = number_text.find('.');
    if (!try_parse_long(dot == std::string::npos ? number_text : number_text.substr(0, dot),
                        integer))
        return std::nullopt;
    std::string spoken = english::number(integer);
    if (!fraction.empty())
        spoken += " point " + english::digits(fraction);
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

std::optional<std::string> format_russian(std::string_view candidate) {
    std::smatch match;
    const std::string value(candidate);
    if (!std::regex_match(value, match, russian_measurement()))
        return std::nullopt;
    long long integer = 0;
    if (!try_parse_long(match[1].str(), integer))
        return std::nullopt;
    const auto fraction = match[2].str();
    const auto unit = match[3].str();
    if (!fraction.empty()) {
        if (fraction.size() > 3)
            return std::nullopt;
        if (unit.find("руб") == 0 || unit == "руб.")
            return russian::ru_decimal(integer, fraction) + " рубля";
        return russian::ru_decimal(integer, fraction) + " " + unit;
    }
    if (unit.find("руб") == 0 || unit == "руб.")
        return russian::ru_number(integer) + " " +
               russian::ru_form(integer, "рубль", "рубля", "рублей");
    const bool kilogram = unit.find("кг") == 0 || unit.find("килограмм") == 0;
    const bool kilometer = unit.find("км") == 0 || unit.find("километр") == 0;
    const bool centimeter = unit.find("см") == 0 || unit.find("сантиметр") == 0;
    const bool millimeter = unit.find("мм") == 0 || unit.find("миллиметр") == 0;
    const char* one = kilogram       ? "килограмм"
                      : kilometer    ? "километр"
                      : centimeter   ? "сантиметр"
                      : millimeter   ? "миллиметр"
                      : unit == "м"  ? "метр"
                      : unit == "ГБ" ? "гигабайт"
                                     : "мегабайт";
    const char* few = kilogram       ? "килограмма"
                      : kilometer    ? "километра"
                      : centimeter   ? "сантиметра"
                      : millimeter   ? "миллиметра"
                      : unit == "м"  ? "метра"
                      : unit == "ГБ" ? "гигабайта"
                                     : "мегабайта";
    const char* many = kilogram       ? "килограммов"
                       : kilometer    ? "километров"
                       : centimeter   ? "сантиметров"
                       : millimeter   ? "миллиметров"
                       : unit == "м"  ? "метров"
                       : unit == "ГБ" ? "гигабайт"
                                      : "мегабайт";
    return russian::ru_number(integer) + " " + russian::ru_form(integer, one, few, many);
}

template <typename Formatter>
void collect_candidates(const MappedText& input,
                        const std::regex& pattern,
                        const TechnicalRangeIndex& technical_index,
                        Formatter formatter,
                        WarningSink& warnings,
                        std::vector<SourceEdit>& edits) {
    std::size_t technical_cursor = 0;
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), pattern), end; it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position(2));
        const auto finish = begin + static_cast<std::size_t>(it->length(2));
        const bool technical_overlap =
            overlaps(technical_index.ranges, begin, finish, technical_cursor);
        const bool candidate_inside_technical =
            technical_overlap && technical_index.ranges[technical_cursor].offset <= begin;
        if (candidate_inside_technical || preserved(input, begin, finish))
            continue;
        const auto replacement = formatter(it->str(2));
        if (!replacement) {
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unable to parse mixed-language candidate",
                         input.source_range(begin, finish));
            continue;
        }
        if (*replacement != input.text.substr(begin, finish - begin))
            edits.push_back({begin, finish, *replacement});
    }
}

template <typename Formatter>
void protect_malformed_candidates(const MappedText& input,
                                  const std::regex& pattern,
                                  const TechnicalRangeIndex& technical_index,
                                  Formatter formatter,
                                  WarningSink& warnings) {
    std::size_t technical_cursor = 0;
    const Utf8Document document(input.text);
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), pattern), end; it != end;
         ++it) {
        auto begin = static_cast<std::size_t>(it->position(1));
        const auto finish = begin + static_cast<std::size_t>(it->length(1));
        const bool technical_overlap =
            overlaps(technical_index.ranges, begin, finish, technical_cursor);
        const bool candidate_inside_technical =
            technical_overlap && technical_index.ranges[technical_cursor].offset <= begin;
        if (candidate_inside_technical || preserved(input, begin, finish))
            continue;
        const auto continuation_end = [&] {
            std::size_t index = codepoint_index_at_or_after(document, finish);
            const auto start = index;
            while (index < document.points.size()) {
                auto connector = index;
                while (connector < document.points.size() &&
                       is_horizontal_space(document.points[connector].value))
                    ++connector;
                if (connector >= document.points.size())
                    break;
                const auto value = document.points[connector].value;
                if (value != '/' && value != '-' && value != '+' && value != '=' && value != '%' &&
                    value != '*' && !is_range_connector(value))
                    break;
                auto number = connector + 1;
                while (number < document.points.size() &&
                       is_horizontal_space(document.points[number].value))
                    ++number;
                if (number >= document.points.size() || !is_digit(document.points[number].value))
                    break;
                while (number < document.points.size() && is_digit(document.points[number].value))
                    ++number;
                if (number + 1 < document.points.size() &&
                    (document.points[number].value == '.' ||
                     document.points[number].value == ',') &&
                    is_digit(document.points[number + 1].value)) {
                    number += 2;
                    while (number < document.points.size() &&
                           is_digit(document.points[number].value))
                        ++number;
                }
                index = number;
            }
            return index == start ? finish : document.span_from_codepoints(start, index).byte_end;
        }();
        const auto candidate_end = continuation_end;
        const auto point_index = codepoint_index_at_or_after(document, begin);
        const auto previous = point_index == 0 ? 0U : document.points[point_index - 1].value;
        const bool valid_prefix = point_index == 0 || is_horizontal_space(previous) ||
                                  previous == '\n' || previous == '\r' || previous == '(';
        if (!valid_prefix) {
            if (point_index != 0 && (previous == '/' || previous == '+' || previous == '-' ||
                                     is_range_connector(previous)))
                begin = document.points[point_index - 1].offset;
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unsupported mixed-language numeric boundary",
                         input.source_range(begin, candidate_end));
            continue;
        }
        if (candidate_end != finish || !formatter(it->str(1))) {
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unsupported mixed-language numeric candidate",
                         input.source_range(begin, candidate_end));
        }
    }
}

} // namespace

TechnicalRangeIndex build_technical_range_index(std::string_view text) {
    return {scan_technical_ranges(std::string(text))};
}

Language detect_mixed_language(std::string_view text,
                               bool& has_cyrillic,
                               bool& has_latin,
                               const TechnicalRangeIndex& technical_index) {
    std::vector<Utf8CodePoint> points;
    decode_utf8(text, points);
    std::size_t cyrillic_count = 0;
    std::size_t latin_count = 0;
    std::size_t cursor = 0;
    for (const auto& point : points) {
        while (cursor < technical_index.ranges.size() &&
               technical_index.ranges[cursor].offset + technical_index.ranges[cursor].length <=
                   point.offset)
            ++cursor;
        if (cursor < technical_index.ranges.size() &&
            technical_index.ranges[cursor].offset <= point.offset)
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

MappedText normalize_mixed_candidates(MappedText text,
                                      WarningSink& warnings,
                                      Language dominant_language,
                                      const TechnicalRangeIndex& technical_index) {
    std::vector<SourceEdit> edits;
    if (dominant_language == Language::Russian) {
        protect_malformed_candidates(
            text, malformed_english_candidate(), technical_index, format_english, warnings);
        protect_malformed_candidates(
            text, malformed_russian_candidate(), technical_index, format_russian, warnings);
        collect_candidates(
            text, foreign_english_candidate(), technical_index, format_english, warnings, edits);
    } else if (dominant_language == Language::English) {
        protect_malformed_candidates(
            text, malformed_russian_candidate(), technical_index, format_russian, warnings);
        protect_malformed_candidates(
            text, malformed_english_candidate(), technical_index, format_english, warnings);
        collect_candidates(
            text, foreign_russian_candidate(), technical_index, format_russian, warnings, edits);
    }
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail
