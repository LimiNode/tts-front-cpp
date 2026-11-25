# Bounded Russian noun-catalog working oracle

This oracle-only follow-up is based on PR #93 merge
`4fec4703b1d86a2eae085e3827226ca6ff467b6c`. The reviewed head was
`7d5a8108127a8fddc9f547483ca0704ba4dbab72`; its push/PR CI passed 12/12 each,
and [post-merge CI](https://github.com/LimiNode/tts-front-cpp/actions/runs/45507218904)
passed 12/12 on the exact merge SHA before oracle edits started.

## Scope and baseline

Append thirteen representative sentences, not another noun/number/mode matrix:
ten Russian normalizations, one unchanged English normalization control and
two unchanged protected-URL controls. All twelve newly supported nouns appear;
the original corpus retains the existing `книга` and `ошибка` coverage.

```text
                          cases / correct / preservation / refusal / incorrect
before:                   763 / 661 / 102 / 0 / 0
after:                    776 / 672 / 104 / 0 / 0
```

All original 763 TSV rows and the header are retained byte-for-byte in Git's
canonical LF representation. The diff is thirteen appended rows, zero removed
rows. The snapshot changes only its three counts; all eight existing diagnostic
exceptions are unchanged. Published archives, production code, API, version,
characterization inventory and direct-test expectations are not modified.

The gold is independently authored in `tools/russian_noun_agreement_gold.py`,
using the existing generator. It imports neither the production noun catalog
nor frontend output. The module retains its twelve book rows and appends these
thirteen rows. No external dictionary data or new generator is introduced.

## Reviewed old/new readings

The following are exact normalized outputs before #93 (`3c9714a`) and after
its merge (`4fec470`). Pronunciation matches normalized output in every row;
original source bytes are unchanged and ordered diagnostics are `none` on both
builds. Only eight rows change numeral gender, as already approved in #93.
Sources and independent full expectations are literal gold in the module.

| ID suffix | Before #93 | Approved after #93 |
| --- | --- | --- |
| `team` | один команда подготовила два сообщения. | одна команда подготовила два сообщения. |
| `parking` | На парковке два машины, а рядом один дом. | На парковке две машины, а рядом один дом. |
| `room` | В комнате один окно и два стола. | В комнате одно окно и два стола. |
| `report` | В отчёте двадцать один строка и четыре слова. | В отчёте двадцать одна строка и четыре слова. |
| `deployment` | Доступно один место; настроено два сервера. | Доступно одно место; настроено два сервера. |
| `dash-quotes` | В плане два задачи — «запуск» и «проверка». | В плане две задачи — «запуск» и «проверка». |
| `compound-one` | В журнале сто двадцать один сообщение. | В журнале сто двадцать одно сообщение. |
| `compound-few` | В очереди сто двадцать два задачи. | В очереди сто двадцать две задачи. |

All changed IDs have prefix `ru-noun-catalog-`. Their modes are explicit RU
(`team`, `report`, `dash-quotes`), Auto (`parking`, `deployment`, `compound-few`)
and SegmentCandidates (`room`, `compound-one`). The already-agreed source noun
form selects the catalog paradigm; `121` and `122` exercise the final-two-digit
rule. The independent phrase after `;` and the dash before quotes retain their
reviewed scanner behavior.

Five neighboring controls are unchanged on both builds:

- `ru-noun-catalog-files` (SegmentCandidates): `В папке двадцать два файла и три сообщения.`
- `ru-noun-catalog-teen-control` (explicit RU): `В очереди сто двенадцать задач.`
  The `11–14` exception uses the final two digits, not the final digit.
- `ru-noun-catalog-url` (SegmentCandidates): `Ссылка https://example.com/2/задачи сохранена.`
- `en-noun-catalog-control` (explicit EN): `The plan has two tasks.`
- `en-noun-catalog-url` (explicit EN): `Link https://example.com/2/tasks is unchanged.`

Both URLs preserve the complete source and emit no warnings. The English row
checks compatibility, not a new English noun-agreement policy. Unit/currency/
time, technical and full-continuation controls remain in the unchanged corpus;
the original book continuation rows still pin their exact warning spans.

## Regression evidence and limits

The independent noun gold, compound-number cases and admission-neighbor
matrices in `tests/text_frontend_tests.cpp`, `russian_noun_agreement_gold.hpp`
and `russian_noun_admission_gold.hpp` were reviewed and merged in #93 before
these corpus expectations. They cover all fourteen nouns and modes, signs,
grouping, unknown/mismatched forms, combining marks, punctuation and protected
neighbors, with full text fields and exact ordered diagnostic spans. The
quoted-dash sentence also has a literal direct integration regression.

Two separately linked Release probes compare all thirteen proposed sentences
against exact `3c9714a` and the merged source tree: eight intended gender
deltas, five identical controls, no original/diagnostic drift and no exceptions.
The proposed gold is checked against the new library, not generated from it.
Transient probes are under `.temp/noun-oracle-*`; they are not corpus gold.

This is not a decision on mismatched nouns, unknown morphology, governors,
digital ordinals or ambiguous `7 книга`. Issue #95 and the GCC 14 stack failure
remain separate release blockers. No tag or release is part of this PR.

Reproduce the oracle and archive checks with:

```text
python tools/generate_quality_corpus.py --check
python tools/check_quality_archive.py
ctest --test-dir .build/debug --output-on-failure
ctest --test-dir .build/release --output-on-failure
.build/release/tts_front_quality_corpus_audit
.build/release/tts_front_quality_archive_smoke --strict v0.2.2
```
