#include "source_range_index.hpp"

#include <algorithm>
#include <iterator>

namespace tts_front::detail {

void SourceRangeIndex::insert(const SourceRange range) {
    if (range.length == 0)
        return;
    SourceRange merged = range;
    auto next = ranges_.lower_bound({range.offset, 0});
    if (next != ranges_.begin()) {
        const auto previous = std::prev(next);
        if (previous->offset + previous->length >= merged.offset) {
            const auto end =
                std::max(merged.offset + merged.length, previous->offset + previous->length);
            merged.offset = previous->offset;
            merged.length = end - merged.offset;
            next = ranges_.erase(previous);
        }
    }
    while (next != ranges_.end() && next->offset <= merged.offset + merged.length) {
        const auto end = std::max(merged.offset + merged.length, next->offset + next->length);
        merged.length = end - merged.offset;
        next = ranges_.erase(next);
    }
    ranges_.insert(next, merged);
}

bool SourceRangeIndex::overlaps(const SourceRange source) const {
    if (source.length == 0 || ranges_.empty())
        return false;
    const auto intersects = [source](const SourceRange& range) {
        return source.offset < range.offset + range.length &&
               range.offset < source.offset + source.length;
    };
    auto next = ranges_.lower_bound({source.offset, 0});
    if (next != ranges_.end() && intersects(*next))
        return true;
    if (next == ranges_.begin())
        return false;
    return intersects(*std::prev(next));
}

} // namespace tts_front::detail
