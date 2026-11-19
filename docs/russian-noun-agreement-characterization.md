# Russian noun-agreement characterization

The inventory in
[`tests/characterization/russian_noun_agreement_inventory.tsv`](../tests/characterization/russian_noun_agreement_inventory.tsv)
records the current Russian number+noun surfaces. It is characterization data,
not a product oracle: `observed_normalized` and `observed_pronunciation` name
what `v0.2.6` produces today.

The Russian matrix covers feminine (`книга`, `задача`), masculine (`стол`),
neuter (`окно`), and the existing hard-coded `ошибка` control for `0`, `1`, `2`,
`3`, `4`, `5`, `11`, `21`, `22`, and `25`. Direct tests run every row through
RU, Auto, and SegmentCandidates and compare original text, both output fields,
diagnostics, and warning cardinality.

The bounded `книга` rows are classified as `bounded_behavior`: the approved
feminine lexicon now rewrites the complete cardinal candidate (`две книги`,
`двадцать одна книга`). Other generic noun rows remain explicitly classified
as `known_policy_gap`; the formatter still changes only the cardinal number
for those nouns. The `ошибка` rows are retained as an `existing_exception`
control and are implemented by the same bounded scanner path; they must not be
expanded by adding more lexical exceptions.

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

The boundary controls remain separate from this matrix: attached continuations,
punctuation, newline, and inflected forms must preserve their exact source and
diagnostic behavior until that policy is approved.
