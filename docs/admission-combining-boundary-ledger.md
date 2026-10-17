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
