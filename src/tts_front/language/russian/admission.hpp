#pragma once

#include "tts_front/core/candidate_types.hpp"
#include "tts_front/normalization/admission.hpp"

#include <string_view>

namespace tts_front::detail::russian {

const AdmissionRules& admission_rules();

struct GenitiveSurfaceAdmission {
    std::size_t end = 0;
    CandidateSurfaceKind kind = CandidateSurfaceKind::Contextual;
};

GenitiveSurfaceAdmission
classify_genitive_surface(std::string_view text, std::size_t begin, bool malformed_combining);

} // namespace tts_front::detail::russian
