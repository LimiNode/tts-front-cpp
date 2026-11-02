# RU/EN natural-sentence corpus expansion review

Review-only brief for the first product-quality stage after the `v0.2.4`
stability line. This document changes no production code, tests, working
oracle, snapshots, release archives, public API, or CMake metadata.

## Baseline and scope

The current working corpus contains 655 cases and reports:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial      0
```

Its existing category inventory is:

| Category | Cases |
|---|---:|
| integer | 130 |
| measurement | 80 |
| percent | 71 |
| date | 58 |
| currency | 57 |
| mixed_language | 54 |
| time | 52 |
| punctuation | 43 |
| ambiguous_or_technical | 38 |
| decimal | 36 |
| year | 32 |
| grammar | 4 |

The next batch must add sentence-sized evidence around the existing semantics;
it must not manufacture a new behavior target to justify implementation work.
Isolated numeric tokens do not count unless they are the negative control for a
sentence case.

## Proposed inventory

The first batch is capped at 96 new cases: 12 cases in each of eight clusters.
This is large enough to exercise all four modes without turning one review into
a new generated matrix. Each cluster contains a positive sentence, an ordinary
boundary neighbor, a malformed or unsupported neighbor, and a technical or
ownership control as applicable.

| Cluster | Required sentence families |
|---|---|
| Dates | calendar date in ordinary prose; sentence-initial and quoted date; invalid calendar date; attached continuation and independent following number |
| Years | bare year in prose; supported year governor; independent range endpoint; signed, leading-zero, attached-word, and technical-version controls |
| Units | integer and decimal measurement; singular/plural boundary; attached and spaced unit; unknown unit and connector continuation |
| Currency | prefix and suffix currency; integer and decimal amount; grouped amount; malformed separator, sign, and technical-price control |
| Percentages | integer and decimal percent; negative value; sentence-final punctuation; attached/spaced continuation and technical `%` token |
| Mixed language | Russian sentence with English candidate; English sentence with Russian candidate; two independent local owners; unsupported foreign unit and ambiguous ownership |
| Punctuation | parentheses, quotes, comma, colon, dash, newline, sentence terminator, and an ordinary next number that must remain independent |
| Technical neighbors | URL, email, version, IPv4-like surface, hardware identifier, `C++`/`C#`/`HTTP/2`, plus an ordinary candidate immediately before or after the protected span |

The inventory must use complete, natural sentences. Representative source
shapes include, but do not prescribe gold output:

```text
The deployment is scheduled for January 2, 2027.
Backup completed in 2026; retry 7 starts tomorrow.
The package weighs 2.5 kg, not 25 kg.
The invoice total is $1,234.50 after tax.
CPU usage reached 42% during the build.
RTX-4090 processed 2 GB before HTTP/2 closed.

Релиз назначен на 02.01.2027.
Отчёт за 2026 год готов; следующая проверка — 7 ноября.
Посылка весит 2,5 кг, а коробка — 3 кг.
Итоговая сумма — 1 234,50 рубля с учётом налога.
Загрузка достигла 42% во время сборки.
RTX-4090 обработал 2 ГБ до закрытия HTTP/2.
```

These examples are inventory prompts, not expected-output declarations. The
future corpus PR must review every exact source and gold row independently.

## Mode allocation

Each 12-case cluster uses the following allocation unless the mixed-language
cluster needs the ownership-heavy variant shown below:

| Mode | Ordinary cluster | Mixed-language cluster |
|---|---:|---:|
| Explicit Russian | 4 | 2 |
| Explicit English | 4 | 2 |
| Auto | 2 | 4 |
| Auto + `SegmentCandidates` | 2 | 4 |

The current quality TSV supports only `explicit` and `auto_segment`. The
characterization PR must not treat `auto_segment` as proof of default `Auto`
behavior. It must either add a narrowly named `auto` corpus mode to the audit
schema or keep the plain-Auto rows in direct characterization tests. This is a
test-harness choice only; it must not change frontend behavior or public API.

English and Russian explicit rows establish language policy. Plain Auto rows
establish whole-sentence ownership. SegmentCandidates rows establish local
candidate ownership. A case is not counted twice merely because the same input
is convenient in two modes: each row must test a mode-specific contract.

## Required neighbors and diagnostics

Every positive family must have the closest relevant negative and continuation
neighbors:

- unsigned, ASCII `+`/`-`, and Unicode minus where signs are meaningful;
- plain and grouped or decimal forms;
- ordinary whitespace, newline, NBSP, punctuation, and end-of-string;
- attached and spaced `#`, `/`, `%`, sign, ASCII/Cyrillic word, and combining
  mark continuations where the shared scanner owns the boundary;
- an independent following number and an independent lexical word;
- URL, email, version, hardware identifier, and technical-marker controls.

Each row records exact `original_text`, `normalized_text`,
`pronunciation_text`, and the complete ordered diagnostic signature
`(code, byte_offset, byte_length)`. The audit must continue rejecting invalid
UTF-8 spans, duplicate diagnostics, overlapping distinct `UnresolvedNumber`
spans, marker leakage, and changes to `original_text`.

The matrix is not a demand to duplicate every scanner Cartesian product in the
quality TSV. Shared boundary behavior already covered by direct tests should be
referenced; the sentence corpus adds only the representative natural-language
row needed to prove that cleanup and surrounding prose do not change it.

## Gold and provenance

Gold must be authored and reviewed independently; it must never be copied from
a frontend run. Current output may be captured as an observation, but a mismatch
against reviewed gold remains visible and classified.

For locally authored product cases, the corpus PR records the policy document or
issue, authoring date, and `project_derived` status in its inventory receipt. If
a case is adapted from an upstream project, record the repository, exact commit,
original vector or category, license, adaptation date, and whether it is an
`adapted_vector` or `category_derived` case. Do not call a locally invented
sentence an upstream-adapted vector merely because it uses the same category.

Existing release archives are immutable. New rows update only the working gold,
generated TSV, and current snapshot in a corpus-only commit. Any change to an
existing expected output requires a separate semantic decision with explicit
old/new text and a direct regression test.

## PR sequencing

1. Add the reviewed case inventory and direct characterization for all four
   modes without production changes.
2. Add the approved new gold rows, generated TSV, and current snapshot as an
   explicitly labelled working-oracle expansion; preserve every existing row.
3. Classify each newly exposed mismatch as policy gap, implementation defect,
   or justified preservation.
4. Open a focused behavior PR only for an approved cluster. Do not combine it
   with an existing-oracle rewrite or release archive change.

If the new cases all match current behavior, the corpus expansion is complete
without a behavior PR.

## Exit criteria

The expansion is complete when:

- all 96 reviewed sentence rows are present with stable unique IDs and no
  changes to the existing 655 rows;
- all eight clusters contain positive, negative, continuation, punctuation,
  ownership, and technical controls appropriate to that cluster;
- explicit Russian, explicit English, plain Auto, and SegmentCandidates are
  covered as distinct modes;
- gold provenance and the exact ordered diagnostic signatures are reviewable;
- generator check, corpus audit, strict archive audit, checksum checks, Debug
  and Release CTest, format check, header self-containment, install-consumer,
  and scaling gates pass;
- the PR reports before/after totals and every newly exposed mismatch without
  changing production semantics to make the snapshot green;
- exact-head push and PR CI are green, and published release archives remain
  byte-identical.

No scanner cleanup, new language, contextual `за` behavior, or speculative
normalization abstraction belongs to this stage.
