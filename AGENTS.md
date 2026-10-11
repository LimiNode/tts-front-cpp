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
- Do not change an expected output merely to make an audit pass. Any oracle
  change must show the old result, the new contract, the reason, and a regression
  test; release archives are immutable after publication.
- After three to five corrective commits in one area, stop patching and perform
  a simplification and dependency review before adding another special case.

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
- Put all temporary work under `.temp/` (for example, `.temp/build-debug`,
  `.temp/build-release`, `.temp/coverage`, or a temporary worktree).
- Do not create `build*`, `cmake-build-*`, `coverage`, or ad-hoc output
  directories in the repository root.
- Before completing work, remove accidental generated artifacts outside `.temp/`
  and verify that the worktree is clean apart from intentional changes.

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

- Use `.temp/` for builds, probes, generated reports, and temporary worktrees.
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
