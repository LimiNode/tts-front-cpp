# Silero Stress Phase 2: ONNX graph parity

This is the first ONNX gate against the Phase 1 artifact. It is research-only
and does not add ONNX Runtime, LibTorch, model files, or a backend to the C++
core.

## Result

The pinned `homosolver.model` graph exports with dynamic sequence and
homograph-count axes and passes CPU ONNX Runtime parity on both semantic
homograph cases from Phase 1:

- `Солнце село.` — verb reading;
- `Это большое село.` — settlement noun reading.

The receipt records per-case inputs, span IDs, outputs, errors, and the global
maximum error. The current global maximum is below `1e-4`.

Run it with all generated artifacts below `.temp/`:

```text
python tools/silero_phase2_onnx.py \
  --source .temp/silero-source \
  --work-dir .temp/silero-phase2 \
  --receipt docs/research/silero-phase2-onnx-receipt.json
```

The harness checks the pinned source revision, loads the explicitly resolved
artifact through `torch.package.PackageImporter`, runs PyTorch first, exports
the graph with dynamic axes, validates the ONNX model, runs ONNX Runtime on
CPU for both cases, and fails if any per-case error exceeds `1e-4`.

## Span provenance and scope boundary

The `homo_start_ids` and `homo_end_ids` are reference-prepared inputs derived
from the upstream homosolver model contract. They are recorded explicitly in
each case; reproducing the tokenizer and homograph lookup pipeline that
produces them is a later gate.

This is still graph-level parity, not full `SileroStress.__call__` parity. The
upstream tokenizer, n-gram accentor, homograph dictionaries, phrase/regex
rules, `+` marker projection, and sentence-level cascade remain outside the
exported graph.

The next gates are therefore:

1. export and test the accentor path, including its n-gram preprocessing;
2. reproduce tokenizer and homograph lookup inputs from the Phase 1 receipt;
3. compare complete sentence outputs, not only one neural graph call;
4. measure model size, CPU latency, and Windows/MSVC implications.
