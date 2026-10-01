#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <tts_front/pronunciation_dictionary.hpp>
#include <tts_front/text_frontend.hpp>
#include <tts_front/tts_front.hpp>

int main() {
    const std::array<const char*, 3> headers = {"include/tts_front/pronunciation_dictionary.hpp",
                                                "include/tts_front/text_frontend.hpp",
                                                "include/tts_front/tts_front.hpp"};
    for (const char* relative_path : headers) {
        std::ifstream file(std::string(TTS_FRONT_SOURCE_DIR) + "/" + relative_path,
                           std::ios::binary);
        if (!file) {
            std::cerr << "Unable to read public header: " << relative_path << '\n';
            return 1;
        }
        char byte = 0;
        while (file.get(byte)) {
            const auto value = static_cast<unsigned char>(byte);
            if (value < 0x20 && value != '\t' && value != '\n' && value != '\r') {
                std::cerr << "Control character in public header: " << relative_path << '\n';
                return 1;
            }
        }
    }
    return 0;
}
