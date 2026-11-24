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
at that stage has 1,680 executions and exactly 36 approved gender deltas.
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

### Dash punctuation corrective after `a5bc437`

The terminal-connector guard was too broad: it also rejected an ordinary dash
between a cardinal phrase and the following word. The shared cardinal query
now distinguishes a typographic en dash (U+2013) or em dash (U+2014) separated
on both sides by horizontal whitespace and followed by a letter. It is
punctuation, not an incomplete numeric continuation. The Russian source-origin
index and mixed callback reuse that same decision; no language-wrapper guard,
generic admission change or new dependency is introduced.

| Input | `a5bc437` | Corrected RU/Auto/SegmentCandidates |
| --- | --- | --- |
| `2 задачи — это весь план.` | `два задачи — это весь план.` | `две задачи — это весь план.` |
| `1 сообщение — уже в чате.` | `один сообщение — уже в чате.` | `одно сообщение — уже в чате.` |

Both normalized and pronunciation fields receive only the approved gender
change; original text remains byte-identical and diagnostics remain empty.
The same cases with an `OpenAI` head exercise the mixed callback and retain
their complete original ambiguity spans. Attached dashes, bare terminal dashes,
digits/signs/combining marks after a dash, ASCII minus and mathematical minus
do not gain this punctuation admission. Existing `%`, `-`, `#4` and accented
governor controls are unchanged. EN output is unchanged.

The query itself does not cross a newline. The existing earlier whitespace
normalizer may collapse a newline around a dash into a space; such sentence
controls receive the same gender fix after that normalization, without a new
whitespace or span policy.

Independent literal gold adds 24 sentence/neighbor inputs in all four modes,
with 29 approved gender-only deltas against `a5bc437` and exact unchanged
ordered diagnostic spans. The combined admission gold now has 444 inputs,
1,776 executions and 65 approved deltas across its recorded baselines.
A separate 192-case scanner matrix crosses four signs, three plain/grouped/
currency cores and sixteen punctuation/connector neighbors.

A full 2,620-observation comparison against `a5bc437` crosses both plain and
mixed heads, all signs, grouped/currency cores, combining and ASCII/Cyrillic
tails, terminal/attached/spaced dashes, numeric operands and ordinary words.
Exactly 111 readings regain noun gender before punctuation; no EN output,
original text, ordered warning or byte span changes, and no exceptions occur.
The previous 2,848-control probe is byte-identical, including all closed
connector and governor regressions. These reports remain under `.temp/noun-dash/`;
the focused literal gold and scanner matrix are committed direct tests, not
quality-oracle changes.

On this corrective, complete Debug/Release builds and CTest pass 13/13 each.
Format-check, header/umbrella and fixture tests, generator/checksum checks,
maintained strict `v0.2.2` archive audit, install-consumer smoke and both scaling
gates pass. The earlier 7,784-observation comparison is also byte-identical.
The working baseline remains `763 / 661 / 102 / 0 / 0`; the quality oracle,
published archives, public API and version are untouched.

### Quoted phrases and index scaling corrective after `835cf30`

Opening quotes or brackets may occur between a spaced typographic dash and
the lexical phrase. Shared neutral classification recognizes `(`, `[`, `{`,
ASCII single/double quotes, `«`, `‹`, `‘`, `‚`, `“` and `„`. The cardinal query
skips opening delimiters and horizontal spaces, then still requires a letter.
It does not parse an entire quoted sentence or infer a numeral inside quotes.
Empty prefixes, closing delimiters, numeric/sign starts and combining-only
starts remain outside this punctuation admission. The supplied sentences now
read `В плане две задачи — «запуск» и «проверка».` and
`одно сообщение — «Готово».` in RU/Auto/SegmentCandidates, with no diagnostics.
Both text fields change only the numeral gender; original bytes remain intact.

The index regression had a separate cause: its boolean admission query walked
the complete connector continuation before checking the immediately visible
connector. The same rejection now short-circuits before that full-span scan.
No cache, new index model, cursor skipping, or generic diagnostic-boundary
change is introduced. The final independent numeral in a connector chain and
the next phrase after a semicolon keep their exact index membership.

`--cardinal-source-scaling` is a dedicated CI gate on 800/1600/3200/6400
candidates with `/`, `#` and ordinary `;` separators. It checks membership,
prepares all mapped inputs before timing, warms and measures largest first,
and uses the median of three adaptively batched measurements. A least-squares
fit of log time across all four sizes rejects a doubling slope above 3.25x;
individual adjacent ratios remain visible in the log. The exact old Release
build fails at about 4x; the corrected index passes. This isolates the added
index cost; it does not claim that every
legacy full-pipeline path is linear on arbitrary connector chains.

The first gate at `9b1cbb4` failed in both Linux workflows: slash ratios stayed
near 2.1x, but hash ratios jumped only at the last size (3.79x and 4.82x).
Largest-first warmup alone also left a single-size step on local Windows.
Consequently, the gate estimates growth over the complete eightfold range
rather than classifying one allocation/cache transition as asymptotic growth.
The allocator explanation is plausible, not a proven Linux root cause. The
threshold is unchanged, and linking the revised benchmark to the complete
exact `835cf30` Release library still rejects its quadratic path (3.97x fitted
doubling). Three fresh corrected local runs fit 1.81–2.39x across separators.

Direct tests add 64-element index chains across seven connector spellings,
opening/closing/numeric/combining neighbors in the existing four-sign and
three-core scanner matrix, eleven independently expected delimiter sentences
in RU/Auto/SegmentCandidates/EN, and the review sentences and mixed heads.
The historical indexed C++ gold is not expanded or migrated in this corrective.
A fresh 4,340-observation comparison records 380 approved quoted-phrase gender
changes, no EN/original/ordered-diagnostic/span drift, and no exceptions.
The baseline is confirmed with a separate complete Release library build of
`835cf305181ee668ae9785097ed051c8fc2e4a69`, using the same compiler/optimization.
Earlier 2,620 dash cases and the broader 7,784-case comparison stay byte-identical.

Complete Debug/Release builds and CTest pass 13/13 each. Format, maintained
strict archive, generator/checksum, header/umbrella, fixture, consumer and all
three maintained scaling gates pass. The quality baseline is unchanged at
`763 / 661 / 102 / 0 / 0`. The oracle, archives, API and version remain untouched.
The independent GCC 14 stack report and issue #95 are not fixed by this batch;
neither a release nor an oracle update is authorized by these local checks.

An adversarial noun/protected-tail probe at 1k/5k/10k/20k segments measured
0.55/2.74/5.60/11.45 seconds (median of three Release runs): 4.98× for 5× input,
then 2.04× and 2.04× for each doubling. Both maintained scaling gates also pass.

### Punctuation-chain performance corrective after `e31913b`

The earlier local rejection is insufficient when punctuation precedes a
connector. `./ ` and `:# ` can enter the complete attached-continuation scan
before the rejection. Each later numeral then repeats the same suffix walk.
Rejecting these spellings unconditionally would also be incorrect: the
existing `./ -2 задачи` and `,% 2 задачи` paths can have a `Punctuated` outcome
and retain different index membership. This corrective changes cost, not that
admission policy.

The existing source-index build now owns a document-local memo for Contextual
scanner results: continuation endpoints and admission-tail endpoints/outcomes.
The same scanner implementation runs with or without this memo; there is no
second punctuation grammar. Intermediate suffix results are reused, including
the merged outcome of mixed continuation chains. Combining-mark positions are
collected once so the candidate's combining probe does not re-read a cached
long suffix. Storage is transient and proportional to the UTF-8 document's
codepoint count; there is no persistent cache, new header/API, language data,
dependency edge or change to full-span diagnostic callers. Legacy pipeline
cost outside the added source index is not claimed to be fixed.

The scaling gate adds `./ `, `:# ` and the all-members `,% ` control to the
existing `/`, `#`, `;` rows. Counts, timing method and 3.25x fitted-doubling
threshold are unchanged. Linked to the exact saved `e31913b` Release library,
the new gate fails on `./ ` at 4.03x (800/1600/3200/6400: about
28/111/449/1817 ms). The corrected index passes: about 0.54/1.25/2.28/5.54 ms
for `./ ` and 0.50/0.95/2.13/5.32 ms for `:# `; fitted slopes are 2.13x and
2.21x. The original slash/hash controls and the pure-Punctuated control pass.

Direct tests retain exact last-candidate and independent-after-`;` membership
on 64-element chains. A further 216 short chain matrices compare the memoized
index against the uncached scanner query, crossing punctuation/sign spellings,
mixed connector neighbors, plain/attached/grouped/currency cores, combining
marks and ASCII/Cyrillic lexical continuations. Expected admission is not
redefined by these tests. An exact-base comparison of 7,936 observations in
RU/Auto/SegmentCandidates/EN is byte-identical, including every index range,
original/normalized/pronunciation text and ordered warning code/message/span;
there are no exceptions. Reports remain transient under `.temp/noun-final/`.
Oracle, archives, public API and version are untouched.

Full local Debug/Release builds and CTest pass 13/13 each, including corpus
generation/checksum checks, fixtures, header/umbrella tests and the maintained
strict `v0.2.2` archive audit. Format-check, install-consumer and all three
scaling gates pass. The working baseline stays `763 / 661 / 102 / 0 / 0`.
Exact-head CI and a fresh review remain gates before any merge.

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
