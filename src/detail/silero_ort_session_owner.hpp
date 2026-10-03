#pragma once

#include "silero_bundle.hpp"

#include <memory>
#include <onnxruntime_cxx_api.h>

namespace tts_front::detail {

/// Owns the three long-lived Silero sessions for one verified bundle.
/// Sessions are immutable after construction and may be reused by concurrent
/// callers according to the ONNX Runtime session thread-safety contract.
class SileroOrtSessionOwner {
  public:
    explicit SileroOrtSessionOwner(const SileroBundle& bundle);

    Ort::Session& stress() noexcept {
        return *stress_;
    }
    Ort::Session& yo() noexcept {
        return *yo_;
    }
    Ort::Session& homosolver() noexcept {
        return *homosolver_;
    }

  private:
    std::unique_ptr<Ort::Env> environment_;
    std::unique_ptr<Ort::Session> stress_;
    std::unique_ptr<Ort::Session> yo_;
    std::unique_ptr<Ort::Session> homosolver_;
};

} // namespace tts_front::detail
