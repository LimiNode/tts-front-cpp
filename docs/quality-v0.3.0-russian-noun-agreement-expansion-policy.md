# Russian noun-agreement expansion policy review

This is a review-only policy and research brief for the next bounded Russian
noun-agreement behavior cycle. It does not change production code, tests,
working oracle, snapshots, release archives, API, or CMake metadata.

The current baseline is the merged `v0.3.0` working corpus:

```text
763 / 661 / 102 / 0 / 0
```

Published `v0.2.x` quality archives remain immutable.

## Decision boundary

The proposed behavior is surface-local cardinal agreement. A complete numeric
candidate followed by a supported noun may be rewritten using the noun's
reviewed `one`, `few`, and `many` forms. The numeric category is selected from
the absolute value:

```text
11–14                 many
last digit 1          one
last digit 2–4        few
all other values      many
```

The first characterization matrix uses `0`, `1`, `2`, `3`, `4`, `5`, `11`,
`21`, `22`, and `25`. The rule is shared; noun forms are lexical data and are
not inferred from suffixes.

For the proposed catalog rewrite, the source noun must already have the form
selected by the number. This does not describe the current generic-number
baseline: unsupported nouns and mismatched forms may still have only their
number rewritten, without a noun-specific warning. Characterization must
record those actual outputs and diagnostics, not replace them with a desired
fail-closed result.

Full-candidate preservation plus `UnresolvedNumber` for mismatches such as
`2 задача` or `5 стол` is a proposed future behavior requiring separate
approval. This expansion does not introduce it or repair mismatches by guessing
another paradigm. The existing `ошибка` exception keeps its current
compatibility semantics until a separate behavior review changes it.

## Bounded catalog

The catalog has fourteen entries: the two existing entries plus twelve new
candidate entries. Each row is a noun paradigm, not a new code branch.

| Status | Gender | Lemma | `one` | `few` | `many` |
| --- | --- | --- | --- | --- | --- |
| approved-existing | feminine | `книга` | `книга` | `книги` | `книг` |
| approved-existing | feminine | `ошибка` | `ошибка` | `ошибки` | `ошибок` |
| candidate | feminine | `задача` | `задача` | `задачи` | `задач` |
| candidate | feminine | `машина` | `машина` | `машины` | `машин` |
| candidate | feminine | `команда` | `команда` | `команды` | `команд` |
| candidate | feminine | `строка` | `строка` | `строки` | `строк` |
| candidate | masculine | `стол` | `стол` | `стола` | `столов` |
| candidate | masculine | `дом` | `дом` | `дома` | `домов` |
| candidate | masculine | `файл` | `файл` | `файла` | `файлов` |
| candidate | masculine | `сервер` | `сервер` | `сервера` | `серверов` |
| candidate | neuter | `окно` | `окно` | `окна` | `окон` |
| candidate | neuter | `слово` | `слово` | `слова` | `слов` |
| candidate | neuter | `место` | `место` | `места` | `мест` |
| candidate | neuter | `сообщение` | `сообщение` | `сообщения` | `сообщений` |

`письмо` is deliberately deferred from this batch. Although its basic forms
look regular, its plural genitive and inflectional paradigm require a separate
reviewed entry; it must not be added merely because it was mentioned in an
earlier inventory discussion. Irregular or semantically ambiguous words such
as `человек`, `ребёнок`, and `имя` are also out of scope.

Before a behavior PR, every candidate row must carry provenance metadata:
source name and revision, lookup/adaptation date, reviewer, and the exact
forms accepted by the product. A source citation alone does not promote a row
to approved behavior.

## Language and ownership modes

- Explicit `Language::Russian` may apply the catalog to an unambiguous Russian
  surface-local candidate.
- `Language::Auto` requires reliable Russian lexical context. A standalone
  technical token or an ambiguous context does not acquire Russian noun
  agreement from this catalog.
- `SegmentCandidates` uses the local lexical owner of each segment. A global
  language or owner from another segment must not authorize a rewrite.
- Dictionary precedence, technical shielding, URL/email protection, and the
  current `automatic_rewrites` contract remain unchanged.

The behavior PR must change only the agreed normalized/pronunciation fields
for admitted candidates. `original_text`, diagnostics, source spans, and
marker restoration remain governed by the existing scanner and preservation
rules.

## Explicitly unsupported surfaces

The following are outside this expansion. Preserve their current observed
outputs and diagnostics, which are not uniformly fail-closed:

- governors and case-sensitive contexts after `после`, `из`, `до`, or `за`;
- ordinal forms such as `7-я книга` and `7-го издания`;
- ambiguous bare forms such as `7 книга`;
- unknown, unsupported, or conflicting noun analyses;
- units, currency, dates, time, decimals, and existing specialized formatters;
- technical identifiers, URLs, email addresses, and protected ranges;
- attached or spaced continuations that the shared admission endpoint marks as
  one unsupported surface;
- signs and malformed combining-mark boundaries when the current scanner
  preserves them.

No new regular expression pass or suffix heuristic is permitted. Where the
current scanner already preserves an unsupported candidate, retain its exact
source bytes, ordered diagnostics, and UTF-8 byte spans. Where the baseline
still rewrites only a number or emits no warning (for example the `7 книга`
policy gap), record that observation without treating it as approved product
semantics. Extending full-candidate preservation and warnings to those gaps is
a separate policy and behavior decision, not an existing diagnostic contract.

## Architecture and implementation constraints

The behavior implementation, if approved, will use one data-driven catalog
owned by the Russian language domain. The shared numeric scanner/admission
path remains responsible for candidate boundaries, continuations, protection,
and source mapping. The language layer supplies only the noun paradigm and
the common `one/few/many` category selection.

The runtime remains a deterministic, thread-safe C++17 component with no
Python, morphology service, network, ICU, or TTS-runtime dependency. A
build-time generator may be proposed later, but it is not part of this policy
PR and must not silently add third-party data to the distribution.

## Research and provenance notes

The following are reference leads, not runtime dependencies or approved data
imports:

- [pymorphy2 user guide](https://pymorphy2.readthedocs.io/en/stable/user/guide.html)
  documents dictionary-backed analyses and agreement helpers such as
  `make_agree_with_number()`.
- [OpenCorpora downloads](https://opencorpora.org/?page=downloads) are a
  possible build-time research source. Exact dataset revision, redistribution
  terms, and CC BY-SA obligations must be audited before any generated data is
  shipped.
- [Gramdict / Zaliznyak](https://gramdict.ru/) is a useful morphology reference,
  but the published non-commercial licensing caveat excludes automatic product
  inclusion without a separate legal decision.

Until provenance and licensing are approved, the bounded catalog is
product-authored data only. Research tools may generate suggestions or
fixtures outside the repository; they do not define the oracle.

## Required follow-up PRs and exit criteria

1. Characterization-only PR: add the full candidate matrix for RU, Auto, and
   SegmentCandidates, plus negative and boundary controls. Record exact
   `original_text`, `normalized_text`, `pronunciation_text`, ordered
   diagnostics, UTF-8 spans, and marker-leakage checks.
2. Behavior PR: add the single catalog/provider and shared-path integration.
   Keep the existing `книга`/`ошибка` behavior explicit and do not add
   per-word conditionals.
3. Oracle PR: add only representative sentence-sized rows after behavior and
   characterization review. Preserve all 763 existing rows byte-for-byte.
4. Optional data-source or generator PR: separate from behavior and oracle;
   include revision, adaptation date, license decision, and reproducibility
   checks.

The batch is complete only when every supported row has positive and negative
coverage in all three modes, unsupported neighbors retain their baseline
outputs and diagnostics, exact-head CI is green, and no published archive is
changed. Expansion to irregular morphology is a later product decision.
