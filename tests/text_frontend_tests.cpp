#include "tts_front.hpp"

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
    {
        const std::string unsupported_numeric[] = {"1%word",
                                                   "1%2",
                                                   "1 %word",
                                                   "1 %2",
                                                   "1e+3",
                                                   "1.2e-3",
                                                   "1.2#3",
                                                   "1.2$3",
                                                   "123#abc",
                                                   "123$abc",
                                                   "1#abc",
                                                   "1$abc",
                                                   "1*2",
                                                   "1&2",
                                                   "1^2",
                                                   std::string("10") + "\xE2\x80\x93" + "20",
                                                   std::string("10") + "\xE2\x80\x94" + "20",
                                                   std::string("10") + "\xE2\x88\x92" + "20",
                                                   std::string("10 ") + "\xE2\x80\x93" + " 20",
                                                   std::string("10 ") + "\xE2\x80\x94" + " 20",
                                                   "+7 (999) 123-45-67"};
        for (const auto& input : unsupported_numeric) {
            const auto result = frontend.process(input, en);
            CHECK(result.normalized_text == input);
            CHECK(!result.warnings.empty());
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        }
    }
    CHECK(frontend.process("1 %", en).normalized_text == "one percent");
    const std::string russian_percent_suffix =
        std::string("1,2 %") + "\xD1\x81\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xBE";
    {
        const auto result = frontend.process(russian_percent_suffix, ru);
        CHECK(result.normalized_text == russian_percent_suffix);
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
    }
    CHECK(frontend.process("1,234 12,345,678", en).normalized_text ==
          "one thousand two hundred thirty four twelve million three hundred forty five thousand "
          "six hundred seventy eight");
    CHECK(frontend.process("1,234.56", en).normalized_text ==
          "one thousand two hundred thirty four point five six");
    CHECK(frontend.process("1,234% 1,234.56%", en).normalized_text ==
          "one thousand two hundred thirty four percent one thousand two hundred thirty four "
          "point five six percent");
    CHECK(frontend.process("$1,234.56", en).normalized_text ==
          "one thousand two hundred thirty four dollars fifty six cents");
    CHECK(frontend.process("abc1,234 1,23 1,2345", en).normalized_text == "abc1,234 1,23 1,2345");
    for (const auto& input : {std::string("1,23"), std::string("1,2345")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    {
        const auto malformed_group = frontend.process("1,234.56.7", en);
        CHECK(malformed_group.normalized_text == "1,234.56.7");
        CHECK(!malformed_group.warnings.empty());
        CHECK(malformed_group.warnings.front().code == WarningCode::UnresolvedNumber);
    }
    {
        const std::string oversized_group = "x 99,999,999,999,999,999,999";
        const auto result = frontend.process(oversized_group, en);
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 2);
        CHECK(result.warnings.front().length == oversized_group.size() - 2);
    }
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
    CHECK(frontend.process("тест42 тест42% тест12:34", ru).normalized_text ==
          "тест42 тест42% тест12:34");
    CHECK(frontend.process("слово1.2.2026", ru).normalized_text == "слово1.2.2026");
    CHECK(frontend.process("abc2 kg", en).normalized_text == "abc2 kg");
    const auto embedded_grouped = frontend.process("abc1 234", en);
    CHECK(embedded_grouped.normalized_text == "abc1 234");
    CHECK(!embedded_grouped.warnings.empty());
    CHECK(embedded_grouped.warnings.front().code == WarningCode::UnresolvedNumber);
    CHECK(frontend.process("тест1 234", ru).normalized_text == "тест1 234");
    const auto numeric_identifiers = frontend.process("12-34 123-456 555-1234 12/34", en);
    CHECK(numeric_identifiers.normalized_text == "12-34 123-456 555-1234 12/34");
    CHECK(!numeric_identifiers.warnings.empty());
    CHECK(numeric_identifiers.warnings.front().code == WarningCode::UnresolvedNumber);
    CHECK(frontend.process("abc1 23", en).normalized_text == "abc1 23");
    CHECK(frontend.process("abc1 234 56", en).normalized_text == "abc1 234 56");
    CHECK(frontend.process("1.2+3 1.2=3 abc/123 123+abc", en).normalized_text ==
          "1.2+3 1.2=3 abc/123 123+abc");
    CHECK(frontend.process("+7 999 123-45-67", en).normalized_text == "+7 999 123-45-67");
    CHECK(frontend.process("1,2кг", ru).normalized_text == "1,2кг");
    CHECK(frontend.process("1.2%word", en).normalized_text == "1.2%word");
    CHECK(frontend.process("01.02.2026г.", ru).normalized_text == "01.02.2026г.");
    const std::string decomposed_word = "e\xcc\x81"
                                        "42";
    CHECK(frontend.process(decomposed_word, en).normalized_text == decomposed_word);
    CHECK(frontend.process("x$12 RTX-4090 C++17 V2.1.0 #123", en).normalized_text ==
          "x$12 RTX-4090 C++17 V2.1.0 #123");
    const auto check_technical_numeric_tail = [&](const char* value, std::size_t offset) {
        const std::string input = value;
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(input, options);
            if (result.normalized_text != input || result.warnings.size() != 1 ||
                result.warnings.front().code != WarningCode::UnresolvedNumber ||
                result.warnings.front().offset != offset ||
                result.warnings.front().length != input.size() - offset)
                return false;
        }
        return true;
    };
    CHECK(check_technical_numeric_tail("x$+1,234%", 1));
    CHECK(check_technical_numeric_tail("x$-1,234%", 1));
    CHECK(check_technical_numeric_tail("x+1,234%", 1));
    CHECK(check_technical_numeric_tail("x-1,234%", 1));
    CHECK(check_technical_numeric_tail("#1,234%", 0));
    CHECK(check_technical_numeric_tail("C++17,234%", 5));
    CHECK(check_technical_numeric_tail("RTX-4090,234%", 3));
    CHECK(check_technical_numeric_tail("V2.1.0,234%", 6));
    CHECK(check_technical_numeric_tail("a C++17,234%", 7));
    CHECK(check_technical_numeric_tail("HTTP/2,234%", 6));
    CHECK(check_technical_numeric_tail("x$+1,234 %", 1));
    CHECK(check_technical_numeric_tail("C++17,23%", 5));
    CHECK(check_technical_numeric_tail("x$+1,23%", 1));
    CHECK(check_technical_numeric_tail("C++17,1234%", 5));
    CHECK(check_technical_numeric_tail("C++17,12,345%", 5));
    CHECK(check_technical_numeric_tail("C++17,23%1:02", 5));
    CHECK(check_technical_numeric_tail("x$+1,23%1:02", 1));
    CHECK(check_technical_numeric_tail("C++17,23% 456", 5));
    CHECK(check_technical_numeric_tail("C++17,23 456%", 5));
    CHECK(check_technical_numeric_tail("x$+1,23 456%", 1));
    CHECK(check_technical_numeric_tail("C++17,234..5%", 5));
    CHECK(check_technical_numeric_tail("C++17,234...5%", 5));
    CHECK(check_technical_numeric_tail("C++17,234.0..5%", 5));
    {
        std::string long_grouped = "C++17";
        for (int index = 0; index < 7800; ++index)
            long_grouped += ",123";
        long_grouped += '%';
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(long_grouped, options);
            CHECK(result.normalized_text == long_grouped);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 5);
            CHECK(result.warnings.front().length == long_grouped.size() - 5);
        }
    }
    {
        const std::string technical_url = "https://example.com/C++17,234%25";
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(technical_url, options);
            CHECK(result.normalized_text == technical_url);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == technical_url.find(','));
            CHECK(result.warnings.front().length == 7);
        }
    }
    CHECK(frontend.process("1.2.3 12:34:56 1,000.50 1.2.3%", en).normalized_text ==
          "1.2.3 12:34:56 one thousand point five zero 1.2.3%");
    {
        const auto punctuation = frontend.process("12.5%. 1:02.", en);
        CHECK(punctuation.normalized_text == "twelve point five percent. one hour two minutes.");
        CHECK(punctuation.warnings.empty());
    }
    {
        const auto punctuation = frontend.process("01.02.2026.", ru);
        CHECK(punctuation.normalized_text != "01.02.2026.");
        CHECK(!punctuation.normalized_text.empty());
        CHECK(punctuation.normalized_text.back() == '.');
        CHECK(punctuation.warnings.empty());
    }
    {
        const std::string embedded_numeric = "abc1,234 1,23%";
        const auto result = frontend.process(embedded_numeric, en);
        CHECK(result.normalized_text == embedded_numeric);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 3);
        CHECK(result.warnings.front().length == embedded_numeric.size() - 3);
    }
    for (const auto& input : {std::string("1,234.56!"),
                              std::string("$1,234.56!"),
                              std::string("1,234+abc"),
                              std::string("1,234.56%word"),
                              std::string("1.2!"),
                              std::string("1.2?"),
                              std::string("1.2)"),
                              std::string("12:34!"),
                              std::string("01.02.2026?")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
    }
    CHECK(frontend.process("1,234 руб.", ru).normalized_text == "1,234 руб.");
    for (const auto& input : {std::string("$1,234%"),
                              std::string("$1,234.56%"),
                              std::string("x$1,234%"),
                              std::string("$ 1,234%"),
                              std::string("$+1,234%")}) {
        const auto result = frontend.process(input, en);
        const auto numeric_offset = input.front() == 'x' ? 1u : 0u;
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == numeric_offset);
        CHECK(result.warnings.front().length == input.size() - numeric_offset);
    }
    for (const auto& input : {std::string("$ 1,234%"), std::string("$+1,234%")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    const std::string nbsp = "a\xc2\xa0"
                             "b\xe2\x80\xaf"
                             "c\x0b"
                             "d\x0c"
                             "e";
    CHECK(frontend.process(nbsp, en).normalized_text == "a b c d e");
    {
        const std::string nested_currency_url = "https://example.com/path/$1,234%25";
        CHECK(frontend.process(nested_currency_url, en).normalized_text == nested_currency_url);
        CHECK(frontend.process(nested_currency_url, ru).normalized_text == nested_currency_url);
    }

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
    CHECK(frontend.process("QWEN.", dictionary_options).pronunciation_text == "квен.");
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

    // Automatic initialism expansion is opt-in and deliberately allowlisted.
    TextFrontendOptions initialisms;
    initialisms.language = Language::Russian;
    initialisms.normalize = false;
    initialisms.expand_initialisms = true;
    const auto initialism_result = frontend.process("ВК ООО РФ МГУ", initialisms);
    CHECK(initialism_result.pronunciation_text == "вэ ка о о о эр эф эм гэ у");
    CHECK(initialism_result.automatic_rewrites.size() == 4);
    CHECK(initialism_result.words.size() == 4);
    CHECK(initialism_result.words[0].surface == "ВК");
    CHECK(initialism_result.words[0].pronunciation == "вэ ка");
    CHECK(initialism_result.words[0].from_automatic_rewrite);
    TextFrontendOptions english_initialisms = initialisms;
    english_initialisms.language = Language::English;
    CHECK(frontend.process("ВК ООО", english_initialisms).pronunciation_text == "ВК ООО");
    CHECK(frontend.process("ФСБ МФЦ ИП", initialisms).pronunciation_text ==
          "эф эс бэ эм эф цэ и пэ");
    CHECK(frontend.process("ВК вк Вк", initialisms).pronunciation_text == "вэ ка вк Вк");
    CHECK(frontend.process("НАТО МИД ЗАГС", initialisms).pronunciation_text == "НАТО МИД ЗАГС");
    CHECK(frontend.process("ВК🙂 🙂ВК ООО❤️", initialisms).pronunciation_text ==
          "вэ ка🙂 🙂вэ ка о о о❤️");

    PronunciationDictionary rewrite_dictionary;
    CHECK(rewrite_dictionary.add_token("ВК", "явный override"));
    CHECK(rewrite_dictionary.add_token("ООО", "word"));
    CHECK(rewrite_dictionary.add_token("Visual", "word"));
    CHECK(rewrite_dictionary.add_phrase("Visual Studio", "phrase"));
    CHECK(rewrite_dictionary.add_phrase("ООО Ромашка", "phrase acronym"));
    TextFrontendOptions rewrite_options = initialisms;
    rewrite_options.language = Language::English;
    rewrite_options.dictionary = &rewrite_dictionary;
    const auto rewrite_result = frontend.process("ВК Visual Studio", rewrite_options);
    CHECK(rewrite_result.pronunciation_text == "явный override phrase");
    CHECK(rewrite_result.automatic_rewrites.empty());
    CHECK(frontend.process("ООО Ромашка", rewrite_options).pronunciation_text == "phrase acronym");

    PronunciationDictionary phrase_only_dictionary;
    CHECK(phrase_only_dictionary.add_phrase("Visual Studio", "phrase"));
    CHECK(phrase_only_dictionary.add_phrase("ООО Ромашка", "phrase acronym"));
    TextFrontendOptions phrase_options = rewrite_options;
    phrase_options.dictionary = &phrase_only_dictionary;
    phrase_options.expand_initialisms = false;
    const auto opaque_phrase = frontend.process("Visual🙂Studio", phrase_options);
    CHECK(opaque_phrase.pronunciation_text == "Visual🙂Studio");
    CHECK(frontend.process("Visual—Studio", phrase_options).pronunciation_text == "Visual—Studio");
    CHECK(frontend.process("Visual / Studio", phrase_options).pronunciation_text ==
          "Visual / Studio");
    CHECK(frontend.process("ООО🙂Ромашка", phrase_options).pronunciation_text == "ООО🙂Ромашка");
    CHECK(frontend.process("ООО ❤️ Ромашка", phrase_options).pronunciation_text == "ООО ❤️ Ромашка");

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
    const auto automatic_result = frontend.process("тест", automatic);
    CHECK(automatic_result.warnings.size() == 1);
    CHECK(automatic_result.warnings.front().code == WarningCode::AutomaticStressUnavailable);
    CHECK(automatic_result.warnings.front().offset == 0);
    CHECK(automatic_result.warnings.front().length == 0);
    automatic.resolve_stress = false;
    CHECK(frontend.process("тест", automatic).warnings.empty());
    TextFrontendOptions unsupported;
    unsupported.language = static_cast<Language>(999);
    const auto unsupported_result = frontend.process("test", unsupported);
    CHECK(unsupported_result.warnings.size() == 1);
    CHECK(unsupported_result.warnings.front().code == WarningCode::UnsupportedLanguage);
    CHECK(unsupported_result.has_uncertainty());
    CHECK(unsupported_result.warnings.front().offset == 0);
    CHECK(unsupported_result.warnings.front().length == 0);
    TextFrontendOptions no_cleanup;
    no_cleanup.language = Language::English;
    no_cleanup.cleanup_spacing = false;
    CHECK(frontend.process("a  b", no_cleanup).normalized_text == "a  b");
    CHECK(frontend.process("café 😊", en).normalized_text == "café 😊");
    const auto mixed = frontend.process("Привет API", TextFrontendOptions{});
    CHECK(mixed.has_uncertainty());
    {
        const std::string mixed_invalid_input = "Привет hello 99:99";
        const auto mixed_invalid = frontend.process(mixed_invalid_input, TextFrontendOptions{});
        CHECK(mixed_invalid.warnings.size() == 2);
        CHECK(mixed_invalid.warnings[0].code == WarningCode::AmbiguousNormalization);
        CHECK(mixed_invalid.warnings[0].offset == 0);
        CHECK(mixed_invalid.warnings[0].length == mixed_invalid_input.size());
        CHECK(mixed_invalid.warnings[1].offset == std::string("Привет hello ").size());
        CHECK(mixed_invalid.warnings[1].length == 5);
    }
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
        CHECK(invalid_time.warnings[0].length == 5);
        CHECK(invalid_time.warnings[1].offset == 6);
        CHECK(invalid_time.warnings[1].length == 5);
    }
    {
        const auto transformed = frontend.process("1% 99:99", en);
        CHECK(transformed.normalized_text == "one percent 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 3);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("RTX 4090 99:99", en);
        CHECK(transformed.normalized_text == "RTX 4090 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 9);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("a   99:99", en);
        CHECK(transformed.normalized_text == "a 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 4);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("1 234 99:99", en);
        CHECK(transformed.normalized_text == "one thousand two hundred thirty four 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 6);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("тест 99:99", en);
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == std::string("тест ").size());
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("99:99 24:99", en);
        CHECK(transformed.warnings.size() == 2);
        CHECK(transformed.warnings[0].offset == 0);
        CHECK(transformed.warnings[0].length == 5);
        CHECK(transformed.warnings[1].offset == 6);
        CHECK(transformed.warnings[1].length == 5);
    }
    {
        const auto protected_invalid = frontend.process("https://example.com 99:99", en);
        CHECK(protected_invalid.normalized_text == "https://example.com 99:99");
        CHECK(protected_invalid.warnings.size() == 1);
        CHECK(protected_invalid.warnings.front().offset ==
              protected_invalid.original_text.find("99:99"));
        CHECK(protected_invalid.warnings.front().length == 5);
    }
    {
        const std::string protected_duplicate = "https://example.com/99:99 99:99";
        const auto result = frontend.process(protected_duplicate, en);
        CHECK(result.normalized_text == protected_duplicate);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == result.original_text.rfind("99:99"));
        CHECK(result.warnings.front().length == 5);
    }
    {
        const auto multi_transform = frontend.process("1% 1.5 99:99", en);
        CHECK(multi_transform.normalized_text == "one percent one point five 99:99");
        CHECK(multi_transform.warnings.size() == 1);
        CHECK(multi_transform.warnings.front().offset ==
              multi_transform.original_text.find("99:99"));
        CHECK(multi_transform.warnings.front().length == 5);
    }
    {
        const std::string grouped_before_invalid = "1   234 99:99";
        const auto result = frontend.process(grouped_before_invalid, en);
        CHECK(result.normalized_text == "one thousand two hundred thirty four 99:99");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == result.original_text.rfind("99:99"));
        CHECK(result.warnings.front().length == 5);
    }
    {
        const std::string oversized_measurement = std::string(21, '9') + "   kg";
        const auto result = frontend.process(oversized_measurement, en);
        CHECK(result.normalized_text == std::string(21, '9') + " kg");
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == oversized_measurement.size());
    }
    {
        const auto unsupported_decimal = frontend.process("$12.345", en);
        CHECK(unsupported_decimal.normalized_text == "$12.345");
        CHECK(unsupported_decimal.warnings.size() == 1);
        CHECK(unsupported_decimal.warnings.front().offset == 0);
        CHECK(unsupported_decimal.warnings.front().length == 7);
    }
    {
        const auto preserved_with_valid_neighbor = frontend.process("$12.345 2 kg", en);
        CHECK(preserved_with_valid_neighbor.normalized_text == "$12.345 two kilograms");
        CHECK(preserved_with_valid_neighbor.warnings.size() == 1);
        CHECK(preserved_with_valid_neighbor.warnings.front().offset == 0);
        CHECK(preserved_with_valid_neighbor.warnings.front().length == 7);
    }
    {
        const auto malformed_ordinal = frontend.process("11st 2nd", en);
        CHECK(malformed_ordinal.normalized_text == "11st second");
        CHECK(malformed_ordinal.warnings.size() == 1);
        CHECK(malformed_ordinal.warnings.front().offset == 0);
        CHECK(malformed_ordinal.warnings.front().length == 4);
    }
    {
        const auto signed_ordinal = frontend.process("-1st 2nd", en);
        CHECK(signed_ordinal.normalized_text == "-1st second");
        CHECK(signed_ordinal.warnings.size() == 1);
        CHECK(signed_ordinal.warnings.front().offset == 0);
        CHECK(signed_ordinal.warnings.front().length == 4);
    }
    {
        const std::string oversized_ordinal = "999999999999999999999th";
        const auto result = frontend.process(oversized_ordinal, en);
        CHECK(result.normalized_text == oversized_ordinal);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == oversized_ordinal.size());
    }
    {
        const std::string huge_number(80, '9');
        const auto too_large = frontend.process(huge_number, en);
        CHECK(too_large.normalized_text == huge_number);
        CHECK(too_large.warnings.size() == 1);
        CHECK(too_large.warnings.front().offset == 0);
        CHECK(too_large.warnings.front().length == huge_number.size());
    }
    CHECK(frontend.process("99.99.2026", ru).normalized_text == "99.99.2026");
    {
        const auto malformed_date = frontend.process("99.99.2026", ru);
        CHECK(malformed_date.normalized_text == "99.99.2026");
        CHECK(malformed_date.warnings.size() == 1);
        CHECK(malformed_date.warnings.front().offset == 0);
        CHECK(malformed_date.warnings.front().length == std::string("99.99.2026").size());
    }
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
