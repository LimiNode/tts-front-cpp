#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace tts_front::detail {

/// Hash a runtime asset without loading the complete file into memory.
std::string sha256_file_hex(const std::filesystem::path& path,
                            std::size_t chunk_size = 1024 * 1024);

} // namespace tts_front::detail
