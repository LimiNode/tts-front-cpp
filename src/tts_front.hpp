#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front {

/// \brief Language used by the normalization pipeline.
enum class Language {
    Auto,    ///< Detect language from the input; mixed Cyrillic/Latin uses Russian policy.
    Russian, ///< Normalize using Russian rules.
    English  ///< Normalize using English rules.
};

/// \brief Controls how semantic stress information is resolved.
enum class StressMode {
    Disabled,       ///< Do not produce stress values or stress decisions.
    DictionaryOnly, ///< Resolve stress only from pronunciation-dictionary entries.
    Automatic       ///< Request automatic stress from the optional Silero backend.
};

/// \brief Category of a diagnostic emitted by the frontend.
enum class WarningCode {
    InvalidUtf8,                ///< Input or dictionary text is not valid UTF-8.
    UnsupportedLanguage,        ///< The language is known but not implemented.
    AmbiguousNormalization,     ///< Normalization could not be resolved deterministically.
    UnresolvedNumber,           ///< A numeric token could not be normalized.
    AutomaticStressUnavailable, ///< Automatic stress requires an unavailable backend.
    DictionaryParseError        ///< The pronunciation dictionary is invalid.
};

/// \brief Diagnostic associated with an input or normalization operation.
struct TextWarning {
    WarningCode code;       ///< Warning category.
    std::string message;    ///< Human-readable diagnostic message.
    std::size_t offset = 0; ///< UTF-8 byte offset into original_text; zero for global diagnostics.
    std::size_t length = 0; ///< UTF-8 byte length in original_text; zero means no source span.
};

/// \brief Pronunciation and semantic metadata for one normalized token.
struct WordPronunciation {
    std::string surface;           ///< Surface token from normalized_text.
    std::string pronunciation;     ///< Rendered pronunciation; empty means unchanged.
    std::size_t source_offset = 0; ///< Byte offset into normalized_text.
    std::optional<std::size_t> dictionary_replacement; ///< Index of the covered replacement.
    std::optional<std::size_t> stressed_vowel;         ///< Zero-based vowel ordinal.
    bool from_dictionary = false;        ///< Whether pronunciation came from the dictionary.
    bool from_automatic_rewrite = false; ///< Whether an opt-in deterministic rewrite was applied.
};

/// \brief One dictionary replacement applied to pronunciation_text.
struct DictionaryReplacement {
    std::string input;      ///< Source token or phrase.
    std::string output;     ///< Replacement pronunciation.
    std::size_t offset = 0; ///< UTF-8 byte offset in normalized_text.
};

/// \brief One deterministic, non-dictionary pronunciation rewrite.
struct PronunciationRewrite {
    std::string input;      ///< Original token being rewritten.
    std::string output;     ///< Model-neutral pronunciation replacement.
    std::size_t offset = 0; ///< UTF-8 byte offset in normalized_text.
    std::string reason;     ///< Stable reason for the rewrite.
    bool automatic = true;  ///< True for built-in deterministic rules.
};

/// \brief Explain one semantic stress decision.
struct StressDecision {
    std::string word;                          ///< Token or phrase for the decision.
    std::optional<std::size_t> stressed_vowel; ///< Zero-based vowel ordinal.
    bool from_dictionary = false;              ///< Whether the decision came from the dictionary.
    std::string reason;                        ///< Human-readable decision source.
};

/// \brief Deterministic, user-configurable pronunciation overrides.
class PronunciationDictionary {
  public:
    /// Matching policy for an entry pattern.
    enum class Match {
        ExactToken,           ///< Match one token exactly.
        CaseInsensitiveToken, ///< Match one token using RU/EN case folding.
        ExactPhrase           ///< Match a complete token sequence.
    };
    /// One pronunciation replacement and optional stress ordinal.
    struct Entry {
        std::string pattern;             ///< Token or phrase pattern.
        std::string pronunciation;       ///< Model-neutral replacement pronunciation.
        Match match = Match::ExactToken; ///< Matching policy.
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

/// \brief Options for one frontend processing call.
struct TextFrontendOptions {
    Language language = Language::Auto;                  ///< Requested language or auto-detection.
    StressMode stress_mode = StressMode::DictionaryOnly; ///< Stress resolution policy.
    bool cleanup_spacing = true;     ///< Collapse supported whitespace and punctuation spacing.
    bool normalize = true;           ///< Apply deterministic language normalization.
    bool apply_dictionary = true;    ///< Apply the non-owning dictionary when present.
    bool resolve_stress = true;      ///< Resolve semantic stress.
    bool diagnostics = false;        ///< Emit decisions without replacements.
    bool expand_initialisms = false; ///< Expand the conservative Russian allowlist.
    /// Optional verified Silero bundle root. If empty, the backend resolver checks
    /// TTS_FRONT_SILERO_BUNDLE and otherwise reports AutomaticStressUnavailable.
    std::string silero_bundle_path;
    /// Non-owning dictionary; caller must keep it alive for the call.
    const PronunciationDictionary* dictionary = nullptr;
};

/// \brief All text stages, replacements, token metadata, and diagnostics.
struct TextFrontendResult {
    std::string original_text;            ///< Original UTF-8 input text.
    std::string normalized_text;          ///< Deterministically normalized UTF-8 text.
    std::string pronunciation_text;       ///< Text after semantic dictionary/automatic rewrites.
    std::vector<WordPronunciation> words; ///< Token-level pronunciation metadata.
    std::vector<DictionaryReplacement>
        dictionary_replacements;                          ///< Applied dictionary replacements.
    std::vector<PronunciationRewrite> automatic_rewrites; ///< Applied built-in rewrites.
    std::vector<StressDecision> stress_decisions;         ///< Semantic stress decisions.
    std::vector<TextWarning> warnings;                    ///< Diagnostics emitted by processing.
    /// Return true when processing emitted any warning or the result may be unusable.
    bool has_uncertainty() const noexcept;
};

/// \brief Reusable, thread-safe text normalization pipeline.
class TextFrontend {
  public:
    TextFrontend();
    ~TextFrontend();
    TextFrontend(const TextFrontend&) = default;
    TextFrontend& operator=(const TextFrontend&) = default;
    /// \brief Process UTF-8 text according to the selected language and stages.
    /// \param text Input UTF-8 text.
    /// \param options Pipeline options and optional dictionary.
    /// \return All intermediate text and semantic diagnostics.
    TextFrontendResult process(std::string_view text,
                               const TextFrontendOptions& options = {}) const;

  private:
    struct Impl;
    mutable std::shared_ptr<Impl> impl_;
};

/// Return the stable textual name of a language value.
const char* to_string(Language language) noexcept;
/// Return the stable textual name of a warning code.
const char* to_string(WarningCode code) noexcept;

} // namespace tts_front
