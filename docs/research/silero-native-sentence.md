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
  --stress .temp/silero-phase3/accentor-stress.onnx \
  --yo .temp/silero-phase3/accentor-yo.onnx \
  --homo .temp/silero-phase2-dynamic/homosolver.onnx \
  --receipt docs/research/silero-native-sentence-receipt.json
```

It passes UTF-8 bytes to the native process, compares final output byte for
byte, and verifies the expected route counts for all six Phase 1 vectors. The
receipt sets `full_native_call_parity` to `true` only after every output and
route check succeeds. It also records hashes for the executable, all three
graphs, and the exported native asset manifest.

The phrase rules in this prototype are deliberately corpus-scoped exports from
the pinned Phase 1 vectors. They are not yet a complete upstream phrase
resource. Consequently this closes the research correctness gate, not the
production asset/distribution contract. ONNX Runtime remains an optional
research dependency; benchmark, bundle packaging, and `StressMode::Automatic`
integration are separate follow-up work.
