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

The private sentence backend now performs tokenization, embeddings, phrase and
homograph routing, inference, and model-neutral semantic stress mapping.
`StressMode::Automatic` remains deliberately disconnected until this private
contract is reviewed independently.

The private sentence result now preserves route provenance through replacement
and re-tokenization. Each processed word is labeled `model`, `exception`,
`phrase`, or `homosolver`; the label is not inferred from the final spelling.
With a verified bundle and both executables available, the production probe
can be audited against the research executable with:

```text
python tools/silero_production_sentence_parity.py \
  --production build-production-ort/Release/tts_front_silero_sentence_probe.exe \
  --research build-native-ort/Release/silero_native_sentence.exe \
  --assets .temp/ci-silero-bundle \
  --stress .temp/ci-silero-bundle/stress.onnx \
  --yo .temp/ci-silero-bundle/yo.onnx \
  --homo .temp/ci-silero-bundle/homosolver.onnx \
  --receipt .temp/ci-production-sentence-parity.json
```

The parity gate compares UTF-8 output after removing the research-only `+`
stress marker, route counts, and stressed-vowel ordinals. The Windows CI job
recreates the bundle from the pinned upstream checkout before running this
gate; no model or graph bytes are committed to the repository.
