#pragma once

#include "tts_front/core/mapped_text.hpp"

namespace tts_front::detail::english {

MappedText collapse_comma_grouped_numbers(const MappedText& input);

} // namespace tts_front::detail::english
