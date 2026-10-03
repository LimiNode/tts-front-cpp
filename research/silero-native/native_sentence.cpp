#include "utf8.hpp"

#include <onnxruntime_cxx_api.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Codepoints = std::vector<std::uint32_t>;

struct Token {
    std::string raw;
    std::string clean;
    bool process = false;
    std::size_t byte_start = 0;
    std::size_t byte_end = 0;
};

struct PhraseRule {
    std::string word;
    std::string marked;
    std::string variant;
};

struct Bundle {
    std::unordered_map<std::string, std::size_t> ngram_ids;
    std::vector<float> weights;
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<std::string, std::pair<int, int>> exceptions;
    std::unordered_map<std::string, std::array<std::string, 2>> homographs;
    std::vector<PhraseRule> phrase_rules;
    std::size_t dimension = 0;
    int pad_id = 0;
    int unk_id = 1;
    int cls_id = 2;
    int sep_id = 3;
    int homo_start_id = 0;
    int homo_end_id = 0;
};

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (true) {
        const auto tab = line.find('\t', begin);
        fields.push_back(line.substr(begin, tab == std::string::npos ? tab : tab - begin));
        if (tab == std::string::npos) {
            return fields;
        }
        begin = tab + 1;
    }
}

std::ifstream open_required(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("missing native asset: " + path.string());
    }
    return stream;
}

void require_manifest(const std::filesystem::path& path) {
    auto manifest = open_required(path);
    const std::string content((std::istreambuf_iterator<char>(manifest)), {});
    if (content.find("silero_native_asset_manifest") == std::string::npos ||
        content.find("source_revision") == std::string::npos ||
        content.find("sha256") == std::string::npos) {
        throw std::runtime_error("invalid native asset manifest");
    }
}

std::unordered_map<std::string, std::size_t> load_ids(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::size_t> ids;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 2 || fields[0].empty() || fields[1].empty()) {
            throw std::runtime_error("malformed indexed asset: " + path.string());
        }
        std::size_t consumed = 0;
        const auto id = std::stoull(fields[0], &consumed);
        if (consumed != fields[0].size()) {
            throw std::runtime_error("malformed indexed asset id");
        }
        ids.emplace(fields[1], static_cast<std::size_t>(id));
    }
    return ids;
}

std::unordered_map<std::string, int> load_vocab(const std::filesystem::path& path) {
    const auto raw = load_ids(path);
    std::unordered_map<std::string, int> result;
    for (const auto& item : raw) {
        if (item.second > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("vocabulary id outside int range");
        }
        result.emplace(item.first, static_cast<int>(item.second));
    }
    return result;
}

std::unordered_map<std::string, std::pair<int, int>> load_exceptions(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::pair<int, int>> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 3 || fields[0].empty()) {
            throw std::runtime_error("malformed exceptions asset");
        }
        result.emplace(fields[0], std::make_pair(std::stoi(fields[1]), std::stoi(fields[2])));
    }
    return result;
}

std::unordered_map<std::string, std::array<std::string, 2>> load_homographs(
    const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::array<std::string, 2>> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 3 || fields[0].empty() || fields[1].empty() || fields[2].empty()) {
            throw std::runtime_error("malformed homographs asset");
        }
        result.emplace(fields[0], std::array<std::string, 2>{fields[1], fields[2]});
    }
    return result;
}

std::vector<PhraseRule> load_phrase_rules(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::vector<PhraseRule> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 3 || fields[0].empty() || fields[1].empty() || fields[2].empty()) {
            throw std::runtime_error("malformed phrase rules asset");
        }
        result.push_back({fields[0], fields[1], fields[2]});
    }
    return result;
}

Bundle load_bundle(const std::filesystem::path& root) {
    require_manifest(root / "manifest.json");
    Bundle bundle;
    bundle.ngram_ids = load_ids(root / "ngrams.tsv");
    bundle.vocab = load_vocab(root / "bert-vocab.tsv");
    bundle.exceptions = load_exceptions(root / "exceptions.tsv");
    bundle.homographs = load_homographs(root / "homodict.tsv");
    bundle.phrase_rules = load_phrase_rules(root / "phrase-rules.tsv");
    auto weights = open_required(root / "embedding.f32");
    std::vector<char> bytes{std::istreambuf_iterator<char>(weights), std::istreambuf_iterator<char>()};
    if (bytes.size() % sizeof(float) != 0) {
        throw std::runtime_error("embedding weights are not float32");
    }
    bundle.dimension = 16;
    bundle.weights.resize(bytes.size() / sizeof(float));
    std::memcpy(bundle.weights.data(), bytes.data(), bytes.size());
    if (bundle.weights.size() % bundle.dimension != 0) {
        throw std::runtime_error("embedding weights have invalid dimension");
    }
    const auto find_id = [&](const std::string& token) {
        const auto found = bundle.vocab.find(token);
        if (found == bundle.vocab.end()) {
            throw std::runtime_error("missing special vocabulary token: " + token);
        }
        return found->second;
    };
    bundle.pad_id = find_id("[PAD]");
    bundle.unk_id = find_id("[UNK]");
    bundle.cls_id = find_id("[CLS]");
    bundle.sep_id = find_id("[SEP]");
    bundle.homo_start_id = find_id("[HOMO]");
    bundle.homo_end_id = find_id("[/HOMO]");
    return bundle;
}

std::string lower_ru(const std::string& text) {
    std::string result;
    for (auto codepoint : silero_native::decode_utf8(text)) {
        if (codepoint >= 0x410 && codepoint <= 0x42f) {
            codepoint += 0x20;
        } else if (codepoint == 0x401) {
            codepoint = 0x451;
        }
        silero_native::append_utf8(result, codepoint);
    }
    return result;
}

std::string clean_word(const std::string& text) {
    std::string result;
    for (auto codepoint : silero_native::decode_utf8(lower_ru(text))) {
        if ((codepoint >= 0x430 && codepoint <= 0x44f) || codepoint == 0x451) {
            silero_native::append_utf8(result, codepoint);
        }
    }
    return result;
}

bool delimiter(std::uint32_t codepoint) {
    return codepoint == ' ' || codepoint == '\t' || codepoint == '\r' || codepoint == '\n' ||
           codepoint == '.' || codepoint == ',' || codepoint == '!' || codepoint == '?' ||
           codepoint == ';' || codepoint == ':' || codepoint == '<' || codepoint == '>' ||
           codepoint == '=' || codepoint == '(' || codepoint == ')' || codepoint == '/' ||
           codepoint == '\\';
}

std::vector<Token> tokenize(const std::string& sentence) {
    std::vector<Token> result;
    const auto codepoints = silero_native::decode_utf8(sentence);
    std::size_t byte_offset = 0;
    std::size_t begin = 0;
    std::string current;
    auto flush_word = [&](std::size_t end) {
        if (current.empty()) {
            return;
        }
        std::vector<std::string> parts;
        std::size_t part_begin = 0;
        while (true) {
            const auto dash = current.find('-', part_begin);
            parts.push_back(current.substr(part_begin, dash == std::string::npos ? dash : dash - part_begin));
            if (dash == std::string::npos) {
                break;
            }
            part_begin = dash + 1;
        }
        std::size_t local = begin;
        for (std::size_t index = 0; index < parts.size(); ++index) {
            std::string raw = parts[index];
            if (index + 1 < parts.size()) {
                raw.push_back('-');
            }
            const auto clean = clean_word(raw);
            result.push_back({raw, clean, !clean.empty() && (index + 1 < parts.size() || clean != "то"), local,
                              local + raw.size()});
            local += raw.size();
        }
        current.clear();
        begin = end;
    };
    for (const auto codepoint : codepoints) {
        std::string encoded;
        silero_native::append_utf8(encoded, codepoint);
        if (delimiter(codepoint)) {
            flush_word(byte_offset);
            result.push_back({encoded, "", false, byte_offset, byte_offset + encoded.size()});
            begin = byte_offset + encoded.size();
        } else {
            if (current.empty()) {
                begin = byte_offset;
            }
            current += encoded;
        }
        byte_offset += encoded.size();
    }
    flush_word(sentence.size());
    return result;
}

std::vector<std::string> word_ngrams(const std::string& word) {
    const auto codepoints = silero_native::decode_utf8(word);
    std::vector<std::uint32_t> padded{ '<' };
    padded.insert(padded.end(), codepoints.begin(), codepoints.end());
    padded.push_back('>');
    std::vector<std::string> result;
    for (std::size_t size = 1; size <= codepoints.size() + 3; ++size) {
        for (std::size_t start = 0; start + size <= padded.size(); ++start) {
            std::string gram;
            for (std::size_t index = start; index < start + size; ++index) {
                silero_native::append_utf8(gram, padded[index]);
            }
            result.push_back(std::move(gram));
        }
    }
    return result;
}

std::vector<float> embed(const Bundle& bundle, const std::string& word) {
    std::vector<float> output(bundle.dimension, 0.0f);
    std::size_t count = 0;
    for (const auto& gram : word_ngrams(lower_ru(word))) {
        const auto found = bundle.ngram_ids.find(gram);
        if (found == bundle.ngram_ids.end()) {
            continue;
        }
        if (found->second >= bundle.weights.size() / bundle.dimension) {
            throw std::runtime_error("ngram id outside embedding weights");
        }
        const auto offset = found->second * bundle.dimension;
        for (std::size_t index = 0; index < bundle.dimension; ++index) {
            output[index] += bundle.weights[offset + index];
        }
        ++count;
    }
    if (count == 0) {
        const auto found = bundle.ngram_ids.find("UNK");
        if (found == bundle.ngram_ids.end() || found->second >= bundle.weights.size() / bundle.dimension) {
            throw std::runtime_error("missing or invalid UNK embedding");
        }
        const auto offset = found->second * bundle.dimension;
        for (std::size_t index = 0; index < bundle.dimension; ++index) {
            output[index] = bundle.weights[offset + index];
        }
        return output;
    }
    for (auto& value : output) {
        value /= static_cast<float>(count);
    }
    return output;
}

class WordPiece {
public:
    explicit WordPiece(const Bundle& bundle) : bundle_(bundle) {}

    std::vector<std::int64_t> encode(const std::string& sentence) const {
        std::vector<std::int64_t> result{bundle_.cls_id};
        for (const auto& token : basic_tokens(sentence)) {
            const auto ids = encode_token(token);
            result.insert(result.end(), ids.begin(), ids.end());
        }
        result.push_back(bundle_.sep_id);
        return result;
    }

private:
    std::vector<std::string> basic_tokens(const std::string& sentence) const {
        std::vector<std::string> result;
        std::string current;
        auto flush = [&] {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        };
        for (std::size_t offset = 0; offset < sentence.size();) {
            if (sentence.compare(offset, 6, "[HOMO]") == 0) {
                flush();
                result.emplace_back("[HOMO]");
                offset += 6;
                continue;
            }
            if (sentence.compare(offset, 7, "[/HOMO]") == 0) {
                flush();
                result.emplace_back("[/HOMO]");
                offset += 7;
                continue;
            }
            const auto codepoint = silero_native::decode_utf8(sentence.substr(offset)).front();
            std::string encoded;
            silero_native::append_utf8(encoded, codepoint);
            if (codepoint == ' ' || codepoint == '\t' || codepoint == '\r' || codepoint == '\n') {
                flush();
            } else if (codepoint < 128 && ((codepoint >= 33 && codepoint <= 47) ||
                                           (codepoint >= 58 && codepoint <= 64) ||
                                           (codepoint >= 91 && codepoint <= 96) ||
                                           (codepoint >= 123 && codepoint <= 126))) {
                flush();
                std::string punctuation;
                silero_native::append_utf8(punctuation, codepoint);
                result.push_back(punctuation);
            } else {
                silero_native::append_utf8(current, codepoint);
            }
            offset += encoded.size();
        }
        flush();
        return result;
    }

    std::vector<std::int64_t> encode_token(const std::string& token) const {
        if (token == "[HOMO]" || token == "[/HOMO]") {
            return {bundle_.vocab.at(token)};
        }
        const auto chars = silero_native::decode_utf8(token);
        std::vector<std::int64_t> ids;
        std::size_t start = 0;
        while (start < chars.size()) {
            std::size_t end = chars.size();
            std::string found;
            int found_id = bundle_.unk_id;
            while (start < end) {
                std::string candidate = start == 0 ? "" : "##";
                for (std::size_t index = start; index < end; ++index) {
                    silero_native::append_utf8(candidate, chars[index]);
                }
                const auto item = bundle_.vocab.find(candidate);
                if (item != bundle_.vocab.end()) {
                    found = candidate;
                    found_id = item->second;
                    break;
                }
                --end;
            }
            if (found.empty()) {
                return {bundle_.unk_id};
            }
            ids.push_back(found_id);
            start = end;
        }
        return ids;
    }

    const Bundle& bundle_;
};

std::vector<std::string> session_input_names(Ort::Session& session, Ort::AllocatorWithDefaultOptions& allocator) {
    std::vector<std::string> names;
    for (std::size_t index = 0; index < session.GetInputCount(); ++index) {
        auto name = session.GetInputNameAllocated(index, allocator);
        names.emplace_back(name.get());
    }
    return names;
}

std::vector<Ort::Value> run_outputs(Ort::Session& session, std::vector<Ort::Value>& inputs,
                                    const std::vector<std::string>& names) {
    Ort::AllocatorWithDefaultOptions allocator;
    const auto outputs = [&] {
        std::vector<std::string> result;
        for (std::size_t index = 0; index < session.GetOutputCount(); ++index) {
            auto name = session.GetOutputNameAllocated(index, allocator);
            result.emplace_back(name.get());
        }
        return result;
    }();
    std::vector<const char*> input_ptrs;
    std::vector<const char*> output_ptrs;
    for (const auto& name : names) input_ptrs.push_back(name.c_str());
    for (const auto& name : outputs) output_ptrs.push_back(name.c_str());
    return session.Run(Ort::RunOptions{nullptr}, input_ptrs.data(), inputs.data(), inputs.size(),
                       output_ptrs.data(), output_ptrs.size());
}

std::vector<float> run_classifier(Ort::Session& session, const std::vector<float>& values, std::size_t rows,
                                  std::size_t columns, std::size_t expected_output_columns,
                                  Ort::MemoryInfo& memory) {
    const std::array<std::int64_t, 2> input_shape{static_cast<std::int64_t>(rows), static_cast<std::int64_t>(columns)};
    std::vector<Ort::Value> inputs;
    inputs.emplace_back(Ort::Value::CreateTensor<float>(memory, const_cast<float*>(values.data()), values.size(),
                                                        input_shape.data(), input_shape.size()));
    Ort::AllocatorWithDefaultOptions allocator;
    const auto names = session_input_names(session, allocator);
    auto outputs = run_outputs(session, inputs, names);
    if (outputs.empty()) {
        throw std::runtime_error("classifier returned no outputs");
    }
    const auto info = outputs.at(0).GetTensorTypeAndShapeInfo();
    const auto output_shape = info.GetShape();
    if (output_shape.size() != 2 || output_shape[0] != static_cast<std::int64_t>(rows) ||
        output_shape[1] != static_cast<std::int64_t>(expected_output_columns)) {
        throw std::runtime_error("unexpected classifier output shape");
    }
    const auto count = info.GetElementCount();
    const auto* data = outputs.at(0).GetTensorData<float>();
    return std::vector<float>(data, data + count);
}

std::vector<float> run_homograph(Ort::Session& session, const std::vector<std::int64_t>& ids,
                                 const std::vector<std::int64_t>& starts, const std::vector<std::int64_t>& ends,
                                 std::size_t sequence, Ort::MemoryInfo& memory) {
    const std::array<std::int64_t, 2> input_shape{1, static_cast<std::int64_t>(sequence)};
    const std::array<std::int64_t, 1> span_shape{1};
    std::vector<Ort::Value> inputs;
    inputs.emplace_back(Ort::Value::CreateTensor<std::int64_t>(memory, const_cast<std::int64_t*>(ids.data()), ids.size(),
                                                               input_shape.data(), input_shape.size()));
    inputs.emplace_back(Ort::Value::CreateTensor<std::int64_t>(memory, const_cast<std::int64_t*>(starts.data()), 1,
                                                               span_shape.data(), 1));
    inputs.emplace_back(Ort::Value::CreateTensor<std::int64_t>(memory, const_cast<std::int64_t*>(ends.data()), 1,
                                                               span_shape.data(), 1));
    Ort::AllocatorWithDefaultOptions allocator;
    const auto names = session_input_names(session, allocator);
    auto outputs = run_outputs(session, inputs, names);
    if (outputs.empty()) {
        throw std::runtime_error("homosolver returned no outputs");
    }
    const auto info = outputs.at(0).GetTensorTypeAndShapeInfo();
    const auto shape = info.GetShape();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape.size() != 2 || shape[0] != 1 ||
        shape[1] != 1 || info.GetElementCount() != 1) {
        throw std::runtime_error("unexpected homosolver output shape");
    }
    const auto* data = outputs.at(0).GetTensorData<float>();
    return {data[0]};
}

std::string clean_context(const std::string& text, bool start) {
    std::string filtered;
    for (const auto codepoint : silero_native::decode_utf8(text)) {
        if ((codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 'A' && codepoint <= 'Z') ||
            (codepoint >= '0' && codepoint <= '9') || codepoint == ' ' || codepoint == '.' || codepoint == '!' ||
            codepoint == '?' || codepoint == ',' || codepoint == '-') {
            silero_native::append_utf8(filtered, codepoint);
        } else if ((codepoint >= 0x410 && codepoint <= 0x44f) || codepoint == 0x401 || codepoint == 0x451) {
            silero_native::append_utf8(filtered, codepoint);
        }
    }
    std::string collapsed;
    bool space = false;
    for (const auto codepoint : silero_native::decode_utf8(filtered)) {
        if (codepoint == ' ') {
            space = true;
        } else {
            if (space && !collapsed.empty()) collapsed.push_back(' ');
            space = false;
            silero_native::append_utf8(collapsed, codepoint);
        }
    }
    if (start) {
        while (!collapsed.empty() && std::string(" .,!?-").find(collapsed.front()) != std::string::npos) {
            collapsed.erase(collapsed.begin());
        }
        const auto codepoints = silero_native::decode_utf8(collapsed);
        if (!codepoints.empty()) {
            auto titled = codepoints;
            if (titled.front() >= 0x430 && titled.front() <= 0x44f) {
                titled.front() -= 0x20;
            } else if (titled.front() == 0x451) {
                titled.front() = 0x401;
            }
            std::string result;
            for (std::size_t index = 0; index < titled.size(); ++index) {
                auto codepoint = titled[index];
                if (index != 0) {
                    if (codepoint >= 0x410 && codepoint <= 0x42f) {
                        codepoint += 0x20;
                    } else if (codepoint == 0x401) {
                        codepoint = 0x451;
                    }
                }
                silero_native::append_utf8(result, codepoint);
            }
            return result;
        }
        return collapsed;
    }
    collapsed = lower_ru(collapsed);
    if (collapsed.empty() || std::string(".!?").find(collapsed.back()) == std::string::npos) {
        collapsed.push_back('.');
    }
    return collapsed;
}

std::string marked_context(const std::string& sentence, const Token& token) {
    const auto left = clean_context(sentence.substr(0, token.byte_start), true);
    const auto right = clean_context(sentence.substr(token.byte_end), false);
    return left + " [HOMO] " + token.clean + " [/HOMO] " + right;
}

bool is_vowel(std::uint32_t codepoint) {
    return codepoint == 0x430 || codepoint == 0x43e || codepoint == 0x443 || codepoint == 0x44b ||
           codepoint == 0x44d || codepoint == 0x438 || codepoint == 0x435 || codepoint == 0x44f ||
           codepoint == 0x451 || codepoint == 0x44e;
}

Codepoints accentuate_word(const std::string& raw, const std::string& clean, int stress_id, int yo_id,
                           const std::pair<int, int>* exception, const std::vector<float>& stress_logits,
                           const std::vector<float>& yo_logits) {
    auto output = silero_native::decode_utf8(raw);
    const auto lower = silero_native::decode_utf8(lower_ru(raw));
    const bool have_stress = std::find(output.begin(), output.end(), '+') != output.end();
    const auto e = static_cast<std::uint32_t>(0x435);
    const auto yo = static_cast<std::uint32_t>(0x451);
    if (exception != nullptr && !have_stress) {
        if (exception->second >= 0 && static_cast<std::size_t>(exception->second) < output.size()) {
            output[exception->second] = output[exception->second] == 0x415 ? 0x401 : yo;
        }
        if (exception->first >= 0 && static_cast<std::size_t>(exception->first) <= output.size()) {
            output.insert(output.begin() + exception->first, '+');
        }
        return output;
    }
    const bool have_yo = std::find(lower.begin(), lower.end(), yo) != lower.end();
    if (have_yo && !have_stress) {
        std::vector<std::uint32_t> result;
        for (const auto codepoint : output) {
            if (codepoint == yo || codepoint == 0x401) result.push_back('+');
            result.push_back(codepoint);
        }
        return result;
    }
    std::vector<int> vowels;
    std::vector<int> e_positions;
    for (std::size_t index = 0; index < lower.size(); ++index) {
        if (lower[index] == e) e_positions.push_back(static_cast<int>(index));
        if (lower[index] == e || lower[index] == yo || lower[index] == 0x430 || lower[index] == 0x43e ||
            lower[index] == 0x443 || lower[index] == 0x44b || lower[index] == 0x44d || lower[index] == 0x438 ||
            lower[index] == 0x443 || lower[index] == 0x44f || lower[index] == 0x44e) {
            vowels.push_back(static_cast<int>(index));
        }
    }
    if (vowels.empty()) return output;
    std::vector<int> stress_ids;
    if (have_stress) {
        int before = 0;
        for (const auto codepoint : lower) {
            if (codepoint == '+') {
                stress_ids.push_back(before);
            } else if (is_vowel(codepoint)) {
                ++before;
            }
        }
    } else if (stress_id >= 0 && stress_id < static_cast<int>(vowels.size())) {
        stress_ids.push_back(stress_id);
    }
    std::vector<int> stress_positions;
    for (const auto id : stress_ids) {
        if (id >= 0 && id < static_cast<int>(vowels.size())) {
            stress_positions.push_back(vowels[id]);
        }
    }
    const auto yo_probability = [&](const std::vector<float>& logits) {
        const auto maximum = *std::max_element(logits.begin(), logits.end());
        float sum = 0.0f;
        for (const auto value : logits) sum += std::exp(value - maximum);
        return std::exp(logits[yo_id] - maximum) / sum > 0.5f;
    };
    if (!e_positions.empty() && yo_id > 0 && yo_id <= static_cast<int>(e_positions.size()) &&
        yo_probability(yo_logits)) {
        const auto position = e_positions[yo_id - 1];
        if (std::find(stress_positions.begin(), stress_positions.end(), position) != stress_positions.end()) {
            output[position] = output[position] == 0x415 ? 0x401 : yo;
        }
    }
    bool set_stress = !have_stress;
    if (vowels.size() == 1) {
        stress_positions = {vowels.front()};
        set_stress = true;
    }
    if (!have_stress && set_stress && !stress_positions.empty()) {
        std::sort(stress_positions.begin(), stress_positions.end());
        int shift = 0;
        for (const auto position : stress_positions) {
            output.insert(output.begin() + position + shift, '+');
            ++shift;
        }
    }
    (void)clean;
    (void)stress_logits;
    return output;
}

std::string encode(const Codepoints& codepoints) {
    std::string result;
    for (const auto codepoint : codepoints) silero_native::append_utf8(result, codepoint);
    return result;
}

std::string preserve_case(const std::string& raw, const std::string& variant) {
    const auto source = silero_native::decode_utf8(raw);
    auto result = silero_native::decode_utf8(variant);
    if (!source.empty() && !result.empty()) {
        const auto uppercase = [](std::uint32_t codepoint) {
            return (codepoint >= 0x410 && codepoint <= 0x42f) || codepoint == 0x401;
        };
        if (uppercase(source.front())) {
            if (result.front() >= 0x430 && result.front() <= 0x44f) {
                result.front() -= 0x20;
            } else if (result.front() == 0x451) {
                result.front() = 0x401;
            }
        }
    }
    return encode(result);
}

} // namespace

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        const bool trace = argc == 10 && std::string(argv[9]) == "--trace";
        if ((argc != 9 && !trace) || std::string(argv[1]) != "--assets" ||
            std::string(argv[3]) != "--stress" || std::string(argv[5]) != "--yo" ||
            std::string(argv[7]) != "--homo") {
            std::cerr << "usage: silero_native_sentence --assets DIR --stress FILE --yo FILE --homo FILE [--trace]\n";
            return 2;
        }
        const auto bundle = load_bundle(argv[2]);
        Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "silero-native-sentence");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        const auto stress_path = std::filesystem::u8path(std::string(argv[4]));
        const auto yo_path = std::filesystem::u8path(std::string(argv[6]));
        const auto homo_path = std::filesystem::u8path(std::string(argv[8]));
        Ort::Session stress_session(environment, stress_path.c_str(), options);
        Ort::Session yo_session(environment, yo_path.c_str(), options);
        Ort::Session homo_session(environment, homo_path.c_str(), options);
        Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
        std::size_t line_begin = 0;
        while (line_begin < input.size()) {
            const auto line_end = input.find('\n', line_begin);
            std::string sentence = input.substr(line_begin, line_end == std::string::npos ? line_end : line_end - line_begin);
            if (!sentence.empty() && sentence.back() == '\r') {
                sentence.pop_back();
            }
            const auto source_tokens = tokenize(sentence);
            int model_route = 0;
            int exception_route = 0;
            int phrase_route = 0;
            int homosolver_route = 0;
            struct Replacement {
                std::size_t begin;
                std::size_t end;
                std::string value;
            };
            std::vector<Replacement> replacements;
            const WordPiece wordpiece(bundle);
            for (const auto& token : source_tokens) {
                const auto homograph = bundle.homographs.find(token.clean);
                if (!token.process || homograph == bundle.homographs.end()) {
                    continue;
                }
                const auto marked = marked_context(sentence, token);
                std::string variant;
                for (const auto& rule : bundle.phrase_rules) {
                    if (rule.word == token.clean && rule.marked == marked) {
                        variant = rule.variant;
                        break;
                    }
                }
                if (variant.empty()) {
                    ++homosolver_route;
                    const auto ids = wordpiece.encode(marked);
                    const auto start = std::find(ids.begin(), ids.end(), bundle.homo_start_id);
                    const auto end = std::find(ids.begin(), ids.end(), bundle.homo_end_id);
                    if (start == ids.end() || end == ids.end() || start >= end) {
                        throw std::runtime_error("homograph markers missing from native WordPiece output");
                    }
                    const auto logits = run_homograph(homo_session, ids,
                                                       {static_cast<std::int64_t>(std::distance(ids.begin(), start))},
                                                       {static_cast<std::int64_t>(std::distance(ids.begin(), end))},
                                                       ids.size(), memory);
                    const int prediction = 1.0f / (1.0f + std::exp(-logits.front())) >= 0.5f ? 1 : 0;
                    variant = homograph->second[static_cast<std::size_t>(prediction)];
                } else {
                    ++phrase_route;
                }
                replacements.push_back({token.byte_start, token.byte_end, preserve_case(token.raw, variant)});
            }
            if (!replacements.empty()) {
                std::string transformed;
                std::size_t cursor = 0;
                for (const auto& replacement : replacements) {
                    transformed += sentence.substr(cursor, replacement.begin - cursor);
                    transformed += replacement.value;
                    cursor = replacement.end;
                }
                transformed += sentence.substr(cursor);
                sentence = transformed;
            }
            const auto tokens = tokenize(sentence);
            std::vector<std::string> words;
            for (const auto& token : tokens) words.push_back(token.clean);
            std::vector<float> embeddings;
            for (const auto& word : words) {
                const auto row = embed(bundle, word);
                embeddings.insert(embeddings.end(), row.begin(), row.end());
            }
            const auto stress = run_classifier(stress_session, embeddings, words.size(), bundle.dimension, 10, memory);
            const auto yo = run_classifier(yo_session, embeddings, words.size(), bundle.dimension, 7, memory);
            std::string output;
            std::size_t row = 0;
            for (const auto& token : tokens) {
                if (!token.process) {
                    output += token.raw;
                    ++row;
                    continue;
                }
                const auto stress_begin = stress.begin() + static_cast<std::ptrdiff_t>(row * 10);
                const auto yo_begin = yo.begin() + static_cast<std::ptrdiff_t>(row * 7);
                const int stress_id = static_cast<int>(std::distance(stress_begin, std::max_element(stress_begin, stress_begin + 10)));
                const int yo_id = static_cast<int>(std::distance(yo_begin, std::max_element(yo_begin, yo_begin + 7)));
                const auto exception = bundle.exceptions.find(token.clean);
                const auto homograph = bundle.homographs.find(token.clean);
                if (exception != bundle.exceptions.end()) {
                    ++exception_route;
                } else if (homograph == bundle.homographs.end()) {
                    ++model_route;
                }
                const auto transformed = accentuate_word(token.raw, token.clean, stress_id, yo_id,
                                                         exception == bundle.exceptions.end() ? nullptr : &exception->second,
                                                         std::vector<float>(stress_begin, stress_begin + 10),
                                                         std::vector<float>(yo_begin, yo_begin + 7));
                output += encode(transformed);
                ++row;
            }
            std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
            std::cout.put('\n');
            if (trace) {
                std::cerr << "TRACE model=" << model_route << ";exception=" << exception_route
                          << ";phrase=" << phrase_route << ";homosolver=" << homosolver_route << '\n';
            }
            if (line_end == std::string::npos) {
                break;
            }
            line_begin = line_end + 1;
        }
        return 0;
    } catch (const Ort::Exception& error) {
        std::cerr << "ONNX Runtime failure: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "native sentence failed: " << error.what() << '\n';
        return 1;
    }
}
