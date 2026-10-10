# EN/RU quality baseline for v0.2.2

This report records the reviewed EN/RU quality contract shipped with `v0.2.2`.
The immutable corpus and snapshot are archived under
[`tests/quality/archive/v0.2.2/`](../tests/quality/archive/v0.2.2/).

| Outcome | Cases |
| --- | ---: |
| Correct transformation | 338 |
| Justified preservation | 35 |
| Unnecessary refusal | 0 |
| Incorrect or partial transformation | 0 |
| **Total** | **373** |

The cycle reviewed English spoken dates, Russian contextual agreement, decimal
units and terminal punctuation, bracketed numbers, and mixed-language dominant
language behavior. Unsupported foreign decimal surfaces remain fail-closed.

The strict archive audit executes all 373 archived cases and compares outcomes,
diagnostic exceptions, and exact outputs. The generator check keeps the working
corpus synchronized with `tools/generate_quality_corpus.py`.
