#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front {

/// rief Language used by the normalization pipeline.
enum class Language {
    /// Detect language from the input; mixed Cyrillic/Latin uses Russian policy.
    Auto,
    Russian,
    English
};

/// rief Controls how semantic stress information is resolved.
enum class StressMode {
    /// Do not produce stress values or stress decisions.
    Disabled,
    /// Resolve stress only from explicit pronunciation-dictionary entries.
    DictionaryOnly,
    /// Request automatic stress; currently reports unavailable-backend warning.
    Automatic
};

/// rief Category of a diagnostic emitted by the frontend.
enum class WarningCode {
    InvalidUtf8,
    UnsupportedLanguage,
    AmbiguousNormalization,
    UnresolvedNumber,
    AutomaticStressUnavailable,
    DictionaryParseError
};

/// rief Diagnostic associated with an input or normalization operation.
struct TextWarning {
    /// Warning category.
    WarningCode code;
    /// Human-readable diagnostic message.
    std::string message;
    /// UTF-8 byte offset in the corresponding input text.
    std::size_t offset = 0;
    /// Length in UTF-8 bytes.
    std::size_t length = 0;
};

/// rief Pronunciation and semantic metadata for one normalized token.
struct WordPronunciation {
    /// Surface token from normalized_text.
    std::string surface;
    /// Model-neutral rendered pronunciation for this token; empty means unchanged.
    std::string pronunciation;
    /// Byte offset into normalized_text (only an offset, never a stress index).
    std::size_t source_offset = 0;
    /// Index into TextFrontendResult::dictionary_replacements when this token is covered.
    std::optional<std::size_t> dictionary_replacement;
    /// Ordinal of the stressed vowel (zero-based), not a UTF-8 byte/code-point offset.
    std::optional<std::size_t> stressed_vowel;
    bool from_dictionary = false;
};

/// rief One dictionary replacement applied to pronunciation_text.
struct DictionaryReplacement {
    /// Source token or phrase.
    std::string input;
    /// Replacement pronunciation.
    std::string output;
    /// UTF-8 byte offset in normalized_text.
    std::size_t offset = 0;
};

/// rief Explain one semantic stress decision.
struct StressDecision {
    /// Token or phrase for which the decision was made.
    std::string word;
    /// Zero-based vowel ordinal, not a UTF-8 byte/code-point offset.
    std::optional<std::size_t> stressed_vowel;
    /// Whether the decision came from the pronunciation dictionary.
    bool from_dictionary = false;
    /// Human-readable decision source.
    std::string reason;
};

/// \brief Deterministic, user-configurable pronunciation overrides.
class PronunciationDictionary {
  public:
    /// Matching policy for an entry pattern.
    enum class Match { ExactToken, CaseSensitiveToken, CaseInsensitiveToken, ExactPhrase };
    /// One pronunciation replacement and optional stress ordinal.
    struct Entry {
        /// Token or phrase pattern.
        std::string pattern;
        /// Model-neutral replacement pronunciation.
        std::string pronunciation;
        /// Matching policy.
        Match match = Match::ExactToken;
        /// Zero-based vowel ordinal in the pronunciation. Invalid ordinals are rejected.
        std::optional<std::size_t> stressed_vowel;
    };

    /// Return false when an entry is invalid or duplicates an existing key.
    bool add_entry(Entry entry);
    /// Add an exact-token pronunciation entry.
    bool add_token(std::string token,
                   std::string pronunciation,
                   std::optional<std::size_t> stressed_vowel = std::nullopt);
    /// Add a token entry matched using the supported RU/EN case folding.
    bool add_case_insensitive_token(std::string token,
                                    std::string pronunciation,
                                    std::optional<std::size_t> stressed_vowel = std::nullopt);
    /// Add an exact phrase entry matched on complete token sequences.
    bool add_phrase(std::string phrase,
                    std::string pronunciation,
                    std::optional<std::size_t> stressed_vowel = std::nullopt);
    /// Load a strict JSON dictionary transactionally; failures leave entries unchanged.
    bool load_file(const std::string& path, std::vector<TextWarning>* warnings = nullptr);
    /// Find a token entry; the returned pointer remains valid until this dictionary mutates.
    const Entry* find_token(std::string_view token) const;
    const std::vector<Entry>& entries() const noexcept {
        return m_entries;
    }

  private:
    std::vector<Entry> m_entries;
};

/// rief Options for one frontend processing call.
struct TextFrontendOptions {
    /// Requested language or automatic language detection.
    Language language = Language::Auto;
    /// Stress resolution policy.
    StressMode stress_mode = StressMode::DictionaryOnly;
    /// Collapse Unicode whitespace and spacing around punctuation.
    bool cleanup_unicode = true;
    /// Apply deterministic language normalization rules.
    bool normalize = true;
    /// Apply the non-owning dictionary pointer when present.
    bool apply_dictionary = true;
    /// Resolve semantic stress according to stress_mode.
    bool resolve_stress = true;
    /// Emit decisions for tokens without dictionary replacements.
    bool diagnostics = false;
    /// Non-owning dictionary; caller must keep it alive for the call.
    const PronunciationDictionary* dictionary = nullptr;
};

/// rief All text stages, replacements, token metadata, and diagnostics.
struct TextFrontendResult {
    /// Original UTF-8 input text.
    std::string original_text;
    /// Deterministically normalized UTF-8 text.
    std::string normalized_text;
    /// Text after pronunciation dictionary replacement.
    std::string pronunciation_text;
    std::vector<WordPronunciation> words;
    std::vector<DictionaryReplacement> dictionary_replacements;
    std::vector<StressDecision> stress_decisions;
    std::vector<TextWarning> warnings;
    /// Return true when warnings indicate an ambiguous or unresolved result.
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

/// Return the stable textual name of a language value.
const char* to_string(Language language) noexcept;
/// Return the stable textual name of a warning code.
const char* to_string(WarningCode code) noexcept;

} // namespace tts_front
