#include "detail/silero_asset_hash.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__  \
                      << "\n";                                                                    \
            return 1;                                                                               \
        }                                                                                           \
    } while (false)

int main() {
    const auto path = std::filesystem::temp_directory_path() / "tts_front_sha256_asset_test.bin";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "abc";
    }
    CHECK(tts_front::detail::sha256_file_hex(path, 1) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(tts_front::detail::sha256_file_hex(path, 64) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    bool rejected_chunk = false;
    try {
        (void)tts_front::detail::sha256_file_hex(path, 0);
    } catch (const std::invalid_argument&) {
        rejected_chunk = true;
    }
    CHECK(rejected_chunk);
    std::filesystem::remove(path);
    return 0;
}
