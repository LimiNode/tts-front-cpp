#pragma once

#include "tts_front/core/normalization_support.hpp"
#include "tts_front/core/utf8.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

using SurfaceClassifier =
    void (*)(std::string_view, const Utf8Document&, std::size_t, bool&, bool&);

enum class CandidateValidity {
    NotApplicable,
    SupportedEnglishGrouping,
    MalformedEnglishGrouping,
    SupportedRussianDate,
};

using CandidateValidityClassifier = CandidateValidity (*)(std::string_view, std::size_t, bool);

struct AdmissionRules {
    bool dotted_dates = false;
    SurfaceClassifier classify_surface = nullptr;
    CandidateValidityClassifier classify_candidate = nullptr;
};

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
