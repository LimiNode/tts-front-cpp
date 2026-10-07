#pragma once

#include "tts_front.hpp"

#include <string_view>

namespace tts_front::detail {

Language detect_language(std::string_view text, bool& has_cyrillic, bool& has_latin);

} // namespace tts_front::detail
