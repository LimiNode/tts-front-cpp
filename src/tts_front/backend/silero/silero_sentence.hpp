#pragma once

#include "silero_bundle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tts_front::detail {

struct SileroPhraseRule {
    std::string word;
    std::string marked;
    std::string variant;
};

struct SileroToken {
    std::string raw;
    std::string clean;
    bool process = false;
    bool classifier_input = false;
    std::size_t byte_start = 0;
    std::size_t byte_end = 0;
};

struct SileroRuntimeData {
    std::unordered_map<std::string, std::size_t> ngram_ids;
    std::vector<float> embedding_weights;
    std::unordered_map<std::string, int> vocab;
    std::unordered_map<std::string, std::pair<int, int>> exceptions;
    std::unordered_map<std::string, std::array<std::string, 2>> homographs;
    std::unordered_map<std::string, std::vector<SileroPhraseRule>> phrase_rules;
    std::size_t dimension = 0;
    int pad_id = 0;
    int unk_id = 1;
    int cls_id = 2;
    int sep_id = 3;
    int homo_start_id = 0;
    int homo_end_id = 0;

    static SileroRuntimeData load(const SileroBundle& bundle);
};

std::vector<SileroToken> silero_tokenize(std::string_view sentence);
std::string silero_lower_ru(std::string_view text);
std::string silero_clean_word(std::string_view text);
std::vector<float> silero_embed(const SileroRuntimeData& data, std::string_view word);

class SileroWordPiece {
  public:
    explicit SileroWordPiece(const SileroRuntimeData& data) : data_(data) {}

    std::vector<std::int64_t> encode(std::string_view sentence) const;

  private:
    std::vector<std::string> basic_tokens(std::string_view sentence) const;
    std::vector<std::int64_t> encode_token(std::string_view token) const;

    const SileroRuntimeData& data_;
};

std::optional<std::string> silero_phrase_variant(const SileroRuntimeData& data,
                                                 std::string_view word,
                                                 std::string_view marked);
std::string silero_marked_context(std::string_view sentence, const SileroToken& token);
std::string silero_preserve_case(std::string_view raw, std::string_view variant);

} // namespace tts_front::detail
