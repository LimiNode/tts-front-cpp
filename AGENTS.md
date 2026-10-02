# AGENTS.md

Repository-specific instructions for coding agents working on `tts-front-cpp`.

## Project and API

- This is a standalone C++17 text frontend for TTS pipelines.
- Keep the core deterministic, thread-safe, UTF-8 based, and independent of Python,
  PyTorch, Qwen, networking, ICU, Pynini, OpenFST, and any TTS runtime.
- The canonical public include is:

  ```cpp
  #include <tts_front.hpp>
  ```

- Do not add empty forwarding headers or speculative public abstractions.
- Preserve fail-closed behavior for unsupported or ambiguous input and keep diagnostics
  honest about source offsets.
- Reuse the shared UTF-8 helpers under `src/detail/`; do not add duplicate validators
  or decoders.
- Fixed normalization regexes must be compiled once. Do not add dynamic regex caches,
  mutexes, or speculative dictionary indexes without a benchmark and explicit need.

## Workspace hygiene

- Keep the repository root free of generated builds, test output, coverage files,
  downloaded tools, and other temporary artifacts.
- Put all temporary work under `tts-front-cpp/.temp/` (for example,
  `.temp/build-debug`, `.temp/build-release`, `.temp/coverage`).
- The `.temp/` directory is ignored by Git. Do not create `build*`, `cmake-build-*`,
  `coverage`, or ad-hoc output directories in the repository root.
- Before completing work, remove accidental generated artifacts outside `.temp/` and
  verify `git status` is clean apart from intentional changes.

## Coding and documentation

- Use C++17, UTF-8 source, four spaces, no tabs, and the repository `.clang-format`.
- Keep public API documentation semantic and free of control characters.
- Do not introduce `char8_t` or mechanically convert existing literals to `u8""`.
- Keep normalization pipelines explicit; avoid abstractions that only reduce line count.
- Every behavior change requires a regression test. Corpus fixtures adapted from an
  upstream project must record the exact source revision and adaptation date.

## Verification

Run format-check, Debug and Release builds, CTest, fixture tests, the header
self-containment test, and the install-consumer smoke test. Use `.temp/` for all build
directories and temporary files. Do not claim CI success without checking the exact
pushed commit.

Before handoff, report the branch, exact commit, PR (when applicable), behavior/API
changes, tests, CI status, deferred work, and workspace cleanliness.
