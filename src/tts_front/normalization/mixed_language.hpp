#pragma once

#include "tts_front.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

namespace tts_front::detail {

/// Detect the dominant language while excluding recognized technical spans.
Language detect_mixed_language(std::string_view text, bool& has_cyrillic, bool& has_latin);

/// Normalize explicitly marked foreign-language number/unit candidates while
/// leaving the surrounding sentence in its dominant language.
MappedText
normalize_mixed_candidates(MappedText text, WarningSink& warnings, Language dominant_language);

} // namespace tts_front::detail
