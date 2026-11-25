#pragma once

#include "tts_front/core/candidate_types.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/utf8_document.hpp"

#include <optional>

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

// One document per cursor; increasing candidate queries visit each left-owner
// codepoint once and classify each owner once, including excluded numeric chains.
struct SignedColonOwnerCursor {
    std::size_t point = 0;
    std::size_t owner_begin = 0;
    std::optional<std::size_t> classified_owner;
    bool numeric_owner = false;
};

// A signed numeric/currency surface attached to a colon is unsupported. Return
// its complete prefix start, including either currency/sign order, for atomic
// admission before language or mixed-mode formatting. A preceding numeric
// component keeps ownership of its colon (clock/ratio compounds are unchanged).
std::optional<std::size_t> scan_signed_colon_prefix(const Utf8Document& document,
                                                    std::size_t numeric_begin,
                                                    SignedColonOwnerCursor* cursor = nullptr);

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

// Returns the complete typed admission result for a numeric surface. The
// result combines attached lexical suffixes, spaced connectors and following
// numeric tokens and records which continuation class extended the core.
AdmissionResult scan_admission(const Utf8Document& document,
                               const CandidateSurface& candidate,
                               CandidateSurfaceKind kind);

// Returns the admission result for a language-owned typed core. The matcher
// supplies the first codepoint after the core; the shared scanner owns all
// attached/spaced continuation rules and reports the typed outcome.
AdmissionResult
scan_admission(const Utf8Document& document, std::size_t begin, CandidateSurfaceKind kind);

// Returns whether `begin` is the first codepoint of a signed or unsigned
// numeric surface. Language and mixed-mode passes use this admission endpoint
// instead of reimplementing digit/sign boundary checks.
bool is_numeric_surface_start(const Utf8Document& document, std::size_t begin);

// Narrow cardinal+noun admission. Reject attached words, unsupported signs,
// signed-token fragments, compound connector tails (even without a following
// operand), and trailing components of space-grouped numbers.
// A horizontally spaced typographic en/em dash before a word is punctuation.
// Existing generic formatting may still handle those surfaces compatibly.
bool is_independent_cardinal_candidate(const Utf8Document& document,
                                       const CandidateSurface& candidate);

// Exact numeral origins admitted in an unshielded snapshot. A later pass may
// use the index after shielding without interpreting opaque marker bytes.
struct CardinalSourceIndex {
    std::vector<SourceRange> ranges;

    bool contains(SourceRange source) const;
};

CardinalSourceIndex build_cardinal_source_index(const MappedText& text);

std::size_t scan_numeric_continuation_points(const Utf8Document& document,
                                             std::size_t start,
                                             bool consume_lexical_suffix);

} // namespace tts_front::detail
