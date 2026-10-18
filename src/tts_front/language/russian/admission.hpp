#pragma once

#include "tts_front/core/candidate_types.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/normalization/admission.hpp"

#include <optional>
#include <string_view>

namespace tts_front::detail::russian {

const AdmissionRules& admission_rules();

struct GenitiveSurfaceAdmission {
    std::size_t end = 0;
    CandidateSurfaceKind kind = CandidateSurfaceKind::Contextual;
};

GenitiveSurfaceAdmission
classify_genitive_surface(std::string_view text, std::size_t begin, bool malformed_combining);

// Language-owned boundary for decomposed known suffix surfaces such as `м́`.
// The returned range is a complete source byte span, including continuations.
std::optional<SourceRange> scan_combining_suffix(const Utf8Document& document, std::size_t begin);

MappedText protect_combining_suffix_candidates(MappedText text,
                                               WarningSink& warnings,
                                               std::vector<ProtectedSpan>& protected_spans);

} // namespace tts_front::detail::russian
