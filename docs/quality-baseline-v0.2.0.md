# EN/RU quality baseline for v0.2.0

This report measures the released `v0.2.0` implementation at commit
`77a0c04138b367342f375dcf7050ce4e193e2bef` against manually specified desired TTS renderings.
The expected output was generated from reviewed templates and word forms; it was not copied from
the library output.

## Corpus and method

The corpus contains 352 natural-sentence cases, split evenly between English and Russian. It covers
integers, measurements, currencies, percentages, positive and negative decimals, times, dates,
punctuation, technical spans, and mixed-language phrases. Twenty mixed cases exercise
`MixedLanguagePolicy::SegmentCandidates`; the manually written natural-sentence tail adds varied
brackets, quotes, Unicode punctuation, machine dates, currencies, units, and mixed technical
contexts beyond the generated number matrix.

The audit assigns every case to one of four outcomes:

- `correct_transformation`: output equals the desired spoken form;
- `justified_preservation`: an intentionally ambiguous or unsupported input is unchanged;
- `unnecessary_refusal`: a desired normalization is left unchanged;
- `incorrect_or_partial`: output changes, but does not equal the desired spoken form.

Semantic differences are reported but do not fail CTest. Corpus integrity and safety invariants do
fail the test: valid UTF-8, valid warning byte spans on code-point boundaries, no duplicate warning
for the same code/span, no overlapping `UnresolvedNumber` spans, unchanged `original_text`, and no
leaked internal marker bytes in normalized or pronunciation text.

The audit is a strict regression gate for the released baseline. The snapshot records the case
count, all four outcome counts, every mismatch ID, the exact normalized/pronunciation output for
each mismatch, and exact diagnostics for currently known diagnostic exceptions. A new mismatch,
changed mismatch set, changed count, changed output, or changed warning span fails CTest. Intentional
improvements require a reviewed snapshot update in
`tests/quality/en_ru_snapshot.tsv`.

## Baseline results

| Outcome | Cases |
|---|---:|
| Correct transformation | 278 |
| Justified preservation | 23 |
| Unnecessary refusal | 34 |
| Incorrect or partial transformation | 17 |
| **Total** | **352** |

The implementation matches 301 of 352 desired outcomes (85.5%). Among the 329 cases that request
normalization, 278 match the desired rendering (84.5%). All 23 intentional preservation cases are
preserved.

| Language | Correct | Justified preserve | Unnecessary refusal | Incorrect/partial |
|---|---:|---:|---:|---:|
| EN | 133 | 14 | 17 | 7 |
| RU | 141 | 9 | 17 | 9 |

| Category | Correct | Justified preserve | Unnecessary refusal | Incorrect/partial |
|---|---:|---:|---:|---:|
| Ambiguous or technical | 0 | 16 | 0 | 0 |
| Currency | 20 | 2 | 2 | 1 |
| Date | 9 | 5 | 0 | 4 |
| Decimal | 22 | 0 | 0 | 5 |
| Integer | 93 | 0 | 31 | 1 |
| Measurement | 42 | 0 | 0 | 2 |
| Mixed language | 15 | 0 | 1 | 4 |
| Percent | 32 | 0 | 0 | 0 |
| Punctuation | 32 | 0 | 0 | 0 |
| Time | 13 | 0 | 0 | 0 |

## Discrepancy clusters

1. **Ordinary closing punctuation is over-protected (31 cases).** An integer immediately before
   `).` is preserved with `UnresolvedNumber`, for example `The retry threshold is (2).`. These are
   ordinary sentence boundaries, not malformed numeric continuations.
2. **A parenthesized decimal currency is over-protected (2 cases).** `$12.50` and `$1.01` normalize
   in ordinary prose but are preserved in `Price ($12.50) includes tax.`.
3. **The sign of negative zero is lost (4 cases).** `-0.01` and `-0,01` are rendered as positive
   zero in both tested contexts, without a warning.
4. **English calendar dates use cardinal day forms (4 cases).** For example, `January 2, 2026`
   becomes `January two, two thousand twenty six` instead of `January second, ...`; 1999 is also
   rendered as a full cardinal rather than the preferred spoken year form.
5. **Russian context-sensitive agreement is incomplete (4 cases in the expanded tail).** Mixed
   sentences produce contextually incorrect cardinal forms in error counts, prepositional phrases,
   and decimal units.
6. **Unit and punctuation rendering remain context-sensitive (3 cases).** Fractional `kg` keeps
   its abbreviation, a grouped ruble sentence drops its terminal period, and a bracketed integer
   is preserved with a warning.
7. **Mixed-language local formatting remains asymmetric (5 cases).** Foreign units can use a
   different surface style from the dominant language, while technical candidates may still emit a
   warning or refuse a nearby ordinary number.

The next corrective work should start with ordinary `).` boundary admission and negative-zero sign
preservation because they affect otherwise unambiguous input. English date ordinals and Russian
contextual morphology require explicit language rules and should follow as focused changes backed
by this corpus.

## Reproduction

Regenerate the deterministic corpus and run the audit with:

```text
python tools/generate_quality_corpus.py
python tools/generate_quality_corpus.py --check
cmake -S . -B .temp/quality -DTTS_FRONT_BUILD_TESTS=ON
cmake --build .temp/quality --parallel
ctest --test-dir .temp/quality -R tts_front_quality_corpus_audit --output-on-failure
```

The complete gold data is in `tests/quality/en_ru_sentences.tsv`; the strict snapshot is in
`tests/quality/en_ru_snapshot.tsv`. The snapshot stores exact normalized and pronunciation strings
for every known mismatch. The audit prints up to 100 concrete mismatches so a future baseline update
remains reviewable.
