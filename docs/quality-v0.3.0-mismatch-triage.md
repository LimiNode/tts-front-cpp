# v0.3.0 natural-sentence mismatch triage

Review-only analysis for the 36 `incorrect_or_partial` cases in the
v0.3.0 EN/RU natural-sentence baseline. This document is based on
`fd70deda52329f8bfad161761bb21bef1c28cb1c` and intentionally changes no
production code, public API, CMake/version metadata, TSV gold, snapshots, or
release archives.

The baseline has 655 cases (`531 / 88 / 0 / 36`). The table below records the
current implementation output versus a proposed semantic contract. `N/P`
means the proposed `normalized_text` and `pronunciation_text`; they are equal
for every case in this report. The ten mixed-language rows currently carry an
`ambiguous_normalization` diagnostic in the snapshot; the other 26 rows have
`diagnostics=none`. A future policy PR must decide whether that ambiguity
diagnostic remains after local-head ownership is made explicit. No mismatch in
this set is diagnostic-only.

## Classification key

- **Implementation bug** — the language policy is sufficiently determined, but
  the implementation does not produce it.
- **Erroneous gold** — the current expected output is not a defensible
  sentence-level contract (for example, wrong case or wrong currency scale).
- **Ambiguous language policy** — a product decision is needed before calling
  either output a defect; the proposed output below is the recommended policy
  for a future behavior PR.
- **Diagnostic-only** — output is acceptable and only diagnostics differ. None
  of the 36 IDs has this classification; all have `diagnostics=none` in the
  snapshot and an output mismatch.

## Per-ID triage

### Mixed-language policy

The recommended rule is local lexical ownership: a number follows the
language of its nearby lexical head/segment, not the dominant language of the
whole sentence. Technical identifiers, URLs and email addresses remain
protected. This makes the English/Russian pairs below deterministic without
switching the language of an entire sentence.

| ID | Classification | Current implementation | Proposed N/P | Assessment |
|---|---|---|---|---|
| `en-v030-mixed-0-0` | Ambiguous language policy | `OpenAI API processed two запроса.` | `OpenAI API processed два запроса.` | Russian noun `запроса` owns the number; the current segment choice is inconsistent with local agreement. |
| `en-v030-mixed-2-0` | Ambiguous language policy | `RTX-4090 completed four задачи.` | `RTX-4090 completed четыре задачи.` | The Russian head `задачи` requires a Russian numeral; `RTX-4090` remains technical text. |
| `en-v030-mixed-3-0` | Ambiguous language policy | `HTTP/2 returned five ошибок.` | `HTTP/2 returned пять ошибок.` | The Russian head `ошибок` controls the number; `HTTP/2` is a protected identifier. |
| `en-v030-mixed-6-1` | Ambiguous language policy | `Build семь passed в CI. Проверка завершена.` | `Build seven passed в CI. Проверка завершена.` | The number belongs to the English clause; the following Russian fragment must not retroactively change its language. |
| `en-v030-mixed-7-0` | Ambiguous language policy | `Version V2.1.0 includes three исправления.` | `Version V2.1.0 includes три исправления.` | Russian noun `исправления` owns the numeral; the version token is technical and untouched. |
| `ru-v030-mixed-2-0` | Ambiguous language policy | `RTX-4090 завершил четыре tasks.` | `RTX-4090 завершил four tasks.` | English head `tasks` owns the number. |
| `ru-v030-mixed-3-0` | Ambiguous language policy | `HTTP/2 вернул пять errors.` | `HTTP/2 вернул five errors.` | English head `errors` owns the number. |
| `ru-v030-mixed-4-0` | Ambiguous language policy | `Отправьте два reports на dev@example.com.` | `Отправьте two reports на dev@example.com.` | English head `reports` owns the number; `dev@example.com` is protected. |
| `ru-v030-mixed-6-1` | Ambiguous language policy | `Сборка seven прошла в CI. Validation complete.` | `Сборка семь прошла в CI. Validation complete.` | The Russian subject `Сборка` owns the bare numeral; the later English sentence is a separate segment. |
| `ru-v030-mixed-7-0` | Ambiguous language policy | `Версия V2.1.0 содержит три fixes.` | `Версия V2.1.0 содержит three fixes.` | English head `fixes` owns the number; the version remains protected. |

These ten cases are not suitable for silently changing gold until the local
head/segment rule is accepted as the mixed-language contract. The proposed
outputs are the recommended oracle for the subsequent behavior PR.

### English year policy

The proposed English policy reads a four-digit year in a date-like context as
two pairs when that is the established spoken-year form. Thus 1999 is
`nineteen ninety nine`, rather than the cardinal `one thousand nine hundred
ninety nine`. If a product decision instead selects cardinal years, both gold
rows would be policy changes rather than implementation defects.

| ID | Classification | Current implementation | Proposed N/P | Assessment |
|---|---|---|---|---|
| `en-v030-year-0-a` | Ambiguous language policy (implementation gap) | `The archive was created in one thousand nine hundred ninety nine.` | `The archive was created in nineteen ninety nine.` | The formatter applies a cardinal-number path to a year surface. Adopt the spoken-year rule before changing code. |
| `en-v030-year-0-b` | Ambiguous language policy (implementation gap) | `Records from one thousand nine hundred ninety nine remain available.` | `Records from nineteen ninety nine remain available.` | Same year policy in a prepositional context; no diagnostic is needed once the policy is explicit. |

### Russian case, dates and orthography

The case policy used below is explicit:

- `после` and `из` govern genitive (`после двух попыток`, `из пяти узлов`);
- temporal `за` governs accusative (`за две тысячи первый год`), while a
  different construction such as `из-за` would have its own syntax;
- `на` in `встреча назначена на ...` is accusative;
- `дату ...` names the date in genitive (`дату первого января`);
- generated Russian uses canonical `ё` in these stressed ordinal forms.

| ID | Classification | Current implementation | Proposed N/P | Assessment |
|---|---|---|---|---|
| `ru-v030-control-4` | Implementation bug | `После два попыток система ответила.` | `После двух попыток система ответила.` | `после` requires genitive; the implementation emits a cardinal form. |
| `ru-v030-control-5` | Implementation bug | `После двадцать два попыток система ответила.` | `После двадцати двух попыток система ответила.` | The same genitive rule applies to a compound number; this is not limited to values below 10. |
| `ru-v030-date-0-0` | Implementation bug (orthography) | `Встреча назначена на первое января две тысячи двадцать четвертого года.` | `Встреча назначена на первое января две тысячи двадцать четвёртого года.` | Accusative after `на` is correct; only the canonical `ё` spelling is lost. |
| `ru-v030-date-0-1` | Erroneous gold + implementation gap | `Подтвердите дату первое января две тысячи двадцать четвертого года.` | `Подтвердите дату первого января две тысячи двадцать четвёртого года.` | The gold uses nominative-like `первое` after `дату`; genitive `первого` is required, in addition to the `ё` decision. |
| `ru-v030-date-1-0` | Implementation bug (orthography) | `Встреча назначена на двадцать девятое февраля две тысячи двадцать четвертого года.` | `Встреча назначена на двадцать девятое февраля две тысячи двадцать четвёртого года.` | Accusative date after `на` is correct; canonical `ё` is missing. |
| `ru-v030-date-1-1` | Erroneous gold + implementation gap | `Подтвердите дату двадцать девятое февраля две тысячи двадцать четвертого года.` | `Подтвердите дату двадцать девятого февраля две тысячи двадцать четвёртого года.` | `дату` requires genitive `двадцать девятого`; the current gold is not a valid target sentence. |

The `ё/е` choice must be made once for generated Russian text. This report
recommends `ё` in normalized and pronunciation text; an ASCII-compatible
`е` policy would instead require an explicit oracle decision and a separate
gold review.

### Decimal currency scale

| ID | Classification | Current implementation | Proposed N/P | Assessment |
|---|---|---|---|---|
| `ru-v030-money-7-a` | Ambiguous language policy (gold is inadequate) | `Итоговая цена — две целых пятьдесят сотых рубля.` | `Итоговая цена — два рубля пятьдесят копеек.` | Money should be rendered in major/minor units. `пять десятых` is not an acceptable representation of the currency scale, while `пятьдесят сотых` is mathematically exact but unnatural. |
| `ru-v030-money-7-b` | Ambiguous language policy (gold is inadequate) | `Бухгалтерия учла две целых пятьдесят сотых рубля сегодня.` | `Бухгалтерия учла два рубля пятьдесят копеек сегодня.` | Same minor-unit policy in a verbal context; the surrounding syntax does not change the currency scale. |

The behavior PR should define rounding, trailing zeros, and the boundary
between decimal-number wording and currency-specific minor units before
updating either implementation or gold.

### Russian year inflection

For `в ... году`, the year is an ordinal in the prepositional phrase. For
`за ... год`, it is an ordinal in the accusative phrase. The current gold for
all `-b` rows uses prepositional endings (`-ом`) while leaving `год` in a
non-agreeing form, so those gold entries must not be copied into a behavior
PR without correction.

| ID | Classification | Current implementation | Proposed N/P | Assessment |
|---|---|---|---|---|
| `ru-v030-year-0-a` | Implementation bug | `Архив создан в одна тысяча девятьсот девяносто девять году.` | `Архив создан в тысяча девятьсот девяносто девятом году.` | Year-style reading omits cardinal `одна`; the final component is ordinal/prepositional. |
| `ru-v030-year-0-b` | Erroneous gold + implementation bug | `Отчёт за одна тысяча девятьсот девяносто девять год доступен.` | `Отчёт за тысяча девятьсот девяносто девятый год доступен.` | `за` is accusative here, so gold `девятом год` is wrong; implementation also fails to inflect the year. |
| `ru-v030-year-1-a` | Implementation bug | `Архив создан в две тысячи году.` | `Архив создан в двухтысячном году.` | The conventional round-year form is the fused ordinal `двухтысячном`. |
| `ru-v030-year-1-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи год доступен.` | `Отчёт за двухтысячный год доступен.` | Accusative requires `двухтысячный`, not `двухтысячном год`. |
| `ru-v030-year-2-a` | Implementation bug | `Архив создан в две тысячи один году.` | `Архив создан в две тысячи первом году.` | The final year component is ordinal and agrees with `году`. |
| `ru-v030-year-2-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи один год доступен.` | `Отчёт за две тысячи первый год доступен.` | Accusative ordinal is `первый`; current gold incorrectly uses the prepositional form. |
| `ru-v030-year-3-a` | Implementation bug | `Архив создан в две тысячи двенадцать году.` | `Архив создан в две тысячи двенадцатом году.` | Prepositional ordinal ending is required. |
| `ru-v030-year-3-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи двенадцать год доступен.` | `Отчёт за две тысячи двенадцатый год доступен.` | Accusative ordinal ending is required. |
| `ru-v030-year-4-a` | Implementation bug | `Архив создан в две тысячи двадцать году.` | `Архив создан в две тысячи двадцатом году.` | Prepositional ordinal ending is required. |
| `ru-v030-year-4-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи двадцать год доступен.` | `Отчёт за две тысячи двадцатый год доступен.` | Accusative ordinal ending is required. |
| `ru-v030-year-5-a` | Implementation bug | `Архив создан в две тысячи двадцать четыре году.` | `Архив создан в две тысячи двадцать четвёртом году.` | Prepositional ordinal plus the recommended `ё` spelling. |
| `ru-v030-year-5-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи двадцать четыре год доступен.` | `Отчёт за две тысячи двадцать четвёртый год доступен.` | Accusative ordinal plus `ё`; current gold uses neither. |
| `ru-v030-year-6-a` | Implementation bug | `Архив создан в две тысячи двадцать шесть году.` | `Архив создан в две тысячи двадцать шестом году.` | Prepositional ordinal ending is required. |
| `ru-v030-year-6-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи двадцать шесть год доступен.` | `Отчёт за две тысячи двадцать шестой год доступен.` | Accusative ordinal ending is required. |
| `ru-v030-year-7-a` | Implementation bug | `Архив создан в две тысячи сорок два году.` | `Архив создан в две тысячи сорок втором году.` | Prepositional ordinal ending is required. |
| `ru-v030-year-7-b` | Erroneous gold + implementation bug | `Отчёт за две тысячи сорок два год доступен.` | `Отчёт за две тысячи сорок второй год доступен.` | Accusative ordinal ending is required. |

## Responsibility boundary for mixed language

The mixed-language layer should select the language of a number from a local
lexical head or an unambiguous segment boundary. It should not:

- infer the language from a technical identifier (`RTX-4090`, `HTTP/2`,
  `V2.1.0`);
- switch the entire sentence because of one foreign word;
- translate protected URL/email/technical surfaces;
- perform Russian case agreement for an English head, or English number
  formatting for a Russian head, without an explicit segment decision.

When no local head exists (for example, a bare number between two language
segments), the fail-closed choice is to preserve the candidate and emit the
existing ambiguity diagnostic rather than guess. This boundary should be
tested independently from language-specific grammar.

## Proposed behavior PRs (maximum three)

### 1. English year/date and Russian year/date policy

Define and implement the year grammar and case-aware date formatter, including
the `ё/е` policy. Regression matrix:

- English years: 1900, 1999, 2000, 2001, 2024 in sentence-initial, `in`, and
  `from` contexts;
- Russian years 1999/2000/2001/2012/2020/2024/2026/2042 after `в` and `за`;
- numeric dates 01.01.2024 and 29.02.2024 after `на` and after `дату`;
- invalid dates and attached continuations (`2024#4`, `2024/4`) must remain
  atomic with one full diagnostic;
- both `ё` and `е` inputs, with one documented output policy.

### 2. Russian genitive agreement and decimal currency

Implement case/number agreement only where the grammar is explicit and use
fail-closed preservation for unsupported contexts. Regression matrix:

- `после`, `из`, temporal `за`, and `из-за` with 1, 2, 5, 21, 22, 42, 100;
- masculine/feminine/neuter heads and plural genitive nouns;
- `1,50`, `2,50`, `2,05`, `100,00` rubles with explicit minor-unit wording;
- rounding and trailing-zero policy, including `руб.`, `₽`, and full currency
  names;
- negative and mixed-language neighbors that must not be partially rewritten.

### 3. Mixed-language segment ownership

Make local-head ownership explicit and keep technical/URL/email protection
outside the language decision. Regression matrix:

- EN and RU hosts with foreign noun heads (`2 запроса`, `2 reports`);
- bare numbers next to a language boundary, with expected ambiguity
  diagnostics;
- technical identifiers, URLs and email addresses containing digits;
- units/currency whose symbol and noun belong to different segments;
- EN/RU/Auto plus `SegmentCandidates`, checking unchanged source bytes,
  complete warning spans, no marker leakage and no overlapping
  `UnresolvedNumber` diagnostics.

No snapshot, gold, archive, or implementation change is part of this
review-only PR.
