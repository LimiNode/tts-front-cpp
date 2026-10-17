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

NumericSurfaceRange scan_lexical_suffix(const Utf8Document& document, std::size_t begin);

// Extends a numeric candidate across a connector separated from the next
// numeric token by horizontal whitespace.  This is deliberately limited to
// compound connectors; comma, dot and colon remain list/terminal boundaries.
std::size_t scan_spaced_numeric_continuation(const Utf8Document& document,
                                             std::size_t begin,
                                             CandidateSurfaceKind kind);

// Returns the complete admission boundary for a typed surface.  The result
// combines attached lexical suffixes, spaced connectors and a following
// numeric token in one pass so callers cannot accidentally normalize a tail
// after rejecting the numeric head.
std::size_t scan_candidate_admission_end(const Utf8Document& document,
                                         const CandidateSurface& candidate,
                                         CandidateSurfaceKind kind);

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix);

} // namespace tts_front::detail
