# Silero native runtime benchmark

The benchmark characterizes the research runtime without changing the
correctness corpus. It measures:

- total bundle/graph size and SHA-256 verification time;
- cold initialization (process startup, bundle loading, and ORT session creation);
- cold first-sentence latency for ordinary, homograph, phrase-hit, and mixed cases;
- warm p50/p95/p99 latency with one process and reused ORT sessions;
- peak working set when the platform exposes it (Windows uses
  `GetProcessMemoryInfo`).

Run it with the exact native executable and hashed bundle:

```text
python tools/silero_native_benchmark.py \
  --executable .temp/silero-native-fullcall-build/silero_native_sentence.exe \
  --assets .temp/silero-native-assets-fullcall \
  --stress .temp/silero-native-assets-fullcall/stress.onnx \
  --yo .temp/silero-native-assets-fullcall/yo.onnx \
  --homo .temp/silero-native-assets-fullcall/homosolver.onnx \
  --cold-runs 5 \
  --warm-runs 30 \
  --output docs/research/silero-native-benchmark-receipt.json
```

Warm measurements use the executable's `--stream` mode. This keeps the three
ORT sessions alive and measures request/response latency per sentence rather
than process startup. The production baseline is Windows/MSVC, one thread;
the receipt records the host and sample counts so measurements from other
machines are not treated as interchangeable.

This is characterization only. It does not assert a latency budget or make
ONNX Runtime a dependency of the core library. Those decisions belong to the
subsequent private backend and packaging stages.
