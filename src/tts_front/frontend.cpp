#include "tts_front.hpp"
#include "tts_front/backend/silero/silero_stress_backend.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/spacing.hpp"
#include "tts_front/core/tokenization.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/language/english/mixed_candidates.hpp"
#include "tts_front/language/english/normalizer.hpp"
#include "tts_front/language/russian/initialisms.hpp"
#include "tts_front/language/russian/mixed_candidates.hpp"
#include "tts_front/language/russian/normalizer.hpp"
#include "tts_front/normalization/admission.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"
#include "tts_front/normalization/language_detection.hpp"
#include "tts_front/normalization/mixed_language.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tts_front {
namespace {

using CodePoint = detail::Utf8CodePoint;
using detail::decode_utf8;
using detail::MappedText;
using detail::WarningSink;

void add_warning(std::vector<TextWarning>& warnings,
                 WarningCode code,
                 std::string message,
                 std::size_t offset,
                 std::size_t length) {
    warnings.push_back({code, std::move(message), offset, length});
}

[[maybe_unused]] std::optional<std::filesystem::path>
resolve_silero_bundle(const TextFrontendOptions& options) {
    if (!options.silero_bundle_path.empty())
        return std::filesystem::path(options.silero_bundle_path);
    if (const auto* environment = std::getenv("TTS_FRONT_SILERO_BUNDLE");
        environment != nullptr && *environment != '\0')
        return std::filesystem::path(environment);
    return std::nullopt;
}

} // namespace

struct TextFrontend::Impl {
    std::mutex silero_mutex;
    std::unordered_map<std::string, std::shared_ptr<detail::SileroStressBackend>> silero_backends;

    std::shared_ptr<detail::SileroStressBackend>
    backend_for(const std::filesystem::path& bundle_root) {
        std::error_code error;
        const auto resolved_root = std::filesystem::absolute(bundle_root, error);
        const auto stable_root = (error ? bundle_root : resolved_root).lexically_normal();
        const auto key = stable_root.string();
        std::lock_guard lock(silero_mutex);
        if (const auto found = silero_backends.find(key); found != silero_backends.end())
            return found->second;
        auto backend = std::make_shared<detail::SileroStressBackend>(
            detail::SileroStressBackendConfig{stable_root});
        silero_backends.emplace(key, backend);
        return backend;
    }
};

TextFrontend::TextFrontend() : impl_(std::make_shared<Impl>()) {}
TextFrontend::~TextFrontend() = default;

bool TextFrontendResult::has_uncertainty() const noexcept {
    return !warnings.empty();
}

TextFrontendResult TextFrontend::process(std::string_view input,
                                         const TextFrontendOptions& options) const {
    TextFrontendResult result;
    result.original_text = std::string(input);
    std::vector<CodePoint> points;
    if (!decode_utf8(input, points)) {
        add_warning(
            result.warnings, WarningCode::InvalidUtf8, "Input is not valid UTF-8", 0, input.size());
        return result;
    }
    WarningSink warning_sink{result.warnings, {}};
    MappedText text = MappedText::from_original(input, &warning_sink.preserved_ranges);
    if (options.cleanup_spacing)
        text = detail::cleanup_spacing(text);
    bool has_cyrillic = false;
    bool has_latin = false;
    std::optional<detail::TechnicalRangeIndex> technical_index;
    Language language = options.language;
    if (language == Language::Auto) {
        if (options.mixed_language_policy == MixedLanguagePolicy::SegmentCandidates) {
            technical_index = detail::build_technical_range_index(text.text);
            language =
                detail::detect_mixed_language(text.text, has_cyrillic, has_latin, *technical_index);
        } else {
            language = detail::detect_language(text.text, has_cyrillic, has_latin);
        }
    }
    if (options.language == Language::Auto && has_cyrillic && has_latin)
        warning_sink.add_range(
            WarningCode::AmbiguousNormalization,
            options.mixed_language_policy == MixedLanguagePolicy::DominantLanguage
                ? "Mixed Cyrillic/Latin input uses dominant-language normalization"
                : "Mixed Cyrillic/Latin input uses segmented candidate normalization",
            0,
            input.size());
    if (language != Language::Russian && language != Language::English) {
        result.warnings.push_back({WarningCode::UnsupportedLanguage, "Unsupported language", 0, 0});
        return result;
    }
    if (options.normalize) {
        if (options.language == Language::Auto &&
            options.mixed_language_policy == MixedLanguagePolicy::SegmentCandidates &&
            has_cyrillic && technical_index) {
            if (language == Language::Russian)
                text = detail::normalize_mixed_candidates(std::move(text),
                                                          warning_sink,
                                                          detail::russian::mixed_language_rules(),
                                                          detail::english::mixed_language_rules(),
                                                          *technical_index);
            else
                text = detail::normalize_mixed_candidates(std::move(text),
                                                          warning_sink,
                                                          detail::english::mixed_language_rules(),
                                                          detail::russian::mixed_language_rules(),
                                                          *technical_index);
        }
        switch (language) {
        case Language::Russian:
            text = detail::russian::normalize(std::move(text), warning_sink);
            break;
        case Language::English:
            text = detail::english::normalize(std::move(text), warning_sink);
            break;
        case Language::Auto:
            break;
        }
    }
    if (options.cleanup_spacing)
        text = detail::cleanup_spacing(text);
    result.normalized_text = text.text;
    result.pronunciation_text = text.text;
    const bool stress_enabled =
        options.resolve_stress && options.stress_mode != StressMode::Disabled;

    // Dictionary phrases are matched on complete token sequences, longest first, without rescanning output.
    std::vector<const PronunciationDictionary::Entry*> matched_entries;
    const auto spans = detail::token_spans(text.text);
    matched_entries.assign(spans.size(), nullptr);
    std::vector<std::string> automatic_replacements(spans.size());
    if ((options.apply_dictionary && options.dictionary) || options.expand_initialisms) {
        struct PhraseCandidate {
            const PronunciationDictionary::Entry* entry = nullptr;
            std::vector<detail::TokenSpan> spans;
        };
        std::vector<PhraseCandidate> phrases;
        if (options.apply_dictionary && options.dictionary) {
            for (const auto& entry : options.dictionary->entries()) {
                if (entry.match == PronunciationDictionary::Match::ExactPhrase)
                    phrases.push_back({&entry, detail::token_spans(entry.pattern)});
            }
        }
        std::string rendered;
        std::size_t cursor = 0;
        for (std::size_t i = 0; i < spans.size();) {
            const PronunciationDictionary::Entry* best = nullptr;
            std::size_t best_end = i;
            for (const auto& phrase : phrases) {
                const auto& entry = *phrase.entry;
                const auto& phrase_spans = phrase.spans;
                if (phrase_spans.empty() || i + phrase_spans.size() > spans.size())
                    continue;
                const auto input_phrase = text.text.substr(
                    spans[i].begin, spans[i + phrase_spans.size() - 1].end - spans[i].begin);
                const auto pattern_phrase =
                    entry.pattern.substr(phrase_spans.front().begin,
                                         phrase_spans.back().end - phrase_spans.front().begin);
                const bool match = input_phrase == pattern_phrase;
                if (match && (!best || phrase_spans.size() > best_end - i)) {
                    best = &entry;
                    best_end = i + phrase_spans.size();
                }
            }
            const auto token = text.text.substr(spans[i].begin, spans[i].end - spans[i].begin);
            if (best) {
                for (std::size_t j = i; j < best_end; ++j)
                    matched_entries[j] = best;
                rendered.append(text.text, cursor, spans[i].begin - cursor);
                rendered += best->pronunciation;
                const auto phrase_surface =
                    text.text.substr(spans[i].begin, spans[best_end - 1].end - spans[i].begin);
                result.dictionary_replacements.push_back(
                    {phrase_surface, best->pronunciation, spans[i].begin});
                if (options.resolve_stress && options.stress_mode != StressMode::Disabled &&
                    best->stressed_vowel)
                    result.stress_decisions.push_back({phrase_surface,
                                                       best->stressed_vowel,
                                                       true,
                                                       "pronunciation dictionary phrase"});
                cursor = spans[best_end - 1].end;
                i = best_end;
                continue;
            }
            rendered.append(text.text, cursor, spans[i].begin - cursor);
            const auto* entry = options.apply_dictionary && options.dictionary
                                    ? options.dictionary->find_token(token)
                                    : nullptr;
            if (entry) {
                matched_entries[i] = entry;
                rendered += entry->pronunciation;
                if (entry->pronunciation != token)
                    result.dictionary_replacements.push_back(
                        {token, entry->pronunciation, spans[i].begin});
            } else if (options.expand_initialisms && language == Language::Russian) {
                if (const auto replacement = detail::russian::safe_initialism(token)) {
                    automatic_replacements[i] = *replacement;
                    rendered += *replacement;
                    result.automatic_rewrites.push_back(
                        {token, *replacement, spans[i].begin, "safe Russian initialism", true});
                } else {
                    rendered += token;
                }
            } else {
                rendered += token;
            }
            cursor = spans[i].end;
            ++i;
        }
        rendered += text.text.substr(cursor);
        result.pronunciation_text = std::move(rendered);
    }

    const auto normalized_spans = detail::token_spans(result.normalized_text);
    for (std::size_t span_index = 0; span_index < normalized_spans.size(); ++span_index) {
        const auto& span = normalized_spans[span_index];
        const std::string token = result.normalized_text.substr(span.begin, span.end - span.begin);
        WordPronunciation word;
        word.surface = token;
        word.source_offset = span.begin;
        for (std::size_t replacement_index = 0;
             replacement_index < result.dictionary_replacements.size();
             ++replacement_index) {
            const auto& replacement = result.dictionary_replacements[replacement_index];
            const auto replacement_end = replacement.offset + replacement.input.size();
            if (span.begin >= replacement.offset && span.end <= replacement_end) {
                word.dictionary_replacement = replacement_index;
                word.from_dictionary = true;
                if (span.begin == replacement.offset)
                    word.pronunciation = replacement.output;
                break;
            }
        }
        const auto* matched_entry =
            options.dictionary && options.apply_dictionary && span_index < matched_entries.size()
                ? matched_entries[span_index]
                : nullptr;
        if (matched_entry) {
            if (matched_entry->match != PronunciationDictionary::Match::ExactPhrase) {
                word.from_dictionary = true;
                word.pronunciation = matched_entry->pronunciation;
                if (options.resolve_stress && options.stress_mode != StressMode::Disabled)
                    word.stressed_vowel = matched_entry->stressed_vowel;
            }
        }
        if (!matched_entry && span_index < automatic_replacements.size() &&
            !automatic_replacements[span_index].empty()) {
            word.pronunciation = automatic_replacements[span_index];
            word.from_automatic_rewrite = true;
        }
        result.words.push_back(word);
        if (stress_enabled && matched_entry &&
            matched_entry->match == PronunciationDictionary::Match::ExactPhrase)
            continue;
        if (stress_enabled && (options.diagnostics || word.from_dictionary))
            result.stress_decisions.push_back({token,
                                               word.stressed_vowel,
                                               word.from_dictionary,
                                               word.from_dictionary
                                                   ? "pronunciation dictionary"
                                                   : "no deterministic stress rule"});
    }
    if (options.resolve_stress && options.stress_mode == StressMode::Automatic) {
        const auto add_unavailable_warning = [&](std::string message) {
            result.warnings.push_back(
                {WarningCode::AutomaticStressUnavailable, std::move(message), 0, 0});
        };
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
        const auto bundle_root = resolve_silero_bundle(options);
        if (!bundle_root) {
            add_unavailable_warning(
                "Automatic stress bundle is not configured (set TTS_FRONT_SILERO_BUNDLE or "
                "TextFrontendOptions::silero_bundle_path)");
        } else {
            const auto deterministic_pronunciation = result.pronunciation_text;
            const auto deterministic_words = result.words;
            const auto deterministic_stress_decisions = result.stress_decisions;
            try {
                struct ProtectedRewrite {
                    std::size_t begin;
                    std::size_t end;
                    std::string output;
                };
                std::vector<ProtectedRewrite> protected_rewrites;
                for (std::size_t index = 0; index < normalized_spans.size();) {
                    const auto* entry =
                        index < matched_entries.size() ? matched_entries[index] : nullptr;
                    if (entry) {
                        std::size_t end = index + 1;
                        if (entry->match == PronunciationDictionary::Match::ExactPhrase) {
                            while (end < matched_entries.size() && matched_entries[end] == entry)
                                ++end;
                        }
                        protected_rewrites.push_back({normalized_spans[index].begin,
                                                      normalized_spans[end - 1].end,
                                                      entry->pronunciation});
                        index = end;
                    } else if (index < automatic_replacements.size() &&
                               !automatic_replacements[index].empty()) {
                        protected_rewrites.push_back({normalized_spans[index].begin,
                                                      normalized_spans[index].end,
                                                      automatic_replacements[index]});
                        ++index;
                    } else {
                        ++index;
                    }
                }

                std::string protected_input;
                std::size_t cursor = 0;
                for (const auto& rewrite : protected_rewrites) {
                    protected_input.append(result.normalized_text, cursor, rewrite.begin - cursor);
                    protected_input.append(rewrite.end - rewrite.begin, '\x01');
                    cursor = rewrite.end;
                }
                protected_input.append(
                    result.normalized_text, cursor, result.normalized_text.size() - cursor);

                const auto backend = impl_->backend_for(*bundle_root);
                const auto semantic = backend->process(protected_input);
                auto automatic_words = result.words;
                auto automatic_stress_decisions = result.stress_decisions;
                for (const auto& word : semantic.words) {
                    const auto span_index =
                        std::find_if(normalized_spans.begin(),
                                     normalized_spans.end(),
                                     [&](const detail::TokenSpan span) {
                                         return span.begin == word.source_offset;
                                     });
                    if (span_index == normalized_spans.end())
                        continue;
                    const auto index =
                        static_cast<std::size_t>(span_index - normalized_spans.begin());
                    if (index >= automatic_words.size() || automatic_words[index].from_dictionary ||
                        automatic_words[index].from_automatic_rewrite)
                        continue;
                    automatic_words[index].pronunciation = word.pronunciation;
                    automatic_words[index].stressed_vowel = word.stressed_vowel;
                    automatic_stress_decisions.push_back({automatic_words[index].surface,
                                                          word.stressed_vowel,
                                                          false,
                                                          "silero " + word.reason});
                }
                auto automatic_pronunciation = semantic.pronunciation_text;
                std::size_t marker_cursor = 0;
                for (const auto& rewrite : protected_rewrites) {
                    const auto marker = std::string(rewrite.end - rewrite.begin, '\x01');
                    const auto marker_position =
                        automatic_pronunciation.find(marker, marker_cursor);
                    if (marker_position == std::string::npos)
                        throw std::runtime_error("Silero backend lost a protected rewrite span");
                    automatic_pronunciation.replace(marker_position, marker.size(), rewrite.output);
                    marker_cursor = marker_position + rewrite.output.size();
                }
                result.words = std::move(automatic_words);
                result.stress_decisions = std::move(automatic_stress_decisions);
                result.pronunciation_text = std::move(automatic_pronunciation);
            } catch (const std::exception& error) {
                result.words = deterministic_words;
                result.stress_decisions = deterministic_stress_decisions;
                result.pronunciation_text = deterministic_pronunciation;
                add_unavailable_warning(std::string("Automatic stress backend unavailable: ") +
                                        error.what());
            }
        }
#else
        add_unavailable_warning(
            "Automatic stress requires a build with TTS_FRONT_ENABLE_ONNX_STRESS=ON");
#endif
    }
    return result;
}

const char* to_string(Language language) noexcept {
    switch (language) {
    case Language::Auto:
        return "auto";
    case Language::Russian:
        return "russian";
    case Language::English:
        return "english";
    }
    return "unknown";
}
const char* to_string(MixedLanguagePolicy policy) noexcept {
    switch (policy) {
    case MixedLanguagePolicy::DominantLanguage:
        return "dominant_language";
    case MixedLanguagePolicy::SegmentCandidates:
        return "segment_candidates";
    }
    return "unknown";
}
const char* to_string(WarningCode code) noexcept {
    switch (code) {
    case WarningCode::InvalidUtf8:
        return "invalid_utf8";
    case WarningCode::UnsupportedLanguage:
        return "unsupported_language";
    case WarningCode::AmbiguousNormalization:
        return "ambiguous_normalization";
    case WarningCode::UnresolvedNumber:
        return "unresolved_number";
    case WarningCode::AutomaticStressUnavailable:
        return "automatic_stress_unavailable";
    case WarningCode::DictionaryParseError:
        return "dictionary_parse_error";
    }
    return "unknown";
}
} // namespace tts_front
