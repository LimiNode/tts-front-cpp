# Russian date spelling and context policy review

Review-only proposal for the next `v0.3.0` behavior PR. This document is
based on merge commit `5f14e344209a9f5dfd5364e61aa88ff310bf261b`
(PR #26). It intentionally changes no production code, tests, quality gold,
working snapshot, release archive, CMake metadata, or public API.

## Current baseline and scope

The working corpus has 655 cases:

```text
correct_transformation 561
justified_preservation  88
unnecessary_refusal      0
incorrect_or_partial     6
```

Four mismatches belong to Russian dates. The remaining two currency
mismatches are outside this review.

| ID | Current year fragment | Current oracle fragment |
|---|---|---|
| `ru-v030-date-0-0` | `две тысячи двадцать четвертого года` | `две тысячи двадцать четвёртого года` |
| `ru-v030-date-0-1` | `две тысячи двадцать четвертого года` | `две тысячи двадцать четвёртого года` |
| `ru-v030-date-1-0` | `две тысячи двадцать четвертого года` | `две тысячи двадцать четвёртого года` |
| `ru-v030-date-1-1` | `две тысячи двадцать четвертого года` | `две тысячи двадцать четвёртого года` |

The current mismatch is therefore lexical spelling only: both
`normalized_text` and `pronunciation_text` use `четвертого`, while the reviewed
gold uses `четвёртого`. Calendar validation, day wording, month wording,
diagnostics, and source spans already agree with the current oracle.

## Decision 1: canonical `ё`

The recommended contract is to preserve `ё` in generated Russian words whose
lexical form contains it. For this date group:

```text
01.01.2024
→ первое января две тысячи двадцать четвёртого года

29.02.2024
→ двадцать девятое февраля две тысячи двадцать четвёртого года
```

The same spelling should be used in `normalized_text` and
`pronunciation_text`. This is consistent with existing generated forms such as
`трёх`, `четвёртом`, and `четвёртый`, and avoids asking a downstream TTS
backend to recover a known vowel distinction.

The alternatives are:

1. use `е` in both outputs and change the oracle;
2. use `е` in `normalized_text` but `ё` in `pronunciation_text`.

The first alternative would redefine an already reviewed gold spelling merely
to match the current formatter. The second introduces an output divergence for
a word whose pronunciation is already known deterministically. Neither is
recommended for this behavior PR.

## Decision 2: surrounding grammatical context

The earlier triage raised a separate question:

```text
Встреча назначена на 01.01.2024.
→ Встреча назначена на первое января ...

Подтвердите дату 01.01.2024.
→ Подтвердите дату первое января ...       (surface-local reading)
→ Подтвердите дату первого января ...      (contextual genitive reading)
```

The current working oracle uses the surface-local reading for both variants.
Changing `первое` to `первого` after `дату` requires lexical context ownership
outside the numeric date surface. It is not needed to close the four current
mismatches and is a materially different behavioral hypothesis.

The recommended scope for the next behavior PR is therefore:

- keep the date formatter surface-local;
- keep the existing neuter ordinal day form for accepted numeric dates;
- fix only the canonical `ё` spelling in the genitive year formatter;
- defer context-sensitive `дату первого января` wording to a separate
  review and behavior PR.

If contextual agreement is selected later, it must define at least `на`,
`дату`, `с`, `до`, `после`, quoted dates, sentence-initial dates, and contexts
without a reliable lexical owner. Ambiguous contexts should remain unchanged
with one candidate diagnostic instead of receiving a guessed case.

## Admission and calendar invariants

This spelling correction must not alter date admission. The existing contract
remains:

- validate the real Gregorian calendar date, including leap years;
- format an admitted date atomically;
- preserve an invalid date with one `UnresolvedNumber` over the complete date;
- preserve an unsupported attached continuation atomically;
- leave terminal punctuation outside the candidate span;
- do not reinterpret a machine-readable or ranged numeric surface as a date.

The implementation should reuse the existing Russian ordinal formatter. It
must not add a date-specific replacement pass or a regex exception for 2024.

## Required behavior matrix

Before changing the working oracle, the behavior PR should verify:

- positive lexical forms: years ending in 4, including `2004`, `2024`, and
  `2044`;
- unchanged year neighbors: `1999`, `2000`, and `2025`;
- valid dates: `01.01.2024` and leap day `29.02.2024`;
- invalid dates: `29.02.2025`, `31.04.2024`, and out-of-range components;
- attached continuations: ASCII and Cyrillic words, `#`, `/`, `%`, signs, and
  a combining mark;
- sentence-final punctuation and an ordinary independent following number;
- `Language::Russian`, `Language::Auto`, and `SegmentCandidates` where the
  date path is reachable;
- exact `normalized_text`, `pronunciation_text`, diagnostic count, and UTF-8
  byte spans.

Each changed row needs an unchanged positive or adversarial neighbor. Because
this proposal does not change admission, any diagnostic or span drift is a
regression rather than part of the new contract.

## Oracle and release boundary

The review leaves the current `561/88/0/6` snapshot unchanged. A future
behavior PR should first update the shared Russian genitive ordinal spelling
and add direct regressions. A separate oracle commit may then update only the
four approved `ru-v030-date-0/1-*` rows and record the resulting baseline.

Published `v0.2.0`, `v0.2.1`, `v0.2.2`, and `v0.2.3` archives remain immutable.
The two `ru-v030-money-7-*` mismatches remain for a separate currency-policy
review.
