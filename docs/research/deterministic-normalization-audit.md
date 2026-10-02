# Deterministic normalization audit

This note records the first corpus-driven slice after the v0.1/source-mapping
baseline. It deliberately separates implemented behavior from candidate grammar
work; unsupported forms must remain unchanged or produce an explicit warning.

## Current coverage

The executable corpus covers RU/EN cardinals, grouped and negative numbers,
percentages, decimals, times, currencies, measurements, RU dates/years,
abbreviations, whitespace cleanup, protected technical spans, and invalid-span
regressions. Provenance-reviewed vectors are pinned in `source-survey.md`.

## Gap inventory

| Category | Current behavior | Decision |
|---|---|---|
| English ordinal suffixes (`1st`, `22nd`) | digits are otherwise handled as cardinals; suffix forms were not normalized | implement deterministic ordinal grammar with suffix validation |
| English calendar dates | no rule; locale interpretation is ambiguous | keep unchanged until an explicit locale contract exists |
| English years | cardinal fallback only | keep unchanged as a semantic policy decision, not a silent date guess |
| malformed ordinal suffix (`11st`) | must not fall through to cardinal normalization | preserve and emit `UnresolvedNumber` |
| currency/measurement technical spans | fixed protected-span pipeline | add cases only when corpus audit identifies a concrete gap |
| malformed dates, decimals, grouped numbers | preserve and diagnose when a recognizer claims the form | extend vectors around each new rule |

## First implementation slice

The initial broader-normalization slice adds English ordinal expansion for values
accepted by the existing bounded integer parser. It validates the grammatical
suffix (`11th`, not `11st`) before replacing, and maps generated output through
the existing private provenance runs. Invalid ordinal spans are preserved for
later passes.

This slice does not add locale-dependent date parsing, semantic year guessing,
new languages, stress backends, or benchmark-driven caches.

## Acceptance vectors

- `1st 2nd 3rd 4th 11th 12th 13th 21st 42nd 100th 101st`
  → `first second third fourth eleventh twelfth thirteenth twenty first forty second one hundredth one hundred first`;
- `11st 2nd` keeps `11st` unchanged, normalizes `2nd`, and emits one warning;
- ordinal output remains compatible with technical protection and source mapping.
