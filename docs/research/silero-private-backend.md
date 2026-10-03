# Private Silero backend lifecycle

The library now has a private production boundary for the pinned native Silero
bundle. It is intentionally not part of `tts_front.hpp` and is disabled in the
default build.

## Bundle verification

`src/detail/silero_bundle.*` validates `manifest.json` before any graph is
opened. The loader checks the complete compatibility profile:

- record type, schema, bundle version, and pinned ORT version;
- pinned upstream source revision and model SHA-256;
- every required runtime asset with bounded-memory streaming SHA-256;
- missing, malformed, undeclared, or modified assets fail closed.

The current profile is `silero-native-phase1-v1` with ONNX Runtime `1.30.0`.
The loader is private and returns only manifest-verified paths to subsequent
components.

## Optional ORT owner

Configure the private owner explicitly:

```text
cmake -S . -B build \
  -DTTS_FRONT_ENABLE_ONNX_STRESS=ON \
  -DTTS_FRONT_ONNX_RUNTIME_ROOT=/path/to/onnxruntime
```

The runtime root must contain `include/onnxruntime_cxx_api.h` and the matching
`lib/onnxruntime` library. The owner creates `stress`, `yo`, and `homosolver`
sessions once per backend instance and keeps them alive for reuse. ONNX Runtime
is not linked when the option is `OFF`.

The current batch establishes lifecycle and verification only. Sentence
tokenization, embeddings, homograph routing, inference, and semantic stress
result mapping remain the next private backend slice; `StressMode::Automatic`
still reports its existing unavailable-backend warning until that contract is
implemented and tested.
