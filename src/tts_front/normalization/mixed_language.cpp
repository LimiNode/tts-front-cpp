#include "tts_front/normalization/mixed_language.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/technical/admission.hpp"
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
    return technical::scan_protected_ranges(text);
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

struct SourceRangeIndex {
    std::vector<SourceRange> ranges;

    bool overlaps(std::size_t begin, std::size_t end, std::size_t& cursor) const {
        while (cursor < ranges.size() && ranges[cursor].offset + ranges[cursor].length <= begin)
            ++cursor;
        return cursor < ranges.size() && ranges[cursor].offset < end;
    }
};

SourceRangeIndex typed_candidate_ranges(const MixedLanguageRules& dominant,
                                        const MixedLanguageRules& foreign,
                                        std::string_view text) {
    SourceRangeIndex index;
    const std::array<std::vector<MixedCandidateMatch>, 2> matches = {dominant.scan_candidates(text),
                                                                     foreign.scan_candidates(text)};
    std::array<std::size_t, 2> cursors{};
    const auto append = [&](SourceRange range) {
        if (range.length == 0)
            return;
        if (!index.ranges.empty() &&
            range.offset <= index.ranges.back().offset + index.ranges.back().length) {
            const auto end = std::max(index.ranges.back().offset + index.ranges.back().length,
                                      range.offset + range.length);
            index.ranges.back().length = end - index.ranges.back().offset;
        } else {
            index.ranges.push_back(range);
        }
    };
    for (;;) {
        std::size_t selected = matches.size();
        for (std::size_t stream = 0; stream < matches.size(); ++stream) {
            if (cursors[stream] == matches[stream].size())
                continue;
            if (selected == matches.size() ||
                matches[stream][cursors[stream]].begin < matches[selected][cursors[selected]].begin)
                selected = stream;
        }
        if (selected == matches.size())
            break;
        const auto& match = matches[selected][cursors[selected]++];
        append(SourceRange{match.begin, match.end - match.begin});
    }
    return index;
}

void collect_local_number_candidates(const MappedText& input,
                                     const MixedLanguageRules& dominant,
                                     const MixedLanguageRules& foreign,
                                     Language dominant_language,
                                     const TechnicalRangeIndex& technical_index,
                                     WarningSink& warnings,
                                     std::vector<SourceEdit>& edits) {
    const Utf8Document document(input.text);
    if (!document.valid)
        return;
    const auto typed_ranges = typed_candidate_ranges(dominant, foreign, input.text);
    const auto formatter_for = [&](bool cyrillic) {
        if (dominant_language == Language::Russian)
            return cyrillic ? dominant.number_formatter : foreign.number_formatter;
        return cyrillic ? foreign.number_formatter : dominant.number_formatter;
    };
    std::size_t technical_cursor = 0;
    std::size_t typed_cursor = 0;
    for (std::size_t point = 0; point < document.points.size(); ++point) {
        const auto value = document.points[point].value;
        const bool signed_start = (value == '+' || value == '-' || value == 0x2212) &&
                                  point + 1 < document.points.size() &&
                                  text::is_digit(document.points[point + 1].value);
        const bool unsigned_start =
            text::is_digit(value) &&
            (point == 0 || !text::is_digit(document.points[point - 1].value));
        if (!signed_start && !unsigned_start)
            continue;
        if (point > 0 &&
            (text::is_letter(document.points[point - 1].value) ||
             document.points[point - 1].value == '_' || document.points[point - 1].value == '.' ||
             document.points[point - 1].value == ',' || document.points[point - 1].value == ':'))
            continue;
        const auto candidate =
            scan_candidate_surface(document, point, CandidateSurfaceKind::Contextual);
        if (!candidate.valid() || candidate.numeric.exponent != ExponentState::None ||
            candidate.numeric.end != candidate.numeric.digits_end)
            continue;
        const auto begin = document.points[point].offset;
        const auto core_end = document.span_from_codepoints(0, candidate.numeric.end).byte_end;
        if (overlaps(technical_index.ranges, begin, core_end, technical_cursor) ||
            typed_ranges.overlaps(begin, core_end, typed_cursor) ||
            preserved(input, begin, core_end))
            continue;
        auto word_begin = candidate.numeric.end;
        while (word_begin < document.points.size() &&
               text::is_horizontal_space(document.points[word_begin].value))
            ++word_begin;
        if (word_begin >= document.points.size() ||
            !text::is_letter(document.points[word_begin].value))
            continue;
        bool has_cyrillic = false;
        bool has_latin = false;
        auto word_end = word_begin;
        while (word_end < document.points.size() &&
               (text::is_letter(document.points[word_end].value) ||
                text::is_combining_mark(document.points[word_end].value))) {
            has_cyrillic = has_cyrillic || text::is_cyrillic(document.points[word_end].value);
            has_latin = has_latin || text::is_latin(document.points[word_end].value);
            ++word_end;
        }
        if (has_cyrillic == has_latin)
            continue;
        const auto word_begin_byte = document.points[word_begin].offset;
        const auto word_end_byte = document.span_from_codepoints(0, word_end).byte_end;
        if (overlaps(technical_index.ranges, word_begin_byte, word_end_byte, technical_cursor))
            continue;
        const auto word = input.text.substr(word_begin_byte, word_end_byte - word_begin_byte);
        if (technical::has_known_identifier_prefix(word))
            continue;
        const auto formatter = formatter_for(has_cyrillic);
        if (formatter == nullptr)
            continue;
        const auto continuation =
            scan_typed_admission_end(document, word_end, CandidateSurfaceKind::Contextual);
        if (continuation > word_end) {
            const auto finish = document.span_from_codepoints(0, continuation).byte_end;
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unsupported mixed-language numeric candidate",
                         input.source_range(begin, finish));
            continue;
        }
        const auto token = input.text.substr(begin, core_end - begin);
        const auto replacement = formatter(token);
        if (replacement && *replacement != input.text.substr(begin, core_end - begin))
            edits.push_back({begin, core_end, *replacement});
    }
}

void collect_candidates(const MappedText& input,
                        const MixedLanguageRules& rules,
                        const TechnicalRangeIndex& technical_index,
                        WarningSink& warnings,
                        std::vector<SourceEdit>& edits) {
    std::size_t technical_cursor = 0;
    const Utf8Document document(input.text);
    for (const auto& match : rules.scan_candidates(input.text)) {
        const auto begin = match.begin;
        const auto finish = match.end;
        const bool technical_overlap =
            overlaps(technical_index.ranges, begin, finish, technical_cursor);
        const bool candidate_inside_technical =
            technical_overlap && technical_index.ranges[technical_cursor].offset <= begin;
        if (candidate_inside_technical || preserved(input, begin, finish))
            continue;
        // A recognized mixed-language core is not independently formatable
        // when the shared admission scanner sees an attached/spaced tail.
        // The malformed pass has already emitted the single atomic warning;
        // leave this core byte-identical instead of rewriting its head.
        const auto continuation_begin = codepoint_index_at_or_after(document, finish);
        if (scan_typed_admission_end(document,
                                     continuation_begin,
                                     CandidateSurfaceKind::Contextual) > continuation_begin)
            continue;
        const auto replacement = rules.formatter(match.value);
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
    for (const auto& match : rules.scan_malformed(input.text)) {
        auto begin = match.begin;
        const auto finish = match.end;
        const bool technical_overlap =
            overlaps(technical_index.ranges, begin, finish, technical_cursor);
        const bool candidate_inside_technical =
            technical_overlap && technical_index.ranges[technical_cursor].offset <= begin;
        if (candidate_inside_technical || preserved(input, begin, finish))
            continue;
        auto continuation = codepoint_index_at_or_after(document, finish);
        for (;;) {
            const auto attached = scan_candidate_continuation(
                document, continuation, CandidateSurfaceKind::Contextual);
            const auto spaced = scan_spaced_numeric_continuation(
                document, continuation, CandidateSurfaceKind::Contextual);
            const auto next = std::max(attached, spaced);
            if (next <= continuation)
                break;
            continuation = next;
        }
        const auto continuation_begin = codepoint_index_at_or_after(document, finish);
        const auto continued_end =
            document.span_from_codepoints(continuation_begin, continuation).byte_end;
        const auto candidate_end = continued_end > finish ? continued_end : finish;
        const auto point_index = codepoint_index_at_or_after(document, begin);
        const auto previous = point_index == 0 ? 0U : document.points[point_index - 1].value;
        const bool valid_prefix = point_index == 0 || text::is_horizontal_space(previous) ||
                                  previous == '\n' || previous == '\r' || previous == '(';
        if (!valid_prefix) {
            if (point_index != 0 && (previous == '/' || previous == '+' || previous == '-' ||
                                     text::is_range_connector(previous)))
                begin = document.points[point_index - 1].offset;
            warnings.add(WarningCode::UnresolvedNumber,
                         "Unsupported mixed-language numeric boundary",
                         input.source_range(begin, candidate_end));
            continue;
        }
        const auto combining_inside = [&] {
            const auto first = codepoint_index_at_or_after(document, begin);
            const auto last = codepoint_index_at_or_after(document, candidate_end);
            return std::any_of(
                document.points.begin() + static_cast<std::ptrdiff_t>(first),
                document.points.begin() + static_cast<std::ptrdiff_t>(last),
                [](const auto& point) { return text::is_combining_mark(point.value); });
        }();
        if (candidate_end != finish || combining_inside || !rules.formatter(match.value))
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
        cyrillic_count += text::is_cyrillic(point.value) ? 1 : 0;
        latin_count += text::is_latin(point.value) ? 1 : 0;
    }
    has_cyrillic = cyrillic_count != 0;
    has_latin = latin_count != 0;
    if (has_cyrillic && has_latin && cyrillic_count >= latin_count)
        return Language::Russian;
    return has_cyrillic && !has_latin ? Language::Russian : Language::English;
}

MappedText normalize_mixed_candidates(MappedText text,
                                      WarningSink& warnings,
                                      const MixedLanguageRules& dominant,
                                      const MixedLanguageRules& foreign,
                                      Language dominant_language,
                                      bool enable_local_ownership,
                                      const TechnicalRangeIndex& technical_index) {
    std::vector<SourceEdit> edits;
    protect_malformed_candidates(text, dominant, technical_index, warnings);
    protect_malformed_candidates(text, foreign, technical_index, warnings);
    if (enable_local_ownership)
        collect_local_number_candidates(
            text, dominant, foreign, dominant_language, technical_index, warnings, edits);
    collect_candidates(text, foreign, technical_index, warnings, edits);
    return apply_source_edits(text, std::move(edits));
}

} // namespace tts_front::detail
