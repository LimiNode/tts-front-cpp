#pragma once

#include "silero_bundle.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace tts_front::detail {

class SileroOrtSessionOwner;

struct SileroStressBackendConfig {
    std::filesystem::path bundle_root;
    std::string ort_version = std::string(kSileroBundleOrtVersion);
    std::size_t hash_chunk_size = 1U << 20;
};

/// Private lifecycle boundary for the future semantic Silero sentence path.
/// Construction validates the complete bundle and, when enabled, owns the
/// reusable ORT sessions.  Sentence orchestration is intentionally not exposed
/// through the public frontend until its semantic result contract is complete.
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
    const SileroBundle& bundle() const noexcept {
        return bundle_;
    }

  private:
    SileroBundle bundle_;
    bool sessions_ready_ = false;
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    std::unique_ptr<SileroOrtSessionOwner> sessions_;
#endif
};

} // namespace tts_front::detail
