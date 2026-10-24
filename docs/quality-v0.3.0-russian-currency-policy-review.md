# Russian decimal currency policy review

Review-only proposal for the next `v0.3.0` behavior PR. This document is
based on merge commit `d772c051b5ced1cf1e10d3c28921178976463364`
(PR #28). It intentionally changes no production code, tests, quality gold,
working snapshot, release archive, CMake metadata, or public API.

## Current baseline and scope

The working corpus has 655 cases:

```text
correct_transformation 565
justified_preservation  88
unnecessary_refusal      0
incorrect_or_partial     2
```

The only remaining semantic mismatches are the two Russian decimal-currency
rows:

| ID | Input | Current output | Review target in triage |
|---|---|---|---|
| `ru-v030-money-7-a` | `Итоговая цена — 2,50 руб.` | `Итоговая цена — две целых пятьдесят сотых рубля.` | `Итоговая цена — два рубля пятьдесят копеек.` |
| `ru-v030-money-7-b` | `Бухгалтерия учла 2,50 руб. сегодня.` | `Бухгалтерия учла две целых пятьдесят сотых рубля сегодня.` | `Бухгалтерия учла два рубля пятьдесят копеек сегодня.` |

The mismatch is semantic formatting, not admission: the current candidate is
rewritten atomically, warnings are absent, and the source span is valid.

## Recommended product policy

For Russian values explicitly carrying a ruble currency surface, use monetary
major/minor units rather than a generic decimal reading:

```text
2,50 руб.   → два рубля пятьдесят копеек
2,05 руб.   → два рубля пять копеек
2,5 руб.    → два рубля пятьдесят копеек
2,00 руб.   → два рубля
0,50 руб.   → ноль рублей пятьдесят копеек
```

The formatter must preserve the sign and use the existing Russian plural
forms for the major unit (`рубль`, `рубля`, `рублей`) and minor unit
(`копейка`, `копейки`, `копеек`). A missing fractional part remains an integer
amount (`2 руб.` → `два рубля`).

The recommendation treats one fractional digit as tenths of a ruble and pads
it to kopecks (`2,5` is fifty kopecks). Two fractional digits are kopecks.
Fractions with more than two digits require an explicit rounding policy; until
that policy is approved, preserve the original candidate with one
`UnresolvedNumber` rather than silently truncating or rounding.

## Policy decisions still required

### Zero and sign

Define exact wording for `0`, negative values, and negative zero:

```text
0 руб.       → ноль рублей
-2,50 руб.   → минус два рубля пятьдесят копеек
-0,50 руб.   → минус ноль рублей пятьдесят копеек
-0,00 руб.   → минус ноль рублей   (or an explicitly approved zero policy)
```

The behavior PR must not infer whether `-0,00` loses its sign. It should adopt
one documented choice and test it.

### Major-unit agreement

The major noun follows the integer amount, including compound values:

```text
1,01 руб.   → один рубль одна копейка
2,50 руб.   → два рубля пятьдесят копеек
5,01 руб.   → пять рублей одна копейка
21,21 руб.  → двадцать один рубль двадцать одна копейка
42,42 руб.  → сорок два рубля сорок две копейки
```

Values above the currently supported genitive/agreement range must be tested
explicitly. If the formatter cannot establish a noun-safe form, it should
preserve and diagnose rather than emit a guessed inflection.

### Currency surfaces and punctuation

The policy must state whether the same monetary expansion applies to:

- `руб.`, `рубль`, `рубля`, and `рублей`;
- `₽` and a currency name supplied in surrounding text;
- terminal `.`, `!`, `?`, `)`, `,`, `;`, and `...`;
- attached lexical continuations (`2,50 руб.foo`) and connectors (`#`, `/`, `%`);
- grouped values (`1 250,50 руб.`) and signed/grouped forms.

Known unsupported or ambiguous forms must remain byte-identical with one full
candidate warning. Currency punctuation must not be duplicated or swallowed.

### Mixed-language ownership

`Language::Auto` and `SegmentCandidates` need an explicit decision for
`Use 2,50 руб.`, `Используем 2,50 GB`, and `2,50 руб. today`. The local
currency surface should not be reformatted merely because a neighboring word
is Latin. Technical identifiers, URLs, and email addresses remain protected.

## Required behavior matrix

Before changing the oracle, the behavior PR should verify positive and negative
neighbors for:

- `0`, `1`, `2`, `5`, `21`, `42`, `100`, and grouped values;
- fractions `,0`, `,00`, `,5`, `,05`, `,50`, `,99`, and more than two digits;
- unsigned, `+`, ASCII `-`, and Unicode minus;
- `руб.`, full noun forms, `₽`, and unsupported foreign currency symbols;
- ordinary terminal punctuation and attached/spaced continuations;
- `Language::Russian`, `Language::Auto`, and `SegmentCandidates`;
- exact `normalized_text`, `pronunciation_text`, warning count/code, and
  UTF-8 byte spans.

Every accepted monetary form needs an adversarial neighbor: invalid lexical
tail, technical identifier, URL/email, independent following number, combining
mark, and end-of-string. A form with unsupported precision must be preserved
atomically rather than partially normalized.

## Oracle and release boundary

This review leaves the current `565/88/0/2` snapshot unchanged. A future
behavior PR should implement the approved major/minor policy and add direct
regressions first. A separate oracle commit may then update only
`ru-v030-money-7-a` and `ru-v030-money-7-b`, with old/new outputs and the
precision, sign, noun-agreement, punctuation, and mixed-language decisions
documented.

Published `v0.2.0`, `v0.2.1`, `v0.2.2`, and `v0.2.3` archives remain immutable.
