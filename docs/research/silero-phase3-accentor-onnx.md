# Silero Stress Phase 3: accentor classifier parity

This research pass isolates the two neural classifier heads used by
`AccentorNgram`. The upstream `accentor.model` accepts `List[str]` and performs
custom n-gram dictionary lookup inside a ScriptModule; exporting that string
preprocessing directly is not a valid ONNX boundary yet.

Instead, the pass exports two small ONNX graphs:

- `stress_clf`: 16-dimensional n-gram embedding to stress logits;
- `yo_clf`: 16-dimensional n-gram embedding to yo logits.

Both graphs have a dynamic `word_count` axis. The same exported artifacts are
exercised at `word_count = 1` and `word_count = 7` using words from the Phase 1
corpus, including the positive `ё` case `елка`. CPU ONNX Runtime matches
PyTorch argmax decisions for every word and both heads; the maximum raw-logit
error is `0.03125` (the yo head has logits around `2.8e5`, so this remains
sub-ppm relative error). The receipt stores per-case word lists, per-word
argmax decisions, artifact hashes, and per-head errors.

Run with temporary exports under `.temp/`:

```text
python tools/silero_phase3_accentor_onnx.py \
  --source .temp/silero-source \
  --work-dir .temp/silero-phase3 \
  --receipt docs/research/silero-phase3-accentor-receipt.json
```

The committed receipt records the exact source/model hashes and the two
dynamic-shape cases. `full_call_parity` remains `false`.

## Remaining boundary

This is not full `SileroStress.__call__` parity. The n-gram vocabulary,
character preprocessing, tokenization, exception lookup, `+` marker
projection, and sentence-level cascade remain Python/reference behavior. The
next pass must reproduce that embedding boundary and compare complete Phase 1
sentence outputs before any native integration proposal.
