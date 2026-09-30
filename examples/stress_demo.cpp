#include <iostream>
#include "tts_front/tts_front.hpp"
int main() {
  tts_front::PronunciationDictionary dictionary; dictionary.add_token("замок", "замок", 1);
  tts_front::TextFrontendOptions options; options.language = tts_front::Language::Russian; options.dictionary = &dictionary; options.diagnostics = true;
  const auto result = tts_front::TextFrontend{}.process("замок", options);
  for (const auto& word : result.words) std::cout << word.surface << ": " << (word.stressed_vowel ? std::to_string(*word.stressed_vowel) : "unknown") << "\n";
}
