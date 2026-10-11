# `core` rules

Core is neutral infrastructure. It must not contain English/Russian words,
language-specific grammar, unit tables, or technical identifier lists.

- Use the shared UTF-8 document and source-span mapping; do not decode
  `substr()` repeatedly inside a scan.
- Numeric surface and continuation policy must have one implementation. New
  callers must consume it instead of reimplementing signs, exponent markers,
  Unicode dashes, suffixes, or boundaries.
- Keep scanners linear in input size. Any exception that scans backwards or
  performs a second pass needs an adversarial benchmark and an explanation.
- Core APIs should describe data and invariants, not English/Russian policy.
- Add boundary tests for malformed, signed, embedded, technical, URL/email,
  Unicode, and very long inputs whenever scanner behavior changes.
