# `normalization` rules

Common normalization is an orchestrator and admission layer, not a second
language implementation.

- Use the shared numeric scanner, protected-range index, warning sink, and edit
  script. Do not add a regex-only boundary policy for a new numeric form.
- Fixed regexes are acceptable for stable lexical shapes after admission. They
  must not decide whether a malformed or technical candidate may be split.
- A rejected candidate must block every later normalizer from rewriting its
  continuation, including whitespace-linked numbers and `/`, `#`, `$`, `%`,
  signs, Unicode dashes, and attached words.
- Preserve candidate spans in original bytes. Duplicate warnings with
  identical `(code, offset, length)` are forbidden, and distinct non-empty
  `UnresolvedNumber` spans must not overlap. Global diagnostics and warnings
  from different categories may overlap candidate-specific warnings.
- Any new pass must state its ordering relative to technical protection and
  explain why it cannot observe a protected range.
