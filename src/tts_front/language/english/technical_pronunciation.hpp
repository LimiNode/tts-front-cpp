#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tts_front::detail::english {

std::optional<std::string> technical_pronunciation(std::string_view token);

} // namespace tts_front::detail::english
