#include "tts_front/core/normalization_support.hpp"

#include <algorithm>
#include <utility>

namespace tts_front::detail {

void WarningSink::add_range(WarningCode code,
                            std::string message,
                            std::size_t offset,
                            std::size_t length) {
    const auto duplicate =
        std::find_if(warnings.begin(), warnings.end(), [&](const TextWarning& warning) {
            return warning.code == code && warning.offset == offset && warning.length == length;
        });
    if (duplicate != warnings.end())
        return;
    warnings.push_back({code, std::move(message), offset, length});
}

void WarningSink::add(WarningCode code, std::string message, SourceRange source) {
    if (source.length != 0) {
        const auto insertion =
            std::lower_bound(preserved_ranges.begin(),
                             preserved_ranges.end(),
                             source,
                             [](const SourceRange& left, const SourceRange& right) {
                                 if (left.offset != right.offset)
                                     return left.offset < right.offset;
                                 return left.length < right.length;
                             });
        if (insertion == preserved_ranges.end() || insertion->offset != source.offset ||
            insertion->length != source.length)
            preserved_ranges.insert(insertion, source);
    }
    add_range(code, std::move(message), source.offset, source.length);
}

void add_protected_candidate(const MappedText& text,
                             const NumericCandidate& candidate,
                             WarningSink& warnings,
                             std::vector<ProtectedSpan>& protected_spans,
                             std::vector<SourceEdit>& edits) {
    const auto begin = candidate.span.byte_begin;
    const auto end = candidate.span.byte_end;
    if (begin >= end)
        return;
    if (!edits.empty() && begin < edits.back().end)
        return;
    const auto marker = marker_for(text.text, protected_spans.size());
    warnings.add(WarningCode::UnresolvedNumber,
                 "Unsupported numeric-like candidate preserved verbatim",
                 text.source_range(begin, end));
    protected_spans.push_back({marker, text.text.substr(begin, end - begin)});
    edits.push_back({begin, end, marker});
}

std::string marker_for(std::string_view text, std::size_t index) {
    std::string suffix;
    do {
        suffix.push_back(static_cast<char>('a' + (index % 26)));
        index /= 26;
    } while (index != 0);
    std::string marker = "\x01tts_front_protected_" + suffix + "\x02";
    while (text.find(marker) != std::string::npos)
        marker.insert(marker.size() - 1, "x");
    return marker;
}

bool overlaps_preserved_range(const MappedText& text, std::size_t begin, std::size_t end) {
    if (!text.preserved_ranges || begin >= end)
        return false;
    const auto source = text.source_range(begin, end);
    const auto& ranges = *text.preserved_ranges;
    const auto overlaps = [source](const SourceRange& range) {
        return source.offset < range.offset + range.length &&
               range.offset < source.offset + source.length;
    };
    const auto next = std::lower_bound(
        ranges.begin(), ranges.end(), source.offset, [](const SourceRange& range, std::size_t at) {
            return range.offset < at;
        });
    return (next != ranges.end() && overlaps(*next)) ||
           (next != ranges.begin() && overlaps(*std::prev(next)));
}

} // namespace tts_front::detail
