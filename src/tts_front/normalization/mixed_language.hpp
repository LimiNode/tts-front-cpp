#pragma once

#include "tts_front.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct TechnicalRangeIndex {
    std::vector<SourceRange> ranges;
};

struct MixedCandidateMatch {
    std::size_t begin = 0;
    std::size_t end = 0;
    std::string_view value;
};

using MixedCandidateScanner = std::vector<MixedCandidateMatch> (*)(std::string_view);
using MixedCandidateFormatter = std::optional<std::string> (*)(std::string_view);

struct MixedLanguageRules {
    MixedCandidateScanner scan_candidates;
    MixedCandidateScanner scan_malformed;
    MixedCandidateFormatter formatter;
};

TechnicalRangeIndex build_technical_range_index(std::string_view text);

/// Detect the dominant language while excluding recognized technical spans.
Language detect_mixed_language(std::string_view text,
                               bool& has_cyrillic,
                               bool& has_latin,
                               const TechnicalRangeIndex& technical_index);

/// Normalize explicitly marked foreign-language number/unit candidates while
/// leaving the surrounding sentence in its dominant language.
MappedText normalize_mixed_candidates(MappedText text,
                                      WarningSink& warnings,
                                      const MixedLanguageRules& dominant,
                                      const MixedLanguageRules& foreign,
                                      const TechnicalRangeIndex& technical_index);

} // namespace tts_front::detail
