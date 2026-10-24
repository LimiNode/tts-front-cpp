#pragma once

#include "tts_front/core/normalization_support.hpp"

#include <string_view>
#include <vector>

namespace tts_front::detail::technical {

MappedText protect(MappedText text, std::vector<ProtectedSpan>& protected_spans);
MappedText restore(MappedText text, const std::vector<ProtectedSpan>& protected_spans);
MappedText protect_numeric_candidates(MappedText text,
                                      WarningSink& warnings,
                                      std::vector<ProtectedSpan>& protected_spans);

// Return merged ranges that technical shielding will protect. Language
// admission may use this preflight index to avoid diagnosing numeric-looking
// substrings inside URLs, e-mails, and identifiers.
std::vector<SourceRange> scan_protected_ranges(std::string_view text);

} // namespace tts_front::detail::technical
