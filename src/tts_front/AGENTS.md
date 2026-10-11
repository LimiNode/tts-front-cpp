# `src/tts_front` domain rules

The frontend is a deterministic pipeline with explicit ownership. Keep the
dependency direction visible and avoid solving a local problem by adding a
cross-domain shortcut.

## Ownership

- `core/` owns UTF-8 decoding, codepoint classification, source spans, mapped
  text, edit scripts, tokenization, numeric surfaces, and neutral text helpers.
- `normalization/` owns common admission and pipeline coordination. It may
  consume language and technical interfaces, but must not own language words,
  grammatical forms, or language-specific regexes.
- `language/` owns English and Russian patterns, formatters, lexical data,
  language admission, and mixed-candidate formatting.
- `technical/` owns technical identifiers, URLs/emails, protected ranges, and
  restoration. It must not duplicate language-formatting rules.
- `serialization/`, `dictionary/`, and `backend/` remain independent domains.

## Pipeline invariants

- A candidate is either transformed as one complete source span or preserved as
  one complete source span with a diagnostic. Never normalize a suffix or a
  continuation after rejecting the head.
- `original_text` is byte-for-byte stable. Diagnostics use original byte
  offsets, have non-overlapping spans, and never expose internal markers.
- Technical protection is established before language passes can interpret a
  protected range. Any pre-pass must explicitly skip protected ranges.
- All edits are source-mapped and applied through the shared edit machinery.

When a change crosses these boundaries, document the reason and update the
relevant domain tests rather than adding a private exception.
