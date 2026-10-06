#pragma once

#include "source_span.hpp"
#include "utf8.hpp"

#include <string_view>
#include <vector>

namespace tts_front::detail {

struct Utf8Document {
    std::string_view bytes;
    std::vector<Utf8CodePoint> points;
    bool valid = false;

    explicit Utf8Document(std::string_view input);
    SourceSpan span_from_bytes(std::size_t begin, std::size_t end) const;
    SourceSpan span_from_codepoints(std::size_t begin, std::size_t end) const;
};

std::size_t codepoint_index_at_or_after(const Utf8Document& document, std::size_t byte_offset);

} // namespace tts_front::detail
