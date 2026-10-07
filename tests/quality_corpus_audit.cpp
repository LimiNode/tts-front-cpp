#include "tts_front.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

enum class Outcome {
    CorrectTransformation,
    JustifiedPreservation,
    UnnecessaryRefusal,
    IncorrectOrPartial,
};

struct QualityCase {
    std::string id;
    std::string language;
    std::string mode;
    std::string category;
    std::string expectation;
    std::string input;
    std::string expected;
};

const char* name(Outcome outcome) {
    switch (outcome) {
    case Outcome::CorrectTransformation:
        return "correct_transformation";
    case Outcome::JustifiedPreservation:
        return "justified_preservation";
    case Outcome::UnnecessaryRefusal:
        return "unnecessary_refusal";
    case Outcome::IncorrectOrPartial:
        return "incorrect_or_partial";
    }
    return "unknown";
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (true) {
        const auto tab = line.find('\t', begin);
        fields.push_back(line.substr(begin, tab == std::string::npos ? tab : tab - begin));
        if (tab == std::string::npos)
            return fields;
        begin = tab + 1;
    }
}

bool valid_utf8(const std::string& value) {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const auto first = static_cast<unsigned char>(value[offset]);
        if (first <= 0x7F) {
            ++offset;
            continue;
        }
        std::size_t length = 0;
        unsigned int codepoint = 0;
        unsigned int minimum = 0;
        if ((first & 0xE0) == 0xC0) {
            length = 2;
            codepoint = first & 0x1F;
            minimum = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            length = 3;
            codepoint = first & 0x0F;
            minimum = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            length = 4;
            codepoint = first & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (offset + length > value.size())
            return false;
        for (std::size_t index = 1; index < length; ++index) {
            const auto byte = static_cast<unsigned char>(value[offset + index]);
            if ((byte & 0xC0) != 0x80)
                return false;
            codepoint = (codepoint << 6U) | (byte & 0x3FU);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            return false;
        offset += length;
    }
    return true;
}

bool utf8_boundary(const std::string& value, std::size_t offset) {
    return offset <= value.size() &&
           (offset == value.size() || (static_cast<unsigned char>(value[offset]) & 0xC0U) != 0x80U);
}

bool contains_marker(const std::string& value) {
    return value.find('\x01') != std::string::npos || value.find('\x02') != std::string::npos;
}

Outcome classify(const QualityCase& item, const std::string& actual) {
    if (item.expectation == "preserve")
        return actual == item.input ? Outcome::JustifiedPreservation : Outcome::IncorrectOrPartial;
    if (actual == item.expected)
        return Outcome::CorrectTransformation;
    if (actual == item.input)
        return Outcome::UnnecessaryRefusal;
    return Outcome::IncorrectOrPartial;
}

std::string quoted(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (const char character : value) {
        if (character == '"' || character == '\\')
            result.push_back('\\');
        result.push_back(character);
    }
    result.push_back('"');
    return result;
}

} // namespace

int main() {
    const std::string path =
        std::string(TTS_FRONT_SOURCE_DIR) + "/tests/quality/en_ru_sentences.tsv";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open quality corpus: " << path << "\n";
        return EXIT_FAILURE;
    }

    std::string line;
    if (!std::getline(file, line)) {
        std::cerr << "Quality corpus is empty\n";
        return EXIT_FAILURE;
    }
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != "id\tlanguage\tmode\tcategory\texpectation\tinput\texpected") {
        std::cerr << "Unexpected quality corpus header\n";
        return EXIT_FAILURE;
    }

    std::vector<QualityCase> cases;
    std::set<std::string> ids;
    std::map<std::string, std::size_t> category_sizes;
    std::array<std::size_t, 2> language_sizes{};
    std::size_t line_number = 1;
    while (std::getline(file, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const auto fields = split_tabs(line);
        if (fields.size() != 7) {
            std::cerr << "Malformed TSV row at line " << line_number << "\n";
            return EXIT_FAILURE;
        }
        QualityCase item{
            fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6]};
        if (item.id.empty() || item.category.empty() || item.input.empty() ||
            item.expected.empty() || !ids.insert(item.id).second ||
            (item.language != "en" && item.language != "ru") ||
            (item.mode != "explicit" && item.mode != "auto_segment") ||
            (item.expectation != "normalize" && item.expectation != "preserve") ||
            (item.expectation == "preserve") != (item.input == item.expected) ||
            !valid_utf8(item.input) || !valid_utf8(item.expected) || contains_marker(item.input) ||
            contains_marker(item.expected)) {
            std::cerr << "Invalid quality case at line " << line_number << ": " << item.id << "\n";
            return EXIT_FAILURE;
        }
        ++language_sizes[item.language == "en" ? 0 : 1];
        ++category_sizes[item.category];
        cases.push_back(std::move(item));
    }
    if (cases.size() < 300 || language_sizes[0] < 100 || language_sizes[1] < 100 ||
        category_sizes.size() < 8) {
        std::cerr << "Quality corpus is smaller or less diverse than required\n";
        return EXIT_FAILURE;
    }

    tts_front::TextFrontend frontend;
    std::map<Outcome, std::size_t> outcomes;
    std::map<std::string, std::map<Outcome, std::size_t>> by_language;
    std::map<std::string, std::map<Outcome, std::size_t>> by_category;
    struct Mismatch {
        QualityCase item;
        Outcome outcome;
        std::string actual;
        std::size_t warnings = 0;
    };
    std::vector<Mismatch> mismatches;

    for (const auto& item : cases) {
        tts_front::TextFrontendOptions options;
        if (item.mode == "auto_segment") {
            options.language = tts_front::Language::Auto;
            options.mixed_language_policy = tts_front::MixedLanguagePolicy::SegmentCandidates;
        } else {
            options.language =
                item.language == "en" ? tts_front::Language::English : tts_front::Language::Russian;
        }
        const auto result = frontend.process(item.input, options);
        if (!valid_utf8(result.normalized_text) || !valid_utf8(result.pronunciation_text) ||
            contains_marker(result.normalized_text) || contains_marker(result.pronunciation_text)) {
            std::cerr << "Invalid UTF-8 or leaked marker in result for " << item.id << "\n";
            return EXIT_FAILURE;
        }
        std::set<std::tuple<tts_front::WarningCode, std::size_t, std::size_t>> warning_keys;
        for (const auto& warning : result.warnings) {
            if (warning.offset > item.input.size() ||
                warning.length > item.input.size() - warning.offset ||
                !utf8_boundary(item.input, warning.offset) ||
                !utf8_boundary(item.input, warning.offset + warning.length)) {
                std::cerr << "Invalid warning span for " << item.id << ": offset=" << warning.offset
                          << " length=" << warning.length << "\n";
                return EXIT_FAILURE;
            }
            if (!warning_keys.emplace(warning.code, warning.offset, warning.length).second) {
                std::cerr << "Duplicate warning for " << item.id << ": "
                          << tts_front::to_string(warning.code) << "\n";
                return EXIT_FAILURE;
            }
        }
        const auto outcome = classify(item, result.normalized_text);
        ++outcomes[outcome];
        ++by_language[item.language][outcome];
        ++by_category[item.category][outcome];
        if (outcome == Outcome::UnnecessaryRefusal || outcome == Outcome::IncorrectOrPartial)
            mismatches.push_back({item, outcome, result.normalized_text, result.warnings.size()});
    }

    const std::array<Outcome, 4> order = {Outcome::CorrectTransformation,
                                          Outcome::JustifiedPreservation,
                                          Outcome::UnnecessaryRefusal,
                                          Outcome::IncorrectOrPartial};
    std::cout << "quality corpus: " << cases.size() << " cases\n";
    std::cout << "outcomes\n";
    for (const auto outcome : order)
        std::cout << "  " << name(outcome) << ": " << outcomes[outcome] << "\n";
    std::cout << "languages\n";
    for (const auto& language : {std::string("en"), std::string("ru")}) {
        std::cout << "  " << language << ":";
        for (const auto outcome : order)
            std::cout << " " << name(outcome) << "=" << by_language[language][outcome];
        std::cout << "\n";
    }
    std::cout << "categories\n";
    for (const auto& entry : by_category) {
        std::cout << "  " << entry.first << ":";
        for (const auto outcome : order) {
            const auto found = entry.second.find(outcome);
            const auto count = found == entry.second.end() ? 0 : found->second;
            std::cout << " " << name(outcome) << "=" << count;
        }
        std::cout << "\n";
    }
    std::cout << "semantic mismatches: " << mismatches.size() << "\n";
    const auto shown = std::min<std::size_t>(mismatches.size(), 50);
    for (std::size_t index = 0; index < shown; ++index) {
        const auto& mismatch = mismatches[index];
        std::cout << "  [" << mismatch.item.id << "] " << name(mismatch.outcome)
                  << " warnings=" << mismatch.warnings << "\n"
                  << "    input=" << quoted(mismatch.item.input) << "\n"
                  << "    expected=" << quoted(mismatch.item.expected) << "\n"
                  << "    actual=" << quoted(mismatch.actual) << "\n";
    }
    if (mismatches.size() > shown)
        std::cout << "  ... " << mismatches.size() - shown << " more mismatches\n";
    std::cout << "quality corpus invariants passed\n";
    return EXIT_SUCCESS;
}
