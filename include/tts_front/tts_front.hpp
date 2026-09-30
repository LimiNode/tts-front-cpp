#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front {

enum class Language { Auto, Russian, English };
enum class StressMode { Disabled, DictionaryOnly, Automatic };
enum class WarningCode {
  InvalidUtf8, UnsupportedLanguage, AmbiguousNormalization,
  UnresolvedNumber, AutomaticStressUnavailable, DictionaryParseError
};

struct TextWarning {
  WarningCode code;
  std::string message;
  std::size_t offset = 0;
  std::size_t length = 0;
};

struct WordPronunciation {
  std::string surface;
  std::optional<std::size_t> stressed_vowel;
  bool from_dictionary = false;
};

struct DictionaryReplacement {
  std::string input;
  std::string output;
  std::size_t offset = 0;
};

struct StressDecision {
  std::string word;
  std::optional<std::size_t> stressed_vowel;
  bool from_dictionary = false;
  std::string reason;
};

/// \brief Deterministic, user-configurable pronunciation overrides.
class PronunciationDictionary {
public:
  enum class Match { ExactToken, CaseSensitiveToken, CaseInsensitiveToken, ExactPhrase };
  struct Entry {
    std::string pattern;
    std::string pronunciation;
    Match match = Match::ExactToken;
    std::optional<std::size_t> stressed_vowel;
  };

  void add_entry(Entry entry);
  void add_token(std::string token, std::string pronunciation,
                 std::optional<std::size_t> stressed_vowel = std::nullopt);
  void add_case_insensitive_token(std::string token, std::string pronunciation,
                                  std::optional<std::size_t> stressed_vowel = std::nullopt);
  void add_phrase(std::string phrase, std::string pronunciation,
                  std::optional<std::size_t> stressed_vowel = std::nullopt);
  bool load_file(const std::string& path, std::vector<TextWarning>* warnings = nullptr);
  const Entry* find_token(std::string_view token) const noexcept;
  const std::vector<Entry>& entries() const noexcept { return m_entries; }

private:
  std::vector<Entry> m_entries;
};

struct TextFrontendOptions {
  Language language = Language::Auto;
  StressMode stress_mode = StressMode::DictionaryOnly;
  bool cleanup_unicode = true;
  bool normalize = true;
  bool apply_dictionary = true;
  bool resolve_stress = true;
  bool diagnostics = false;
  const PronunciationDictionary* dictionary = nullptr;
};

struct TextFrontendResult {
  std::string original_text;
  std::string normalized_text;
  std::string pronunciation_text;
  std::vector<WordPronunciation> words;
  std::vector<DictionaryReplacement> dictionary_replacements;
  std::vector<StressDecision> stress_decisions;
  std::vector<TextWarning> warnings;
  bool has_uncertainty() const noexcept;
};

/// \brief Reusable, thread-safe text normalization pipeline.
class TextFrontend {
public:
  TextFrontend() = default;
  /// \brief Process UTF-8 text according to the selected language and stages.
  /// \param text Input UTF-8 text.
  /// \param options Pipeline options and optional dictionary.
  /// \return All intermediate text and semantic diagnostics.
  TextFrontendResult process(std::string_view text,
                             const TextFrontendOptions& options = {}) const;
};

const char* to_string(Language language) noexcept;
const char* to_string(WarningCode code) noexcept;

} // namespace tts_front
