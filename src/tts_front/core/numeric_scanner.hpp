#pragma once

#include "tts_front/core/candidate_types.hpp"
#include "tts_front/core/utf8_document.hpp"

namespace tts_front::detail {

NumericSurfaceIndex build_numeric_surface_index(const Utf8Document& document);

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix);

} // namespace tts_front::detail
