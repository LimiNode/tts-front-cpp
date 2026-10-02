# Silero Stress Phase 4: sentence-level orchestration

This pass composes the already pinned neural graphs with an independent
sentence-level Python pipeline:

```text
tokenize
→ branch selection
→ exception lookup
→ accentor stress/yo ONNX
→ homograph phrase/neural decision
→ homosolver ONNX
→ ё reconstruction
→ stress reconstruction
→ final sentence
```

All six Phase 1 vectors now have exact output parity. The receipt records the
hybrid output, reference output, homograph decision path, and hashes of the
three ONNX artifacts. The run is deterministic across two independent
executions.

Run with the pinned artifacts under `.temp/`:

```text
python tools/silero_phase4_fullcall.py \
  --source .temp/silero-source \
  --stress-onnx .temp/silero-phase3-corrective/accentor-stress.onnx \
  --yo-onnx .temp/silero-phase3-corrective/accentor-yo.onnx \
  --homosolver-onnx .temp/silero-phase2-dynamic/homosolver.onnx \
  --work-dir .temp/silero-phase4-fullcall \
  --receipt docs/research/silero-phase4-fullcall-receipt.json
```

## Remaining native boundary

The hybrid harness still uses the pinned upstream `SimpleBertTokenizer` and
compiled phrase-rule resources when preparing homograph inputs. Thus this is
an exact Python/ONNX orchestration gate, not yet a dependency-free native
runtime. The next step is the C++ prototype, which must replace those two
upstream helpers with hashed, explicitly ported artifacts while preserving this
receipt's six-vector output contract.
