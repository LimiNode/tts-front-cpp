# Russian `за` date-context policy review

Review-only document for the deferred Russian agreement decision after the
`v0.2.4` release. This document changes no production code, tests, oracle,
snapshots, public API, CMake metadata, or published release archives.

## Current behavior

The Russian date formatter currently uses the surface-local day form unless a
supported context policy explicitly selects another case. Therefore the
current output is:

```text
за 01.01.2024
→ за первое января две тысячи двадцать четвёртого года
```

This output remains the contract until a product decision approves a
contextual interpretation. A valid date with no approved interpretation is not
malformed and must not receive `UnresolvedNumber` merely because its governor
is unresolved.

## Decision for the current product contract

The current product choice is to keep valid dates after `за` surface-local.
The formatter does not infer a temporal, duration, or object/reference reading
from `за` alone, does not apply a contextual case rewrite, and does not emit a
warning for an otherwise valid date. The characterization matrix in PR #63
records this contract in Russian, Auto, and SegmentCandidates modes.

This closes the deferred item for the current release line. A future temporal
interpretation requires a separate product decision with a bounded pattern,
explicit fallback, regression tests, and a separately justified oracle change;
no such behavior is part of the current contract.

## Why `за` is ambiguous

The preposition `за` has several ordinary readings, and the date surface alone
does not identify which one is intended:

| Reading | Example | Grammatical question |
|---|---|---|
| Temporal boundary | `за 01.01.2024` in a deadline or interval statement | Is this a date boundary or a duration endpoint? |
| Duration/period | `за 01.01.2024` in a period-like phrase | Is an omitted noun understood, and which case does it govern? |
| Object/reference | `плата за 01.01.2024` | Is the date an object of reference rather than a temporal boundary? |
| Lexical or mixed context | `за 01.01.2024 today` | Which language segment owns the date and its agreement? |

The policy must not infer the intended reading from the numeric value, sentence
distance, or a single neighboring token. If the context does not establish one
meaning with a bounded rule, the formatter keeps the current surface-local
output until a later decision.

## Alternatives requiring a product decision

### A. Keep `за` surface-local

Every valid date after `за` remains:

```text
за 01.01.2024
→ за первое января две тысячи двадцать четвёртого года
```

This is the safest compatibility option and requires no new contextual
classifier.

### B. Support one explicitly bounded temporal construction

The product could define a narrow temporal pattern and its exact output, for
example a documented deadline or interval phrase. That decision must state:

- the exact governor and allowed lexical distance;
- the day and month case, including `1`, `2`, `5`, `21`, `22`, `42`, `100`, and
  compound values where applicable;
- whether punctuation, quotes, newline, or technical tokens break the context;
- the behavior in `Russian`, `Auto`, and `SegmentCandidates`;
- the fallback when the same surface can be read as duration or object;
- exact `normalized_text`, `pronunciation_text`, diagnostics, and UTF‑8 byte
  spans for positive and negative neighbors.

### C. Preserve and diagnose ambiguous `за` contexts

The product may choose a fail-closed diagnostic for an explicitly identified
ambiguous contextual surface. This would be a new diagnostic contract, not an
implicit consequence of missing context. It must preserve the complete source
span, avoid partial rewrites, and be approved separately from implementation.

## Required decision matrix

Before a behavior PR, compare at least these rows in Russian, Auto, and
SegmentCandidates. English is a control whenever the shared candidate scanner
is involved.

| Dimension | Required values |
|---|---|
| reading | temporal, duration, object/reference, ambiguous |
| date | `01.01.2024`, `29.02.2024`, `31.04.2024`, `01.01.2000` |
| value shape | `1`, `2`, `5`, `21`, `22`, `42`, `100`, compound values |
| boundary | sentence end, `.`, `!`, `,`, quote, newline, independent number |
| continuation | attached ASCII/Cyrillic word, `#`, `/`, `%`, sign, combining mark |
| ownership | Russian sentence, Latin neighbor, mixed sentence, technical token |
| mode | Russian, Auto, SegmentCandidates, English control |

Every row must record exact normalized and pronunciation text plus the complete
ordered diagnostic set `(code, offset, length)`. Pair each proposed positive
case with the nearest surface-local, malformed, continuation, and technical
neighbors. The matrix must prove that no contextual rewrite partially changes a
date, terminal punctuation, or an independent following number.

## Boundaries and sequencing

The shared scanner remains responsible for calendar validity, atomic candidate
spans, continuation handling, and technical shielding. Any approved `за` policy
belongs in the Russian language layer; common normalization must not learn
Russian prepositions or grammatical cases.

The work is intentionally split into separate steps:

1. approve one bounded meaning and its fallback contract;
2. add behavior regressions without changing release archives;
3. if expected outputs change, update the working oracle in a separate,
   justified commit;
4. verify exact-head CI and post-merge CI before the next policy track.

Unless a future product decision explicitly approves a bounded contextual
interpretation, `за 01.01.2024` and equivalent valid surfaces retain the
current surface-local normalization with no warning.
