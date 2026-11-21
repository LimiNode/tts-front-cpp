# Mixed signed-number source edits

## Corrective intent

The local-number collector in `SegmentCandidates` must visit each complete
numeric core once. On `main@5df13c9`, its cursor visited a sign and then the
first digit of the same signed token. For supported ASCII signs this could
schedule overlapping source edits and throw. For unsupported Unicode minus,
the formatter rejected the whole token but the second visit incorrectly
rewrote its unsigned interior.

Advance the collector cursor to the shared scanner's `numeric.end` after a
valid integer scan. Keep the edit anchored at the original signed begin. Do
not consume the following lexical word or independent next number. This is
one source-edit correction, not a new numeric grammar, noun agreement rule,
public API, or pronunciation option. The existing language formatters still
decide which signs they support. No wrapper-level sign guard or new scanner
is introduced.

## Before and after

Captured on 2026-11-22, with default options except the specified mode:

| Input in SegmentCandidates | Base `5df13c9` | Corrected text (both outputs) | Ordered diagnostics |
| --- | --- | --- | --- |
| `OpenAI 2 задачи` | `OpenAI два задачи` | unchanged | `AmbiguousNormalization@0:21` |
| `OpenAI +2 задачи` | overlapping-source-edit exception | `OpenAI два задачи` | `AmbiguousNormalization@0:22` |
| `OpenAI -2 задачи` | overlapping-source-edit exception | `OpenAI минус два задачи` | `AmbiguousNormalization@0:22` |
| `OpenAI −2 задачи` | `OpenAI −два задачи`, only ambiguity warning | `OpenAI −2 задачи` | `AmbiguousNormalization@0:24;UnresolvedNumber@7:17` |

The Unicode-minus result is preservation through the existing formatter's
`nullopt` and existing unresolved-number path, not a guessed spoken reading.
ASCII plus/minus results use the existing formatter on the whole signed core;
there is no new noun rewrite (`задачи` remains generic on this base).

A base/current adversarial probe covered 2 lexical heads, 4 signs (unsigned,
plus, ASCII minus, Unicode minus), 7 plain/grouped/currency/unit surfaces,
12 boundary tails, and EN/RU/Auto/SegmentCandidates: 2,688 runs. The cursor fix
removed all 30 source-edit exceptions and corrected 15 Unicode-minus partial
rewrites. The remaining 2,643 results, including every EN, RU, and plain Auto
result, were byte-identical in outputs and ordered diagnostic spans.

The committed [boundary fixture](../tests/characterization/mixed_signed_number_boundaries.tsv)
contains 720 exact mode observations: all four signs with plain, grouped,
currency, and known-unit surfaces; suffix combining marks, combining plus an
ordinary word, ASCII/Cyrillic continuations, punctuation, end-of-string,
attached/spaced connectors, and an independent following number. A reverse
Russian-head/English-noun slice checks both local-language formatter directions.
The harness compares original text, both output fields, ordered diagnostic
codes and UTF-8 byte spans; it rejects duplicates, overlapping unresolved
spans, and marker leakage. The full probe's newline slice additionally keeps
the next independent number under observation.

Working oracle, snapshots, published archives, and release version are not
changed. Noun-catalog work remains a separate behavior PR.
