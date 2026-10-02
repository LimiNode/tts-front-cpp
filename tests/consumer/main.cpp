#include <iostream>
#include <tts_front.hpp>
int main() {
    tts_front::TextFrontendOptions options;
    options.language = tts_front::Language::English;
    const auto result = tts_front::TextFrontend{}.process("There are 2 voices.", options);
    if (result.normalized_text != "There are two voices.")
        return 1;
    std::cout << result.normalized_text << "\n";
    return 0;
}
