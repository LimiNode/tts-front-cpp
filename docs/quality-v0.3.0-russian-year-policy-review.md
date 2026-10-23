# Russian year and agreement policy review

Review-only proposal for the next `v0.3.0` behavior PR. This document is
based on merge commit `dca9bb7729a6729afd995c9dfa48cd00762d2c56` (PR #24).
It intentionally changes no production code, tests, gold data, working
snapshot, release archive, CMake metadata, or public API.

## Current baseline and scope

The working corpus has 655 cases:

```text
correct_transformation 545
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial     22
```

The Russian-year group contains 16 mismatch IDs:

```text
ru-v030-year-0-a/b  1999
ru-v030-year-1-a/b  2000
ru-v030-year-2-a/b  2001
ru-v030-year-3-a/b  2012
ru-v030-year-4-a/b  2020
ru-v030-year-5-a/b  2024
ru-v030-year-6-a/b  2026
ru-v030-year-7-a/b  2042
```

The `-a` rows use `в N году` and the `-b` rows use `за N год`. Four Russian
date mismatches are tracked separately; their year component uses a genitive
date form and must not be silently changed by the year behavior PR.

## Observed behavior and proposed contract

The current normalizer emits cardinal fragments in year contexts. For example:

```text
Архив создан в 1999 году.
→ Архив создан в одна тысяча девятьсот девяносто девять году.

Отчёт за 1999 год доступен.
→ Отчёт за одна тысяча девятьсот девяносто девять год доступен.
```

The proposed contract is context-sensitive ordinal agreement. The year
number and the noun must be formatted as one grammatical phrase:

```text
в 1999 году  → в тысяча девятьсот девяносто девятом году
за 1999 год  → за тысяча девятьсот девяносто девятый год

в 2000 году  → в двухтысячном году
за 2000 год  → за двухтысячный год

в 2001 году  → в две тысячи первом году
за 2001 год  → за две тысячи первый год

в 2024 году  → в две тысячи двадцать четвёртом году
за 2024 год  → за две тысячи двадцать четвёртый год
```

The recommendation is to keep `ё` in the canonical Russian output for forms
such as `четвёртом` and `четвёртый`. If product policy instead chooses a
ё-less spelling, that is an oracle decision and must be recorded explicitly;
the behavior PR must not change gold merely to match whichever spelling the
existing formatter happens to produce.

## Policy choices requiring approval

### Year contexts

Recommended policy:

- `в <year> году` is a prepositional year context and uses a masculine
  prepositional ordinal;
- `за <year> год` is an accusative year context and uses a masculine
  accusative ordinal;
- the noun (`году`/`год`) is part of the context contract, not an independent
  suffix guessed from the numeric value;
- dates (`01.01.2024`) remain under the separate date policy and use genitive
  year wording.

Alternatives are to keep cardinal years in explicit Russian contexts or to
support only the `в ... году` form and fail closed for `за ... год`. Either
alternative must preserve source bytes and emit one candidate diagnostic for
unsupported contexts.

### Numeric range

The behavior PR should define and test at least:

| Range | Examples | Decision needed |
|---|---|---|
| 1000–1899 | `в 1899 году`, `за 1899 год` | ordinal decomposition and spelling |
| 1900–1999 | `в 1900/1999 году` | `тысяча ... девятом` form |
| 2000–2099 | `в 2000/2001/2024 году` | compound ordinal agreement |
| 2100+ | `в 2100 году`, `за 2101 год` | supported or fail closed |
| bare number | `1999` | remain cardinal without year context |
| ranges | `с 1999 по 2001 год` | separate range policy or atomic preservation |

Until a range policy is approved, `с ... по ...` and `from/to`-like Russian
constructions should be preserved atomically rather than formatting only one
endpoint.

## Required behavior matrix

Before changing the oracle, the behavior PR must test each approved value in:

- `в N году` and `за N год`;
- bare `N`, date-adjacent `N года`, and sentence-initial placement;
- `с N по M` (positive if a range policy is approved, otherwise one warning);
- attached continuations (`1999год`, `1999#4`, `1999/4`) and ordinary next
  numbers (`1999 2000`);
- `Language::Russian`, `Language::Auto`, and `SegmentCandidates` where the
  shared admission path is involved;
- exact normalized text, pronunciation text, warning code/count, and UTF-8
  byte spans.

Every new positive case needs a negative/adversarial neighbor. In particular,
an unsupported agreement context must not rewrite only the numeral while
leaving `год`/`году` unchanged.

## Oracle and release boundary

This review does not alter the current `545/88/0/22` snapshot. A future
behavior PR must contain production behavior and direct regressions first. A
separate oracle commit may then update only the approved Russian-year rows,
with old/new outputs and the chosen spelling documented. Release archives
`v0.2.0`, `v0.2.1`, `v0.2.2`, and `v0.2.3` remain immutable.
