# Silero native prototype: preprocessing boundary

The first native prototype ports the two deterministic preprocessing pieces
needed before ONNX Runtime integration:

- C++17 UTF-8 Russian case folding and Silero n-gram generation/mean pooling;
- C++17 WordPiece tokenization for the pinned homosolver vocabulary, including
  `[HOMO]` and `[/HOMO]` markers.

The asset exporter writes only to `.temp/` and records hashes for the pinned
model, n-gram dictionary, embedding weights, BERT vocabulary, and homograph
dictionary. No model or generated asset is committed.

Build and run the preprocessing prototype:

```text
python tools/silero_export_native_assets.py \
  --source .temp/silero-source \
  --output .temp/silero-native-assets

cmake -S research/silero-native -B .temp/silero-native-build
cmake --build .temp/silero-native-build --config Release

python tools/silero_native_parity.py \
  --source .temp/silero-source \
  --assets .temp/silero-native-assets \
  --preprocess-exe .temp/silero-native-build/silero_native_preprocess.exe \
  --wordpiece-exe .temp/silero-native-build/silero_native_wordpiece.exe \
  --receipt .temp/silero-native-assets/parity-receipt.json
```

Observed native parity:

- embedding max absolute error: `7.152557373046875e-07` (`1e-5` tolerance);
- `Солнце село.` tokenizer IDs: `[2, 40227, 1806, 18, 3]`;
- `Это большое село.` tokenizer IDs: `[2, 5130, 15640, 1806, 18, 3]`;
- marked `[HOMO] мел [/HOMO]` IDs also match the pinned reference exactly.

## Remaining native gates

This is intentionally not production integration and does not modify the
`tts_front` core. ONNX Runtime C++ graph execution, phrase-rule resource
port, stress/yo reconstruction, and full native sentence parity remain open.
The committed receipt therefore uses `full_native_call_parity = false`.
