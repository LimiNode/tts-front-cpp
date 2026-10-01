#include "tts_front/tts_front.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

namespace {
std::string field(const std::string& line, const char* name) {
    const std::regex pattern(std::string("\\\"") + name + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    return std::regex_search(line, match, pattern) ? match[1].str() : std::string();
}

bool valid_adaptation_date(const std::string& value) {
    static const std::regex pattern(R"(\d{4}-\d{2}-\d{2})");
    return std::regex_match(value, pattern);
}
} // namespace
int main() {
    tts_front::TextFrontend frontend;
    std::size_t count = 0;
    for (const char* fixture_name : {"basic.jsonl", "upstream_adapted.jsonl"}) {
        const bool requires_adaptation_date = std::string(fixture_name) == "upstream_adapted.jsonl";
        const std::string path =
            std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/" + fixture_name;
        std::ifstream file(path);
        if (!file) {
            std::cerr << "Cannot open fixture corpus: " << path << "\n";
            return EXIT_FAILURE;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty())
                continue;
            const auto input = field(line, "input");
            const auto language = field(line, "language");
            const auto expected = field(line, "expected");
            const auto category = field(line, "category");
            const auto source = field(line, "source");
            const auto adapted = field(line, "adapted");
            if (input.empty() || expected.empty() || category.empty() || source.empty() ||
                (requires_adaptation_date && !valid_adaptation_date(adapted))) {
                std::cerr << "Malformed fixture line: " << line << "\n";
                return EXIT_FAILURE;
            }
            tts_front::TextFrontendOptions options;
            options.language =
                language == "ru" ? tts_front::Language::Russian : tts_front::Language::English;
            const auto result = frontend.process(input, options);
            if (result.normalized_text != expected) {
                std::cerr << "Fixture mismatch [" << category << "] expected='" << expected
                          << "' actual='" << result.normalized_text << "'\n";
                return EXIT_FAILURE;
            }
            ++count;
        }
    }
    if (count < 10) {
        std::cerr << "Fixture corpus unexpectedly small\n";
        return EXIT_FAILURE;
    }
    std::cout << "fixture tests passed: " << count << "\n";
    return EXIT_SUCCESS;
}
