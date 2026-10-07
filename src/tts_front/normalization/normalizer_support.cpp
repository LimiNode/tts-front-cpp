#include "tts_front/normalization/normalizer_support.hpp"

#include <utility>

namespace tts_front::detail {

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match) {
    const auto begin = static_cast<std::size_t>(match.position());
    warnings.add(code, std::move(message), text.source_range(begin, begin + match.length()));
}

void add_warning(WarningSink& warnings,
                 WarningCode code,
                 std::string message,
                 const MappedText& text,
                 const std::smatch& match,
                 std::size_t group) {
    const auto begin = static_cast<std::size_t>(match.position(group));
    warnings.add(code, std::move(message), text.source_range(begin, begin + match.length(group)));
}

void add_warning_span(WarningSink& warnings,
                      WarningCode code,
                      std::string message,
                      const MappedText& text,
                      const std::smatch& match,
                      std::size_t begin_group,
                      std::size_t end_group) {
    const auto begin = static_cast<std::size_t>(match.position(begin_group));
    const auto end = static_cast<std::size_t>(match.position(end_group)) +
                     static_cast<std::size_t>(match.length(end_group));
    warnings.add(code, std::move(message), text.source_range(begin, end));
}

void add_warning_without_suffix(WarningSink& warnings,
                                WarningCode code,
                                std::string message,
                                const MappedText& text,
                                const std::smatch& match,
                                std::size_t suffix_group) {
    const auto begin = static_cast<std::size_t>(match.position());
    const auto length = static_cast<std::size_t>(match.length() - match.length(suffix_group));
    warnings.add(code, std::move(message), text.source_range(begin, begin + length));
}

} // namespace tts_front::detail
