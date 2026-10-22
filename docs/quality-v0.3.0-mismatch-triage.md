# v0.3.0 quality-cycle triage

Review-only triage of the working quality corpus after merge commit
`a3c321a92ebb5cd4fa49d291d04888cfa55947b7` (the merge of PR #22).
This document intentionally changes no production code, public API, CMake
metadata, TSV gold, snapshots, or release archives.

## Baseline and scope

The current corpus contains 655 cases with the following audit result:

```text
correct_transformation 535
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial     32
```

The 32 mismatches are exactly:

- 10 mixed-language cases;
- 16 Russian year cases;
- 4 Russian date cases;
- 2 Russian currency cases.

There are no remaining English year mismatches. The English spoken-year
policy from PR #22 resolved the two earlier `en-v030-year-*` rows. The
published `v0.2.0`, `v0.2.1`, and `v0.2.2` archives remain immutable.

The proposed outputs below are review targets, not new oracle values. A future
behavior PR must first approve the relevant product policy, add positive and
negative neighbors, and only then update the working snapshot.

## Priority 1: mixed-language ownership (10 cases)

The current implementation follows the dominant or surrounding segment in
some rows, while the snapshot expects the language of the local lexical head.
The recommended policy is local ownership: a number is formatted in the
language of the noun or clause it belongs to; a technical identifier, URL, or
email never changes that decision.

| ID | Current output | Proposed policy output |
|---|---|---|
| `en-v030-mixed-0-0` | `OpenAI API processed two запроса.` | `OpenAI API processed два запроса.` |
| `en-v030-mixed-2-0` | `RTX-4090 completed four задачи.` | `RTX-4090 completed четыре задачи.` |
| `en-v030-mixed-3-0` | `HTTP/2 returned five ошибок.` | `HTTP/2 returned пять ошибок.` |
| `en-v030-mixed-6-1` | `Build семь passed в CI. Проверка завершена.` | `Build seven passed в CI. Проверка завершена.` |
| `en-v030-mixed-7-0` | `Version V2.1.0 includes three исправления.` | `Version V2.1.0 includes три исправления.` |
| `ru-v030-mixed-2-0` | `RTX-4090 завершил четыре tasks.` | `RTX-4090 завершил four tasks.` |
| `ru-v030-mixed-3-0` | `HTTP/2 вернул пять errors.` | `HTTP/2 вернул five errors.` |
| `ru-v030-mixed-4-0` | `Отправьте два reports на dev@example.com.` | `Отправьте two reports на dev@example.com.` |
| `ru-v030-mixed-6-1` | `Сборка seven прошла в CI. Validation complete.` | `Сборка семь прошла в CI. Validation complete.` |
| `ru-v030-mixed-7-0` | `Версия V2.1.0 содержит три fixes.` | `Версия V2.1.0 содержит three fixes.` |

The behavior PR must also define the fallback when no local head exists. The
safe fallback is unchanged source text plus the existing
`AmbiguousNormalization` diagnostic, rather than guessing from one foreign
word. Coverage should include EN, RU, Auto, and `SegmentCandidates`, with
technical identifiers and protected URL/email neighbors.

## Priority 2: English years and dates

No English year mismatch remains in the post-merge baseline. The next English
quality pass should therefore be regression-only around the accepted policy:

- bare years remain cardinal;
- `in`/`from` and month-name dates use the approved spoken-year policy;
- `from ... to ...` remains fail-closed until a range policy is approved;
- attached continuations remain atomic and preserve source bytes.

New English date behavior should not be mixed into the Russian date or mixed
language oracle change. It needs its own review and neighbor matrix.

## Priority 3: Russian years and grammatical agreement (16 cases)

The eight year pairs (`-a` and `-b`) expose two independent decisions:

1. `в ... году` is a prepositional year context and needs an ordinal ending;
2. temporal `за ... год` is an accusative context and needs a different
   ordinal ending, with the noun agreeing with it.

The current outputs and policy targets are:

| IDs | Current output (year fragment) | Proposed policy fragment |
|---|---|---|
| `ru-v030-year-0-a/b` | `одна тысяча девятьсот девяносто девять году/год` | `тысяча девятьсот девяносто девятом году` / `тысяча девятьсот девяносто девятый год` |
| `ru-v030-year-1-a/b` | `две тысячи году/год` | `двухтысячном году` / `двухтысячный год` |
| `ru-v030-year-2-a/b` | `две тысячи один году/год` | `две тысячи первом году` / `две тысячи первый год` |
| `ru-v030-year-3-a/b` | `две тысячи двенадцать году/год` | `две тысячи двенадцатом году` / `две тысячи двенадцатый год` |
| `ru-v030-year-4-a/b` | `две тысячи двадцать году/год` | `две тысячи двадцатом году` / `две тысячи двадцатый год` |
| `ru-v030-year-5-a/b` | `две тысячи двадцать четыре году/год` | `две тысячи двадцать четвёртом году` / `две тысячи двадцать четвёртый год` |
| `ru-v030-year-6-a/b` | `две тысячи двадцать шесть году/год` | `две тысячи двадцать шестом году` / `две тысячи двадцать шестой год` |
| `ru-v030-year-7-a/b` | `две тысячи сорок два году/год` | `две тысячи сорок втором году` / `две тысячи сорок второй год` |

These targets require an explicit product decision about year-style Russian
numerals, ordinal inflection, and the canonical `ё` policy. The `-b` gold
rows currently contain prepositional-like endings after `за`; they must be
reviewed rather than copied mechanically into a behavior PR.

The regression matrix should include `в`, `за`, `после`, `из`, values 1, 2, 5,
21, 22, 42, 100, masculine/feminine/neuter heads, and unsupported contexts
that must be preserved with a diagnostic.

## Priority 4: Russian dates (4 cases)

| ID | Current output | Proposed policy target |
|---|---|---|
| `ru-v030-date-0-0` | `... на первое января две тысячи двадцать четвертого года.` | `... на первое января две тысячи двадцать четвёртого года.` |
| `ru-v030-date-0-1` | `... дату первое января две тысячи двадцать четвертого года.` | `... дату первого января две тысячи двадцать четвёртого года.` |
| `ru-v030-date-1-0` | `... на двадцать девятое февраля две тысячи двадцать четвертого года.` | `... на двадцать девятое февраля две тысячи двадцать четвёртого года.` |
| `ru-v030-date-1-1` | `... дату двадцать девятое февраля две тысячи двадцать четвертого года.` | `... дату двадцать девятого февраля две тысячи двадцать четвёртого года.` |

This group combines orthography (`ё` versus `е`) with case agreement after
`на` and `дату`. The behavior PR must separate those decisions, validate real
calendar dates, and add invalid-date and attached-continuation neighbors.

## Priority 5: Russian currency (2 cases)

| ID | Current output | Proposed policy target |
|---|---|---|
| `ru-v030-money-7-a` | `Итоговая цена — две целых пятьдесят сотых рубля.` | `Итоговая цена — два рубля пятьдесят копеек.` |
| `ru-v030-money-7-b` | `Бухгалтерия учла две целых пятьдесят сотых рубля сегодня.` | `Бухгалтерия учла два рубля пятьдесят копеек сегодня.` |

Currency wording needs a dedicated policy for major/minor units, rounding,
trailing zeros, `руб.`, `₽`, full currency names, and mixed-language units.
The proposed wording is a review target, not an oracle change.

## Proposed behavior-PR sequence

1. Mixed-language local-head ownership and ambiguity fallback.
2. Russian year/date inflection, after deciding `ё`/`е` and case contracts.
3. Russian genitive agreement and decimal currency, with shared unit policy.
4. English date/year neighbor audit only if new corpus evidence requires it.

Each PR should contain one behavioral hypothesis, direct regressions, adversarial
neighbors, and a before/after quality report. Release archives must not change.
The `v0.3.0` working snapshot may change only after the corresponding oracle
decision is documented separately from the implementation.
