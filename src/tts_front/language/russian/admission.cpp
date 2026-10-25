#include "tts_front/language/russian/admission.hpp"

#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/text/codepoint_classification.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/language/russian/patterns.hpp"

#include <algorithm>
#include <array>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tts_front::detail::russian {
namespace {

void classify_surface(std::string_view unit,
                      const Utf8Document& document,
                      const std::size_t unit_begin,
                      bool& known_stem,
                      bool& valid_surface) {
    const auto starts_with = [unit](std::string_view prefix) {
        return unit.size() >= prefix.size() && unit.compare(0, prefix.size(), prefix) == 0;
    };
    static constexpr std::array<std::string_view, 30> surfaces = {
        "руб",       "руб.",       "рубль",      "рубля",       "рублей",     "кг",
        "kg",        "килограмм",  "килограмма", "килограммов", "км",         "километр",
        "километра", "километров", "см",         "сантиметр",   "сантиметра", "сантиметров",
        "мм",        "миллиметр",  "миллиметра", "миллиметров", "ГБ",         "МБ",
        "м",         "метр",       "метра",      "метров",      "GB",         "MB"};
    const auto has_combining_suffix = [unit, &document, unit_begin](const std::size_t byte_offset) {
        if (unit_begin >= document.points.size())
            return false;
        const auto absolute_offset = document.points[unit_begin].offset + byte_offset;
        auto point = codepoint_index_at_or_after(document, absolute_offset);
        if (point >= document.points.size() || document.points[point].offset != absolute_offset ||
            !text::is_combining_mark(document.points[point].value))
            return false;
        // A mark after a known unit stem is malformed only when it terminates
        // the surface or continues into a non-Cyrillic tail.  In a decomposed
        // ordinary word (`руб́ежей`, `метр́ов`) the next Cyrillic letter proves
        // that the prefix is lexical rather than a unit/currency surface.
        while (point < document.points.size() &&
               text::is_combining_mark(document.points[point].value))
            ++point;
        return point == document.points.size() || !text::is_cyrillic(document.points[point].value);
    };
    for (const auto surface : surfaces) {
        if (unit == surface) {
            valid_surface = true;
            known_stem = true;
        } else if (starts_with(surface)) {
            // Attached technical tails are ASCII (`kgfoo`, `руб.foo`). A
            // Cyrillic continuation belongs to an ordinary Russian word such
            // as `рубежей`, not to the unit/currency catalog.
            const auto continuation = static_cast<unsigned char>(unit[surface.size()]);
            const auto combining_tail = has_combining_suffix(surface.size());
            if (surface != "м" || combining_tail)
                known_stem = known_stem || continuation < 0x80 || combining_tail;
        }
    }
}

} // namespace

std::optional<SourceRange> scan_combining_suffix(const Utf8Document& document,
                                                 const std::size_t begin) {
    const auto surface = scan_candidate_surface(document, begin, CandidateSurfaceKind::Contextual);
    if (!surface.valid())
        return std::nullopt;

    const auto numeric_end = scan_candidate_continuation(
        document, surface.numeric.end, CandidateSurfaceKind::Contextual);
    const auto suffix = scan_lexical_suffix(document, numeric_end);
    const auto numeric_combining =
        std::any_of(document.points.begin() + static_cast<std::ptrdiff_t>(surface.numeric.end),
                    document.points.begin() + static_cast<std::ptrdiff_t>(numeric_end),
                    [](const auto& point) { return text::is_combining_mark(point.value); });
    const auto suffix_combining = std::find_if(
        document.points.begin() + static_cast<std::ptrdiff_t>(suffix.continuation_begin),
        document.points.begin() + static_cast<std::ptrdiff_t>(suffix.end),
        [](const auto& point) { return text::is_combining_mark(point.value); });
    if (!numeric_combining &&
        (suffix.end <= suffix.continuation_begin ||
         suffix_combining == document.points.begin() + static_cast<std::ptrdiff_t>(suffix.end)))
        return std::nullopt;

    auto after_mark =
        suffix_combining == document.points.begin() + static_cast<std::ptrdiff_t>(suffix.end)
            ? suffix.continuation_begin
            : static_cast<std::size_t>(suffix_combining - document.points.begin());
    if (!numeric_combining) {
        while (after_mark < suffix.end &&
               text::is_combining_mark(document.points[after_mark].value))
            ++after_mark;
        if (after_mark != suffix.end && text::is_cyrillic(document.points[after_mark].value))
            return std::nullopt;
    }

    auto suffix_end =
        numeric_combining && suffix.end <= suffix.continuation_begin ? numeric_end : suffix.end;
    bool known_stem = numeric_combining && suffix.end <= suffix.continuation_begin;
    bool valid_surface = false;
    std::string_view unit;
    if (!known_stem) {
        const auto unit_end =
            document.points[suffix.end - 1].offset + document.points[suffix.end - 1].length;
        unit = document.bytes.substr(document.points[suffix.continuation_begin].offset,
                                     unit_end - document.points[suffix.continuation_begin].offset);
        classify_surface(unit, document, suffix.continuation_begin, known_stem, valid_surface);
    }
    if (!known_stem || (valid_surface && !numeric_combining))
        return std::nullopt;

    if (unit.size() >= std::string_view("руб").size() &&
        unit.compare(0, std::string_view("руб").size(), "руб") == 0 &&
        suffix_end < document.points.size() && document.points[suffix_end].value == '.')
        ++suffix_end;

    auto admitted = surface;
    admitted.end = suffix_end;
    const auto full_end = scan_admission(document, admitted, CandidateSurfaceKind::Contextual).end;
    const auto range_begin = document.points[begin].offset;
    const auto range_end =
        document.points[full_end - 1].offset + document.points[full_end - 1].length;
    return SourceRange{range_begin, range_end - range_begin};
}

MappedText protect_combining_suffix_candidates(MappedText text,
                                               WarningSink& warnings,
                                               std::vector<ProtectedSpan>& protected_spans) {
    const Utf8Document document(text.text);
    if (!document.valid)
        return text;

    std::vector<SourceEdit> edits;
    for (std::size_t point = 0; point < document.points.size(); ++point) {
        const auto value = document.points[point].value;
        const auto signed_start = (value == '+' || value == '-' || value == 0x2212) &&
                                  point + 1 < document.points.size() &&
                                  text::is_digit(document.points[point + 1].value);
        const auto unsigned_start =
            text::is_digit(value) &&
            (point == 0 || !text::is_digit(document.points[point - 1].value));
        if (!signed_start && !unsigned_start)
            continue;

        const auto range = scan_combining_suffix(document, point);
        const auto candidate =
            scan_candidate_surface(document, point, CandidateSurfaceKind::Contextual);
        const auto numeric_end = candidate.valid()
                                     ? scan_candidate_continuation(document,
                                                                   candidate.numeric.end,
                                                                   CandidateSurfaceKind::Contextual)
                                     : point;
        auto admission_candidate = candidate;
        admission_candidate.end = std::max(admission_candidate.end, numeric_end);
        const auto admission_end =
            admission_candidate.valid()
                ? scan_admission(document, admission_candidate, CandidateSurfaceKind::Contextual)
                      .end
                : point;
        const auto numeric_combining =
            candidate.valid() &&
            std::any_of(document.points.begin() +
                            static_cast<std::ptrdiff_t>(candidate.numeric.end),
                        document.points.begin() + static_cast<std::ptrdiff_t>(numeric_end),
                        [](const auto& value) { return text::is_combining_mark(value.value); });
        if (!range && (!numeric_combining || admission_end <= candidate.end))
            continue;

        const auto candidate_range =
            range ? *range
                  : SourceRange{document.points[point].offset,
                                document.span_from_codepoints(point, admission_end).byte_end -
                                    document.points[point].offset};
        if (overlaps_preserved_range(
                text, candidate_range.offset, candidate_range.offset + candidate_range.length))
            continue;

        const auto span = document.span_from_bytes(candidate_range.offset,
                                                   candidate_range.offset + candidate_range.length);
        add_protected_candidate(
            text,
            NumericCandidate{span, NumericCandidateKind::Grouped, true, false, false},
            warnings,
            protected_spans,
            edits);
        point =
            codepoint_index_at_or_after(document, candidate_range.offset + candidate_range.length);
    }
    return apply_source_edits(text, std::move(edits));
}

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
        return malformed_combining ? std::max(direct, end_at_projected(pattern)) : direct;
    };
    const auto& russian_patterns = patterns();
    const auto percent_end = classify_end(russian_patterns.admission_percent);
    const auto measurement_end = classify_end(russian_patterns.admission_measurement);
    auto currency_end = classify_end(russian_patterns.admission_currency);
    for (std::size_t dots = 0;
         malformed_combining && currency_end < text.size() && text[currency_end] == '.' && dots < 3;
         ++dots)
        ++currency_end;
    const auto special_end = std::max({percent_end, measurement_end, currency_end});
    return {special_end,
            percent_end != 0 && percent_end == special_end ? CandidateSurfaceKind::Percent
                                                           : CandidateSurfaceKind::Contextual};
}

const AdmissionRules& admission_rules() {
    static const AdmissionRules rules{nullptr, nullptr, false, true, &classify_surface};
    return rules;
}

std::vector<SourceRange> scan_unsupported_currency(const Utf8Document& document) {
    std::vector<SourceRange> ranges;
    if (!document.valid)
        return ranges;

    using Iterator = std::regex_iterator<std::string_view::const_iterator>;
    const auto& pattern = patterns().admission_unsupported_currency;
    for (Iterator it(document.bytes.begin(), document.bytes.end(), pattern), end; it != end; ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto core_end = begin + static_cast<std::size_t>(it->length());
        const auto continuation_begin = codepoint_index_at_or_after(document, core_end);
        const auto continuation_end =
            scan_admission(document, continuation_begin, CandidateSurfaceKind::Currency).end;
        const auto finish_point = std::max(continuation_begin, continuation_end);
        const auto finish = document.span_from_codepoints(0, finish_point).byte_end;
        ranges.push_back({begin, finish - begin});
    }

    std::sort(ranges.begin(), ranges.end(), [](const SourceRange& left, const SourceRange& right) {
        return left.offset < right.offset ||
               (left.offset == right.offset && left.length > right.length);
    });
    std::vector<SourceRange> merged;
    for (const auto& range : ranges) {
        if (!merged.empty() && range.offset < merged.back().offset + merged.back().length) {
            const auto end =
                std::max(merged.back().offset + merged.back().length, range.offset + range.length);
            merged.back().length = end - merged.back().offset;
        } else {
            merged.push_back(range);
        }
    }
    return merged;
}

} // namespace tts_front::detail::russian
