# Russian agreement and date-context policy review

Review-only proposal for the next Russian grammar behavior track. This document
is based on merge commit `edeeb5340d1cb9b1d7f5514aabb583e63cbdfb95` (PR #55).
It changes no production code, tests, working-corpus gold, snapshots, public
API, CMake metadata, or published release archives.

## Current baseline and deferred scope

The working corpus remains at 655 cases:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial     0
```

Canonical Russian date spelling (`ё`) is already implemented. The remaining
question is contextual agreement: a numeric date formatter currently chooses
the day form from the date surface itself, without interpreting the surrounding
governor.

For example, the current stable output is:

```text
Встреча назначена на 01.01.2024.
→ Встреча назначена на первое января две тысячи двадцать четвёртого года.

Подтвердите дату 01.01.2024.
→ Подтвердите дату первое января две тысячи двадцать четвёртого года.
```

The second sentence may require genitive agreement (`первого января`), but
changing it is a product decision, not a scanner correction. This review keeps
the existing surface-local output until the context contract is approved.

## Decisions required

### Contexts with a reliable governor

The policy must explicitly choose which contexts can request a grammatical case:

| Context | Surface-local candidate | Contextual candidate |
|---|---|---|
| `на 01.01.2024` | `на первое января` | `на первое января` |
| `дату 01.01.2024` | `дату первое января` | `дату первого января` |
| `после 01.01.2024` | `после первое января` | `после первого января` |
| `из 01.01.2024` | `из первое января` | `из первого января` |

The contextual option must define the supported preposition/governor set,
gender and case for day and month, and behavior when the governor is separated
by punctuation, a quote, a newline, or a technical token. It must not infer a
case from a distant word or from a numeric value alone.

### Fail-closed boundary

When no reliable governor is available, preserve the complete admitted date and
emit the existing candidate diagnostic rather than guessing. In particular,
these surfaces must remain separate from contextual agreement:

```text
01.01.2024
01.01.2024, 7
01.01.2024#4
01.01.2024́word
https://example.com/01.01.2024
```

Technical ranges, URLs, and email-like text remain protected before language
context discovery. An invalid date is never repaired by a grammar pass.

## Required behavior matrix

Before implementation, run every applicable row through Russian, Auto, and
SegmentCandidates, with English controls where the shared admission path is
affected. Record exact normalized/pronunciation text and the complete ordered
diagnostic set `(code, offset, length)`.

| Dimension | Required values |
|---|---|
| governor | `на`, `дату`, `после`, `до`, `из`, `за`, absent |
| date | `01.01.2024`, `29.02.2024`, `31.04.2024`, `01.01.2000` |
| boundary | sentence end, `.`, `!`, `,`, quote, newline, independent number |
| continuation | attached ASCII/Cyrillic word, `#`, `/`, `%`, sign, combining mark |
| ownership | Russian sentence, Latin neighbor, mixed sentence, technical token |
| mode | RU, Auto, SegmentCandidates, EN control |

Positive and negative neighbors must be paired. The matrix must check that a
contextual rewrite never partially changes the date, never changes terminal
punctuation ownership, and never creates duplicate or overlapping diagnostics.
UTF-8 offsets and lengths are measured in original source bytes.

## Architecture and oracle boundary

The date admission scanner continues to own calendar validity, atomic source
spans, continuation handling, and technical shielding. Any future context
policy belongs in the Russian language layer; common normalization must not
learn Russian prepositions or grammatical cases.

This review deliberately separates the work:

1. approve the context and fail-closed contract;
2. add a focused behavior PR with direct regressions and adversarial neighbors;
3. if approved outputs change, update only the working oracle in a separate
   justified commit;
4. keep all `v0.2.x` release archives immutable.

No new oracle value is proposed here. Until the product decision is made,
`дату первое января` and similar surface-local forms remain the contract.
