# Silero Stress Phase 2: ONNX graph parity

This is the first ONNX gate against the Phase 1 artifact. It is research-only
and does not add ONNX Runtime, LibTorch, model files, or a backend to the C++
core.

## Result

The pinned `homosolver.model` graph exports with the legacy TorchScript ONNX
exporter and passes CPU ONNX Runtime parity on the representative
`Солнце село.` span. The committed receipt records the exact model hash, input
IDs, span IDs, artifact hash, runtime versions, both outputs, and maximum
absolute error.

Run it with all generated artifacts below `.temp/`:

```text
python tools/silero_phase2_onnx.py \
  --source .temp/silero-source \
  --work-dir .temp/silero-phase2 \
  --receipt docs/research/silero-phase2-onnx-receipt.json
```

The harness checks the pinned source revision, loads the explicitly resolved
artifact through `torch.package.PackageImporter`, runs PyTorch first, exports
the graph, validates the ONNX model, runs ONNX Runtime on CPU, and fails if
the maximum absolute error exceeds `1e-4`.

## Scope boundary

This is graph-level parity, not full `SileroStress.__call__` parity. The
upstream tokenizer, n-gram accentor, homograph dictionaries, phrase/regex
rules, `+` marker projection, and sentence-level cascade remain outside the
exported graph. Full reference parity must not be claimed until those layers
are represented and checked as well.

The next gates are therefore:

1. export and test the accentor path, including its n-gram preprocessing;
2. reproduce tokenizer and homograph lookup inputs from the Phase 1 receipt;
3. compare complete sentence outputs, not only one neural graph call;
4. measure model size, CPU latency, and Windows/MSVC implications.
