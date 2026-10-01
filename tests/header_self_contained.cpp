#include <fstream>
#include <iostream>
#include <string>
#include <tts_front/tts_front.hpp>

int main() {
    constexpr const char* relative_path = "include/tts_front/tts_front.hpp";
    std::ifstream file(std::string(TTS_FRONT_SOURCE_DIR) + "/" + relative_path, std::ios::binary);
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
    return 0;
}
