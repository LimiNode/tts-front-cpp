#pragma once

#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

namespace tts_front::detail::russian {

MappedText normalize(MappedText text, WarningSink& warnings);

} // namespace tts_front::detail::russian
