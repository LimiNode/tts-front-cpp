# EN/RU quality baseline for v0.2.1

This report records the quality contract shipped with `v0.2.1`. The corpus is
the manually reviewed 373-case EN/RU set in
[`tests/quality/archive/v0.2.1/`](../tests/quality/archive/v0.2.1/).

| Outcome | Cases |
| --- | ---: |
| Correct transformation | 324 |
| Justified preservation | 34 |
| Unnecessary refusal | 2 |
| Incorrect or partial transformation | 13 |
| **Total** | **373** |

The snapshot records the exact mismatch IDs, normalized and pronunciation
outputs, and diagnostics. The remaining 15 mismatch IDs are intentionally
carried forward as known quality debt: English dates, Russian contextual
agreement, units, and mixed-language cases. They are not silently promoted to
correct behavior by this patch release.

The archive is checked by `tools/check_quality_archive.py` and executed by the
strict CTest `tts_front_quality_v021_archive`.
