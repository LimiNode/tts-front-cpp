#include "silero_sentence.hpp"

#include "tts_front/core/utf8.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace tts_front::detail {
namespace {

std::ifstream open_required(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("missing native asset: " + path.string());
    return input;
}

std::vector<std::string> split_tab(const std::string& line) {
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

std::size_t parse_size(std::string_view value, std::string_view description) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string_view::npos)
        throw std::runtime_error("malformed " + std::string(description));
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(std::string(value), &consumed);
        if (consumed != value.size() || parsed > std::numeric_limits<std::size_t>::max())
            throw std::out_of_range("size");
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        throw std::runtime_error("malformed " + std::string(description));
    }
}

int parse_int(std::string_view value, std::string_view description) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoi(std::string(value), &consumed);
        if (consumed != value.size())
            throw std::out_of_range("integer");
        return parsed;
    } catch (...) {
        throw std::runtime_error("malformed " + std::string(description));
    }
}

std::unordered_map<std::string, std::size_t> load_ids(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::size_t> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 2 || fields[0].empty() || fields[1].empty())
            throw std::runtime_error("malformed indexed asset: " + path.string());
        if (!result.emplace(fields[1], parse_size(fields[0], "indexed asset id")).second)
            throw std::runtime_error("duplicate indexed asset entry: " + fields[1]);
    }
    return result;
}

std::unordered_map<std::string, int> load_vocab(const std::filesystem::path& path) {
    const auto raw = load_ids(path);
    std::unordered_map<std::string, int> result;
    for (const auto& [token, id] : raw) {
        if (id > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("vocabulary id outside int range");
        result.emplace(token, static_cast<int>(id));
    }
    return result;
}

std::unordered_map<std::string, std::pair<int, int>>
load_exceptions(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::pair<int, int>> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 3 || fields[0].empty())
            throw std::runtime_error("malformed exceptions asset");
        if (!result
                 .emplace(fields[0],
                          std::make_pair(parse_int(fields[1], "exception index"),
                                         parse_int(fields[2], "exception ё index")))
                 .second)
            throw std::runtime_error("duplicate exception entry: " + fields[0]);
    }
    return result;
}

std::unordered_map<std::string, std::array<std::string, 2>>
load_homographs(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::array<std::string, 2>> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 3 || fields[0].empty() || fields[1].empty() || fields[2].empty())
            throw std::runtime_error("malformed homographs asset");
        if (!result.emplace(fields[0], std::array<std::string, 2>{fields[1], fields[2]}).second)
            throw std::runtime_error("duplicate homograph entry: " + fields[0]);
    }
    return result;
}

std::unordered_map<std::string, std::vector<SileroPhraseRule>>
load_phrase_rules(const std::filesystem::path& path) {
    auto input = open_required(path);
    std::unordered_map<std::string, std::vector<SileroPhraseRule>> result;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = split_tab(line);
        if (fields.size() != 3 || fields[0].empty() || fields[1].empty() || fields[2].empty())
            throw std::runtime_error("malformed phrase rules asset");
        result[fields[0]].push_back({fields[0], fields[1], fields[2]});
    }
    return result;
}

std::vector<std::uint32_t> decode_values(std::string_view text) {
    std::vector<Utf8CodePoint> points;
    if (!decode_utf8(text, points))
        throw std::runtime_error("invalid UTF-8 in Silero sentence input");
    std::vector<std::uint32_t> result;
    result.reserve(points.size());
    for (const auto point : points)
        result.push_back(point.value);
    return result;
}

void append_utf8(std::string& output, std::uint32_t value) {
    if (value < 0x80) {
        output.push_back(static_cast<char>(value));
    } else if (value < 0x800) {
        output.push_back(static_cast<char>(0xc0 | (value >> 6)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value < 0x10000) {
        output.push_back(static_cast<char>(0xe0 | (value >> 12)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (value >> 18)));
        output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    }
}

std::vector<std::string> word_ngrams(std::string_view word) {
    const auto codepoints = decode_values(word);
    std::vector<std::uint32_t> padded{'<'};
    padded.insert(padded.end(), codepoints.begin(), codepoints.end());
    padded.push_back('>');
    std::vector<std::string> result;
    for (std::size_t size = 1; size <= codepoints.size() + 3; ++size) {
        for (std::size_t start = 0; start + size <= padded.size(); ++start) {
            std::string gram;
            for (std::size_t index = start; index < start + size; ++index)
                append_utf8(gram, padded[index]);
            result.push_back(std::move(gram));
        }
    }
    return result;
}

bool delimiter(std::uint32_t value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '.' ||
           value == ',' || value == '!' || value == '?' || value == ';' || value == ':' ||
           value == '<' || value == '>' || value == '=' || value == '(' || value == ')' ||
           value == '/' || value == '\\';
}

bool word_codepoint(std::uint32_t value) {
    return (value >= 0x410 && value <= 0x44f) || value == 0x401 || value == 0x451 || value == '+';
}

bool phrase_boundary(std::uint32_t value) {
    return (value >= 0x410 && value <= 0x44f) || value == 0x401 || value == 0x451 || value == '-';
}

std::string encode_values(const std::vector<std::uint32_t>& values) {
    std::string result;
    for (const auto value : values)
        append_utf8(result, value);
    return result;
}

std::string clean_context(std::string_view text, bool start) {
    std::string filtered;
    for (const auto value : decode_values(text)) {
        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == ' ' || value == '.' || value == '!' ||
            value == '?' || value == ',' || value == '-')
            append_utf8(filtered, value);
        else if ((value >= 0x410 && value <= 0x44f) || value == 0x401 || value == 0x451)
            append_utf8(filtered, value);
    }
    std::string collapsed;
    bool space = false;
    for (const auto value : decode_values(filtered)) {
        if (value == ' ') {
            space = true;
        } else {
            if (space && !collapsed.empty())
                collapsed.push_back(' ');
            space = false;
            append_utf8(collapsed, value);
        }
    }
    if (!start) {
        collapsed = silero_lower_ru(collapsed);
        if (collapsed.empty() ||
            std::string_view(".!?").find(collapsed.back()) == std::string_view::npos)
            collapsed.push_back('.');
        return collapsed;
    }
    while (!collapsed.empty() &&
           std::string_view(" .,!?-").find(collapsed.front()) != std::string_view::npos)
        collapsed.erase(collapsed.begin());
    auto values = decode_values(collapsed);
    if (values.empty())
        return collapsed;
    if (values.front() >= 0x430 && values.front() <= 0x44f)
        values.front() -= 0x20;
    else if (values.front() == 0x451)
        values.front() = 0x401;
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (values[index] >= 0x410 && values[index] <= 0x42f)
            values[index] += 0x20;
        else if (values[index] == 0x401)
            values[index] = 0x451;
    }
    return encode_values(values);
}

} // namespace

SileroRuntimeData SileroRuntimeData::load(const SileroBundle& bundle) {
    SileroRuntimeData result;
    result.ngram_ids = load_ids(bundle.asset("ngrams.tsv"));
    result.vocab = load_vocab(bundle.asset("bert-vocab.tsv"));
    result.exceptions = load_exceptions(bundle.asset("exceptions.tsv"));
    result.homographs = load_homographs(bundle.asset("homodict.tsv"));
    result.phrase_rules = load_phrase_rules(bundle.asset("phrase-rules.tsv"));

    auto input = open_required(bundle.asset("embedding.f32"));
    std::vector<char> bytes{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>()};
    if (bytes.size() % sizeof(float) != 0)
        throw std::runtime_error("embedding weights are not float32");
    result.dimension = 16;
    result.embedding_weights.resize(bytes.size() / sizeof(float));
    if (!bytes.empty())
        std::memcpy(result.embedding_weights.data(), bytes.data(), bytes.size());
    if (result.embedding_weights.empty() || result.embedding_weights.size() % result.dimension != 0)
        throw std::runtime_error("embedding weights have invalid dimension");

    const auto find_id = [&](std::string_view token) {
        const auto found = result.vocab.find(std::string(token));
        if (found == result.vocab.end())
            throw std::runtime_error("missing special vocabulary token: " + std::string(token));
        return found->second;
    };
    result.pad_id = find_id("[PAD]");
    result.unk_id = find_id("[UNK]");
    result.cls_id = find_id("[CLS]");
    result.sep_id = find_id("[SEP]");
    result.homo_start_id = find_id("[HOMO]");
    result.homo_end_id = find_id("[/HOMO]");
    return result;
}

std::string silero_lower_ru(const std::string_view text) {
    std::string result;
    for (auto value : decode_values(text)) {
        if (value >= 0x410 && value <= 0x42f)
            value += 0x20;
        else if (value == 0x401)
            value = 0x451;
        append_utf8(result, value);
    }
    return result;
}

std::string silero_clean_word(const std::string_view text) {
    std::string result;
    for (const auto value : decode_values(silero_lower_ru(text))) {
        if ((value >= 0x430 && value <= 0x44f) || value == 0x451)
            append_utf8(result, value);
    }
    return result;
}

std::vector<SileroToken> silero_tokenize(const std::string_view sentence) {
    const auto codepoints = decode_values(sentence);
    std::vector<std::size_t> offsets(codepoints.size() + 1, 0);
    for (std::size_t index = 0; index < codepoints.size(); ++index) {
        std::string encoded;
        append_utf8(encoded, codepoints[index]);
        offsets[index + 1] = offsets[index] + encoded.size();
    }
    std::vector<SileroToken> result;
    const auto emit = [&](std::size_t begin, std::size_t end, bool process, bool classifier_input) {
        if (begin == end)
            return;
        const auto byte_begin = offsets[begin];
        const auto byte_end = offsets[end];
        const auto raw = sentence.substr(byte_begin, byte_end - byte_begin);
        result.push_back({std::string(raw),
                          process ? silero_clean_word(raw) : "",
                          process,
                          classifier_input,
                          byte_begin,
                          byte_end});
    };
    std::size_t index = 0;
    while (index < codepoints.size()) {
        if (delimiter(codepoints[index])) {
            emit(index, index + 1, false, true);
            ++index;
            continue;
        }
        const auto segment_begin = index;
        while (index < codepoints.size() && !delimiter(codepoints[index]))
            ++index;
        std::size_t cursor = segment_begin;
        while (cursor < index) {
            if (word_codepoint(codepoints[cursor])) {
                const auto word_begin = cursor;
                while (cursor < index && word_codepoint(codepoints[cursor]))
                    ++cursor;
                const bool hyphenated = cursor + 1 < index && codepoints[cursor] == '-' &&
                                        word_codepoint(codepoints[cursor + 1]);
                if (hyphenated)
                    ++cursor;
                const auto raw =
                    sentence.substr(offsets[word_begin], offsets[cursor] - offsets[word_begin]);
                const auto clean = silero_clean_word(raw);
                emit(word_begin, cursor, !clean.empty() && clean != "то", true);
            } else {
                const auto opaque_begin = cursor;
                while (cursor < index && !word_codepoint(codepoints[cursor]))
                    ++cursor;
                emit(opaque_begin, cursor, false, false);
            }
        }
    }
    return result;
}

std::vector<float> silero_embed(const SileroRuntimeData& data, const std::string_view word) {
    std::vector<float> output(data.dimension, 0.0f);
    std::size_t count = 0;
    for (const auto& gram : word_ngrams(silero_lower_ru(word))) {
        const auto found = data.ngram_ids.find(gram);
        if (found == data.ngram_ids.end())
            continue;
        if (found->second >= data.embedding_weights.size() / data.dimension)
            throw std::runtime_error("ngram id outside embedding weights");
        const auto offset = found->second * data.dimension;
        for (std::size_t index = 0; index < data.dimension; ++index)
            output[index] += data.embedding_weights[offset + index];
        ++count;
    }
    if (count == 0) {
        const auto found = data.ngram_ids.find("UNK");
        if (found == data.ngram_ids.end() ||
            found->second >= data.embedding_weights.size() / data.dimension)
            throw std::runtime_error("missing or invalid UNK embedding");
        const auto offset = found->second * data.dimension;
        for (std::size_t index = 0; index < data.dimension; ++index)
            output[index] = data.embedding_weights[offset + index];
        return output;
    }
    for (auto& value : output)
        value /= static_cast<float>(count);
    return output;
}

std::vector<std::string> SileroWordPiece::basic_tokens(const std::string_view sentence) const {
    std::vector<std::string> result;
    std::string current;
    const auto flush = [&] {
        if (!current.empty()) {
            result.push_back(std::move(current));
            current.clear();
        }
    };
    const auto values = decode_values(sentence);
    std::vector<std::size_t> offsets(values.size() + 1, 0);
    for (std::size_t index = 0; index < values.size(); ++index) {
        std::string encoded;
        append_utf8(encoded, values[index]);
        offsets[index + 1] = offsets[index] + encoded.size();
    }
    for (std::size_t index = 0; index < values.size();) {
        const auto byte_offset = offsets[index];
        if (sentence.substr(byte_offset, 6) == "[HOMO]") {
            flush();
            result.emplace_back("[HOMO]");
            index += 6;
            continue;
        }
        if (sentence.substr(byte_offset, 7) == "[/HOMO]") {
            flush();
            result.emplace_back("[/HOMO]");
            index += 7;
            continue;
        }
        std::string encoded;
        append_utf8(encoded, values[index]);
        if (values[index] == ' ' || values[index] == '\t' || values[index] == '\r' ||
            values[index] == '\n') {
            flush();
        } else if (values[index] < 128 && ((values[index] >= 33 && values[index] <= 47) ||
                                           (values[index] >= 58 && values[index] <= 64) ||
                                           (values[index] >= 91 && values[index] <= 96) ||
                                           (values[index] >= 123 && values[index] <= 126))) {
            flush();
            result.push_back(encoded);
        } else {
            current += encoded;
        }
        ++index;
    }
    flush();
    return result;
}

std::vector<std::int64_t> SileroWordPiece::encode_token(const std::string_view token) const {
    if (token == "[HOMO]" || token == "[/HOMO]")
        return {data_.vocab.at(std::string(token))};
    const auto chars = decode_values(token);
    std::vector<std::int64_t> ids;
    std::size_t start = 0;
    while (start < chars.size()) {
        std::size_t end = chars.size();
        std::string found;
        int found_id = data_.unk_id;
        while (start < end) {
            std::string candidate = start == 0 ? "" : "##";
            for (std::size_t index = start; index < end; ++index)
                append_utf8(candidate, chars[index]);
            const auto item = data_.vocab.find(candidate);
            if (item != data_.vocab.end()) {
                found = candidate;
                found_id = item->second;
                break;
            }
            --end;
        }
        if (found.empty())
            return {data_.unk_id};
        ids.push_back(found_id);
        start = end;
    }
    return ids;
}

std::vector<std::int64_t> SileroWordPiece::encode(const std::string_view sentence) const {
    std::vector<std::int64_t> result{data_.cls_id};
    for (const auto& token : basic_tokens(sentence)) {
        const auto ids = encode_token(token);
        result.insert(result.end(), ids.begin(), ids.end());
    }
    result.push_back(data_.sep_id);
    return result;
}

std::optional<std::string> silero_phrase_variant(const SileroRuntimeData& data,
                                                 const std::string_view word,
                                                 const std::string_view marked) {
    const auto found = data.phrase_rules.find(std::string(word));
    if (found == data.phrase_rules.end())
        return std::nullopt;
    const auto marked_lower = silero_lower_ru(marked);
    std::optional<std::size_t> best_position;
    std::string result;
    for (const auto& rule : found->second) {
        const auto pattern = silero_lower_ru(rule.marked);
        std::size_t offset = 0;
        while (true) {
            const auto position = marked_lower.find(pattern, offset);
            if (position == std::string::npos)
                break;
            const auto left = decode_values(marked_lower.substr(0, position));
            const auto right = decode_values(marked_lower.substr(position + pattern.size()));
            const bool left_ok = left.empty() || !phrase_boundary(left.back());
            const bool right_ok = right.empty() || !phrase_boundary(right.front());
            if (left_ok && right_ok && (!best_position || position < *best_position)) {
                best_position = position;
                result = rule.variant;
                break;
            }
            offset = position + 1;
        }
    }
    if (!best_position)
        return std::nullopt;
    return result;
}

std::string silero_marked_context(const std::string_view sentence, const SileroToken& token) {
    return clean_context(sentence.substr(0, token.byte_start), true) + " [HOMO] " + token.clean +
           " [/HOMO] " + clean_context(sentence.substr(token.byte_end), false);
}

std::string silero_preserve_case(const std::string_view raw, const std::string_view variant) {
    const auto source = decode_values(raw);
    auto result = decode_values(variant);
    if (!source.empty() && !result.empty()) {
        const auto uppercase = [](const std::uint32_t value) {
            return (value >= 0x410 && value <= 0x42f) || value == 0x401;
        };
        if (uppercase(source.front())) {
            if (result.front() >= 0x430 && result.front() <= 0x44f)
                result.front() -= 0x20;
            else if (result.front() == 0x451)
                result.front() = 0x401;
        }
    }
    return encode_values(result);
}

} // namespace tts_front::detail
