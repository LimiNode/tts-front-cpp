#pragma once

#include "tts_front.hpp"
#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/source_span.hpp"
#include "tts_front/normalization/candidate_scanner.hpp"

#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct WarningSink {
    std::vector<TextWarning>& warnings;
    std::vector<SourceRange> preserved_ranges;

    void add_range(WarningCode code, std::string message, std::size_t offset, std::size_t length);
    void add(WarningCode code, std::string message, SourceRange source);
};

struct ProtectedSpan {
    std::string marker;
    std::string value;
};

using SurfaceClassifier = void (*)(std::string_view, bool&, bool&);

struct AdmissionRules {
    const std::regex* comma_grouped_value = nullptr;
    const std::regex* comma_grouped_percent = nullptr;
    bool comma_grouping = false;
    bool dotted_dates = false;
    SurfaceClassifier classify_surface = nullptr;
};

std::string marker_for(std::string_view text, std::size_t index);
void add_protected_candidate(const MappedText& text,
                             const NumericCandidate& candidate,
                             WarningSink& warnings,
                             std::vector<ProtectedSpan>& protected_spans,
                             std::vector<SourceEdit>& edits);

bool numeric_match_has_valid_boundaries(const std::vector<Utf8CodePoint>& points,
                                        std::size_t begin,
                                        std::size_t end);
bool try_parse_long(std::string_view token, long long& value);
bool valid_date(int day, int month, int year);

MappedText protect_malformed_numeric_candidates(MappedText text,
                                                WarningSink& warnings,
                                                std::vector<ProtectedSpan>& protected_spans,
                                                const AdmissionRules& rules);
MappedText collapse_grouped_numbers(const MappedText& input);
} // namespace tts_front::detail
