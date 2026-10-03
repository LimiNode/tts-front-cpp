#include "silero_ort_session_owner.hpp"

#include <stdexcept>

namespace tts_front::detail {

SileroOrtSessionOwner::SileroOrtSessionOwner(const SileroBundle& bundle)
    : environment_(std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "tts-front-silero")) {
    if (bundle.ort_version != OrtGetApiBase()->GetVersionString())
        throw std::runtime_error("ONNX Runtime version does not match the verified bundle");
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(1);
    options.SetInterOpNumThreads(1);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    try {
        const auto stress_path = bundle.asset("stress.onnx");
        const auto yo_path = bundle.asset("yo.onnx");
        const auto homosolver_path = bundle.asset("homosolver.onnx");
        stress_ = std::make_unique<Ort::Session>(*environment_, stress_path.c_str(), options);
        yo_ = std::make_unique<Ort::Session>(*environment_, yo_path.c_str(), options);
        homosolver_ =
            std::make_unique<Ort::Session>(*environment_, homosolver_path.c_str(), options);
    } catch (const Ort::Exception& error) {
        throw std::runtime_error(std::string("unable to create Silero ORT sessions: ") +
                                 error.what());
    }
}

} // namespace tts_front::detail
