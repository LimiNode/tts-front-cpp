#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct TokenSpan {
    std::size_t begin = 0;
    std::size_t end = 0;
};

// Opt-in combining marks extend source spans; legacy token boundaries stay unchanged.
std::vector<TokenSpan> token_spans(std::string_view text, bool include_combining_marks = false);

} // namespace tts_front::detail
