#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Codepoints = std::vector<std::uint32_t>;

Codepoints decode(const std::string& text) {
    Codepoints out;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80) { out.push_back(lead); ++i; }
        else if ((lead & 0xe0) == 0xc0) { out.push_back(((lead & 0x1f) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3f)); i += 2; }
        else if ((lead & 0xf0) == 0xe0) { out.push_back(((lead & 0x0f) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6) | (static_cast<unsigned char>(text[i + 2]) & 0x3f)); i += 3; }
        else if ((lead & 0xf8) == 0xf0) { out.push_back(((lead & 7) << 18) | ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 12) | ((static_cast<unsigned char>(text[i + 2]) & 0x3f) << 6) | (static_cast<unsigned char>(text[i + 3]) & 0x3f)); i += 4; }
        else throw std::runtime_error("invalid UTF-8");
    }
    return out;
}

void append(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) { out.push_back(static_cast<char>(0xc0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 63))); }
    else { out.push_back(static_cast<char>(0xe0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63))); out.push_back(static_cast<char>(0x80 | (cp & 63))); }
}

bool whitespace(std::uint32_t cp) { return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n'; }
bool punctuation(std::uint32_t cp) {
    return (cp < 128 && ((cp >= 33 && cp <= 47) || (cp >= 58 && cp <= 64) || (cp >= 91 && cp <= 96) || (cp >= 123 && cp <= 126))) || cp == 0xab || cp == 0xbb || cp == 0x2014;
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
    if (!input) throw std::runtime_error("cannot open bert-vocab.tsv");
    std::string line;
    while (std::getline(input, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) throw std::runtime_error("malformed vocab line");
        vocab.ids[line.substr(tab + 1)] = std::stoi(line.substr(0, tab));
    }
    return vocab;
}

std::vector<std::string> basic_tokens(const std::string& sentence) {
    std::vector<std::string> result;
    std::string current;
    auto flush = [&] { if (!current.empty()) { result.push_back(current); current.clear(); } };
    const auto codepoints = decode(sentence);
    for (std::size_t index = 0; index < codepoints.size();) {
        if (codepoints[index] == '[') {
            const std::vector<std::uint32_t> homo_start{'[', 'H', 'O', 'M', 'O', ']'};
            const std::vector<std::uint32_t> homo_end{'[', '/', 'H', 'O', 'M', 'O', ']'};
            if (index + homo_start.size() <= codepoints.size() &&
                std::equal(homo_start.begin(), homo_start.end(), codepoints.begin() + static_cast<std::ptrdiff_t>(index))) {
                flush();
                result.emplace_back("[HOMO]");
                index += homo_start.size();
                continue;
            }
            if (index + homo_end.size() <= codepoints.size() &&
                std::equal(homo_end.begin(), homo_end.end(), codepoints.begin() + static_cast<std::ptrdiff_t>(index))) {
                flush();
                result.emplace_back("[/HOMO]");
                index += homo_end.size();
                continue;
            }
        }
        const auto cp = codepoints[index++];
        if (whitespace(cp)) { flush(); continue; }
        if (punctuation(cp)) { flush(); std::string token; append(token, cp); result.push_back(token); continue; }
        append(current, cp);
    }
    flush();
    return result;
}

std::vector<int> wordpiece(const Vocabulary& vocab, const std::string& token) {
    const auto chars = decode(token);
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
            for (std::size_t i = start; i < end; ++i) append(candidate, chars[i]);
            const auto it = vocab.ids.find(candidate);
            if (it != vocab.ids.end()) { found = candidate; found_id = it->second; break; }
            --end;
        }
        if (found.empty()) return {vocab.unk};
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

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 || std::string(argv[1]) != "--vocab") {
            std::cerr << "usage: silero_native_wordpiece --vocab bert-vocab.tsv\n";
            return 2;
        }
        const auto vocab = load(argv[2]);
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            const auto ids = encode(vocab, line);
            std::cout << line << '\t';
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (i != 0) std::cout << ',';
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
