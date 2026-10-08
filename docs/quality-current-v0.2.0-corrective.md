# Current EN/RU quality report after the negative-zero corrective

This report describes the post-release corrective branch after `v0.2.0`. It is intentionally
separate from [the immutable `v0.2.0` baseline](quality-baseline-v0.2.0.md), which remains tied to
release commit `77a0c04138b367342f375dcf7050ce4e193e2bef`.

The corpus now contains 367 cases, including signed-time, negative-ordinal and mixed negative-zero
regressions. The strict snapshot records exact outputs and diagnostics for all known mismatches.

| Outcome | Cases |
|---|---:|
| Correct transformation | 324 |
| Justified preservation | 28 |
| Unnecessary refusal | 2 |
| Incorrect or partial transformation | 13 |
| **Total** | **367** |

The corrective closes sign loss for ordinary and mixed negative zero. Signed clock expressions and
negative ordinals are now fail-closed: the complete source span is preserved with one
`UnresolvedNumber` warning. Remaining mismatches are the pre-existing English date, Russian
contextual morphology, unit-surface and mixed-language policy clusters.

Regenerate and verify this report with:

```text
python tools/generate_quality_corpus.py --check
ctest --test-dir .temp/quality-debug -R tts_front_quality_corpus_audit --output-on-failure
```
