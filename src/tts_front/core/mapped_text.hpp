#pragma once

#include "source_span.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct MappedRun {
    std::size_t output_begin = 0;
    std::size_t output_end = 0;
    SourceRange source;
    bool direct_copy = false;
};

struct MappedText {
    std::string text;
    std::vector<MappedRun> runs;
    const std::vector<SourceRange>* preserved_ranges = nullptr;

    static MappedText from_original(std::string_view original,
                                    const std::vector<SourceRange>* preserved_ranges);
    SourceRange source_range(std::size_t begin, std::size_t end) const;
    void append_copy(const MappedText& source, std::size_t begin, std::size_t end);
    void append_generated(const MappedText& source,
                          std::size_t begin,
                          std::size_t end,
                          std::string_view replacement);

  private:
    void append(std::string_view value, SourceRange source, bool direct_copy);
};

} // namespace tts_front::detail
