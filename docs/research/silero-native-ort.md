# Native ONNX Runtime execution boundary

The native preprocessing prototype now has an optional ONNX Runtime probe.
It is deliberately outside the `tts_front` core and is enabled only when
`SILERO_NATIVE_ORT_ROOT` points to a pinned ONNX Runtime C/C++ package.

The current research pin is ONNX Runtime `1.30.0`, matching the Phase 2/3
receipts. The probe supports the two graph input contracts already proven in
Python:

- `accentor`: a text file containing `rows columns` followed by float32
  embedding values;
- `homograph`: a text file containing `batch sequence`, flattened int64
  `input_ids`, then int64 `homo_start_ids` and `homo_end_ids`.

It loads all graph outputs and prints their resolved shape and float values.
This is an execution boundary probe, not yet sentence orchestration or
production integration.

Example configuration:

```text
cmake -S research/silero-native -B .temp/silero-native-ort-build \
  -DSILERO_NATIVE_ORT_ROOT=.temp/onnxruntime-win-x64-1.30.0
cmake --build .temp/silero-native-ort-build --config Release

silero_native_ort_probe.exe \
  --graph .temp/silero-phase3/accentor-stress.onnx \
  --kind accentor \
  --input .temp/stress-input.txt
```

The package itself and ONNX artifacts remain untracked under `.temp/`.
The fail-closed parity runner exercises the same graph cases through the
native probe and Python ONNX Runtime reference:

```text
python tools/silero_native_ort_parity.py \
  --source .temp/silero-source \
  --stress-onnx .temp/silero-phase3/accentor-stress.onnx \
  --yo-onnx .temp/silero-phase3/accentor-yo.onnx \
  --homosolver-onnx .temp/silero-phase2-dynamic/homosolver.onnx \
  --probe .temp/silero-native-ort-build/silero_native_ort_probe.exe \
  --receipt .temp/silero-native-ort-receipt.json
```

The runner requires output-shape, float-error, and argmax parity for stress,
yo, and both Phase 2 homograph contexts. This first step establishes a
version-pinned execution adapter without adding ONNX Runtime to the library
target; `full_native_call_parity` remains false.

The first local receipt (`silero-native-ort-receipt.json`) records native
execution against ORT 1.30.0 for all four cases: stress, yo, and both
homosolver contexts. Output shapes and argmax decisions match the Python
reference; the observed max absolute error is `0.0` for each case.

The native ORT CI job also generates a tiny opset-17 Identity graph and runs
the probe end-to-end. This validates runtime loading and tensor transport
without committing model artifacts; it is a smoke check, not Silero parity.
