# Combining-mark admission boundary ledger

This ledger records the differential cases for the combining-mark admission
change. The base is PR #20 HEAD `29baf4e`; the head is the corrective commit
that updates this file. Outputs are compared byte-for-byte, including warning
spans. No corpus snapshot or release archive is changed.

| Mode | Input | Base result | Corrected result | Contract |
| --- | --- | --- | --- | --- |
| EN | `2 с́` | preserved; `UnresolvedNumber 0:6` | `two с́`; no warning | unknown Cyrillic lexical word is not an English unit |
| EN | `2 а́.` | preserved; `UnresolvedNumber 0:6` | `two а́.`; no warning | terminal punctuation remains outside the number |
| RU | `2 м́` | preserved; `UnresolvedNumber 0:6` | preserved; one `UnresolvedNumber 0:6` | short-unit combining surface is fail-closed |
| RU | `2 м́foo` | preserved; `UnresolvedNumber 0:9` | preserved; one `UnresolvedNumber 0:9` | attached ASCII tail remains atomic |
| RU | `2 м́.` | preserved; `UnresolvedNumber 0:6` | preserved; one `UnresolvedNumber 0:6` | terminal punctuation is not absorbed |
| RU/Auto/SegmentCandidates | `2 месяцев` | `два месяцев`; no warning | unchanged | ordinary Cyrillic word remains lexical |
| RU/Auto/SegmentCandidates | `2 м́ежей` | `два м́ежей`; no warning | unchanged | combining mark followed by Cyrillic continuation is lexical |
| RU/Auto/SegmentCandidates | `2 руб́ежей` | `два руб́ежей`; no warning | unchanged | known stem prefix is not enough to classify a word as a unit |
| RU/Auto/SegmentCandidates | `2 метр́ополитенов` | `два метр́ополитенов`; no warning | unchanged | longer Cyrillic continuation remains lexical |

For `Auto` and `SegmentCandidates`, an overlapping broad mixed-language
diagnostic may still be emitted by their explicit policy; the matrix requires
exactly one `UnresolvedNumber` for each malformed short-unit case and checks its
complete byte span. English has no Russian short-unit admission rule.

## Decimal/sign mixed-candidate correction

The following cases were compared against PR #20 HEAD `791e0f2`. The old
segmented path began at the fractional tail or omitted the combining mark;
the corrected path preserves the complete UTF-8 candidate in RU, Auto and
SegmentCandidates.

| Input | Base span | Corrected span | Modes |
| --- | ---: | ---: | --- |
| `2.5 м́` | `2:6` | `0:8` | RU/Auto/SegmentCandidates |
| `2,5 м́` | `2:6` | `0:8` | RU/Auto/SegmentCandidates |
| `+2.5 м́` | `3:6` | `0:9` | RU/Auto/SegmentCandidates |
| `+2,5 м́` | `3:6` | `0:9` | RU/Auto/SegmentCandidates |
| `-2.5 м́` | `3:6` | `0:9` | RU/Auto/SegmentCandidates |
| `-2,5 м́` | `3:6` | `0:9` | RU/Auto/SegmentCandidates |
| `−2.5 м́` | `5:6` | `0:11` | RU/Auto/SegmentCandidates |
| `−2,5 м́` | `5:6` | `0:11` | RU/Auto/SegmentCandidates |

## Continuation after a combining short unit

The base `3c4a7c5` stopped the malformed range at `м́`, allowing a numeric
tail to be normalized separately. The corrective boundary consumes attached
numeric/connector continuations while keeping terminal punctuation outside.

| Input | Base diagnostics | Corrected diagnostics | Modes |
| --- | --- | --- | --- |
| `2 м́%1` | `0:6`; tail rewritten | one `UnresolvedNumber 0:8` | RU/Auto/SegmentCandidates |
| `2 м́1` | `0:6`, `6:1` | one `UnresolvedNumber 0:7` | RU/Auto/SegmentCandidates |
| `2 м́/1` | `0:6`, `7:1` | one `UnresolvedNumber 0:8` | RU/Auto/SegmentCandidates |
| `+2.5 м́%1` | `0:9`; tail rewritten | one `UnresolvedNumber 0:11` | RU/Auto/SegmentCandidates |
| `+2 %́` | `0:2` | one `UnresolvedNumber 0:6` | RU/Auto/SegmentCandidates |
| `2 /́3` | partial warning/tail rewrite | one `UnresolvedNumber 0:6` | RU/Auto/SegmentCandidates |
| `2 #́3` | partial warning/tail rewrite | one `UnresolvedNumber 0:6` | RU/Auto/SegmentCandidates |
