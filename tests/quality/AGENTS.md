# Quality corpus and archive rules

The working corpus measures current behavior; a release archive records a
historical contract. They are different artifacts.

- Never edit a release archive to make a new implementation pass.
- A working-corpus expectation change requires a documented semantic decision,
  an explicit old/new result, and a direct regression test. Mark it as an
  oracle change in the PR description.
- Keep generator output, TSV, snapshot, diagnostics, and checksums synchronized.
- The audit must detect new mismatches, stale exceptions, changed normalized or
  pronunciation output, diagnostic drift, duplicate/overlapping spans, marker
  leakage, and original-text changes.
- A zero-mismatch report is not sufficient when the oracle changed in the same
  commit. Report the before/after baseline and classify the change.
- Every new quality case needs a positive/negative neighbor and, for numeric
  behavior, explicit EN/RU/Auto or mixed-policy coverage.
