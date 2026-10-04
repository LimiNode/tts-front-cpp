#include "silero_stress_backend.hpp"

#include "utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
#include "silero_ort_session_owner.hpp"
#include "silero_sentence.hpp"

#include <onnxruntime_cxx_api.h>
#endif

namespace tts_front::detail {

#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
namespace {

using Codepoints = std::vector<std::uint32_t>;

Codepoints decode_values(std::string_view text) {
    std::vector<Utf8CodePoint> points;
    if (!decode_utf8(text, points))
        throw std::runtime_error("invalid UTF-8 in Silero sentence input");
    Codepoints result;
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

std::string encode_values(const Codepoints& values) {
    std::string result;
    for (const auto value : values)
        append_utf8(result, value);
    return result;
}

std::string without_stress_markers(std::string_view text) {
    auto values = decode_values(text);
    values.erase(std::remove(values.begin(), values.end(), '+'), values.end());
    return encode_values(values);
}

bool is_vowel(std::uint32_t value) {
    return value == 0x430 || value == 0x43e || value == 0x443 || value == 0x44b || value == 0x44d ||
           value == 0x438 || value == 0x435 || value == 0x44f || value == 0x451 || value == 0x44e;
}

std::optional<std::size_t> vowel_at_character(const Codepoints& values, std::size_t position) {
    std::size_t ordinal = 0;
    for (std::size_t index = 0; index < values.size() && index <= position; ++index) {
        if (is_vowel(values[index])) {
            if (index == position)
                return ordinal;
            ++ordinal;
        }
    }
    return std::nullopt;
}

struct AccentResult {
    Codepoints values;
    std::optional<std::size_t> stressed_vowel;
};

AccentResult accentuate(const SileroToken& token,
                        int stress_id,
                        int yo_id,
                        const std::pair<int, int>* exception,
                        const std::vector<float>& yo_logits) {
    auto output = decode_values(token.raw);
    const auto lower = decode_values(silero_lower_ru(token.raw));
    const bool has_stress_marker = std::find(output.begin(), output.end(), '+') != output.end();
    const auto yo = static_cast<std::uint32_t>(0x451);
    const auto upper_yo = static_cast<std::uint32_t>(0x401);
    const auto e = static_cast<std::uint32_t>(0x435);
    if (exception != nullptr && !has_stress_marker) {
        if (exception->second >= 0 && static_cast<std::size_t>(exception->second) < output.size())
            output[exception->second] = output[exception->second] == 0x415 ? upper_yo : yo;
        if (exception->first >= 0 && static_cast<std::size_t>(exception->first) < output.size()) {
            const auto stressed =
                vowel_at_character(lower, static_cast<std::size_t>(exception->first));
            if (stressed)
                return {output, stressed};
        }
    }

    std::vector<std::size_t> vowels;
    std::vector<std::size_t> e_positions;
    for (std::size_t index = 0; index < lower.size(); ++index) {
        if (lower[index] == e)
            e_positions.push_back(index);
        if (is_vowel(lower[index]))
            vowels.push_back(index);
    }
    if (vowels.empty())
        return {output, std::nullopt};

    std::optional<std::size_t> stressed_position;
    if (has_stress_marker) {
        std::size_t ordinal = 0;
        for (const auto value : lower) {
            if (value == '+' && ordinal < vowels.size())
                stressed_position = vowels[ordinal];
            else if (is_vowel(value))
                ++ordinal;
        }
    } else if (stress_id >= 0 && static_cast<std::size_t>(stress_id) < vowels.size()) {
        stressed_position = vowels[static_cast<std::size_t>(stress_id)];
    }
    if (vowels.size() == 1)
        stressed_position = vowels.front();

    if (!e_positions.empty() && yo_id > 0 &&
        static_cast<std::size_t>(yo_id) <= e_positions.size() && !yo_logits.empty()) {
        const auto maximum = *std::max_element(yo_logits.begin(), yo_logits.end());
        float sum = 0.0f;
        for (const auto value : yo_logits)
            sum += std::exp(value - maximum);
        const auto probability =
            std::exp(yo_logits[static_cast<std::size_t>(yo_id)] - maximum) / sum;
        if (probability > 0.5f && stressed_position &&
            *stressed_position == e_positions[static_cast<std::size_t>(yo_id) - 1]) {
            output[*stressed_position] = output[*stressed_position] == 0x415 ? upper_yo : yo;
        }
    }
    const auto stressed_vowel =
        stressed_position ? vowel_at_character(lower, *stressed_position) : std::nullopt;
    output.erase(std::remove(output.begin(), output.end(), '+'), output.end());
    return {output, stressed_vowel};
}

std::vector<std::string> session_input_names(Ort::Session& session,
                                             Ort::AllocatorWithDefaultOptions& allocator) {
    std::vector<std::string> names;
    for (std::size_t index = 0; index < session.GetInputCount(); ++index) {
        auto name = session.GetInputNameAllocated(index, allocator);
        names.emplace_back(name.get());
    }
    return names;
}

std::vector<float> run_classifier(Ort::Session& session,
                                  const std::vector<float>& values,
                                  std::size_t rows,
                                  std::size_t columns,
                                  std::size_t expected_columns,
                                  Ort::MemoryInfo& memory) {
    const std::array<std::int64_t, 2> shape{static_cast<std::int64_t>(rows),
                                            static_cast<std::int64_t>(columns)};
    std::vector<Ort::Value> inputs;
    inputs.emplace_back(Ort::Value::CreateTensor<float>(
        memory, const_cast<float*>(values.data()), values.size(), shape.data(), shape.size()));
    Ort::AllocatorWithDefaultOptions allocator;
    const auto names = session_input_names(session, allocator);
    std::vector<const char*> input_names;
    for (const auto& name : names)
        input_names.push_back(name.c_str());
    std::vector<std::string> output_name_storage;
    for (std::size_t index = 0; index < session.GetOutputCount(); ++index) {
        auto name = session.GetOutputNameAllocated(index, allocator);
        output_name_storage.emplace_back(name.get());
    }
    std::vector<const char*> output_names;
    for (const auto& name : output_name_storage)
        output_names.push_back(name.c_str());
    auto outputs = session.Run(Ort::RunOptions{nullptr},
                               input_names.data(),
                               inputs.data(),
                               inputs.size(),
                               output_names.data(),
                               output_names.size());
    if (outputs.empty())
        throw std::runtime_error("classifier returned no outputs");
    const auto info = outputs.front().GetTensorTypeAndShapeInfo();
    const auto output_shape = info.GetShape();
    if (output_shape.size() != 2 || output_shape[0] != static_cast<std::int64_t>(rows) ||
        output_shape[1] != static_cast<std::int64_t>(expected_columns))
        throw std::runtime_error("unexpected classifier output shape");
    const auto* data = outputs.front().GetTensorData<float>();
    return {data, data + info.GetElementCount()};
}

float run_homograph(Ort::Session& session,
                    const std::vector<std::int64_t>& ids,
                    std::int64_t start,
                    std::int64_t end,
                    Ort::MemoryInfo& memory) {
    const std::array<std::int64_t, 2> shape{1, static_cast<std::int64_t>(ids.size())};
    const std::array<std::int64_t, 1> span_shape{1};
    std::vector<Ort::Value> inputs;
    inputs.emplace_back(Ort::Value::CreateTensor<std::int64_t>(
        memory, const_cast<std::int64_t*>(ids.data()), ids.size(), shape.data(), shape.size()));
    inputs.emplace_back(Ort::Value::CreateTensor<std::int64_t>(
        memory, &start, 1, span_shape.data(), span_shape.size()));
    inputs.emplace_back(Ort::Value::CreateTensor<std::int64_t>(
        memory, &end, 1, span_shape.data(), span_shape.size()));
    Ort::AllocatorWithDefaultOptions allocator;
    const auto names = session_input_names(session, allocator);
    std::vector<const char*> input_names;
    for (const auto& name : names)
        input_names.push_back(name.c_str());
    std::vector<std::string> output_name_storage;
    for (std::size_t index = 0; index < session.GetOutputCount(); ++index) {
        auto name = session.GetOutputNameAllocated(index, allocator);
        output_name_storage.emplace_back(name.get());
    }
    std::vector<const char*> output_names;
    for (const auto& name : output_name_storage)
        output_names.push_back(name.c_str());
    auto outputs = session.Run(Ort::RunOptions{nullptr},
                               input_names.data(),
                               inputs.data(),
                               inputs.size(),
                               output_names.data(),
                               output_names.size());
    if (outputs.empty())
        throw std::runtime_error("homosolver returned no outputs");
    const auto info = outputs.front().GetTensorTypeAndShapeInfo();
    const auto shape_out = info.GetShape();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape_out.size() != 2 ||
        shape_out[0] != 1 || shape_out[1] != 1 || info.GetElementCount() != 1)
        throw std::runtime_error("unexpected homosolver output shape");
    return outputs.front().GetTensorData<float>()[0];
}

} // namespace
#endif

SileroStressBackend::SileroStressBackend(const SileroStressBackendConfig& config)
    : bundle_(load_silero_bundle(config.bundle_root, config.ort_version, config.hash_chunk_size)) {
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    runtime_data_ = std::make_unique<SileroRuntimeData>(SileroRuntimeData::load(bundle_));
    runtime_data_ready_ = true;
    sessions_ = std::make_unique<SileroOrtSessionOwner>(bundle_);
    sessions_ready_ = true;
#endif
}

SileroStressBackend::~SileroStressBackend() = default;

SileroSentenceResult SileroStressBackend::process(const std::string_view sentence) const {
#if !defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    (void)sentence;
    throw std::runtime_error("Silero sentence backend requires ONNX Runtime support");
#else
    if (!runtime_data_ || !sessions_)
        throw std::runtime_error("Silero sentence backend is not initialized");
    if (!is_valid_utf8(sentence))
        throw std::runtime_error("Silero sentence input is not valid UTF-8");

    std::string transformed(sentence);
    const auto source_tokens = silero_tokenize(transformed);
    struct Replacement {
        std::size_t begin;
        std::size_t end;
        std::string value;
    };
    std::vector<Replacement> replacements;
    const SileroWordPiece wordpiece(*runtime_data_);
    for (const auto& token : source_tokens) {
        const auto homograph = runtime_data_->homographs.find(token.clean);
        if (!token.process || homograph == runtime_data_->homographs.end())
            continue;
        auto variant = silero_phrase_variant(
            *runtime_data_, token.clean, silero_marked_context(transformed, token));
        if (!variant) {
            const auto ids = wordpiece.encode(silero_marked_context(transformed, token));
            const auto start = std::find(ids.begin(), ids.end(), runtime_data_->homo_start_id);
            const auto end = std::find(ids.begin(), ids.end(), runtime_data_->homo_end_id);
            if (start == ids.end() || end == ids.end() || start >= end)
                throw std::runtime_error("homograph markers missing from WordPiece output");
            auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            const auto logit = run_homograph(sessions_->homosolver(),
                                             ids,
                                             static_cast<std::int64_t>(start - ids.begin()),
                                             static_cast<std::int64_t>(end - ids.begin()),
                                             memory);
            const auto prediction = 1.0f / (1.0f + std::exp(-logit)) >= 0.5f ? 1U : 0U;
            variant = homograph->second[prediction];
        }
        replacements.push_back(
            {token.byte_start, token.byte_end, silero_preserve_case(token.raw, *variant)});
    }
    if (!replacements.empty()) {
        transformed.clear();
        std::size_t cursor = 0;
        const auto original = std::string(sentence);
        for (const auto& replacement : replacements) {
            transformed.append(original, cursor, replacement.begin - cursor);
            transformed += replacement.value;
            cursor = replacement.end;
        }
        transformed.append(original, cursor, original.size() - cursor);
    }

    const auto tokens = silero_tokenize(transformed);
    std::vector<std::string> words;
    std::vector<std::size_t> rows(tokens.size(), std::numeric_limits<std::size_t>::max());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (!tokens[index].classifier_input)
            continue;
        rows[index] = words.size();
        words.push_back(tokens[index].clean);
    }
    std::vector<float> embeddings;
    for (const auto& word : words) {
        const auto embedding = silero_embed(*runtime_data_, word);
        embeddings.insert(embeddings.end(), embedding.begin(), embedding.end());
    }
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const auto stress = words.empty() ? std::vector<float>{}
                                      : run_classifier(sessions_->stress(),
                                                       embeddings,
                                                       words.size(),
                                                       runtime_data_->dimension,
                                                       10,
                                                       memory);
    const auto yo =
        words.empty()
            ? std::vector<float>{}
            : run_classifier(
                  sessions_->yo(), embeddings, words.size(), runtime_data_->dimension, 7, memory);
    SileroSentenceResult result;
    result.pronunciation_text.reserve(transformed.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const auto& token = tokens[index];
        if (!token.process) {
            result.pronunciation_text += token.raw;
            continue;
        }
        const auto row = rows[index];
        if (row == std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("processable token has no classifier row");
        const auto stress_begin = stress.begin() + static_cast<std::ptrdiff_t>(row * 10);
        const auto yo_begin = yo.begin() + static_cast<std::ptrdiff_t>(row * 7);
        const auto stress_id =
            static_cast<int>(std::max_element(stress_begin, stress_begin + 10) - stress_begin);
        const auto yo_id = static_cast<int>(std::max_element(yo_begin, yo_begin + 7) - yo_begin);
        const auto exception = runtime_data_->exceptions.find(token.clean);
        const auto accent =
            accentuate(token,
                       stress_id,
                       yo_id,
                       exception == runtime_data_->exceptions.end() ? nullptr : &exception->second,
                       std::vector<float>(yo_begin, yo_begin + 7));
        const auto pronunciation = encode_values(accent.values);
        result.pronunciation_text += pronunciation;
        result.words.push_back(
            {without_stress_markers(token.raw),
             pronunciation,
             token.byte_start,
             accent.stressed_vowel,
             exception != runtime_data_->exceptions.end(),
             runtime_data_->homographs.find(token.clean) != runtime_data_->homographs.end(),
             exception != runtime_data_->exceptions.end() ? "exception" : "model"});
    }
    return result;
#endif
}

} // namespace tts_front::detail
