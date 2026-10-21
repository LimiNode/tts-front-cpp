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

// Returns the codepoint at the beginning of a currency prefix associated with
// the numeric surface at `numeric_begin`, or `numeric_begin` when none exists.
// Both sign placements (`-$1`, `$-1`) and intervening horizontal space are
// admitted. The backward scan is bounded by the immediately preceding prefix,
// so disjoint candidates remain linear over a document.
std::size_t scan_currency_prefix_begin(const Utf8Document& document, std::size_t numeric_begin);

// Scans the numeric component of a currency surface, including an optional
// leading sign and repeated comma/dot digit groups. Validation of grouping and
// decimal precision remains language-owned; this function supplies only the
// complete neutral boundary used before typed continuation admission.
std::size_t scan_currency_numeric_end(const Utf8Document& document, std::size_t numeric_begin);

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

// Returns the continuation boundary for a typed surface whose core was
// recognized by a language-specific matcher (for example a month-name date
// or a unit/currency regex).  The matcher supplies the first codepoint after
// the core; the shared scanner owns all attached/spaced continuation rules.
std::size_t scan_typed_admission_end(const Utf8Document& document,
                                     std::size_t begin,
                                     CandidateSurfaceKind kind);

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix);

} // namespace tts_front::detail
