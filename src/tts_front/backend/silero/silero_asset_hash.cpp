#include "silero_asset_hash.hpp"

#include "sha256.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace tts_front::detail {

std::string sha256_file_hex(const std::filesystem::path& path, std::size_t chunk_size) {
    if (chunk_size == 0)
        throw std::invalid_argument("SHA-256 chunk size must be positive");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("unable to open runtime asset: " + path.string());
    Sha256 digest;
    std::vector<char> buffer(chunk_size);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            digest.update(reinterpret_cast<const std::uint8_t*>(buffer.data()),
                          static_cast<std::size_t>(count));
        }
    }
    if (!input.eof())
        throw std::runtime_error("unable to read runtime asset: " + path.string());
    const auto bytes = digest.finish();
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : bytes)
        output << std::setw(2) << static_cast<unsigned int>(byte);
    return output.str();
}

} // namespace tts_front::detail
