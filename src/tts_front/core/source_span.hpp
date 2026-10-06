#pragma once

#include <cstddef>
#include <string>

namespace tts_front::detail {

struct SourceRange {
    std::size_t offset = 0;
    std::size_t length = 0;
};

struct SourceSpan {
    std::size_t byte_begin = 0;
    std::size_t byte_end = 0;
    std::size_t codepoint_begin = 0;
    std::size_t codepoint_end = 0;
};

struct SourceEdit {
    std::size_t begin = 0;
    std::size_t end = 0;
    std::string replacement;
};

} // namespace tts_front::detail
