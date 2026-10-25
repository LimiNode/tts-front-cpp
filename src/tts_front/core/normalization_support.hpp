#pragma once

#include "tts_front.hpp"
#include "tts_front/core/candidate_types.hpp"
#include "tts_front/core/edit_script.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/source_range_index.hpp"
#include "tts_front/core/source_span.hpp"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace tts_front::detail {

struct WarningSink {
    struct DiagnosticKey {
        WarningCode code;
        std::size_t offset;
        std::size_t length;

        bool operator==(const DiagnosticKey& other) const {
            return code == other.code && offset == other.offset && length == other.length;
        }
    };

    struct DiagnosticKeyHash {
        std::size_t operator()(const DiagnosticKey& key) const;
    };

    std::vector<TextWarning>& warnings;
    SourceRangeIndex preserved_ranges;
    std::unordered_set<DiagnosticKey, DiagnosticKeyHash> diagnostic_keys;

    explicit WarningSink(std::vector<TextWarning>& warning_output, SourceRangeIndex ranges = {});

    void preserve(SourceRange source);
    void add_range(WarningCode code, std::string message, std::size_t offset, std::size_t length);
    void add(WarningCode code, std::string message, SourceRange source);
};

struct ProtectedSpan {
    std::string marker;
    std::string value;
};

std::string marker_for(std::string_view text, std::size_t index);
void add_protected_candidate(const MappedText& text,
                             const NumericCandidate& candidate,
                             WarningSink& warnings,
                             std::vector<ProtectedSpan>& protected_spans,
                             std::vector<SourceEdit>& edits);

bool overlaps_preserved_range(const MappedText& text, std::size_t begin, std::size_t end);

} // namespace tts_front::detail
