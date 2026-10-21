# English year policy review

Status: review-only. This document records a product decision needed before
an English-only behavior change. It is based on release commit `5238391`
(`v0.2.3`) and intentionally changes no production code, tests, corpus gold,
snapshots, quality archives, CMake metadata, or public API.

## Why a decision is needed

The current formatter has two different year paths. A bare numeric token and a
year after `in` or `from` use the cardinal formatter. A month-name date uses a
special year formatter for values from 1000 through 1999. Consequently, the
same year can receive different spoken forms depending on its surface context.
The current `normalized_text` and `pronunciation_text` are identical for all
examples below and no diagnostic is emitted.

| Input | Current output |
|---|---|
| `1900` | `one thousand nine hundred` |
| `1999` | `one thousand nine hundred ninety nine` |
| `2000` | `two thousand` |
| `2001` | `two thousand one` |
| `2024` | `two thousand twenty four` |
| `in 1900` | `in one thousand nine hundred` |
| `in 1999` | `in one thousand nine hundred ninety nine` |
| `in 2000` | `in two thousand` |
| `in 2001` | `in two thousand one` |
| `in 2024` | `in two thousand twenty four` |
| `from 1900` | `from one thousand nine hundred` |
| `from 1999` | `from one thousand nine hundred ninety nine` |
| `from 2000` | `from two thousand` |
| `from 2001` | `from two thousand one` |
| `from 2024` | `from two thousand twenty four` |
| `January 1, 1900` | `January first, nineteen hundred` |
| `January 1, 1999` | `January first, nineteen ninety nine` |
| `January 1, 2000` | `January first, two thousand` |
| `January 1, 2001` | `January first, two thousand one` |
| `January 1, 2024` | `January first, two thousand twenty four` |

The date rows explain why this cannot be resolved by changing a single gold
line: the implementation already has a context-sensitive convention, but the
convention is undocumented and incomplete for non-date year surfaces.

## Policy options

### A. Cardinal policy everywhere

Use the general cardinal formatter for every four-digit year, including dates:

| Year | Spoken form |
|---:|---|
| 1900 | `one thousand nine hundred` |
| 1999 | `one thousand nine hundred ninety nine` |
| 2000 | `two thousand` |
| 2001 | `two thousand one` |
| 2024 | `two thousand twenty four` |

This is mechanically simple and makes bare, prepositional, and date-adjacent
surfaces consistent. It does not match the established conversational form
used by current 1900s month-name dates.

### B. Spoken-year policy for year contexts (recommended for review)

Treat an unqualified four-digit year as a year when it appears in a date-like
surface or an explicitly year-bearing context such as `in`/`from`. Use paired
decades for 1000–1999 and the conventional `two thousand ...` form for
2000–2099:

| Year | Spoken form |
|---:|---|
| 1900 | `nineteen hundred` |
| 1999 | `nineteen ninety nine` |
| 2000 | `two thousand` |
| 2001 | `two thousand one` |
| 2024 | `two thousand twenty four` |

The same rule would apply to `in 1999`, `from 1999`, and `January 1, 1999`.
Numbers outside an approved year context remain cardinal numbers. This option
matches the existing date behavior while making the `in`/`from` decision
explicit.

### C. Explicit year syntax only

Apply spoken-year formatting only when a date parser or an explicit year
pattern has identified the token. Keep bare `1999`, `in 1999`, and `from 1999`
cardinal unless the surrounding grammar is extended first. This minimizes
semantic guessing, but leaves common prose contexts less natural and requires
the behavior PR to define what counts as an explicit year surface.

## Decisions required before implementation

1. Select A, B, or C. The current implementation is not itself a policy.
2. Decide whether `in` and `from` are sufficient year-context markers, or
   whether additional phrases (for example `between 1999 and 2001`) are in
   scope.
3. Define the supported range. This review covers 1900, 1999, 2000, 2001, and
   2024; the behavior PR must state how 1000–1899, 2100+, and non-four-digit
   values behave.
4. Keep `normalized_text` and `pronunciation_text` aligned unless a separate
   pronunciation policy is approved.
5. Preserve fail-closed behavior for attached continuations and technical
   tokens. A year policy must not reopen URL, email, version, or numeric
   candidates that the shared scanner protects.

## Required behavior-PR matrix

After the policy is chosen, the English-only PR should add direct tests for
each of 1900, 1999, 2000, 2001, and 2024 in these contexts:

- bare token;
- `in` and `from` phrases;
- month-name date (`January 1, YEAR`);
- date-adjacent punctuation and sentence-final punctuation;
- attached `#`, `/`, `%`, and ordinary lexical continuations, which must stay
  atomic and fail closed;
- protected URL, email, version, and technical neighbors, which must remain
  unchanged.

The behavior PR must update gold/snapshots only after this matrix passes and
after the product decision is recorded separately from the implementation.
