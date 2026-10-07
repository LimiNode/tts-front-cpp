#pragma once

#include <regex>

namespace tts_front::detail {

namespace english {
struct Patterns;
}
namespace russian {
struct Patterns;
}
namespace technical {
struct Patterns;
}

const technical::Patterns& technical_patterns();
const english::Patterns& english_patterns();
const russian::Patterns& russian_patterns();
const std::regex& grouped_number_pattern();

} // namespace tts_front::detail
