#include "silero_stress_backend.hpp"

#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
#include "silero_ort_session_owner.hpp"
#endif

namespace tts_front::detail {

SileroStressBackend::SileroStressBackend(const SileroStressBackendConfig& config)
    : bundle_(load_silero_bundle(config.bundle_root, config.ort_version, config.hash_chunk_size)) {
#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    sessions_ = std::make_unique<SileroOrtSessionOwner>(bundle_);
    sessions_ready_ = true;
#endif
}

SileroStressBackend::~SileroStressBackend() = default;

} // namespace tts_front::detail
