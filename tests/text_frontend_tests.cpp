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
  CHECK(frontend.process("2 мм 2 м 2 метра", ru).normalized_text == "два миллиметра два метра два метра");
  CHECK(frontend.process("5 руб. 5 рублей", ru).normalized_text == "пять рублей пять рублей");
  CHECK(frontend.process("21:21 22:22", ru).normalized_text == "двадцать один час двадцать одна минута двадцать два часа двадцать две минуты");
  CHECK(frontend.process("21000 22000 11000", ru).normalized_text == "двадцать одна тысяча двадцать две тысячи одиннадцать тысяч");
  CHECK(frontend.process("В 1999 г. В 2000 г. В 2012 г. В 2037 г.", ru).normalized_text == "В тысяча девятьсот девяносто девятом году В двухтысячном году В две тысячи двенадцатом году В две тысячи тридцать седьмом году");
  CHECK(frontend.process("01.02.2026", ru).normalized_text == "первое февраля две тысячи двадцать шестого года");
  CHECK(frontend.process("1,1 2,2 5,1", ru).normalized_text == "одна целая одна десятая две целых две десятых пять целых одна десятая");
  TextFrontendOptions en; en.language = Language::English;
  CHECK(frontend.process("7.5%", en).normalized_text == "seven point five percent");
  CHECK(frontend.process("$12.50 $1 $2", en).normalized_text == "twelve dollars five zero cents one dollar two dollars");
  CHECK(frontend.process("RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2", en).normalized_text == "RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2");
  CHECK(frontend.process("7.05 7.50", en).normalized_text == "seven point zero five seven point five zero");
  CHECK(frontend.process("1:01 2:02 12:30", en).normalized_text == "one hour one minute two hours two minutes twelve hours thirty minutes");
  CHECK(frontend.process("1 kg 2 km", en).normalized_text == "one kilogram two kilometers");
  CHECK(frontend.process("There are 12 GPUs.", en).normalized_text == "There are twelve GPUs.");

  PronunciationDictionary dictionary; CHECK(dictionary.add_token("замок", "замок", 1)); CHECK(dictionary.add_case_insensitive_token("Qwen", "квен")); CHECK(dictionary.add_phrase("New York", "Нью-Йорк")); CHECK(!dictionary.add_token("замок", "замок", 1)); CHECK(dictionary.add_case_insensitive_token("Москва", "москва"));
  TextFrontendOptions dictionary_options; dictionary_options.language = Language::English; dictionary_options.dictionary = &dictionary;
  const auto dictionary_result = frontend.process("QWEN New York", dictionary_options);
  CHECK(dictionary_result.pronunciation_text == "квен Нью-Йорк"); CHECK(dictionary_result.words.front().from_dictionary == true);
  CHECK(dictionary_result.dictionary_replacements.size() == 2); CHECK(dictionary_result.words[1].dictionary_replacement.has_value()); CHECK(dictionary_result.words[2].dictionary_replacement.has_value());
  CHECK(frontend.process("МОСКВА", dictionary_options).pronunciation_text == "москва");
  dictionary_options.language = Language::Russian; CHECK(frontend.process("замок", dictionary_options).words.front().stressed_vowel == 1);
  dictionary_options.stress_mode = StressMode::Disabled; CHECK(!frontend.process("замок", dictionary_options).words.front().stressed_vowel.has_value());
  dictionary_options.stress_mode = StressMode::DictionaryOnly; dictionary_options.resolve_stress = false; CHECK(!frontend.process("замок", dictionary_options).words.front().stressed_vowel.has_value());
  dictionary_options.resolve_stress = true;
  CHECK(frontend.process("New Yorkshire", dictionary_options).pronunciation_text == "New Yorkshire");
  std::vector<TextWarning> dictionary_warnings; PronunciationDictionary loaded;
  CHECK(loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/dictionary.json", &dictionary_warnings));
  CHECK(dictionary_warnings.empty()); CHECK(loaded.find_token("QWEN") != nullptr);
  CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/malformed_dictionary.json", &dictionary_warnings));
  CHECK(loaded.find_token("broken") == nullptr);
  CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/malformed_unknown.json", &dictionary_warnings));
  CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/duplicate_key.json", &dictionary_warnings));
  CHECK(!loaded.add_token(std::string("\xc0\xaf", 2), "bad"));

  TextFrontendOptions automatic; automatic.language = Language::Russian; automatic.stress_mode = StressMode::Automatic;
  CHECK(frontend.process("тест", automatic).warnings.size() == 1);
  automatic.resolve_stress = false; CHECK(frontend.process("тест", automatic).warnings.empty());
  TextFrontendOptions no_cleanup; no_cleanup.language = Language::English; no_cleanup.cleanup_unicode = false;
  CHECK(frontend.process("a  b", no_cleanup).normalized_text == "a  b");
  CHECK(frontend.process("café 😊", en).normalized_text == "café 😊");
  const auto mixed = frontend.process("Привет API", TextFrontendOptions{}); CHECK(mixed.has_uncertainty());
  CHECK(frontend.process("__TTS_PROTECTED_A__ RTX 4090", en).normalized_text == "__TTS_PROTECTED_A__ RTX 4090");
  const std::string huge(80, '9');
  CHECK(frontend.process(huge + "% $" + huge + " " + huge + ".1 " + huge + " kg", en).has_uncertainty());
  CHECK(frontend.process(huge + "% " + huge + ",1 " + huge + " кг", ru).has_uncertainty());
  std::string invalid("\xc0\xaf", 2); CHECK(frontend.process(invalid, en).warnings.front().code == WarningCode::InvalidUtf8);
  std::string surrogate("\xed\xa0\x80", 3); CHECK(frontend.process(surrogate, en).warnings.front().code == WarningCode::InvalidUtf8);
  std::string too_large("\xf4\x90\x80\x80", 4); CHECK(frontend.process(too_large, en).warnings.front().code == WarningCode::InvalidUtf8);
  std::cout << "tts_front tests passed\n"; return EXIT_SUCCESS;
}
