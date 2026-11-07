#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tts_front::detail::russian {

std::optional<std::string> technical_pronunciation(std::string_view token);

} // namespace tts_front::detail::russian
