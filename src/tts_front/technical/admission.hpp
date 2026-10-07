#pragma once

#include "tts_front/core/normalization_support.hpp"

#include <vector>

namespace tts_front::detail::technical {

MappedText protect(MappedText text, std::vector<ProtectedSpan>& protected_spans);
MappedText restore(MappedText text, const std::vector<ProtectedSpan>& protected_spans);
MappedText protect_numeric_candidates(MappedText text,
                                      WarningSink& warnings,
                                      std::vector<ProtectedSpan>& protected_spans);

} // namespace tts_front::detail::technical
