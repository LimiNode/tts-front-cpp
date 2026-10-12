# EN/RU natural-sentence quality baseline v0.3.0

This branch is based on release commit `f828893b` (`v0.2.2`). It is a
test-only baseline: production sources, public API, version metadata, and all
release archives remain unchanged.

## Corpus

The current corpus contains 655 cases:

- 399 cases carried forward from the v0.2.2 working corpus;
- 256 new natural-sentence cases, split evenly into 128 English and 128 Russian;
- explicit EN/RU cases plus `auto_segment` mixed-language cases;
- years and dates, units, currency, percentages, time, punctuation, Russian
  agreement/case contexts, and URL/email/technical negative controls.

Each TSV row has independent gold fields for `expected_normalized` and
`expected_pronunciation`, plus an exact diagnostic signature. The generator
stores the reviewed gold in `tools/natural_sentence_gold.py`; it does not
derive expected output from a frontend run.

The strict snapshot records counts, every mismatch ID, a mismatch class,
exact normalized/pronunciation output, and diagnostic exceptions. The v0.2.2
archive is not modified.

## Current results

The audit reports:

```text
655 cases
correct_transformation  531
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial     36
```

This is a coverage expansion, not a production-quality regression claim:
the carried-forward 399-case baseline was `345 / 54 / 0 / 0`; the 36 new
semantic mismatches are exposed by the additional natural sentences and are
tracked below.

The 36 semantic mismatches are classified as:

- `year`: 18 (English year reading and Russian year agreement);
- `mixed_language`: 10 (segment-language policy and foreign units/words);
- `date`: 4 (Russian orthography in year/date forms);
- `grammar`: 2 (Russian genitive agreement);
- `currency`: 2 (Russian decimal currency wording).

These are recorded in `tests/quality/en_ru_snapshot.tsv`; this PR does not
change implementation or silently rewrite gold to make a mismatch disappear.

## Adversarial scanner checks

The direct frontend test includes a matrix over ASCII and Unicode connectors,
whitespace and terminal boundaries, technical identifiers, URLs, and email
surfaces. It asserts byte-stable `original_text`, valid UTF-8 warning spans,
absence of marker leakage, and no overlapping `UnresolvedNumber` spans. The
matrix is executed for English, Russian, and Auto/segment mode.

## Reproduction

```text
python tools/generate_quality_corpus.py --check
python tools/check_quality_archive.py
cmake --build build --target tts_front_tests tts_front_quality_corpus_audit
ctest --test-dir build --output-on-failure
ctest --test-dir build-release --output-on-failure
build/tts_front_quality_corpus_audit
build/tts_front_quality_archive_smoke --strict v0.2.2
```

## Next behavior priorities

The next behavior PR should be limited to at most three focused clusters:

1. English year/date policy, including spoken-year style and Russian date
   inflection/orthography where the two policies overlap;
2. Russian genitive agreement and decimal currency forms;
3. mixed-language segment policy, especially foreign units and which segment
   supplies number/unit language.

No corrective behavior change is part of this baseline PR.
