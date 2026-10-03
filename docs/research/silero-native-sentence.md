# Silero native sentence parity

This research pass closes the native sentence execution path for the pinned
Phase 1 corpus. The C++ executable starts from the original UTF-8 sentence and
performs tokenization, exception and homograph routing, n-gram embedding
construction, ONNX Runtime classifier/homosolver calls, and final marker/ё
reconstruction without Python in the execution path.

The fail-closed auditor is:

```text
python tools/silero_native_sentence_parity.py \
  --executable .temp/silero-native-fullcall-build/silero_native_sentence.exe \
  --assets .temp/silero-native-assets-fullcall \
  --stress .temp/silero-native-assets-fullcall/stress.onnx \
  --yo .temp/silero-native-assets-fullcall/yo.onnx \
  --homo .temp/silero-native-assets-fullcall/homosolver.onnx \
  --receipt docs/research/silero-native-sentence-receipt.json
```

It passes UTF-8 bytes to the native process, compares final output byte for
byte, and verifies the expected route counts for all six Phase 1 vectors. The
receipt sets `full_native_call_parity` to `true` only after every output and
route check succeeds. It also records hashes for the executable, all three
graphs, and the exported native asset manifest.

At startup the native loader verifies the manifest schema/bundle/ORT version
and recomputes SHA-256 for every runtime asset before loading it. Missing,
truncated, modified, or schema-incompatible bundles fail closed.

Provenance regressions for changing only `source_revision` or only
`model_sha256` are exercised by:

```text
python tools/silero_native_bundle_negative.py \
  --executable .temp/silero-native-fullcall-build/silero_native_sentence.exe \
  --assets .temp/silero-native-assets-fullcall
```

The phrase resource is exported from all literal alternatives in the pinned
upstream `compiled_phrases` table (37,375 deterministic rules), rather than
from only the six parity vectors. The exporter intentionally rejects regex
constructs that cannot be represented by this literal native format. ONNX
Runtime remains an optional research dependency; benchmark and
`StressMode::Automatic` integration are separate follow-up work.
