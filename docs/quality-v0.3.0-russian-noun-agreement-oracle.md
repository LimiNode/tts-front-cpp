# v0.3.0 Russian noun-agreement oracle update

This is the historical book-only oracle update from PR #88. Its scope does
not describe the later catalog merged in #93; see the
[noun-catalog oracle follow-up](russian-noun-catalog-oracle.md) for the expanded
working corpus. The historical decisions and counts below are retained.

This is the working-oracle follow-up for the merged bounded Russian noun
agreement behavior. The behavior and characterization PRs passed before this
oracle change. Published `v0.2.x` archives remain unchanged.

## Decision

The approved bounded lexicon includes the feminine forms
`книга/книги/книг`. The complete cardinal candidate is rewritten for RU,
plain Auto, and `SegmentCandidates` when the source is an unambiguous Russian
sentence. Unsupported attached continuations such as `/4` and `#4` remain
atomic preservation cases with one `UnresolvedNumber` warning.

The existing `ошибка` behavior is not duplicated here. Unknown nouns,
including `задача`, remain outside this oracle update and keep their existing
policy-gap classification.

## Baseline

The previous working corpus was `751 / 651 / 100 / 0 / 0` for cases,
correct transformations, justified preservations, unnecessary refusals, and
incorrect or partial transformations. This update appends twelve
sentence-sized rows and changes the working baseline to:

```text
763 / 661 / 102 / 0 / 0
```

All 751 existing rows are retained byte-for-byte. The ten positive rows cover
explicit Russian, plain Auto, and `SegmentCandidates`; the two continuation
controls record complete-source preservation and exact UTF-8 warning spans.

The new IDs are:

```text
ru-book-oracle-explicit-01..04
ru-book-oracle-auto-01..03
ru-book-oracle-segment-01..03
ru-book-oracle-continuation-01..02
```

The generated TSV and snapshot are updated from the reviewed static gold in
`tools/russian_noun_agreement_gold.py`. No production code, tests, API,
CMake metadata, or published archive files are changed.
