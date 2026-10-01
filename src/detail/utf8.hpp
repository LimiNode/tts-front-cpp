#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct Utf8CodePoint {
    std::uint32_t value = 0;
    std::size_t offset = 0;
    std::size_t length = 0;
};

std::size_t utf8_sequence_length(unsigned char lead) noexcept;
bool decode_utf8(std::string_view text, std::vector<Utf8CodePoint>& output);
bool is_valid_utf8(std::string_view text);

} // namespace tts_front::detail
