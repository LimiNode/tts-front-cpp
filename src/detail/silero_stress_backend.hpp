#pragma once

#include "silero_bundle.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

class SileroOrtSessionOwner;
struct SileroRuntimeData;

struct SileroStressBackendConfig {
    std::filesystem::path bundle_root;
    std::string ort_version = std::string(kSileroBundleOrtVersion);
    std::size_t hash_chunk_size = 1U << 20;
};

enum class SileroWordRoute {
    Exception,
    Model,
    Phrase,
    Homosolver,
};

constexpr std::string_view silero_word_route_name(const SileroWordRoute route) noexcept {
    switch (route) {
    case SileroWordRoute::Exception:
        return "exception";
    case SileroWordRoute::Model:
        return "model";
    case SileroWordRoute::Phrase:
        return "phrase";
    case SileroWordRoute::Homosolver:
        return "homosolver";
    }
    return "model";
}

struct SileroWordResult {
    std::string surface;                       ///< Original token bytes.
    std::string pronunciation;                 ///< Surface with opaque spans preserved.
    std::size_t source_offset = 0;             ///< Byte offset in the processed sentence.
    std::optional<std::size_t> stressed_vowel; ///< Zero-based vowel ordinal.
    bool from_exception = false;               ///< Selected from exceptions.tsv.
    bool from_homograph = false;               ///< Token belongs to homograph.tsv.
    SileroWordRoute route = SileroWordRoute::Model;
    std::string reason; ///< Stable internal route name.
};

struct SileroSentenceResult {
    std::string pronunciation_text;
    std::vector<SileroWordResult> words;
};

/// Private semantic Silero sentence backend. Construction validates the complete
/// bundle, loads deterministic runtime data, and, when enabled, owns reusable
/// ORT sessions. The model-neutral result is intentionally private until the
/// production parity gate is complete.
class SileroStressBackend {
  public:
    explicit SileroStressBackend(const SileroStressBackendConfig& config);
    ~SileroStressBackend();

    bool bundle_validated() const noexcept {
        return true;
    }
    bool sessions_ready() const noexcept {
        return sessions_ready_;
    }
    bool runtime_data_ready() const noexcept {
        return runtime_data_ready_;
    }
    const SileroBundle& bundle() const noexcept {
        return bundle_;
    }
    SileroSentenceResult process(std::string_view sentence) const;

  private:
    SileroBundle bundle_;
    bool sessions_ready_ = false;
    bool runtime_data_ready_ = false;
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    std::unique_ptr<SileroRuntimeData> runtime_data_;
    std::unique_ptr<SileroOrtSessionOwner> sessions_;
#endif
};

} // namespace tts_front::detail
