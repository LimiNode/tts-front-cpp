# Russian year agreement behavior

This note records the reviewed Russian year policy implemented after
`docs/quality-v0.3.0-russian-year-policy-review.md`. It applies to the working
`v0.3.0` corpus. Published `v0.2.x` archive files remain immutable.

## Contract

- `в N году` uses a masculine locative ordinal: `в 1999 году` becomes
  `в тысяча девятьсот девяносто девятом году`.
- `за N год` uses a masculine accusative ordinal: `за 1999 год` becomes
  `за тысяча девятьсот девяносто девятый год`.
- Four-digit years from `1000` through `9999` are supported. Bare numbers and
  date-genitive forms remain on their existing paths.
- Canonical fourth forms retain `ё`, including `четвёртом` and `четвёртый`.
- Signed years, leading-zero years, attached year nouns, unsupported ranges,
  and typed continuations are preserved atomically with one
  `UnresolvedNumber`. The diagnostic covers the complete numeric candidate in
  UTF-8 source bytes.
- Grouped input such as `в 1 999 году` is admitted by the existing grouped
  number path and receives the same locative formatting.

The direct matrix covers the corpus years `1999`, `2000`, `2001`, `2012`,
`2020`, `2024`, `2026`, and `2042`, plus range boundaries, uppercase contexts,
bare and date-adjacent neighbors, punctuation, signs, grouped input, combining
marks, ASCII/Cyrillic continuations, and RU/Auto/SegmentCandidates modes.

## Oracle change

The previous working snapshot was `655 / 545 / 88 / 0 / 22` for correct
transformation / justified preservation / unnecessary refusal / incorrect or
partial transformation. The reviewed snapshot is now
`655 / 561 / 88 / 0 / 6`.

The 16 `ru-v030-year-0..7-{a,b}` mismatch IDs are removed. The `-a` rows now
match locative behavior; the `-b` gold outputs change from the old locative
placeholder to the approved accusative form. The pre-existing `ru-natural-10`
gold changes from:

```text
В две тысячи двадцать шесть году запланировано двенадцать релизов.
```

to:

```text
В две тысячи двадцать шестом году запланировано двенадцать релизов.
```

That row was previously classified as correct only because its gold contained
the same cardinal agreement error. Updating it is part of the same reviewed
year policy, not an additional quality claim. Six unrelated Russian date and
currency mismatches remain.

The strict archive runner accepts a changed historical result only when the
same case ID, input, language, and mode exist in the generated working corpus
and the current normalized text, pronunciation text, and diagnostics exactly
match that reviewed gold. Historical archive corpus, snapshot, and checksum
files are unchanged.
