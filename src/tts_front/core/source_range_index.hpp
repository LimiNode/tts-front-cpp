#pragma once

#include "source_span.hpp"

#include <cstddef>
#include <set>

namespace tts_front::detail {

// An insertion-friendly source-range index shared by preservation and
// diagnostic-admission paths. Ranges are ordered by source offset and length;
// overlap queries inspect only the neighboring intervals.
class SourceRangeIndex {
  public:
    void insert(SourceRange range);
    bool overlaps(SourceRange source) const;

  private:
    struct Compare {
        bool operator()(const SourceRange& left, const SourceRange& right) const {
            if (left.offset != right.offset)
                return left.offset < right.offset;
            return left.length < right.length;
        }
    };

    std::set<SourceRange, Compare> ranges_;
};

} // namespace tts_front::detail
