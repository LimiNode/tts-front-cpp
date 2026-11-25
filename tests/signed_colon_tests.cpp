#include "tts_front.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/utf8_document.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace tts_front;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "CHECK failed: " << #condition << " at line " << __LINE__ << '\n';        \
            return false;                                                                          \
        }                                                                                          \
    } while (false)

namespace {

bool check_result(const TextFrontendResult& result,
                  const std::string& input,
                  const std::string& expected,
                  const std::size_t offset,
                  const std::size_t length,
                  const bool ambiguous) {
    if (result.normalized_text != expected)
        std::cerr << input << " -> " << result.normalized_text << " expected " << expected << '\n';
    CHECK(result.original_text == input);
    CHECK(result.normalized_text == expected);
    CHECK(result.pronunciation_text == expected);
    CHECK(result.warnings.size() == (ambiguous ? 2u : 1u));
    if (ambiguous) {
        CHECK(result.warnings[0].code == WarningCode::AmbiguousNormalization);
        CHECK(result.warnings[0].offset == 0);
        CHECK(result.warnings[0].length == input.size());
    }
    const auto& warning = result.warnings.back();
    CHECK(warning.code == WarningCode::UnresolvedNumber);
    CHECK(warning.offset == offset);
    if (warning.length != length)
        std::cerr << input << " span " << warning.offset << ':' << warning.length << " expected "
                  << offset << ':' << length << '\n';
    CHECK(warning.length == length);
    const detail::Utf8Document document(input);
    CHECK(document.span_from_bytes(offset, offset + length).byte_begin == offset);
    CHECK(document.span_from_bytes(offset, offset + length).byte_end == offset + length);
    for (const auto* value : {&result.normalized_text, &result.pronunciation_text}) {
        CHECK(value->find('\x01') == std::string::npos);
        CHECK(value->find('\x02') == std::string::npos);
    }
    std::set<std::tuple<WarningCode, std::size_t, std::size_t>> keys;
    for (const auto& diagnostic : result.warnings)
        CHECK(keys.emplace(diagnostic.code, diagnostic.offset, diagnostic.length).second);
    return true;
}

bool signed_matrix() {
    TextFrontend frontend;
    struct Tail {
        const char* input;
        const char* candidate;
        const char* english;
        const char* russian;
    };
    const Tail tails[] = {
        {"", "", "", ""},
        {"́", "́", "́", "́"},
        {"́ слово", "́", "́ слово", "́ слово"},
        {"x", "x", "x", "x"},
        {"я", "я", "я", "я"},
        {",", "", ",", ","},
        {".", "", ".", "."},
        {"/4", "/4", "/4", "/4"},
        {" /4", " /4", " /4", " /4"},
        {"#4", "#4", "#4", "#4"},
        {" #4", " #4", " #4", " #4"},
        {" # 4", " # 4", " # 4", " # 4"},
        {"%", "%", "%", "%"},
        {"-", "-", "-", "-"},
        {" #", " #", " #", " #"},
        {" 4", "", " four", " четыре"},
        {"; 1 книга", "", "; one книга", "; одна книга"},
        {" — «план». 1 книга", "", " — «план». one книга", " — «план». одна книга"},
        {"\n1 книга", "", " one книга", " одна книга"}};
    for (const auto* mode : {"ru", "en", "auto", "segment"}) {
        const auto label = std::string(mode);
        TextFrontendOptions options;
        options.language = label == "ru"   ? Language::Russian
                           : label == "en" ? Language::English
                                           : Language::Auto;
        options.mixed_language_policy = label == "segment" ? MixedLanguagePolicy::SegmentCandidates
                                                           : MixedLanguagePolicy::DominantLanguage;
        for (const auto* sign : {"+", "-", "−"}) {
            for (const auto* core : {"2 задачи", "2 002 задачи", "$2 задачи"}) {
                const auto candidate = std::string(sign) + core;
                for (const auto& tail : tails) {
                    const auto input = "OpenAI:" + candidate + tail.input;
                    const auto expected =
                        "OpenAI:" + candidate + (label == "en" ? tail.english : tail.russian);
                    CHECK(check_result(frontend.process(input, options),
                                       input,
                                       expected,
                                       7,
                                       candidate.size() + std::string(tail.candidate).size(),
                                       label == "auto" || label == "segment"));
                }
            }
            // Currency sign after the symbol, scientific and decimal/time cores
            // remain single signed candidates as well.
            for (const auto& candidate : {std::string("$") + sign + "2 задачи /4",
                                          std::string(sign) + "2e+3 задачи #4",
                                          std::string(sign) + "2.5 задачи /4",
                                          std::string(sign) + "2,5 задачи /4",
                                          std::string(sign) + "1:02 задачи /4"}) {
                const auto input = "OpenAI:" + candidate;
                CHECK(check_result(frontend.process(input, options),
                                   input,
                                   input,
                                   7,
                                   candidate.size(),
                                   label == "auto" || label == "segment"));
            }
        }
    }
    return true;
}

bool boundaries() {
    TextFrontend frontend;
    for (const auto* input : {"id2:−2 задачи", "OpenAI2:-2 задачи", "строка2:−2 задачи"}) {
        const detail::Utf8Document document(input);
        const auto colon = std::string(input).find(':');
        const auto point = detail::codepoint_index_at_or_after(document, colon + 1);
        CHECK(detail::scan_signed_colon_prefix(document, point).has_value());
    }
    for (const auto* input : {"12:-2", "1:+2", "1:−2 задачи", "1.2:-2", "1.2a:-2 задачи"}) {
        const detail::Utf8Document document(input);
        const auto colon = std::string(input).find(':');
        const auto point = detail::codepoint_index_at_or_after(document, colon + 1);
        CHECK(!detail::scan_signed_colon_prefix(document, point));
    }
    for (const auto language : {Language::Russian, Language::English, Language::Auto}) {
        for (const auto policy :
             {MixedLanguagePolicy::DominantLanguage, MixedLanguagePolicy::SegmentCandidates}) {
            TextFrontendOptions options;
            options.language = language;
            options.mixed_language_policy = policy;
            const bool english = language != Language::Russian;
            const auto next = english ? std::string("four") : std::string("четыре");
            for (const auto* protected_word :
                 {"HTTP/2", "RTX-4090", "https://example.org/4", "dev@example.org", "foo-bar4"}) {
                const auto input = std::string("OpenAI:-2 ") + protected_word + " 4";
                const auto expected = std::string("OpenAI:-2 ") + protected_word + ' ' + next;
                // A technical neighbor is an opaque independent token, not a noun.
                const auto result = frontend.process(input, options);
                CHECK(result.normalized_text == expected);
                const auto pronunciation =
                    std::string(protected_word) == "HTTP/2"
                        ? std::string("OpenAI:-2 ") +
                              (english ? "H-T-T-P two " : "аш ти ти пи два ") + next
                        : expected;
                CHECK(result.pronunciation_text == pronunciation);
                CHECK(result.original_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.back().code == WarningCode::UnresolvedNumber);
                CHECK(result.warnings.back().offset == 7);
                CHECK(result.warnings.back().length == 2);
            }
            const auto numeric = frontend.process("1.2:-2", options);
            CHECK(numeric.normalized_text == "1.2:-2");
            CHECK(numeric.pronunciation_text == "1.2:-2");
            CHECK(numeric.warnings.size() == 1);
            CHECK(numeric.warnings[0].offset == 0);
            CHECK(numeric.warnings[0].length == 6);
            CHECK(numeric.warnings[0].code == WarningCode::UnresolvedNumber);
            const auto url = frontend.process("https://example.org/id:-2/4", options);
            CHECK(url.normalized_text == "https://example.org/id:-2/4");
            CHECK(url.pronunciation_text == "https://example.org/id:-2/4");
            CHECK(url.warnings.empty());
            PronunciationDictionary dictionary;
            CHECK(dictionary.add_token("задачи", "дела"));
            options.dictionary = &dictionary;
            const auto overridden = frontend.process("OpenAI:−2 задачи", options);
            CHECK(overridden.normalized_text == "OpenAI:−2 задачи");
            CHECK(overridden.pronunciation_text == "OpenAI:−2 дела");
            CHECK(overridden.dictionary_replacements.size() == 1);
            CHECK(overridden.warnings.back().offset == 7);
            CHECK(overridden.warnings.back().length == 17);
            options.dictionary = nullptr;
            options.normalize = false;
            const auto disabled = frontend.process("OpenAI:−2 задачи", options);
            CHECK(disabled.normalized_text == "OpenAI:−2 задачи");
            CHECK(disabled.warnings.size() == (language == Language::Auto ? 1u : 0u));
        }
    }
    return true;
}

} // namespace

int main() {
    return signed_matrix() && boundaries() ? EXIT_SUCCESS : EXIT_FAILURE;
}
