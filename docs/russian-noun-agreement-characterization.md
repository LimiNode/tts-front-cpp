# Russian noun-agreement characterization

The inventory in
[`tests/characterization/russian_noun_agreement_inventory.tsv`](../tests/characterization/russian_noun_agreement_inventory.tsv)
records the current Russian number+noun surfaces. It is characterization data,
not a product oracle: `observed_normalized` and `observed_pronunciation` name
the outputs captured from `main@bc2ca9d` on 2026-11-20. This expansion starts
from the documentation-only PR #91 merge `5593e90`; production code is
identical to that capture revision. No third-party morphology data is imported.

The subsequent [catalog behavior report](russian-noun-catalog-behavior.md)
enumerates intentional changes against these historical observations. The
source TSV and static control header remain unchanged; current behavior tests
use independent gold and explicit reviewed deltas rather than relabeling the
old observations as desired outputs.

The Russian matrix contains fourteen nouns:

- feminine: `книга`, `ошибка`, `задача`, `машина`, `команда`, `строка`;
- masculine: `стол`, `дом`, `файл`, `сервер`;
- neuter: `окно`, `слово`, `место`, `сообщение`.

Each noun has rows for `0`, `1`, `2`, `3`, `4`, `5`, `11`, `21`, `22`, and
`25`: 140 rows, run directly in RU, plain Auto, and SegmentCandidates for 420
observations. The original 50 rows and IDs are retained; 90 rows are appended.
The harness checks the exact number set for each noun, not merely global
counts, and compares original text, both output fields, diagnostics, warning
cardinality, and marker leakage.

The bounded `книга` rows are classified as `bounded_behavior`: the approved
feminine lexicon now rewrites the complete cardinal candidate (`две книги`,
`двадцать одна книга`). Other generic noun rows remain explicitly classified
as `known_policy_gap`; the formatter still changes only the cardinal number
for those nouns. This classification also applies when a masculine output
happens to be grammatical: it does not establish a noun-aware contract. The
`ошибка` rows are retained as an `existing_exception` control and are
implemented by the same bounded scanner path as `книга`, with their existing
compatibility semantics. Neither path demonstrates support for new generic
nouns, and neither may be expanded by adding more lexical exceptions.

## Neighbor and compatibility observations

The static test data in
[`tests/russian_noun_expansion_cases.hpp`](../tests/russian_noun_expansion_cases.hpp)
records 115 neighbor controls. Each has exact normalized and pronunciation
outputs plus separate ordered diagnostic signatures for RU, Auto, and
SegmentCandidates. The signature format is
`code@byte_offset:byte_length`, separated by semicolons in emission order, or
`none`. The harness validates byte boundaries, duplicate diagnostics,
non-overlapping candidate-specific `UnresolvedNumber` spans, and marker
leakage in outputs and diagnostic messages.

One additional mixed-language control records mode-dependent outputs. Fourteen
dictionary controls, each run in all three modes, verify that pronunciation
overrides leave normalized text and diagnostics unchanged. The existing
[ordinal inventory](../tests/characterization/russian_ordinal_inventory.tsv)
is reused for `7 книг`, `7 книга`, `седьмая книга`, `7-я книга`, and
`7-го издания`.

These are observations, not approval of future mismatch or preservation
semantics. For example, `2 задача` currently becomes `два задача`, and `5 стол`
becomes `пять стол`, both without a warning. Conversely, the legacy exception
may repair `2 ошибка` to `две ошибки`. Generic noun mismatches must not be
described as having an existing full-candidate preservation contract. Any
future preservation plus warning requires a separately approved behavior
decision under the
[expansion policy](quality-v0.3.0-russian-noun-agreement-expansion-policy.md).

## English and later behavior decisions

The parallel English inventory in
[`tests/characterization/english_noun_agreement_inventory.tsv`](../tests/characterization/english_noun_agreement_inventory.tsv)
covers regular `book/books`, irregular `person/people`, and the same ten
numbers in EN, Auto, and SegmentCandidates. It records the existing specialized
unit, currency, time, decimal, ordinal, punctuation, and technical-token paths
as controls. Generic English rows are `known_policy_gap`; specialized controls
are not evidence that generic noun agreement exists.

This characterization does not choose a grammar contract, add a morphology or
plural dictionary, or update the working oracle. Russian and English behavior
must be designed as separate bounded policy decisions. Unknown or ambiguous
nouns/plurals need an explicit fail-closed contract before a behavior PR changes
normalized text.

The neighbor controls cover mismatched forms, punctuation, newline, independent
following numbers, ASCII minus, Unicode minus, plus, combining marks, attached
and spaced continuations, URL/email and RTX/C++/HTTP neighbors, governors,
specialized unit/currency/date/time/decimal paths, unknown nouns, and deferred
irregular words. Their exact observed outputs and ordered diagnostics remain
unchanged by this characterization PR. Catalog behavior and representative
working-oracle updates are separate later PRs; release archives remain
immutable.
