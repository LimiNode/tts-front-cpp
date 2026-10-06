#pragma once

#include "tts_front/core/source_span.hpp"

namespace tts_front::detail {

enum class NumericCandidateKind { Technical, Numeric, Grouped, Currency };

struct NumericCandidate {
    SourceSpan span;
    NumericCandidateKind kind = NumericCandidateKind::Numeric;
    bool malformed = false;
    bool embedded = false;
    bool has_percent = false;
};

} // namespace tts_front::detail
