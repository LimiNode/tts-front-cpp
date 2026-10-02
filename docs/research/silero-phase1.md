# Silero Stress Phase 1 receipt

This document records the first reproducible reference pass. It is research
material only: no Silero model, PyTorch, ONNX Runtime, or LibTorch is part of
the `tts_front` core.

## Pinned inputs

- project: `snakers4/silero-stress`;
- source revision: `d38096cae9bf3ac846bbb88705428f3ed3801b96`;
- model artifact: `src/silero_stress/data/accentor.pt`;
- model SHA-256: `aecb207df9db34a079de2ba91edba2a9333839ad24de0cf17e7bc82424876786`;
- model loader: `torch.package.PackageImporter` on the resolved artifact path;
- source license: MIT;
- runtime settings: one Torch intra-op and inter-op thread.

The artifact is intentionally not copied into this repository. Recreate the
local checkout under `.temp/` and run:

```text
python tools/silero_phase1.py \
  --source .temp/silero-source \
  --output docs/research/silero-phase1-vectors.jsonl
```

The harness refuses a moving or different source revision unless
`--allow-different-revision` is explicitly supplied for exploratory work. The
`--model` argument is loaded directly with the same `torch.package` loader as
upstream; its resolved path and SHA-256 are written to the metadata record.
The JSONL receipt starts with metadata and then stores six deterministic
vectors covering exception/model lookup, OOV fallback, two semantic homograph
contexts, the Cyrillic yo/E case, and punctuation boundaries.

Each vector records upstream token boundaries in both character and UTF-8 byte
coordinates, the observed branch (`exception_lookup`, `homograph_lookup`, or
the accentor model path), the model word, and a neutral zero-based
`stressed_vowel` ordinal derived from the `+` marker. Homograph vectors also
carry an expected semantic sense separately from the observed model output;
reference parity and semantic accuracy are intentionally different metrics.

The recorded output is the upstream `SileroStress.__call__` contract with `+`
stress markers. It is not yet the `tts_front::WordPronunciation` contract and
does not imply that `StressMode::Automatic` is available.

## Gate status

The pinned Python reference receipt is complete: source and the artifact
actually passed to `PackageImporter` are hashed, preprocessing and branch
observations are captured, neutral stress ordinals are recorded, and the
harness checks two identical inference repeats. This closes the Phase 1
reference gate only. ONNX export, PyTorch/ONNX parity, native C++ parity, and
CPU/Windows benchmarks remain later gates.
