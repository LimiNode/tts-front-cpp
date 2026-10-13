#pragma once

#include "tts_front/core/candidate_types.hpp"
#include "tts_front/core/utf8_document.hpp"

namespace tts_front::detail {

NumericSurfaceIndex build_numeric_surface_index(const Utf8Document& document);

CandidateSurface scan_candidate_surface(const Utf8Document& document,
                                        std::size_t begin,
                                        CandidateSurfaceKind kind = CandidateSurfaceKind::Numeric);

std::size_t scan_candidate_continuation(const Utf8Document& document,
                                        std::size_t begin,
                                        CandidateSurfaceKind kind);

// Extends a numeric candidate across a connector separated from the next
// numeric token by horizontal whitespace.  This is deliberately limited to
// compound connectors; comma, dot and colon remain list/terminal boundaries.
std::size_t scan_spaced_numeric_continuation(const Utf8Document& document,
                                             std::size_t begin,
                                             CandidateSurfaceKind kind);

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix);

} // namespace tts_front::detail
