#include "tts_front.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Case {
    std::string id;
    std::string language;
    std::string mode;
    std::string input;
    std::string expected;
    std::string diagnostics;
};

struct Snapshot {
    std::size_t cases = 0;
    std::map<std::string, std::size_t> outcomes;
    std::set<std::string> mismatches;
    std::map<std::string, std::string> diagnostic_exceptions;
    std::map<std::string, std::pair<std::string, std::string>> mismatch_outputs;
};

enum class Outcome { Correct, Preserved, Unnecessary, Incorrect };

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

bool unsigned_number(const std::string& value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool parse_count(const std::string& value, std::size_t& result) {
    if (!unsigned_number(value))
        return false;
    try {
        result = static_cast<std::size_t>(std::stoull(value));
    } catch (...) {
        return false;
    }
    return true;
}

bool valid_diagnostic(const std::string& value) {
    if (value == "none")
        return true;
    std::size_t begin = 0;
    while (true) {
        const auto end = value.find(';', begin);
        const auto item = value.substr(begin, end == std::string::npos ? end : end - begin);
        const auto at = item.find('@');
        const auto colon = item.find(':', at == std::string::npos ? at : at + 1);
        if (at == std::string::npos || colon == std::string::npos || at == 0 ||
            !unsigned_number(item.substr(at + 1, colon - at - 1)) ||
            !unsigned_number(item.substr(colon + 1)))
            return false;
        if (end == std::string::npos)
            return true;
        begin = end + 1;
    }
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
        } else if (fields.size() == 3 && fields[0] == "outcome") {
            if (!parse_count(fields[2], snapshot.outcomes[fields[1]]))
                return false;
        } else if (fields.size() == 2 && fields[0] == "mismatch") {
            if (!snapshot.mismatches.insert(fields[1]).second)
                return false;
        } else if (fields.size() == 3 && fields[0] == "diagnostic_exception") {
            if (!valid_diagnostic(fields[2]) ||
                !snapshot.diagnostic_exceptions.emplace(fields[1], fields[2]).second)
                return false;
        } else if (fields.size() == 4 && fields[0] == "mismatch_output") {
            if (!snapshot.mismatch_outputs.emplace(fields[1], std::make_pair(fields[2], fields[3]))
                     .second)
                return false;
        } else {
            return false;
        }
    }
    return snapshot.cases != 0 && snapshot.outcomes.size() == 4;
}

Outcome classify(const Case& item, const std::string& actual) {
    if (item.expected == item.input)
        return actual == item.input ? Outcome::Preserved : Outcome::Incorrect;
    if (actual == item.expected)
        return Outcome::Correct;
    if (actual == item.input)
        return Outcome::Unnecessary;
    return Outcome::Incorrect;
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
    case Outcome::Correct:
        return "correct_transformation";
    case Outcome::Preserved:
        return "justified_preservation";
    case Outcome::Unnecessary:
        return "unnecessary_refusal";
    case Outcome::Incorrect:
        return "incorrect_or_partial";
    }
    return "";
}

} // namespace

int main(int argc, char** argv) {
    bool strict = false;
    std::string archive_version = "v0.2.0";
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--strict") {
            strict = true;
        } else if (argument.rfind("v", 0) == 0) {
            archive_version = argument;
        } else {
            std::cerr << "usage: quality_archive_smoke [--strict] [vX.Y.Z]\n";
            return EXIT_FAILURE;
        }
    }
    const std::string root =
        std::string(TTS_FRONT_SOURCE_DIR) + "/tests/quality/archive/" + archive_version + "/";
    std::ifstream corpus(root + "en_ru_sentences.tsv", std::ios::binary);
    if (!corpus) {
        std::cerr << "cannot open corpus\n";
        return EXIT_FAILURE;
    }
    Snapshot snapshot;
    if (!load_snapshot(root + "en_ru_snapshot.tsv", snapshot)) {
        std::cerr << "cannot load snapshot\n";
        return EXIT_FAILURE;
    }

    std::string line;
    if (!std::getline(corpus, line)) {
        std::cerr << "cannot read header\n";
        return EXIT_FAILURE;
    }
    std::set<std::string> corpus_ids;
    std::map<std::string, std::size_t> actual_outcomes;
    std::set<std::string> actual_mismatches;
    std::map<std::string, std::pair<std::string, std::string>> actual_mismatch_outputs;
    std::set<std::string> actual_diagnostic_exceptions;
    tts_front::TextFrontend frontend;
    std::size_t cases = 0;
    while (std::getline(corpus, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto fields = split_tabs(line);
        if (fields.size() != 8) {
            std::cerr << "bad fields\n";
            return EXIT_FAILURE;
        }
        const Case item{fields[0], fields[1], fields[2], fields[5], fields[6], fields[7]};
        corpus_ids.insert(item.id);
        tts_front::TextFrontendOptions options;
        if (item.mode == "auto_segment") {
            options.language = tts_front::Language::Auto;
            options.mixed_language_policy = tts_front::MixedLanguagePolicy::SegmentCandidates;
        } else {
            options.language =
                item.language == "ru" ? tts_front::Language::Russian : tts_front::Language::English;
        }
        const auto result = frontend.process(item.input, options);
        if (result.original_text != item.input) {
            std::cerr << "original mismatch " << item.id << "\n";
            return EXIT_FAILURE;
        }
        for (const auto& warning : result.warnings) {
            if (warning.offset > item.input.size() ||
                warning.length > item.input.size() - warning.offset) {
                std::cerr << "warning span mismatch " << item.id << "\n";
                return EXIT_FAILURE;
            }
        }
        const auto outcome = classify(item, result.normalized_text);
        ++actual_outcomes[outcome_name(outcome)];
        const auto actual_diagnostics = [&] {
            if (result.warnings.empty())
                return std::string("none");
            std::ostringstream output;
            for (std::size_t index = 0; index < result.warnings.size(); ++index) {
                if (index != 0)
                    output << ';';
                output << tts_front::to_string(result.warnings[index].code) << '@'
                       << result.warnings[index].offset << ':' << result.warnings[index].length;
            }
            return output.str();
        }();
        if (actual_diagnostics != item.diagnostics) {
            const auto exception = snapshot.diagnostic_exceptions.find(item.id);
            if (strict && (exception == snapshot.diagnostic_exceptions.end() ||
                           exception->second != actual_diagnostics)) {
                std::cerr << "strict diagnostic mismatch " << item.id << "\n";
                return EXIT_FAILURE;
            }
            actual_diagnostic_exceptions.insert(item.id);
        }
        if (outcome == Outcome::Unnecessary || outcome == Outcome::Incorrect) {
            actual_mismatches.insert(item.id);
            actual_mismatch_outputs.emplace(
                item.id, std::make_pair(result.normalized_text, result.pronunciation_text));
        }
        if (outcome != Outcome::Unnecessary && outcome != Outcome::Incorrect &&
            (result.normalized_text != item.expected ||
             result.pronunciation_text != item.expected)) {
            std::cerr << "expected output mismatch " << item.id << "\n";
            return EXIT_FAILURE;
        }
        ++cases;
    }
    if (strict) {
        std::set<std::string> expected_diagnostic_exceptions;
        for (const auto& entry : snapshot.diagnostic_exceptions)
            expected_diagnostic_exceptions.insert(entry.first);
        if (actual_outcomes != snapshot.outcomes || actual_mismatches != snapshot.mismatches ||
            actual_mismatch_outputs != snapshot.mismatch_outputs ||
            actual_diagnostic_exceptions != expected_diagnostic_exceptions) {
            std::cerr << "strict archive snapshot mismatch\n";
            return EXIT_FAILURE;
        }
    }
    if (cases != snapshot.cases || snapshot.mismatch_outputs.size() != snapshot.mismatches.size()) {
        std::cerr << "archive metadata mismatch\n";
        return EXIT_FAILURE;
    }
    for (const auto& id : snapshot.mismatches) {
        if (corpus_ids.find(id) == corpus_ids.end() ||
            snapshot.mismatch_outputs.find(id) == snapshot.mismatch_outputs.end()) {
            std::cerr << "archive snapshot references unknown mismatch " << id << "\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << archive_version << " archive execution audit: " << cases << " cases verified\n";
    return EXIT_SUCCESS;
}
