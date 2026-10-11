# Test rules

- Test observable contracts, not private implementation details.
- For every behavior change, add a focused direct regression and at least one
  neighboring case that must remain unchanged.
- Normalization tests should check normalized text, original text, warning code,
  byte offset/length, warning cardinality, overlap, and marker leakage when
  those properties apply.
- Prefer sentence-level/integration tests for pipeline ordering and domain
  interaction. Unit tests are useful for pure formatters and scanners but do
  not replace pipeline coverage.
- Keep adversarial probes small, deterministic, and committed when they encode
  a contract rather than a one-off debugging experiment.
