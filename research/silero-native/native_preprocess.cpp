#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct AssetTable {
    std::unordered_map<std::string, std::size_t> ids;
    std::vector<float> weights;
    std::size_t dimension = 0;
};

std::vector<std::uint32_t> decode_utf8(const std::string& text) {
    std::vector<std::uint32_t> result;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80) {
            result.push_back(lead);
            ++i;
        } else if ((lead & 0xe0) == 0xc0 && i + 1 < text.size()) {
            result.push_back(((lead & 0x1f) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3f));
            i += 2;
        } else if ((lead & 0xf0) == 0xe0 && i + 2 < text.size()) {
            result.push_back(((lead & 0x0f) << 12) |
                             ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6) |
                             (static_cast<unsigned char>(text[i + 2]) & 0x3f));
            i += 3;
        } else if ((lead & 0xf8) == 0xf0 && i + 3 < text.size()) {
            result.push_back(((lead & 0x07) << 18) |
                             ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 12) |
                             ((static_cast<unsigned char>(text[i + 2]) & 0x3f) << 6) |
                             (static_cast<unsigned char>(text[i + 3]) & 0x3f));
            i += 4;
        } else {
            throw std::runtime_error("invalid UTF-8 input");
        }
    }
    return result;
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint < 0x80) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint < 0x10000) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::string lower_ru(const std::string& word) {
    std::string result;
    for (auto codepoint : decode_utf8(word)) {
        if (codepoint >= 0x410 && codepoint <= 0x42f) {
            codepoint += 0x20;
        } else if (codepoint == 0x401) {
            codepoint = 0x451;
        }
        append_utf8(result, codepoint);
    }
    return result;
}

std::vector<std::string> word_ngrams(const std::string& word) {
    auto codepoints = decode_utf8(lower_ru(word));
    std::vector<std::uint32_t> padded;
    padded.reserve(codepoints.size() + 2);
    padded.push_back('<');
    padded.insert(padded.end(), codepoints.begin(), codepoints.end());
    padded.push_back('>');
    std::vector<std::string> result;
    for (std::size_t size = 1; size <= codepoints.size() + 3; ++size) {
        for (std::size_t start = 0; start + size <= padded.size(); ++start) {
            std::string gram;
            for (std::size_t i = start; i < start + size; ++i) {
                append_utf8(gram, padded[i]);
            }
            result.push_back(std::move(gram));
        }
    }
    return result;
}

AssetTable load_assets(const std::string& ngram_path, const std::string& weights_path, std::size_t dimension) {
    AssetTable assets;
    assets.dimension = dimension;
    std::ifstream ngrams(ngram_path, std::ios::binary);
    if (!ngrams) throw std::runtime_error("cannot open ngrams.tsv");
    std::string line;
    while (std::getline(ngrams, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) throw std::runtime_error("malformed ngram line");
        assets.ids[line.substr(tab + 1)] = static_cast<std::size_t>(std::stoull(line.substr(0, tab)));
    }
    std::ifstream weights(weights_path, std::ios::binary);
    if (!weights) throw std::runtime_error("cannot open embedding.f32");
    std::vector<char> bytes{std::istreambuf_iterator<char>(weights), std::istreambuf_iterator<char>()};
    if (bytes.size() % sizeof(float) != 0) throw std::runtime_error("weights are not float32");
    assets.weights.resize(bytes.size() / sizeof(float));
    std::memcpy(assets.weights.data(), bytes.data(), bytes.size());
    if (assets.weights.size() % dimension != 0) throw std::runtime_error("weights do not match dimension");
    return assets;
}

std::vector<float> embed(const AssetTable& assets, const std::string& word) {
    std::vector<float> output(assets.dimension, 0.0f);
    std::size_t count = 0;
    for (const auto& gram : word_ngrams(word)) {
        const auto found = assets.ids.find(gram);
        if (found == assets.ids.end()) continue;
        const auto offset = found->second * assets.dimension;
        if (offset + assets.dimension > assets.weights.size()) throw std::runtime_error("ngram id outside weights");
        for (std::size_t i = 0; i < assets.dimension; ++i) output[i] += assets.weights[offset + i];
        ++count;
    }
    if (count == 0) {
        const auto found = assets.ids.find("UNK");
        if (found == assets.ids.end()) throw std::runtime_error("missing UNK ngram");
        const auto offset = found->second * assets.dimension;
        for (std::size_t i = 0; i < assets.dimension; ++i) output[i] = assets.weights[offset + i];
        return output;
    }
    for (auto& value : output) value /= static_cast<float>(count);
    return output;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5 || std::string(argv[1]) != "--ngrams" || std::string(argv[3]) != "--weights") {
            std::cerr << "usage: silero_native_preprocess --ngrams FILE --weights FILE\n";
            return 2;
        }
        const auto assets = load_assets(argv[2], argv[4], 16);
        std::string word;
        while (std::getline(std::cin, word)) {
            if (word.empty()) continue;
            const auto vector = embed(assets, word);
            std::cout << word;
            std::cout << std::setprecision(9);
            for (const auto value : vector) std::cout << '\t' << value;
            std::cout << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native preprocessing failed: " << error.what() << '\n';
        return 1;
    }
}
