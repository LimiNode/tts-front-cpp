# EN/RU quality baseline for v0.2.0

This report measures the released `v0.2.0` implementation at commit
`77a0c04138b367342f375dcf7050ce4e193e2bef` against manually specified desired TTS
renderings. The expected output was generated from reviewed templates and word forms; it was not
copied from the library output.

## Corpus and method

The corpus contains 318 natural-sentence cases, split evenly between English and Russian. It
covers integers, measurements, currencies, percentages, positive and negative decimals, times,
dates, punctuation, technical spans, and mixed-language phrases. Ten mixed cases exercise
`MixedLanguagePolicy::SegmentCandidates`; the rest select EN or RU explicitly.

The audit assigns every case to one of four outcomes:

- `correct_transformation`: output equals the desired spoken form;
- `justified_preservation`: an intentionally ambiguous or unsupported input is unchanged;
- `unnecessary_refusal`: a desired normalization is left unchanged;
- `incorrect_or_partial`: output changes, but does not equal the desired spoken form.

Semantic differences are reported but do not fail CTest. Corpus integrity and safety invariants do
fail the test: valid UTF-8, valid warning byte spans on code-point boundaries, no duplicate warning
for the same code/span, and no leaked internal marker bytes in normalized or pronunciation text.

## Baseline results

| Outcome | Cases |
|---|---:|
| Correct transformation | 256 |
| Justified preservation | 20 |
| Unnecessary refusal | 32 |
| Incorrect or partial transformation | 10 |
| **Total** | **318** |

The implementation matches 276 of 318 desired outcomes (86.8%). Among the 298 cases that request
normalization, 256 match the desired rendering (85.9%). All 20 intentional preservation cases are
preserved.

| Language | Correct | Justified preserve | Unnecessary refusal | Incorrect/partial |
|---|---:|---:|---:|---:|
| EN | 124 | 12 | 17 | 6 |
| RU | 132 | 8 | 15 | 4 |

| Category | Correct | Justified preserve | Unnecessary refusal | Incorrect/partial |
|---|---:|---:|---:|---:|
| Ambiguous or technical | 0 | 16 | 0 | 0 |
| Currency | 18 | 0 | 2 | 0 |
| Date | 8 | 4 | 0 | 4 |
| Decimal | 20 | 0 | 0 | 4 |
| Integer | 90 | 0 | 30 | 0 |
| Measurement | 40 | 0 | 0 | 0 |
| Mixed language | 8 | 0 | 0 | 2 |
| Percent | 30 | 0 | 0 | 0 |
| Punctuation | 30 | 0 | 0 | 0 |
| Time | 12 | 0 | 0 | 0 |

## Discrepancy clusters

1. **Ordinary closing punctuation is over-protected (30 cases).** An integer immediately before
   `).` is preserved with `UnresolvedNumber`, for example `The retry threshold is (2).` and
   `Контрольное значение равно (2).`. These are ordinary sentence boundaries, not malformed
   numeric continuations.
2. **A parenthesized decimal currency is over-protected (2 cases).** `$12.50` and `$1.01` normalize
   in ordinary prose but are preserved in `Price ($12.50) includes tax.`.
3. **The sign of negative zero is lost (4 cases).** `-0.01` and `-0,01` are rendered as positive
   zero in both tested contexts, without a warning.
4. **English calendar dates use cardinal day forms (4 cases).** For example, `January 2, 2026`
   becomes `January two, two thousand twenty six` instead of `January second, ...`; 1999 is also
   rendered as a full cardinal rather than the preferred spoken year form.
5. **Russian context-sensitive agreement is incomplete (2 cases).** Mixed sentences produce
   `два ошибки` instead of `две ошибки` and `после три попыток` instead of `после трёх попыток`.

The next corrective work should start with ordinary `).` boundary admission and negative-zero sign
preservation because they affect otherwise unambiguous input. English date ordinals and Russian
contextual morphology require explicit language rules and should follow as focused changes backed
by this corpus.

## Reproduction

Regenerate the deterministic corpus and run the audit with:

```text
python tools/generate_quality_corpus.py
cmake -S . -B .temp/quality -DTTS_FRONT_BUILD_TESTS=ON
cmake --build .temp/quality --parallel
ctest --test-dir .temp/quality -R tts_front_quality_corpus_audit --output-on-failure
```

The complete gold data is in `tests/quality/en_ru_sentences.tsv`. The audit prints up to 50 concrete
mismatches so a future baseline update remains reviewable.
