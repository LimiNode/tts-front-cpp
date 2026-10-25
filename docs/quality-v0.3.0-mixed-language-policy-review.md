# Mixed-language ownership policy review

Review-only proposal for the first focused quality track after the clean
post-merge `v0.3.0` baseline. This document is based on merge commit
`709e0a9ed5c0f43a54c26d69353613993500daf9` (PR #31). It changes no production
code, tests, working-corpus gold, snapshots, public API, CMake metadata, or
published release archives.

## Current evidence

The 655-case working corpus currently audits as:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial      0
```

The previous ten mixed-language mismatch rows were resolved by the local-head
ownership behavior PR and its separate oracle update. This review therefore
does not propose changing an existing expected output. It defines the
unrepresented neighbors that must be decided before adding new corpus rows.

`Language::Auto` retains its conservative whole-input behavior by default:
Cyrillic selects Russian, Latin-only input selects English, and genuinely mixed
input emits `AmbiguousNormalization`. `SegmentCandidates` may format a
recognized foreign unit or currency candidate locally, while technical spans,
URLs, and email addresses remain protected.

## Decisions required before behavior work

### Local lexical ownership

The candidate's nearest lexical head should own the number and unit language
when the relationship is explicit:

```text
Use 2 кг for calibration.
Используем 2 GB данных.
OpenAI API обработал 2 requests.
```

In particular, a Latin-only sentence with a recognized English unit is not
ambiguous merely because no additional noun follows it:

```text
Use 2 kg.       # one English candidate
Use 2 kg 3.     # two independent English candidates
```

The policy must define how far the scanner may look for that head and what
happens when both sides contain plausible heads. It must not infer ownership
from one unrelated foreign word elsewhere in the document.

### No-head and conflicting-head fallback

These genuinely mixed or conflicting surfaces need an explicit fail-closed
result rather than a guessed formatter:

```text
2 reports на 3 сервера.
OpenAI обработал 2 requests и 3 задачи.
Deploy 5 replicas в the cluster.
```

For a candidate with no safe local owner, the proposed result is unchanged
source text plus one `AmbiguousNormalization` (or a candidate-specific
`UnresolvedNumber` when the surface itself is unsupported). If two independent
candidates are present, diagnostics from the two categories may coexist; one
must not suppress the other merely because their ranges overlap at document
scope.

### Candidate boundaries

The same typed admission result must be used for recognized and malformed
mixed candidates. The following continuations are distinct policy classes:

```text
2 кг#3       2 кг /3       2 кг % 3
2 kg$3       2 kg +3       2 kg −3
2 кг...foo   2 кѓ         2 кг\n3
```

Connectors, combining marks, punctuation, and ordinary following numbers must
not cause a partial rewrite. A supported candidate followed by an ordinary
independent number may normalize as two candidates; a connector or lexical
continuation keeps the unsupported compound atomic with one full source span.
The exact boundary contract must be checked in bytes, not code points.

## Required neighbor matrix

Before any behavior or oracle commit, run the same rows through `Russian`,
`English`, `Auto`, and `SegmentCandidates` where the mode is applicable. Record
normalized text, pronunciation text, diagnostic code/count, and UTF-8 offset and
length.

| Dimension | Required values |
|---|---|
| number | `0`, `1`, `2`, `5`, `21`, `42`, grouped and decimal |
| sign | unsigned, `+`, ASCII `-`, Unicode `−` |
| ownership | Russian head, English head, no head, conflicting heads |
| unit/currency | `кг`, `GB`, `руб.`, `₽`, full noun forms |
| continuation | attached word, spaced word, independent number, `#`, `/`, `%`, `$`, `+`, `-`, Unicode connectors |
| boundary | end of string, `.`, `!`, `?`, `)`, `,`, `;`, `...`, newline |
| protection | URL, email, technical identifier, combining mark |

At minimum, include paired positive/negative examples such as:

```text
Use 2 кг.                         # local foreign candidate
Use 2 кг 3.                       # independent following number
Use 2 кг #3.                      # atomic connector continuation
RTX-4090 использует 2 GB.         # technical neighbor
user2 кг@example.com              # email-like negative control
2,5 GB данных / 3 кг материалов   # two owned candidates
```

The matrix must also be executed with `SegmentCandidates` and compared with
`Auto`; a whole-input ambiguity warning may coexist with candidate-specific
warnings, but duplicate `(code, offset, length)` diagnostics are forbidden.

## Architecture and oracle boundary

Admission, continuation scanning, technical protection, and source-span
calculation remain shared scanner responsibilities. Language modules provide
ownership policy and formatting only. Do not add wrapper-level early exits,
duplicate regexes, or a second technical-range list to handle one neighbor.

The workflow is deliberately split:

1. this policy review settles ownership, fallback, and continuation semantics;
2. a behavior PR adds the focused matrix and implementation without changing
   release archives;
3. a separate oracle commit changes only newly approved working-corpus rows,
   with old/new output and diagnostics recorded;
4. exact-head push and PR CI, followed by post-merge CI on the merge SHA, gate
   the next track.

No `v0.2.x` archive may change. New languages and speculative scanner cleanup
remain outside this track.

## Recommended next step

Approve the local-head and fail-closed fallback rules above, then implement one
small behavior slice covering `кг`/`GB`, independent following numbers, and
`#`/`/`/`%` continuations across RU/EN/Auto/SegmentCandidates. Expand to
currency, combining marks, and technical negatives only after that slice passes
the full boundary matrix.
