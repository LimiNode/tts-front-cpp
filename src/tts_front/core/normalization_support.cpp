#include "tts_front/core/normalization_support.hpp"

#include <utility>

namespace tts_front::detail {

WarningSink::WarningSink(std::vector<TextWarning>& warning_output, SourceRangeIndex ranges)
    : warnings(warning_output), preserved_ranges(std::move(ranges)) {
    diagnostic_keys.reserve(warnings.size());
    for (const auto& warning : warnings)
        diagnostic_keys.insert({warning.code, warning.offset, warning.length});
}

std::size_t WarningSink::DiagnosticKeyHash::operator()(const DiagnosticKey& key) const {
    const auto code = static_cast<std::size_t>(key.code);
    std::size_t hash = code + static_cast<std::size_t>(0x9e3779b9U);
    hash ^= key.offset + (hash << 6U) + (hash >> 2U);
    hash ^= key.length + (hash << 6U) + (hash >> 2U);
    return hash;
}

void WarningSink::add_range(WarningCode code,
                            std::string message,
                            std::size_t offset,
                            std::size_t length) {
    if (!diagnostic_keys.insert({code, offset, length}).second)
        return;
    warnings.push_back({code, std::move(message), offset, length});
}

void WarningSink::preserve(const SourceRange source) {
    if (source.length != 0) {
        preserved_ranges.insert(source);
    }
}

void WarningSink::add(WarningCode code, std::string message, SourceRange source) {
    preserve(source);
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
    return text.preserved_ranges->overlaps(text.source_range(begin, end));
}

} // namespace tts_front::detail
