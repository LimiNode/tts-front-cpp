#include "tts_front/core/normalization_support.hpp"

#include <algorithm>
#include <utility>

namespace tts_front::detail {

void WarningSink::add_range(WarningCode code,
                            std::string message,
                            std::size_t offset,
                            std::size_t length) {
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

} // namespace tts_front::detail
