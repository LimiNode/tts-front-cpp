# AGENTS.md

Repository rules for agents working on `tts-front-cpp`.

Read this file first, then read the nearest domain-specific `AGENTS.md` before
editing code. The domain files describe ownership and invariants; this file
describes the non-negotiable project contract and the development workflow.

## Project contract

- This is a standalone C++17 text frontend for TTS pipelines.
- Keep the core deterministic, thread-safe, UTF-8 based, and independent of
  Python, PyTorch, Qwen, networking, ICU, Pynini, OpenFST, and TTS runtimes.
- The canonical public include is `<tts_front.hpp>`.
- For valid UTF-8, unsupported or ambiguous normalization candidates are
  fail-closed: preserve their original source bytes and emit an honest
  candidate diagnostic with a byte-accurate span. Invalid UTF-8 is rejected
  with `InvalidUtf8`; the bytes remain available in `original_text`, while
  `normalized_text` need not be produced. Zero-length global diagnostics are
  valid and use offset 0, length 0.
- Do not add empty forwarding headers or speculative public abstractions.

## Ownership and decisions

- Product semantics, language policy, public API, architecture boundaries, and
  quality-oracle changes are developer decisions. The agent may present options
  and trade-offs, but must not silently choose among materially different
  meanings.
- If a language rule is ambiguous, preserve the input and report the ambiguity
  instead of inventing a grammatical or pronunciation contract.
- Document intent, constraints, and invariants. Do not turn `AGENTS.md` into a
  description of every implementation detail.

## Change discipline

- Before editing, identify the contract, the owning domain, the existing helper
  or scanner that should be reused, and the positive and negative cases.
- Keep one behavioral hypothesis per commit. Separate semantic changes, oracle
  changes, architecture refactors, formatting-only changes, and release work.
- Every new behavior needs a focused regression test and an adversarial neighbor
  test. A green corpus snapshot alone is not evidence of correctness.
- Admission changes are scanner-first: do not bypass a shared candidate scanner
  from a language wrapper with a prefix- or suffix-specific `continue`, guard,
  or regex exception. If the scanner cannot distinguish an independent token
  from a connector continuation, fix that distinction at the scanner boundary
  and reuse its result in every language path.
- For every candidate-boundary rule, test the Cartesian neighbors before
  merging: unsigned, `+`, ASCII `-`, and Unicode minus; plain and grouped or
  currency forms; attached and spaced continuations; connector and ordinary
  next-token cases. The contract must state exact source preservation,
  diagnostic count, and byte span for each row.
- Any admission or candidate-boundary change must include a boundary matrix for
  each affected mode: a known suffix followed by a combining mark, a combining
  mark followed by an ordinary lexical word, an ASCII continuation, a Cyrillic
  continuation, punctuation, and end-of-string. Compare every matrix row with
  the base commit and record exact text, diagnostics, and byte spans for both
  the changed case and its unchanged neighbors; cover RU, Auto, and
  SegmentCandidates, plus EN whenever the shared scanner or admission path is
  affected.
- Do not change an expected output merely to make an audit pass. Any oracle
  change must show the old result, the new contract, the reason, and a regression
  test; release archives are immutable after publication.
- After three to five corrective commits in one area, stop patching and perform
  a simplification and dependency review before adding another special case.
- A corrective commit must not change an oracle and its implementation merely
  to make the same audit pass. Keep the behavioral fix, scanner/model change,
  and oracle decision separate so a green snapshot cannot hide a partial
  rewrite or a newly suppressed diagnostic.

## Architecture rules

- Read `src/tts_front/AGENTS.md` and the nearest domain rules for source changes.
- Shared UTF-8, source mapping, edit scripts, numeric surfaces, and neutral text
  classification belong under `src/tts_front/core/`.
- Language-specific patterns, dictionaries, formatters, and mixed-language
  policy belong under `src/tts_front/language/`.
- Technical identifiers and protected ranges belong under
  `src/tts_front/technical/`.
- Common normalization coordinates the pipeline; it must not accumulate English
  or Russian lexical knowledge.
- Do not duplicate numeric-boundary, continuation, UTF-8, or source-span logic
  in a language regex. Reuse the shared scanner and protection index.

## Workspace hygiene

- Keep the repository root free of generated builds, test output, coverage files,
  downloaded tools, and other temporary artifacts.
- Put every CMake build tree and install/consumer staging tree under
  `.build/` (for example, `.build/debug`, `.build/release`,
  `.build/format`, `.build/benchmark`, or `.build/consumer`).
- Put downloaded SDKs, temporary source checkouts, coverage data, probes, and
  other non-build transient files under `.temp/`.
- Do not create `build*`, `stage*`, `consumer-*`, `cmake-build-*`,
  `coverage`, or ad-hoc output directories in the repository root.
- Before completing work, remove accidental generated artifacts outside
  `.build/` and `.temp/`, and verify that the worktree is clean apart from
  intentional changes.

## Dependencies and external layout

- Project-owned third-party dependencies must be direct Git submodules under
  `external/<name>`, with one flat entry per dependency in `.gitmodules`.
- Prefer direct sibling submodules over recursively initialized nested
  submodules. If an upstream dependency requires a nested dependency, keep the
  exception explicit in CMake and document why it cannot be flattened.
- Do not use FetchContent, ExternalProject, or an unpinned network download to
  provide a normal build dependency without an explicit architecture decision.
- Downloaded binary SDKs and CI-only source checkouts are transient inputs, not
  project submodules; keep them under `.temp/` and verify their revision or
  checksum.
- Do not add an empty `external/` directory or `.gitmodules` file when the
  project has no vendored dependency.

## Coding and documentation

- Use C++17, UTF-8 source, four spaces, no tabs, and the repository
  `.clang-format`.
- Keep public API documentation semantic and free of control characters.
- Do not introduce `char8_t` or mechanically convert existing literals to
  `u8""`.
- Keep normalization pipelines explicit; avoid abstractions that only reduce
  line count.
- Reuse the shared UTF-8 helpers under `src/tts_front/core/`; do not add
  duplicate validators or decoders.
- Compile fixed normalization regexes once. Do not add dynamic regex caches,
  mutexes, or speculative dictionary indexes without a benchmark and explicit
  need.
- Every behavior change requires a regression test. Corpus fixtures adapted from
  an upstream project must record the exact source revision and adaptation date.

## Verification and handoff

- Use `.build/` for all CMake builds, CTest trees, and install/consumer
  staging; use `.temp/` for probes, downloads, generated reports, and
  temporary worktrees.
- Run format-check, Debug and Release builds, CTest, fixture tests, the
  header-self-containment test, and install-consumer smoke as applicable.
- For normalization changes, also run the quality corpus, strict archive audit,
  checksum/generator checks, and targeted adversarial probes.
- Check `git diff --check` and the final worktree.
- Do not claim CI success without checking the exact pushed commit. A push CI
  run is not a PR or merge CI run.
- Before handoff, report the branch, exact commit, actual PR number or its
  absence, behavior/API changes, tests, exact CI status, deferred work, and
  workspace cleanliness.

See [`docs/agent-development.md`](docs/agent-development.md) for the workflow
and [`tests/quality/AGENTS.md`](tests/quality/AGENTS.md) for oracle rules.
