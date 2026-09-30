#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include "tts_front/tts_front.hpp"

using namespace tts_front;
#define CHECK(condition) do { if (!(condition)) { std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << "\n"; return EXIT_FAILURE; } } while (false)

int main() {
  TextFrontend frontend; TextFrontendOptions ru; ru.language = Language::Russian;
  CHECK(frontend.process("У меня 42 яблока.", ru).normalized_text == "У меня сорок два яблока.");
  CHECK(frontend.process("1% 2% 5% 11% 21%", ru).normalized_text == "один процент два процента пять процентов одиннадцать процентов двадцать один процент");
  CHECK(frontend.process("1:01 2:02 5:05", ru).normalized_text == "один час одна минута два часа две минуты пять часов пять минут");
  CHECK(frontend.process("1 руб. 2 руб. 5 руб.", ru).normalized_text == "один рубль два рубля пять рублей");
  CHECK(frontend.process("7,5 12 500 -42", ru).normalized_text == "семь целых пять десятых двенадцать тысяч пятьсот минус сорок два");
  CHECK(frontend.process("В 2026 г.", ru).normalized_text == "В две тысячи двадцать шестом году");
  TextFrontendOptions en; en.language = Language::English;
  CHECK(frontend.process("7.5%", en).normalized_text == "seven point five percent");
  CHECK(frontend.process("$12.50 $1 $2", en).normalized_text == "twelve dollars fifty cents one dollar two dollars");
  CHECK(frontend.process("RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2", en).normalized_text == "RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2");
  CHECK(frontend.process("There are 12 GPUs.", en).normalized_text == "There are twelve GPUs.");

  PronunciationDictionary dictionary; dictionary.add_token("замок", "замок", 1); dictionary.add_case_insensitive_token("Qwen", "квен"); dictionary.add_phrase("New York", "Нью-Йорк");
  TextFrontendOptions dictionary_options; dictionary_options.language = Language::English; dictionary_options.dictionary = &dictionary;
  const auto dictionary_result = frontend.process("QWEN New York", dictionary_options);
  CHECK(dictionary_result.pronunciation_text == "квен Нью-Йорк"); CHECK(dictionary_result.words.front().from_dictionary == true);
  dictionary_options.language = Language::Russian; CHECK(frontend.process("замок", dictionary_options).words.front().stressed_vowel == 1);
  CHECK(frontend.process("New Yorkshire", dictionary_options).pronunciation_text == "New Yorkshire");
  std::vector<TextWarning> dictionary_warnings; PronunciationDictionary loaded;
  CHECK(loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/dictionary.json", &dictionary_warnings));
  CHECK(dictionary_warnings.empty()); CHECK(loaded.find_token("QWEN") != nullptr);
  CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/malformed_dictionary.json", &dictionary_warnings));
  CHECK(loaded.find_token("broken") == nullptr);

  TextFrontendOptions automatic; automatic.language = Language::Russian; automatic.stress_mode = StressMode::Automatic;
  CHECK(frontend.process("тест", automatic).warnings.size() == 1);
  TextFrontendOptions no_cleanup; no_cleanup.language = Language::English; no_cleanup.cleanup_unicode = false;
  CHECK(frontend.process("a  b", no_cleanup).normalized_text == "a  b");
  CHECK(frontend.process("café 😊", en).normalized_text == "café 😊");
  const auto mixed = frontend.process("Привет API", TextFrontendOptions{}); CHECK(mixed.has_uncertainty());
  std::string invalid("\xc0\xaf", 2); CHECK(frontend.process(invalid, en).warnings.front().code == WarningCode::InvalidUtf8);
  std::string surrogate("\xed\xa0\x80", 3); CHECK(frontend.process(surrogate, en).warnings.front().code == WarningCode::InvalidUtf8);
  std::string too_large("\xf4\x90\x80\x80", 4); CHECK(frontend.process(too_large, en).warnings.front().code == WarningCode::InvalidUtf8);
  std::cout << "tts_front tests passed\n"; return EXIT_SUCCESS;
}
