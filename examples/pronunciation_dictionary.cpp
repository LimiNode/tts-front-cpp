#include "tts_front/tts_front.hpp"

#include <iostream>
int main() {
    tts_front::PronunciationDictionary dictionary;
    dictionary.add_case_insensitive_token("Qwen", "квен");
    dictionary.add_token("замок", "замок", 1);
    tts_front::TextFrontendOptions options;
    options.language = tts_front::Language::Russian;
    options.dictionary = &dictionary;
    const auto result = tts_front::TextFrontend{}.process("Qwen и замок", options);
    std::cout << result.pronunciation_text << "\n";
    return 0;
}
