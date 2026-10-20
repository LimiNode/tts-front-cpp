# Agent development workflow

This project uses agents to shorten the path from a verified idea to a small,
understandable change. The agent is responsible for implementation work; the
developer remains responsible for semantics, architecture, and quality.

## Before editing

1. Read the root rules and the nearest domain rules.
2. Inspect the current pipeline and find the existing owner of the behavior.
3. Write a short change note: intent, invariant, affected domain, alternatives,
   positive cases, negative cases, and expected diagnostics.
4. Decide whether the task is a behavior change, refactor, oracle change, or
   release operation. Keep those categories separate.

Do not prepare a large implementation specification before obtaining feedback
from the running code. Record the intent and constraints, then use a focused
vertical slice to test the idea.

## During editing

- Prefer the smallest existing abstraction that expresses the contract.
- Search for an existing scanner, formatter, source-map helper, or policy before
  adding another one.
- Treat candidate admission as a shared boundary decision. A language normalizer
  must not special-case one currency, unit, sign, or punctuation prefix by
  skipping the common scanner. Extend the scanner's typed continuation policy
  instead, then make all language and mixed paths consume that result.
- Before accepting a boundary fix, run a neighbor matrix that varies sign,
  grouping, attached versus spaced continuation, connector versus independent
  token, and language mode. Record preserved text, diagnostics, and byte spans;
  do not infer coverage from a single hand-picked example.
- Keep the implementation understandable at the level being changed. A helper
  that only reduces line count is not automatically a useful abstraction.
- For an ambiguous input, choose preservation plus a diagnostic unless the
  project owner has approved a different semantic contract.
- Do not modify tests, snapshots, or documentation to conceal an implementation
  failure. If the intended behavior changed, record that as a separate decision.

## Review loop

1. Run the focused regression.
2. Run the adversarial neighbor matrix.
3. Run the full relevant tests and quality checks.
4. Review the diff for new dependencies, duplicated policy, and unnecessary
   code. In particular, reject new wrapper-level early exits that duplicate or
   weaken scanner policy. After several corrective patches, perform a
   simplification review or compare an independent implementation in a
   temporary worktree.
5. Only then create a PR.

Architecture, language policy, public API, and oracle changes need explicit
developer review; an agent's self-review is evidence, not approval.

## Release gate

A release requires an actual PR, exact-head PR CI, clean release/archive
audits, install-consumer verification, documented changes, and a verified merge
SHA. Do not create a tag from a branch push merely because its push workflow is
green.
