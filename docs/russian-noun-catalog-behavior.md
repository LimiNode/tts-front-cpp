# Bounded Russian noun catalog behavior

This behavior batch follows the project owner's approval of the fourteen-word
scope after PR #92. Its current exact base is PR #94 merge
`3c9714a1133f3226da7b845446289b0736a18571`, whose post-merge
[CI](https://github.com/LimiNode/tts-front-cpp/actions/runs/45060579429) passed 12/12.
Historical noun observations were captured at PR #92 merge `5df13c9`.
It does not approve general morphology or mismatch repair.

## Contract and provenance

The runtime uses one Russian-owned table of gender and `one/few/many` forms.
The existing `plural_form` selects the form from the last two digits of the
absolute value: 11–14 select many; otherwise the final digit selects one for
1, few for 2–4, and many for everything else. This applies equally to compound
values such as 101, 102, 111–114, 121 and 122.

The source noun must already have the selected form. The numeral then receives
the catalog gender: `один/одна/одно`, `два/две`. The old `ошибка` repair is
retained only where it already existed; mixed numeral-only formatting cannot
repair a noun. New entries do not inherit any legacy prefix-preservation rule.

Every row below has the same provenance: product-authored forms supplied and
reviewed by the project owner in the bounded-catalog task and the post-#92
behavior approval, adapted on 2026-11-20. The source revision is the catalog
table in PR #91 (`6c17d6d`), characterized by PR #92 (`088139d`). The following
are the exact accepted forms, not data imported from a morphology dictionary.

| Gender | One | Few | Many |
| --- | --- | --- | --- |
| feminine | книга | книги | книг |
| feminine | ошибка | ошибки | ошибок |
| feminine | задача | задачи | задач |
| feminine | машина | машины | машин |
| feminine | команда | команды | команд |
| feminine | строка | строки | строк |
| masculine | стол | стола | столов |
| masculine | дом | дома | домов |
| masculine | файл | файла | файлов |
| masculine | сервер | сервера | серверов |
| neuter | окно | окна | окон |
| neuter | слово | слова | слов |
| neuter | место | места | мест |
| neuter | сообщение | сообщения | сообщений |

No external generator, Python runtime, dictionary download, public API change,
or dependency is introduced. External morphology sources remain research-only
under the license boundaries in the [policy brief](quality-v0.3.0-russian-noun-agreement-expansion-policy.md).

## Admission and compatibility

The existing shared candidate scanner supplies the whole lexical run; lookup
is exact, so `домашних`, `домовой`, `словаря`, `словесных`, and `словами` do
not match the new catalog by prefix. Scanner/admission, UTF-8 decoding, source
mapping and technical protection are not reimplemented. The two old entries
keep their original preservation behavior through explicit catalog flags.

A narrow query in the shared scanner additionally distinguishes independent
cardinal+noun candidates from attached words, unsupported signs, signed-token
fragments and raw grouped-number components. Both the Russian and mixed paths
reuse it; it does not alter the old generic numeric fallback or its diagnostics.
Attached numeric connectors also disqualify a new cardinal head: the final
digits of `2.2 задачи` and `1:102 задачи` are not independent integers.
The raw-group guard requires a non-lexically-attached head: the digit in `v2`
or `id2` must not suppress an independent following `101 задача` or `102 задачи`.
This remains a conservative grouping guard, not a general numeric-head parser.

For the late Russian pass, admission uses one unshielded, source-mapped snapshot
created with the existing restoration helper. The shared scanner records exact
`(offset, length)` origins of admitted numeral cores; it neither stores whole
noun phrases nor admits merely overlapping source ranges. Edits still target
the shielded text. Thus an opaque `#4` tail cannot disappear from admission,
and collapsed `2 002` retains the same complete source origin. The snapshot is
decoded once, not per candidate. Index construction sorts/deduplicates the
origins and lookup uses binary search; no repeated document scan is introduced.

The noun-only governor check is case-insensitive for `до/из/после/за`, including
mixed case and combining acute stress (U+0301). Matching does not remove or
rewrite source stress marks. Whole-word boundaries still exclude `надо́` and
`непо́сле`. Existing generic genitive handling is deliberately unchanged.

RU and Auto use the existing Russian pass. Mixed SegmentCandidates passes the
already scanned local noun and mapped context to its existing number formatter;
the Russian callback consumes the same catalog. The English callback remains
number-only. No Russian lexical data is added to the common coordinator.

`2 задача`, `7 книга`, ordinal forms, unknown nouns, governors `до/из/после/за`,
combining marks, attached/spaced continuations, and unsupported signs keep the
current baseline, including its gaps and diagnostics. This is not a claim that
all those inputs are already fail-closed. Currency ownership is excluded using
the existing shared currency-prefix scanner. Units, currencies, dates, time,
decimals, technical identifiers, URL/email and dictionary precedence stay on
their existing paths. A separate following cardinal phrase can still change.

The existing grouped-number pass may canonicalize a complete cardinal before
catalog lookup: `2 002 задачи` becomes `две тысячи две задачи`. This formats
the complete 2002, not the final source group in isolation. Malformed grouped
neighbors and all currency-prefixed controls retain their baseline. New
entries also retain generic negative-zero spelling (`минус ноль`); the two
legacy entries retain their old zero behavior.

Raw groups in the earlier mixed path are not newly interpreted by this batch:
`OpenAI 2 002 задачи` keeps the observed `OpenAI два два задачи`, while the
independent `OpenAI 2002 задачи` receives the correct feminine ending. The
former is an existing generic mixed/grouping gap, not an approved rendering.
For an admitted, already agreed legacy noun, the same mixed callback also
applies gender (`OpenAI 2 ошибки`: old `два ошибки`, new `две ошибки`). An
incorrect `OpenAI 2 ошибка` retains its old `два ошибка` result; no mixed-path
noun repair is introduced.

## Intentional changes from #92

Historical TSVs and `russian_noun_expansion_cases.hpp` remain byte-identical.
Independent literal test gold and explicit overrides identify each change:

| Historical rows | Old numeral | New numeral | Count |
| --- | --- | --- | --- |
| task/car/team/line, 01 and 21 | один / двадцать один | одна / двадцать одна | 8 |
| task/car/team/line, 02 and 22 | два / двадцать два | две / двадцать две | 8 |
| window/word/place/message, 01 and 21 | один / двадцать один | одно / двадцать одно | 8 |

These are 24 of 140 rows, each in RU/Auto/SegmentCandidates; the other 116
remain unchanged. Original text and the empty diagnostics remain unchanged.

Eleven of 115 neighbor controls intentionally change only the admitted
`два задачи` to `две задачи`: `task-comma`, `task-newline`, `task-independent`,
`task-minus`, `email`, `rtx`, `mixed-head`, `units`, `currency`, `time`, `date`.
The separate mixed control changes only its Russian phrase. In dictionary
controls, only task/car/team/line change the numeral; the user pronunciation
override still wins. Ordered warning codes and UTF-8 spans remain unchanged in
all these controls. Compound gold adds the same gender changes at 101/102 and
121/122; 111–114 retain many forms in every gender.

### Admission corrective after review of `10c5cd9`

The original boundary matrix missed attached numeric separators and a protected
spaced hash tail. Green CI did not establish compatibility for those inputs.
The corrective preserves these exact main@`5df13c9` observations in RU, Auto
and SegmentCandidates, without adding or suppressing diagnostics:

| Input | Base and corrected result | Rejected `10c5cd9` result |
| --- | --- | --- |
| `2.2 задачи` | `2.2 задачи` | `два.две задачи` |
| `1:102 задачи` | `1:102 задачи` | `один:сто две задачи` |
| `ДО 1 окно` | `ДО один окно` | `ДО одно окно` |
| `2 задачи #4` | `два задачи #4` | `две задачи #4` |

Positive controls remain admitted: `1. 2 задачи` becomes `один. две задачи`,
and `2 002 задачи` becomes `две тысячи две задачи`. Preservation of unsupported
neighbors is compatibility evidence, not approval of their missing grammar.

Independent literal boundary gold records both the base and approved current
readings for 156 inputs in all four modes (624 executions): four signs crossed
with plain/grouped/currency cores and thirteen end/combining/lexical/punctuation/
attached/spaced/independent tails. Only twelve positive input rows intentionally
gain gender in the three Russian modes (36 outputs). All other outputs and all
ordered diagnostics/spans remain equal to the captured baseline. A further
828-observation probe includes the review regressions and case variants;
the original 7,784-observation comparison still has exactly 276 intended gender
changes and no original-text or ordered-diagnostic/span drift. Gold is separate
from the production catalog and does not alter historical inventories or oracle.

### Integration and terminal-boundary corrective after `54c2f56`

PR #94 is merged first. Its complete signed-token edit is retained: the
`OpenAI −2 задачи` mixed control now preserves the complete source with
`AmbiguousNormalization @0:24` followed by `UnresolvedNumber @7:17`.
Exactly 30 readings in its 720-row direct-test fixture intentionally receive
the already approved feminine numeral on this noun-catalog branch. No input
or diagnostic field changes in that fixture; it is not the quality oracle.

The remaining review regressions retain the exact merged-base observations:

| Input | Base and corrected result | Rejected `54c2f56` result |
| --- | --- | --- |
| `До́ 1 окно` | `До́ один окно` | `До́ одно окно` |
| `по́сле 1 окно` | `по́сле один окно` | `по́сле одно окно` |
| `2 задачи%` | `два задачи%` | `две задачи%` |
| `2 задачи-` | `два задачи-` | `две задачи-` |

All four rows keep empty diagnostics in RU/Auto/SegmentCandidates. This is
compatibility, not approval of a grammatical case or a malformed connector.
The narrow shared cardinal query rejects compound connectors after the lexical
suffix even when no operand follows, attached or horizontally spaced. It uses
the existing connector classification; it does not change generic continuation
consumption or warning spans. Ordinary period/comma/colon and an independent
next number remain admitted; `1. 2 задачи` and `2 002 задачи` still gain gender.

The independent literal boundary gold adds 264 terminal-connector inputs:
two prefixes (plain and `OpenAI`), four signs, three plain/grouped/currency
cores, and eleven attached/spaced terminal tails. Across all four modes these
1,056 executions retain exact merged-base normalized/pronunciation text and
ordered diagnostics/spans. Together with the earlier 156 inputs, the matrix
now has 1,680 executions and still exactly 36 approved gender deltas.
Accented governors also have direct compound-number and false-prefix controls.

A fresh 2,848-observation base-versus-current probe additionally covers those
governors, combining marks, ASCII/Cyrillic continuations, punctuation,
independent numbers and mixed heads. It has 186 approved gender-only text
deltas, zero exceptions, zero original-text drift and zero ordered diagnostic
or byte-span drift. No EN output changes. The terminal rows above and all
actual governor rows retain their baseline. Literal gold is independent of
the runtime catalog; no expectations are generated from production formatters.

The combined Debug and Release builds and full CTest suites pass 13/13,
including fixture, header/umbrella, generator/checksum and strict `v0.2.2`
archive checks. Format-check, install-consumer and both maintained scaling
gates pass. The 720-row signed fixture has zero mismatches after its 30
approved gender adaptations; an additional 2,944 signed observations have
zero exceptions or invariant failures. The noun/protected-tail scaling probe
now measures 0.47/2.27/4.67/9.74 seconds at 1k/5k/10k/20k segments (median of
three Release runs), with 4.86x/2.06x/2.08x time ratios.

An adversarial noun/protected-tail probe at 1k/5k/10k/20k segments measured
0.55/2.74/5.60/11.45 seconds (median of three Release runs): 4.98× for 5× input,
then 2.04× and 2.04× for each doubling. Both maintained scaling gates also pass.

## Verification boundary

Direct tests include independent 14 × 18 × 3 gold, governors, prefix negatives,
negative zero, grouped cardinals, signed currency controls, technical neighbors,
dictionary precedence and the existing ordinal inventory. Expected noun forms,
number categories and numeral spellings never come from the production table.

A base-versus-current probe also compares 7,784 observations in RU, Auto,
SegmentCandidates and EN: main and compound matrices; fourteen nouns crossed
with unsigned/plus/ASCII-minus/Unicode-minus, plain/grouped/currency surfaces,
and end/combining/combining-word/ASCII/Cyrillic/punctuation/attached/spaced tails;
governors, false prefixes, and the #92 controls. There are 276 intentional
gender-output changes, zero original-text or ordered-diagnostic/span drift,
and zero EN, governor or currency-prefixed output changes. Probes and reports
are transient under `.temp/noun-behavior/`; they are not oracle gold.

A separate 28-observation technical-head probe compares `v2`/`id2`/`v2000`
neighbors, short and compound following cardinals, and genuine grouped controls
in all four modes. Ten outputs intentionally gain the approved noun gender;
ordered diagnostics and byte spans do not change. In RU/Auto/EN the existing
`v2 101 задача` preservation and warning remain; SegmentCandidates renders
`v2 сто одна задача` without treating `2` as a grouping head. The genuine raw
mixed group `OpenAI 2 101 задача` retains its baseline. Direct tests pin the
full outputs and ordered diagnostic spans for these technical neighbors.

The working oracle, snapshots and release archives are unchanged. The working
baseline remains `763 / 661 / 102 / 0 / 0`. A later oracle PR may add a few
natural representative sentences only after behavior review.

### Separately observed existing defect

The signed-edit exceptions are fixed by the merged PR #94, not by this batch.
The separate inherited no-space-colon case
`SegmentCandidates("OpenAI:−2 задачи")` still produces `OpenAI:−два задачи`
with `AmbiguousNormalization @0:24` on the merged base and this branch.
It is recorded independently in [issue #95](https://github.com/LimiNode/tts-front-cpp/issues/95).
This partial rewrite is not approved behavior and is not repaired or added to
the quality oracle by the noun-catalog batch.

The maintained strict `v0.2.2` archive audit passes. Additional strict checks
of `v0.2.0` and `v0.2.1` report their existing snapshot mismatches on both base
and branch; their normal historical smoke tests pass. No snapshot or archive
is changed to conceal those historical differences.
