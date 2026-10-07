#pragma once

#include "tts_front.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct TechnicalRangeIndex {
    std::vector<SourceRange> ranges;
};

using MixedCandidateFormatter = std::optional<std::string> (*)(std::string_view);

struct MixedLanguageRules {
    const std::regex& candidate;
    const std::regex& malformed;
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
