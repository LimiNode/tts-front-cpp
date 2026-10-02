# Silero Stress Phase 1 receipt

This document records the first reproducible reference pass. It is research
material only: no Silero model, PyTorch, ONNX Runtime, or LibTorch is part of
the `tts_front` core.

## Pinned inputs

- project: `snakers4/silero-stress`;
- source revision: `d38096cae9bf3ac846bbb88705428f3ed3801b96`;
- model artifact: `src/silero_stress/data/accentor.pt`;
- model SHA-256: `aecb207df9db34a079de2ba91edba2a9333839ad24de0cf17e7bc82424876786`;
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
JSONL receipt starts with a metadata record and then stores six deterministic
vectors covering vocabulary lookup, OOV handling, two homograph contexts,
`ё`/`Е` handling, and punctuation boundaries.

The recorded output is the upstream `SileroStress.__call__` contract with `+`
stress markers. It is not yet the `tts_front::WordPronunciation` contract and
does not imply that `StressMode::Automatic` is available.

## Gate status

Phase 1 is complete for the pinned Python reference layer: source and model
identity are hashed, preprocessing is exercised through the upstream public
call, and deterministic outputs are captured. ONNX export, PyTorch/ONNX
parity, native C++ parity, and CPU/Windows benchmarks remain later gates.
