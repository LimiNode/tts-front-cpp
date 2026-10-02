# Silero Stress research plan

This is a research-only track for evaluating automatic Russian stress and
context-sensitive homograph resolution. It does not add a backend, model files,
LibTorch, ONNX Runtime, or new public API to `tts_front`.

## Pinned starting point

- project: `snakers4/silero-stress`;
- revision surveyed by this repository: `d38096cae9bf3ac846bbb88705428f3ed3801b96`;
- license recorded in `docs/research/source-survey.md`: MIT.

The exact model artifact, weights, preprocessing, and tokenizer must be pinned
again when the harness is started. A moving `main` branch is not an acceptable
research input.

## Research sequence

```text
exact source/model revision
        -> Python/PyTorch reference harness
        -> deterministic stress vectors
        -> preprocessing and graph audit
        -> ONNX export + PyTorch/ONNX parity
        -> native C++ prototype + parity
        -> CPU/Windows benchmark
```

Each arrow is a gate. A later stage must not be presented as complete when the
previous parity gate is unresolved.

## Contract to reproduce

The reference harness must record, for every vector:

- input UTF-8 text and token boundaries;
- vocabulary lookup result, OOV path, and homograph/context path;
- model output and selected vowel ordinal;
- normalization of model-specific stress markers such as `+` into the
  model-neutral `WordPronunciation::stressed_vowel` convention;
- deterministic runtime settings and expected tolerance.

The C++ adapter should return semantic stress metadata, never expose Silero's
marker syntax, and preserve the existing dictionary-first cascade:

```text
user PronunciationDictionary
        -> deterministic known-word lookup
        -> automatic backend for unresolved/ambiguous words
        -> unresolved diagnostic
```

## Parity and portability gates

Before any integration proposal, record:

- exact PyTorch vectors and outputs;
- ONNX outputs and maximum deviation from PyTorch;
- native C++ outputs and maximum deviation from the reference;
- model size, cold-start cost, steady-state CPU latency, and memory;
- Windows/MSVC build and execution result;
- licenses and redistribution constraints for weights and runtime.

`StressMode::Automatic` remains unavailable in the core until these gates pass
and the dependency decision is explicitly reviewed. All downloads, exports,
benchmarks, and generated artifacts belong under `.temp/`.
