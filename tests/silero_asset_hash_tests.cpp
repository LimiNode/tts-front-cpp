#include "detail/silero_asset_hash.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__   \
                      << "\n";                                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)

int main() {
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto path = temp_dir / "tts_front_sha256_asset_test.bin";
    const auto missing_path = temp_dir / "tts_front_sha256_asset_missing.bin";

    // Empty input exercises SHA-256 padding without any message blocks.
    { std::ofstream output(path, std::ios::binary | std::ios::trunc); }
    CHECK(tts_front::detail::sha256_file_hex(path, 1) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // This message crosses both the SHA-256 64-byte block boundary and all
    // file-read chunk boundaries exercised below.
    const std::string long_message = "The quick brown fox jumps over the lazy dog. "
                                     "The quick brown fox jumps over the lazy dog. "
                                     "The quick brown fox jumps over the lazy dog. ";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << long_message;
    }
    constexpr auto expected_long_digest =
        "dc985401a68faff03051c78bbf32bb2fd27ba216b0dba19b050b936d534b8ba9";
    for (const auto chunk_size : {std::size_t{1},
                                  std::size_t{7},
                                  std::size_t{64},
                                  std::size_t{65},
                                  std::size_t{1U << 20}}) {
        CHECK(tts_front::detail::sha256_file_hex(path, chunk_size) == expected_long_digest);
    }

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

    bool rejected_missing = false;
    try {
        (void)tts_front::detail::sha256_file_hex(missing_path, 64);
    } catch (const std::runtime_error&) {
        rejected_missing = true;
    }
    CHECK(rejected_missing);

    std::filesystem::remove(path);
    std::filesystem::remove(missing_path);
    return 0;
}
