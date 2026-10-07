#include "mapped_text.hpp"

#include <algorithm>

namespace tts_front::detail {

MappedText MappedText::from_original(std::string_view original,
                                     const std::vector<SourceRange>* preserved_ranges) {
    MappedText result;
    result.text = std::string(original);
    result.preserved_ranges = preserved_ranges;
    if (!original.empty())
        result.runs.push_back({0, original.size(), {0, original.size()}, true});
    return result;
}

SourceRange MappedText::source_range(std::size_t begin, std::size_t end) const {
    SourceRange result;
    bool has_source = false;
    auto run = std::lower_bound(
        runs.begin(), runs.end(), begin, [](const MappedRun& candidate, std::size_t position) {
            return candidate.output_end <= position;
        });
    for (; run != runs.end() && run->output_begin < end; ++run) {
        const auto overlap_begin = std::max(begin, run->output_begin);
        const auto overlap_end = std::min(end, run->output_end);
        if (overlap_begin >= overlap_end)
            continue;
        SourceRange source = run->source;
        if (run->direct_copy) {
            source.offset += overlap_begin - run->output_begin;
            source.length = overlap_end - overlap_begin;
        }
        if (!has_source) {
            result = source;
            has_source = true;
        } else {
            const auto source_end =
                std::max(result.offset + result.length, source.offset + source.length);
            result.offset = std::min(result.offset, source.offset);
            result.length = source_end - result.offset;
        }
    }
    return result;
}

void MappedText::append_copy(const MappedText& source, std::size_t begin, std::size_t end) {
    auto run = std::lower_bound(source.runs.begin(),
                                source.runs.end(),
                                begin,
                                [](const MappedRun& candidate, std::size_t position) {
                                    return candidate.output_end <= position;
                                });
    for (; run != source.runs.end() && run->output_begin < end; ++run) {
        const auto overlap_begin = std::max(begin, run->output_begin);
        const auto overlap_end = std::min(end, run->output_end);
        if (overlap_begin >= overlap_end)
            continue;
        SourceRange origin = run->source;
        if (run->direct_copy) {
            origin.offset += overlap_begin - run->output_begin;
            origin.length = overlap_end - overlap_begin;
        }
        append(source.text.substr(overlap_begin, overlap_end - overlap_begin),
               origin,
               run->direct_copy);
    }
}

void MappedText::append_copy_with_cursor(const MappedText& source,
                                         std::size_t begin,
                                         std::size_t end,
                                         std::size_t& run_cursor) {
    if (begin >= end)
        return;
    if (run_cursor > source.runs.size() ||
        (run_cursor != 0 && source.runs[run_cursor - 1].output_end > begin)) {
        run_cursor = static_cast<std::size_t>(
            std::lower_bound(source.runs.begin(),
                             source.runs.end(),
                             begin,
                             [](const MappedRun& candidate, std::size_t position) {
                                 return candidate.output_end <= position;
                             }) -
            source.runs.begin());
    }
    while (run_cursor < source.runs.size() && source.runs[run_cursor].output_end <= begin)
        ++run_cursor;
    for (std::size_t index = run_cursor;
         index < source.runs.size() && source.runs[index].output_begin < end;
         ++index) {
        const auto& run = source.runs[index];
        const auto overlap_begin = std::max(begin, run.output_begin);
        const auto overlap_end = std::min(end, run.output_end);
        if (overlap_begin >= overlap_end)
            continue;
        SourceRange origin = run.source;
        if (run.direct_copy) {
            origin.offset += overlap_begin - run.output_begin;
            origin.length = overlap_end - overlap_begin;
        }
        append(source.text.substr(overlap_begin, overlap_end - overlap_begin),
               origin,
               run.direct_copy);
        run_cursor = run.output_end <= end ? index + 1 : index;
    }
}

void MappedText::append_generated(const MappedText& source,
                                  std::size_t begin,
                                  std::size_t end,
                                  std::string_view replacement) {
    append(replacement, source.source_range(begin, end), false);
}

void MappedText::append(std::string_view value, SourceRange source, bool direct_copy) {
    if (value.empty())
        return;
    const auto output_begin = text.size();
    text += value;
    const auto output_end = text.size();
    if (direct_copy && !runs.empty() && runs.back().direct_copy &&
        runs.back().output_end == output_begin &&
        runs.back().source.offset + runs.back().source.length == source.offset) {
        runs.back().output_end = output_end;
        runs.back().source.length += source.length;
        return;
    }
    runs.push_back({output_begin, output_end, source, direct_copy});
}

} // namespace tts_front::detail
