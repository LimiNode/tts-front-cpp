#pragma once

#include "tts_front/core/source_span.hpp"
#include "tts_front/core/utf8_document.hpp"

namespace tts_front::detail {

enum class NumericCandidateKind { Technical, Numeric, Grouped, Currency };

struct NumericCandidate {
    SourceSpan span;
    NumericCandidateKind kind = NumericCandidateKind::Numeric;
    bool malformed = false;
    bool embedded = false;
    bool has_percent = false;
};

enum class NumericSign { None, Plus, Minus, UnicodeMinus };

struct NumericSurface {
    std::size_t digits_begin = 0;
    std::size_t digits_end = 0;
    std::size_t end = 0;
    NumericSign sign = NumericSign::None;
    bool exponent_marker = false;

    bool valid() const {
        return digits_end > digits_begin;
    }
};

NumericSurface scan_numeric_surface(const Utf8Document& document, std::size_t begin);

} // namespace tts_front::detail
