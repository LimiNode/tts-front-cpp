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
    std::string diagnostics;
};

struct Snapshot {
    std::size_t cases = 0;
    std::map<std::string, std::size_t> outcomes;
    std::set<std::string> mismatch_ids;
    std::map<std::string, std::string> diagnostic_exceptions;
    std::map<std::string, std::pair<std::string, std::string>> mismatch_outputs;
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

bool valid_utf8(const std::string& value);
bool contains_marker(const std::string& value);

bool unsigned_number(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char character) {
        return character >= '0' && character <= '9';
    });
}

bool valid_diagnostic_signature(const std::string& value) {
    if (value == "none")
        return true;
    std::size_t begin = 0;
    while (true) {
        const auto separator = value.find(';', begin);
        const auto item =
            value.substr(begin, separator == std::string::npos ? separator : separator - begin);
        const auto at = item.find('@');
        const auto colon = item.find(':', at == std::string::npos ? at : at + 1);
        if (at == std::string::npos || colon == std::string::npos || at == 0 ||
            !unsigned_number(item.substr(at + 1, colon - at - 1)) ||
            !unsigned_number(item.substr(colon + 1)))
            return false;
        if (separator == std::string::npos)
            break;
        begin = separator + 1;
    }
    return true;
}

bool parse_count(const std::string& value, std::size_t& result) {
    if (!unsigned_number(value))
        return false;
    try {
        result = static_cast<std::size_t>(std::stoull(value));
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

bool load_snapshot(const std::string& path, Snapshot& snapshot) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const auto fields = split_tabs(line);
        if (fields.size() == 2 && fields[0] == "version" && fields[1] == "1")
            continue;
        if (fields.size() == 2 && fields[0] == "cases") {
            if (!parse_count(fields[1], snapshot.cases))
                return false;
            continue;
        }
        if (fields.size() == 3 && fields[0] == "outcome") {
            if (!parse_count(fields[2], snapshot.outcomes[fields[1]]))
                return false;
            continue;
        }
        if (fields.size() == 2 && fields[0] == "mismatch") {
            if (fields[1].empty() || !snapshot.mismatch_ids.insert(fields[1]).second)
                return false;
            continue;
        }
        if (fields.size() == 3 && fields[0] == "diagnostic_exception") {
            if (fields[1].empty() || !valid_diagnostic_signature(fields[2]) ||
                !snapshot.diagnostic_exceptions.emplace(fields[1], fields[2]).second)
                return false;
            continue;
        }
        if (fields.size() == 4 && fields[0] == "mismatch_output") {
            if (fields[1].empty() ||
                !snapshot.mismatch_outputs.emplace(fields[1], std::make_pair(fields[2], fields[3]))
                     .second ||
                !valid_utf8(fields[2]) || !valid_utf8(fields[3]) || contains_marker(fields[2]) ||
                contains_marker(fields[3]))
                return false;
            continue;
        }
        return false;
    }
    return snapshot.cases != 0 && snapshot.outcomes.size() == 4;
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

std::string diagnostic_signature(const std::vector<tts_front::TextWarning>& warnings) {
    if (warnings.empty())
        return "none";
    std::ostringstream output;
    for (std::size_t index = 0; index < warnings.size(); ++index) {
        if (index != 0)
            output << ';';
        output << tts_front::to_string(warnings[index].code) << '@' << warnings[index].offset << ':'
               << warnings[index].length;
    }
    return output.str();
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
    if (line != "id\tlanguage\tmode\tcategory\texpectation\tinput\texpected\tdiagnostics") {
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
        if (fields.size() != 8) {
            std::cerr << "Malformed TSV row at line " << line_number << "\n";
            return EXIT_FAILURE;
        }
        QualityCase item{
            fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7]};
        if (item.id.empty() || item.category.empty() || item.input.empty() ||
            item.expected.empty() || !ids.insert(item.id).second ||
            (item.language != "en" && item.language != "ru") ||
            (item.mode != "explicit" && item.mode != "auto_segment") ||
            (item.expectation != "normalize" && item.expectation != "preserve") ||
            (item.expectation == "preserve") != (item.input == item.expected) ||
            !valid_diagnostic_signature(item.diagnostics) || !valid_utf8(item.input) ||
            !valid_utf8(item.expected) || contains_marker(item.input) ||
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

    Snapshot snapshot;
    const std::string snapshot_path =
        std::string(TTS_FRONT_SOURCE_DIR) + "/tests/quality/en_ru_snapshot.tsv";
    if (!load_snapshot(snapshot_path, snapshot)) {
        std::cerr << "Invalid or missing quality snapshot: " << snapshot_path << "\n";
        return EXIT_FAILURE;
    }
    for (const auto& exception : snapshot.diagnostic_exceptions) {
        if (ids.find(exception.first) == ids.end()) {
            std::cerr << "Snapshot diagnostic exception references unknown case: "
                      << exception.first << "\n";
            return EXIT_FAILURE;
        }
    }
    for (const auto& output : snapshot.mismatch_outputs) {
        if (ids.find(output.first) == ids.end()) {
            std::cerr << "Snapshot mismatch output references unknown case: " << output.first
                      << "\n";
            return EXIT_FAILURE;
        }
    }

    tts_front::TextFrontend frontend;
    std::map<Outcome, std::size_t> outcomes;
    std::map<std::string, std::map<Outcome, std::size_t>> by_language;
    std::map<std::string, std::map<Outcome, std::size_t>> by_category;
    std::set<std::string> actual_mismatch_ids;
    std::set<std::string> actual_diagnostic_exception_ids;
    std::map<std::string, std::pair<std::string, std::string>> actual_mismatch_outputs;
    std::size_t diagnostic_mismatches = 0;
    std::vector<std::string> unexpected_diagnostics;
    struct Mismatch {
        QualityCase item;
        Outcome outcome;
        std::string actual;
        std::string pronunciation;
        std::size_t warnings = 0;
        std::string diagnostics;
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
        if (result.original_text != item.input) {
            std::cerr << "Result original_text mismatch for " << item.id << "\n";
            return EXIT_FAILURE;
        }
        if (!valid_utf8(result.normalized_text) || !valid_utf8(result.pronunciation_text) ||
            contains_marker(result.normalized_text) || contains_marker(result.pronunciation_text)) {
            std::cerr << "Invalid UTF-8 or leaked marker in result for " << item.id << "\n";
            return EXIT_FAILURE;
        }
        std::set<std::tuple<tts_front::WarningCode, std::size_t, std::size_t>> warning_keys;
        std::vector<std::pair<std::size_t, std::size_t>> unresolved_spans;
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
            if (warning.code == tts_front::WarningCode::UnresolvedNumber && warning.length != 0)
                unresolved_spans.emplace_back(warning.offset, warning.offset + warning.length);
        }
        std::sort(unresolved_spans.begin(), unresolved_spans.end());
        for (std::size_t index = 1; index < unresolved_spans.size(); ++index) {
            if (unresolved_spans[index - 1].second > unresolved_spans[index].first) {
                std::cerr << "Overlapping UnresolvedNumber spans for " << item.id << "\n";
                return EXIT_FAILURE;
            }
        }
        const auto actual_diagnostics = diagnostic_signature(result.warnings);
        if (actual_diagnostics != item.diagnostics)
            ++diagnostic_mismatches;
        if (actual_diagnostics != item.diagnostics &&
            snapshot.diagnostic_exceptions.find(item.id) != snapshot.diagnostic_exceptions.end() &&
            snapshot.diagnostic_exceptions.at(item.id) == actual_diagnostics)
            actual_diagnostic_exception_ids.insert(item.id);
        else if (actual_diagnostics != item.diagnostics) {
            unexpected_diagnostics.push_back(item.id + " expected " + item.diagnostics +
                                             " actual " + actual_diagnostics);
        }
        const auto outcome = classify(item, result.normalized_text);
        ++outcomes[outcome];
        ++by_language[item.language][outcome];
        ++by_category[item.category][outcome];
        if (outcome == Outcome::UnnecessaryRefusal || outcome == Outcome::IncorrectOrPartial) {
            actual_mismatch_ids.insert(item.id);
            actual_mismatch_outputs.emplace(
                item.id, std::make_pair(result.normalized_text, result.pronunciation_text));
            mismatches.push_back({item,
                                  outcome,
                                  result.normalized_text,
                                  result.pronunciation_text,
                                  result.warnings.size(),
                                  diagnostic_signature(result.warnings)});
        }
    }

    const std::array<Outcome, 4> order = {Outcome::CorrectTransformation,
                                          Outcome::JustifiedPreservation,
                                          Outcome::UnnecessaryRefusal,
                                          Outcome::IncorrectOrPartial};
    bool snapshot_matches = snapshot.cases == cases.size();
    for (const auto outcome : order) {
        const auto found = snapshot.outcomes.find(name(outcome));
        snapshot_matches = snapshot_matches && found != snapshot.outcomes.end() &&
                           found->second == outcomes[outcome];
    }
    snapshot_matches = snapshot_matches && actual_mismatch_ids == snapshot.mismatch_ids;
    snapshot_matches = snapshot_matches && actual_mismatch_outputs == snapshot.mismatch_outputs;
    snapshot_matches = snapshot_matches && actual_diagnostic_exception_ids.size() ==
                                               snapshot.diagnostic_exceptions.size();
    if (!unexpected_diagnostics.empty()) {
        snapshot_matches = false;
        for (const auto& diagnostic : unexpected_diagnostics)
            std::cerr << "Unexpected diagnostics: " << diagnostic << "\n";
    }
    if (!snapshot_matches)
        std::cerr << "Quality snapshot mismatch; update it only with a reviewed baseline change\n";
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
    const auto shown = std::min<std::size_t>(mismatches.size(), 100);
    for (std::size_t index = 0; index < shown; ++index) {
        const auto& mismatch = mismatches[index];
        std::cout << "  [" << mismatch.item.id << "] " << name(mismatch.outcome)
                  << " warnings=" << mismatch.warnings << " diagnostics=" << mismatch.diagnostics
                  << "\n"
                  << "    input=" << quoted(mismatch.item.input) << "\n"
                  << "    expected=" << quoted(mismatch.item.expected) << "\n"
                  << "    actual=" << quoted(mismatch.actual) << "\n"
                  << "    pronunciation=" << quoted(mismatch.pronunciation) << "\n";
    }
    if (mismatches.size() > shown)
        std::cout << "  ... " << mismatches.size() - shown << " more mismatches\n";
    std::cout << "diagnostic mismatches covered by snapshot: " << diagnostic_mismatches << "\n";
    std::cout << "quality corpus invariants passed\n";
    return snapshot_matches ? EXIT_SUCCESS : EXIT_FAILURE;
}
