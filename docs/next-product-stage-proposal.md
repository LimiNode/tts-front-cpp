# Next product stage proposal

This is a short planning proposal after the `v0.2.4` stability line. It does
not change production behavior, the oracle, release archives, or the public API.

## Option A — expand the natural sentence corpus

Extend the executable RU/EN corpus with representative sentence-sized inputs:

- dates, years, units, currency and percentages in complete sentences;
- mixed-language ownership and technical neighbors;
- punctuation, newline and independent-number boundaries;
- malformed and fail-closed adversarial neighbors for every new positive row.

Each fixture records the exact normalized and pronunciation text, original text,
ordered diagnostics, UTF-8 byte spans, provenance, and adaptation date. The
existing normalization policy remains unchanged.

## Option B — add a new language

Add one language only after its contract is approved. The proposal must define
language detection and `Auto`/`SegmentCandidates` ownership, supported numeric
surfaces, fail-closed behavior, technical shielding, diagnostics, and a focused
fixture corpus. Shared scanner and source-span logic must be reused; common
normalization must not learn the new language's lexical policy.

## Recommendation and gate

Option A is the lower-risk next step: it improves evidence for the existing
RU/EN product without introducing a new semantic owner or public language enum.
Option B remains viable when a concrete product requirement and maintained
linguistic contract exist.

Before either direction starts, approve one focused product brief and its exit
criteria. The implementation must then be split into characterization,
behavior, and (if needed) oracle commits, with exact-head CI and post-merge CI
verified before the next stage.
