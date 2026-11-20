#pragma once

#include "tts_front/core/mapped_text.hpp"
#include "tts_front/normalization/admission.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace tts_front::detail::russian {

// Numeral-only rendering for an already agreed, scanner-bounded local noun.
std::optional<std::string> bounded_noun_number(std::string_view token,
                                               std::string_view noun,
                                               const MappedText& context,
                                               std::size_t begin);

MappedText normalize(MappedText text, WarningSink& warnings);

} // namespace tts_front::detail::russian
