# Silero Stress Phase 4: preprocessing boundary

This pass independently reproduces the accentor preprocessing boundary from
the pinned Silero artifact. It does not call the upstream tokenizer or
embedding implementation for the values being tested.

The harness reproduces:

- upstream token splitting, hyphen handling, cleaning, and prediction masks;
- `word_ngrams(1..len(word)+3)` with `<`/`>` padding;
- n-gram dictionary lookup, `UNK` fallback, and mean embedding-bag pooling;
- the `[word_count, 16]` boundary consumed by the Phase 3 stress and `yo`
  ONNX heads.

The receipt pins the model and source hashes, the n-gram dictionary hash, the
embedding weight hash, and both ONNX artifact hashes. All six Phase 1 vectors
are exercised. The independent tokenizer matches upstream for every vector,
the maximum embedding error is `7.152557373046875e-07` against a strict
`1e-5` tolerance, and stress/yo argmax parity holds for every token batch.
The harness fails immediately on any tokenization, argmax, or embedding
threshold violation. Targeted cases also exercise hyphenated words, the
special `-то` mask, and `words_to_ignore`.

Run it with the exact Phase 3 artifacts under `.temp/`:

```text
python tools/silero_phase4_boundary.py \
  --source .temp/silero-source \
  --stress-onnx .temp/silero-phase3-corrective/accentor-stress.onnx \
  --yo-onnx .temp/silero-phase3-corrective/accentor-yo.onnx \
  --work-dir .temp/silero-phase4-boundary \
  --receipt docs/research/silero-phase4-boundary-receipt.json
```

## Remaining boundary

This is an embedding/classifier boundary gate, not full
`SileroStress.__call__` parity. Exception application, homograph context
resolution, marker reconstruction, and final sentence output remain explicitly
delegated to the upstream reference in this pass. `full_call_parity` therefore
remains `false`; the next gate must replace that delegation with the hybrid
pipeline and compare final outputs on the same Phase 1 vectors.
