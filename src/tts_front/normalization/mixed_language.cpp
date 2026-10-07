#include "tts_front/normalization/mixed_language.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"
#include "tts_front/technical/patterns.hpp"

#include <algorithm>
#include <array>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {
namespace {

std::vector<SourceRange> scan_technical_ranges(const std::string& text) {
    const auto& patterns = technical::patterns();
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
        for (std::sregex_iterator it(text.begin(), text.end(), *technical[stream]), end; it != end;
             ++it)
            streams[stream].push_back(
                {static_cast<std::size_t>(it->position()), static_cast<std::size_t>(it->length())});
    }
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
    const auto intersects = [source](const SourceRange& range) {
        return source.offset < range.offset + range.length &&
               range.offset < source.offset + source.length;
    };
    auto next = std::lower_bound(
        ranges.begin(), ranges.end(), source.offset, [](const SourceRange& range, std::size_t at) {
            return range.offset < at;
        });
    return (next != ranges.end() && intersects(*next)) ||
           (next != ranges.begin() && intersects(*std::prev(next)));
}

void collect_candidates(const MappedText& input,
                        const MixedLanguageRules& rules,
                        const TechnicalRangeIndex& technical_index,
                        WarningSink& warnings,
                        std::vector<SourceEdit>& edits) {
    std::size_t technical_cursor = 0;
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), rules.candidate), end;
         it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position(2));
        const auto finish = begin + static_cast<std::size_t>(it->length(2));
        const bool technical_overlap =
            overlaps(technical_index.ranges, begin, finish, technical_cursor);
        const bool candidate_inside_technical =
            technical_overlap && technical_index.ranges[technical_cursor].offset <= begin;
        if (candidate_inside_technical || preserved(input, begin, finish))
            continue;
        const auto replacement = rules.formatter(it->str(2));
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

void protect_malformed_candidates(const MappedText& input,
                                  const MixedLanguageRules& rules,
                                  const TechnicalRangeIndex& technical_index,
                                  WarningSink& warnings) {
    std::size_t technical_cursor = 0;
    const Utf8Document document(input.text);
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), rules.malformed), end;
         it != end;
         ++it) {
        auto begin = static_cast<std::size_t>(it->position(1));
        const auto finish = begin + static_cast<std::size_t>(it->length(1));
        const bool technical_overlap =
            overlaps(technical_index.ranges, begin, finish, technical_cursor);
        const bool candidate_inside_technical =
            technical_overlap && technical_index.ranges[technical_cursor].offset <= begin;
        if (candidate_inside_technical || preserved(input, begin, finish))
            continue;
        std::size_t index = codepoint_index_at_or_after(document, finish);
        const auto continuation_start = index;
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
                (document.points[number].value == '.' || document.points[number].value == ',') &&
                is_digit(document.points[number + 1].value)) {
                number += 2;
                while (number < document.points.size() && is_digit(document.points[number].value))
                    ++number;
            }
            index = number;
        }
        const auto candidate_end =
            index == continuation_start
                ? finish
                : document.span_from_codepoints(continuation_start, index).byte_end;
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
        if (candidate_end != finish || !rules.formatter(it->str(1)))
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unsupported mixed-language numeric candidate",
                         input.source_range(begin, candidate_end));
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
                                      const MixedLanguageRules& dominant,
                                      const MixedLanguageRules& foreign,
                                      const TechnicalRangeIndex& technical_index) {
    std::vector<SourceEdit> edits;
    protect_malformed_candidates(text, foreign, technical_index, warnings);
    protect_malformed_candidates(text, dominant, technical_index, warnings);
    collect_candidates(text, foreign, technical_index, warnings, edits);
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail
