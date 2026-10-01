#include "tts_front/tts_front.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace tts_front;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__   \
                      << "\n";                                                                     \
            return EXIT_FAILURE;                                                                   \
        }                                                                                          \
    } while (false)

int main() {
    TextFrontend frontend;
    TextFrontendOptions ru;
    ru.language = Language::Russian;
    CHECK(frontend.process("У меня 42 яблока.", ru).normalized_text == "У меня сорок два яблока.");
    CHECK(frontend.process("1% 2% 5% 11% 21%", ru).normalized_text ==
          "один процент два процента пять процентов одиннадцать процентов двадцать один процент");
    CHECK(frontend.process("1:01 2:02 5:05", ru).normalized_text ==
          "один час одна минута два часа две минуты пять часов пять минут");
    CHECK(frontend.process("1 руб. 2 руб. 5 руб.", ru).normalized_text ==
          "один рубль два рубля пять рублей");
    CHECK(frontend.process("7,5 12 500 -42", ru).normalized_text ==
          "семь целых пять десятых двенадцать тысяч пятьсот минус сорок два");
    CHECK(frontend.process("В 2026 г.", ru).normalized_text == "В две тысячи двадцать шестом году");
    CHECK(frontend.process("2 мм 2 м 2 метра", ru).normalized_text ==
          "два миллиметра два метра два метра");
    CHECK(frontend.process("И т.д. и т.п.", ru).normalized_text == "И так далее и тому подобное");
    CHECK(frontend.process("5 руб. 5 рублей", ru).normalized_text == "пять рублей пять рублей");
    CHECK(frontend.process("21:21 22:22", ru).normalized_text ==
          "двадцать один час двадцать одна минута двадцать два часа двадцать две минуты");
    CHECK(frontend.process("21000 22000 11000", ru).normalized_text ==
          "двадцать одна тысяча двадцать две тысячи одиннадцать тысяч");
    CHECK(frontend.process("В 1999 г. В 2000 г. В 2012 г. В 2037 г.", ru).normalized_text ==
          "В тысяча девятьсот девяносто девятом году В двухтысячном году В две тысячи двенадцатом "
          "году В две тысячи тридцать седьмом году");
    CHECK(frontend.process("01.02.2026", ru).normalized_text ==
          "первое февраля две тысячи двадцать шестого года");
    CHECK(frontend.process("1,1 2,2 5,1", ru).normalized_text ==
          "одна целая одна десятая две целых две десятых пять целых одна десятая");
    TextFrontendOptions en;
    en.language = Language::English;
    CHECK(frontend.process("7.5%", en).normalized_text == "seven point five percent");
    CHECK(frontend.process("$12.50 $12.05 $12.5 $1 $2", en).normalized_text ==
          "twelve dollars fifty cents twelve dollars five cents twelve dollars fifty cents one "
          "dollar two dollars");
    CHECK(
        frontend.process("RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2", en).normalized_text ==
        "RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2");
    CHECK(frontend.process("7.05 7.50", en).normalized_text ==
          "seven point zero five seven point five zero");
    CHECK(frontend.process("1:01 2:02 12:30", en).normalized_text ==
          "one hour one minute two hours two minutes twelve hours thirty minutes");
    CHECK(frontend.process("1 kg 2 km", en).normalized_text == "one kilogram two kilometers");
    CHECK(frontend.process("1 MB 2 MB 1 GB 2 GB", en).normalized_text ==
          "one megabyte two megabytes one gigabyte two gigabytes");
    CHECK(frontend.process("$12.345", en).normalized_text == "$12.345");
    CHECK(frontend.process("1 234 567", en).normalized_text ==
          "one million two hundred thirty four thousand five hundred sixty seven");
    {
        const auto invalid_time = frontend.process("24:00 99:99", en);
        CHECK(invalid_time.normalized_text == "24:00 99:99");
        CHECK(invalid_time.warnings.size() == 2);
    }
    CHECK(frontend.process("There are 12 GPUs.", en).normalized_text == "There are twelve GPUs.");

    PronunciationDictionary dictionary;
    CHECK(dictionary.add_token("замок", "замок", 1));
    CHECK(dictionary.add_case_insensitive_token("Qwen", "квен"));
    CHECK(dictionary.add_phrase("New York", "Нью-Йорк"));
    CHECK(!dictionary.add_token("замок", "замок", 1));
    CHECK(dictionary.add_case_insensitive_token("Москва", "москва"));
    TextFrontendOptions dictionary_options;
    dictionary_options.language = Language::English;
    dictionary_options.dictionary = &dictionary;
    const auto dictionary_result = frontend.process("QWEN New York", dictionary_options);
    CHECK(dictionary_result.pronunciation_text == "квен Нью-Йорк");
    CHECK(dictionary_result.words.front().from_dictionary == true);
    CHECK(dictionary_result.dictionary_replacements.size() == 2);
    CHECK(dictionary_result.words[1].dictionary_replacement.has_value());
    CHECK(dictionary_result.words[2].dictionary_replacement.has_value());
    CHECK(frontend.process("МОСКВА", dictionary_options).pronunciation_text == "москва");
    dictionary_options.language = Language::Russian;
    CHECK(frontend.process("замок", dictionary_options).words.front().stressed_vowel == 1);
    dictionary_options.stress_mode = StressMode::Disabled;
    {
        const auto disabled = frontend.process("замок", dictionary_options);
        CHECK(!disabled.words.front().stressed_vowel.has_value());
        CHECK(disabled.stress_decisions.empty());
    }
    dictionary_options.stress_mode = StressMode::DictionaryOnly;
    dictionary_options.resolve_stress = false;
    {
        const auto disabled = frontend.process("замок", dictionary_options);
        CHECK(!disabled.words.front().stressed_vowel.has_value());
        CHECK(disabled.stress_decisions.empty());
    }
    dictionary_options.resolve_stress = true;
    CHECK(frontend.process("New Yorkshire", dictionary_options).pronunciation_text ==
          "New Yorkshire");
    PronunciationDictionary overlapping_dictionary;
    CHECK(overlapping_dictionary.add_token("New", "single-token"));
    CHECK(overlapping_dictionary.add_phrase("New York", "phrase-pronunciation"));
    dictionary_options.dictionary = &overlapping_dictionary;
    const auto overlapping_result = frontend.process("New York", dictionary_options);
    CHECK(overlapping_result.pronunciation_text == "phrase-pronunciation");
    CHECK(overlapping_result.words.size() == 2);
    CHECK(overlapping_result.words[0].pronunciation == "phrase-pronunciation");
    CHECK(overlapping_result.words[1].pronunciation.empty());
    dictionary_options.dictionary = &dictionary;
    std::vector<TextWarning> dictionary_warnings;
    PronunciationDictionary loaded;
    CHECK(loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/dictionary.json",
                           &dictionary_warnings));
    CHECK(dictionary_warnings.empty());
    CHECK(loaded.find_token("QWEN") != nullptr);
    CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) +
                                "/tests/fixtures/malformed_dictionary.json",
                            &dictionary_warnings));
    CHECK(loaded.find_token("broken") == nullptr);
    CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) +
                                "/tests/fixtures/malformed_unknown.json",
                            &dictionary_warnings));
    CHECK(
        !loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/unknown_field.json",
                          &dictionary_warnings));
    CHECK(
        !loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/duplicate_key.json",
                          &dictionary_warnings));
    CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) +
                                "/tests/fixtures/case_insensitive_duplicates.json",
                            &dictionary_warnings));
    const std::string control_json =
        std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/control_char_runtime.json";
    {
        std::ofstream file(control_json, std::ios::binary);
        file << "[{\"pattern\":\"bad";
        file.put('\x01');
        file << "\",\"pronunciation\":\"ok\"}]";
    }
    CHECK(!loaded.load_file(control_json, &dictionary_warnings));
    std::remove(control_json.c_str());
    CHECK(!loaded.add_case_insensitive_token("QWEN", "other"));
    CHECK(!loaded.add_token(std::string("\xc0\xaf", 2), "bad"));

    TextFrontendOptions automatic;
    automatic.language = Language::Russian;
    automatic.stress_mode = StressMode::Automatic;
    CHECK(frontend.process("тест", automatic).warnings.size() == 1);
    automatic.resolve_stress = false;
    CHECK(frontend.process("тест", automatic).warnings.empty());
    TextFrontendOptions unsupported;
    unsupported.language = static_cast<Language>(999);
    const auto unsupported_result = frontend.process("test", unsupported);
    CHECK(unsupported_result.warnings.size() == 1);
    CHECK(unsupported_result.warnings.front().code == WarningCode::UnsupportedLanguage);
    TextFrontendOptions no_cleanup;
    no_cleanup.language = Language::English;
    no_cleanup.cleanup_spacing = false;
    CHECK(frontend.process("a  b", no_cleanup).normalized_text == "a  b");
    CHECK(frontend.process("café 😊", en).normalized_text == "café 😊");
    const auto mixed = frontend.process("Привет API", TextFrontendOptions{});
    CHECK(mixed.has_uncertainty());
    CHECK(frontend.process("__TTS_PROTECTED_A__ RTX 4090", en).normalized_text ==
          "__TTS_PROTECTED_A__ RTX 4090");
    const std::string huge(80, '9');
    CHECK(frontend.process(huge + "% $" + huge + " " + huge + ".1 " + huge + " kg", en)
              .has_uncertainty());
    CHECK(frontend.process(huge + "% " + huge + ",1 " + huge + " кг", ru).has_uncertainty());
    CHECK(frontend.process("В 2042 г.", ru).normalized_text == "В две тысячи сорок втором году");
    CHECK(frontend.process("В 5001 г.", ru).normalized_text == "В пять тысяч первом году");
    CHECK(
        frontend.process("01.02.2011 01.02.2012 01.02.2019 01.02.2020 01.02.2042 01.02.5001", ru)
            .normalized_text ==
        "первое февраля две тысячи одиннадцатого года первое февраля две тысячи двенадцатого года "
        "первое февраля две тысячи девятнадцатого года первое февраля две тысячи двадцатого года "
        "первое февраля две тысячи сорок второго года первое февраля пять тысяч первого года");
    CHECK(frontend.process("01.02.2025", ru).normalized_text ==
          "первое февраля две тысячи двадцать пятого года");
    CHECK(frontend.process("31.12.1987", ru).normalized_text ==
          "тридцать первое декабря тысяча девятьсот восемьдесят седьмого года");
    CHECK(frontend.process("0,01 0,21 1,123", ru).normalized_text ==
          "ноль целых одна сотая ноль целых двадцать одна сотая одна целая сто двадцать три "
          "тысячных");
    CHECK(frontend.process("1,1234", ru).normalized_text == "1,1234");
    CHECK(frontend.process("7,5%", ru).normalized_text == "семь целых пять десятых процента");
    {
        const auto invalid_time = frontend.process("24:00 99:99", ru);
        CHECK(invalid_time.normalized_text == "24:00 99:99");
        CHECK(invalid_time.warnings.size() == 2);
        CHECK(invalid_time.warnings[0].offset == 0);
        CHECK(invalid_time.warnings[0].length == 0);
        CHECK(invalid_time.warnings[1].offset == 0);
        CHECK(invalid_time.warnings[1].length == 0);
    }
    for (const auto& input : {std::string("1% 99:99"),
                              std::string("RTX 4090 99:99"),
                              std::string("a   99:99"),
                              std::string("1 234 99:99")}) {
        const auto transformed = frontend.process(input, en);
        for (const auto& warning : transformed.warnings) {
            CHECK(warning.offset == 0);
            CHECK(warning.length == 0);
        }
    }
    CHECK(frontend.process("99.99.2026", ru).normalized_text == "99.99.2026");
    std::string many_protected;
    for (int i = 0; i < 35; ++i) {
        if (!many_protected.empty())
            many_protected += ' ';
        many_protected += "https://example.com/item" + std::to_string(i);
    }
    CHECK(frontend.process(many_protected, en).normalized_text == many_protected);
    std::string invalid("\xc0\xaf", 2);
    const auto invalid_result = frontend.process(invalid, en);
    CHECK(invalid_result.warnings.front().code == WarningCode::InvalidUtf8);
    CHECK(invalid_result.warnings.front().offset == 0);
    CHECK(invalid_result.warnings.front().length == invalid.size());
    std::string surrogate("\xed\xa0\x80", 3);
    CHECK(frontend.process(surrogate, en).warnings.front().code == WarningCode::InvalidUtf8);
    std::string too_large("\xf4\x90\x80\x80", 4);
    CHECK(frontend.process(too_large, en).warnings.front().code == WarningCode::InvalidUtf8);
    const std::string emoji("\xf0\x9f\x98\x80", 4);
    CHECK(frontend.process(emoji, en).warnings.empty());
    std::string bad_continuation("\xe2\x28\xa1", 3);
    CHECK(frontend.process(bad_continuation, en).warnings.front().code == WarningCode::InvalidUtf8);
    std::string truncated("\xf0\x9f\x98", 3);
    CHECK(frontend.process(truncated, en).warnings.front().code == WarningCode::InvalidUtf8);
    std::cout << "tts_front tests passed\n";
    return EXIT_SUCCESS;
}
