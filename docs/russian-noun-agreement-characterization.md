# Russian noun-agreement characterization

The inventory in
[`tests/characterization/russian_noun_agreement_inventory.tsv`](../tests/characterization/russian_noun_agreement_inventory.tsv)
records the current Russian number+noun surfaces. It is characterization data,
not a product oracle: `observed_normalized` and `observed_pronunciation` name
what `v0.2.6` produces today.

The matrix covers feminine (`книга`, `задача`), masculine (`стол`), neuter
(`окно`), and the existing hard-coded `ошибка` control for `1`, `2`, `3`, `4`,
`5`, `21`, `22`, and `25`. Direct tests run every row through RU, Auto, and
SegmentCandidates and compare original text, both output fields, diagnostics,
and warning cardinality.

The generic noun rows are explicitly classified as `known_policy_gap`: the
current formatter changes the cardinal number but does not infer noun gender or
case (`два книги`, `один книга`). The `ошибка` rows are retained as an
`existing_exception` control and must not be expanded by adding more lexical
exceptions.

This characterization does not choose a grammar contract, add a morphology
dictionary, or update the working oracle. A later product decision must define
the supported noun inventory and fail-closed behavior for unknown or ambiguous
nouns before a behavior PR changes normalized text.

The boundary controls remain separate from this matrix: attached continuations,
punctuation, newline, and inflected forms must preserve their exact source and
diagnostic behavior until that policy is approved.
