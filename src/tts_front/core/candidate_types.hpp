#pragma once

#include "tts_front/core/source_span.hpp"
#include "tts_front/core/utf8_document.hpp"

#include <vector>

namespace tts_front::detail {

enum class NumericCandidateKind { Technical, Numeric, Grouped, Currency };

enum class CandidateSurfaceKind { Numeric, Percent, Currency, Unit, Date, Contextual, Technical };

struct NumericCandidate {
    SourceSpan span;
    NumericCandidateKind kind = NumericCandidateKind::Numeric;
    bool malformed = false;
    bool embedded = false;
    bool has_percent = false;
};

enum class NumericSign { None, Plus, Minus, UnicodeMinus };
enum class ExponentState { None, Complete, Incomplete, InvalidContinuation };

struct NumericSurface {
    std::size_t digits_begin = 0;
    std::size_t digits_end = 0;
    std::size_t end = 0;
    std::size_t continuation_end = 0;
    NumericSign sign = NumericSign::None;
    ExponentState exponent = ExponentState::None;
    bool malformed_combining = false;

    bool valid() const {
        return digits_end > digits_begin;
    }
};

// The scanner's typed result.  Admission code consumes this single surface
// instead of independently recomputing the numeric core and its continuation
// boundary in each domain.
struct CandidateSurface {
    CandidateSurfaceKind kind = CandidateSurfaceKind::Numeric;
    std::size_t begin = 0;
    std::size_t core_end = 0;
    std::size_t end = 0;
    NumericSurface numeric;
    bool has_combining = false;

    bool valid() const {
        return numeric.valid();
    }
};

enum class AdmissionOutcome { None, Attached, Spaced, Punctuated, Compound };

// The shared admission endpoint returns a typed source boundary and records
// whether admission extended the supplied core through an attached, spaced,
// punctuated, or compound continuation. Callers must decide whether to format
// or preserve the whole [begin, end) surface; they must not reconstruct tails.
struct AdmissionResult {
    CandidateSurfaceKind kind = CandidateSurfaceKind::Numeric;
    std::size_t begin = 0;
    // Boundary of the surface supplied to scan_admission. For a
    // CandidateSurface this is candidate.end; for a language-owned typed core
    // it is the caller-provided continuation begin.
    std::size_t base_end = 0;
    std::size_t end = 0;
    AdmissionOutcome outcome = AdmissionOutcome::None;

    bool extended() const {
        return end > base_end;
    }
};

struct NumericSurfaceRange {
    std::size_t surface_begin = 0;
    std::size_t continuation_begin = 0;
    std::size_t end = 0;
};

struct NumericSurfaceIndex {
    std::vector<NumericSurfaceRange> ranges;

    bool contains(std::size_t point) const;
    bool overlaps(std::size_t begin, std::size_t end) const;
};

NumericSurface scan_numeric_surface(const Utf8Document& document, std::size_t begin);

} // namespace tts_front::detail
