#include "utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

bool whitespace(std::uint32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n';
}
bool punctuation(std::uint32_t cp) {
    return (cp < 128 && ((cp >= 33 && cp <= 47) || (cp >= 58 && cp <= 64) ||
                         (cp >= 91 && cp <= 96) || (cp >= 123 && cp <= 126))) ||
           cp == 0xab || cp == 0xbb || cp == 0x2014;
}

struct Vocabulary {
    std::unordered_map<std::string, int> ids;
    int unk = 1;
    int cls = 2;
    int sep = 3;
};

Vocabulary load(const std::string& path) {
    Vocabulary vocab;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open bert-vocab.tsv");
    }
    std::string line;
    while (std::getline(input, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            throw std::runtime_error("malformed vocab line");
        }
        vocab.ids[line.substr(tab + 1)] = std::stoi(line.substr(0, tab));
    }
    return vocab;
}

std::vector<std::string> basic_tokens(const std::string& sentence) {
    std::vector<std::string> result;
    std::string current;
    auto flush = [&] {
        if (!current.empty()) {
            result.push_back(current);
            current.clear();
        }
    };
    const auto codepoints = silero_native::decode_utf8(sentence);
    for (std::size_t index = 0; index < codepoints.size();) {
        if (codepoints[index] == '[') {
            const std::vector<std::uint32_t> homo_start{'[', 'H', 'O', 'M', 'O', ']'};
            const std::vector<std::uint32_t> homo_end{'[', '/', 'H', 'O', 'M', 'O', ']'};
            if (index + homo_start.size() <= codepoints.size() &&
                std::equal(homo_start.begin(),
                           homo_start.end(),
                           codepoints.begin() + static_cast<std::ptrdiff_t>(index))) {
                flush();
                result.emplace_back("[HOMO]");
                index += homo_start.size();
                continue;
            }
            if (index + homo_end.size() <= codepoints.size() &&
                std::equal(homo_end.begin(),
                           homo_end.end(),
                           codepoints.begin() + static_cast<std::ptrdiff_t>(index))) {
                flush();
                result.emplace_back("[/HOMO]");
                index += homo_end.size();
                continue;
            }
        }
        const auto cp = codepoints[index++];
        if (whitespace(cp)) {
            flush();
            continue;
        }
        if (punctuation(cp)) {
            flush();
            std::string token;
            silero_native::append_utf8(token, cp);
            result.push_back(token);
            continue;
        }
        silero_native::append_utf8(current, cp);
    }
    flush();
    return result;
}

std::vector<int> wordpiece(const Vocabulary& vocab, const std::string& token) {
    const auto chars = silero_native::decode_utf8(token);
    std::vector<int> ids;
    if (token == "[HOMO]" || token == "[/HOMO]") {
        ids.push_back(vocab.ids.at(token));
        return ids;
    }
    std::size_t start = 0;
    while (start < chars.size()) {
        std::size_t end = chars.size();
        std::string found;
        int found_id = vocab.unk;
        while (start < end) {
            std::string candidate = start == 0 ? "" : "##";
            for (std::size_t i = start; i < end; ++i) {
                silero_native::append_utf8(candidate, chars[i]);
            }
            const auto it = vocab.ids.find(candidate);
            if (it != vocab.ids.end()) {
                found = candidate;
                found_id = it->second;
                break;
            }
            --end;
        }
        if (found.empty()) {
            return {vocab.unk};
        }
        ids.push_back(found_id);
        start = end;
    }
    return ids;
}

std::vector<int> encode(const Vocabulary& vocab, const std::string& sentence) {
    std::vector<int> result{vocab.cls};
    for (const auto& token : basic_tokens(sentence)) {
        const auto pieces = wordpiece(vocab, token);
        result.insert(result.end(), pieces.begin(), pieces.end());
    }
    result.push_back(vocab.sep);
    return result;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 || std::string(argv[1]) != "--vocab") {
            std::cerr << "usage: silero_native_wordpiece --vocab bert-vocab.tsv\n";
            return 2;
        }
        const auto vocab = load(argv[2]);
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            const auto ids = encode(vocab, line);
            std::cout << line << '\t';
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (i != 0) {
                    std::cout << ',';
                }
                std::cout << ids[i];
            }
            std::cout << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native wordpiece failed: " << error.what() << '\n';
        return 1;
    }
}
