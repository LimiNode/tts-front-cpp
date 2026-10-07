#pragma once

#include "tts_front.hpp"
#include "tts_front/core/candidate_types.hpp"
#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/source_span.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct WarningSink {
    std::vector<TextWarning>& warnings;
    std::vector<SourceRange> preserved_ranges;

    void add_range(WarningCode code, std::string message, std::size_t offset, std::size_t length);
    void add(WarningCode code, std::string message, SourceRange source);
};

struct ProtectedSpan {
    std::string marker;
    std::string value;
};

std::string marker_for(std::string_view text, std::size_t index);
void add_protected_candidate(const MappedText& text,
                             const NumericCandidate& candidate,
                             WarningSink& warnings,
                             std::vector<ProtectedSpan>& protected_spans,
                             std::vector<SourceEdit>& edits);

} // namespace tts_front::detail
