# v0.3.0 technical-pronunciation oracle update

This is the oracle follow-up for the bounded Auto fallback and
`SegmentCandidates` local-ownership behavior. The production and regression
test commits passed before this oracle change. Published `v0.2.x` archives are
unchanged.

## Decision

For `SegmentCandidates`, a protected technical token uses the nearest reliable
lexical run in its local segment. A configured Auto fallback is used only when
no lexical owner exists; it does not override a reliable local owner. The
technical token's normalized spelling, diagnostics, and UTF-8 source span are
unchanged.

The two suffix variants below previously carried expectations from the older
dominant-language oracle. In each case the nearest lexical owner is the verb
immediately following `HTTP/2`, so the technical reading follows that local
English or Russian segment.

| ID | Old expected pronunciation | New expected pronunciation | Reason |
| --- | --- | --- | --- |
| `en-v030-mixed-3-1` | `аш ти ти пи два returned пять ошибок. Проверка завершена.` | `H-T-T-P two returned пять ошибок. Проверка завершена.` | `returned` is the nearest reliable English owner. |
| `ru-v030-mixed-3-1` | `H-T-T-P two вернул five errors. Validation complete.` | `аш ти ти пи два вернул five errors. Validation complete.` | `вернул` is the nearest reliable Russian owner. |

The normalized outputs, diagnostics (`ambiguous_normalization` with their
existing full spans), automatic-rewrite ordering, and all 751 case IDs remain
unchanged. This update removes two semantic mismatches; it does not alter the
working snapshot counts or any release archive.
