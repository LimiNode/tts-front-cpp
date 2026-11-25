# Atomic signed-colon preservation (#95)

## Proposed corrective contract

This Draft implements the separately authorized general preservation option,
not a new pronunciation for Unicode minus. On base `d565769` the mixed local
collector rejected a sign following `:` before advancing its numeric cursor;
the unsigned interior could then be rewritten. Rejecting digit restarts alone
also exposed an ASCII-minus fallback and was rejected as unsafe.

Before any language or mixed-candidate rewrite, preserve an attached signed
numeric/currency candidate after a nonnumeric colon. ASCII plus, ASCII minus
and U+2212 use one path. Both currency/sign orders, grouped cores, scientific
cores, combining marks, and attached/spaced numeric continuations are admitted
as one source span. Emit exactly one `UnresolvedNumber` for that span, after
any existing global ambiguity diagnostic. The sign/currency prefix belongs
inside the span; the colon does not.

An independent following number and a phrase after `;` remain independent.
Terminal `.`, `,` and ordinary spaced em/en dash before a phrase are punctuation.
An incomplete terminal numeric connector (`%`, `-`, `#` and their supported
spaced forms) belongs to this preserved candidate. The explicit internal
`SignedColon` admission kind supplies that endpoint; existing `Contextual`
admission and its legacy noun controls are unchanged.

The first lexical run following the numeric core belongs to the candidate.
A separate ordinary lexical word after that run is not silently pulled into
its diagnostic merely because the first run has a combining accent.

Terminal-connector inclusion above is limited to end-of-string. For example,
`OpenAI:-2 задачи%; 4` keeps the signed candidate's source bytes, but its
`UnresolvedNumber@7:15` does not include `%` before the semicolon. This Draft
does not widen the existing interior punctuation endpoint policy.

`original_text` remains byte exact. Both output fields preserve the candidate;
the existing independent spacing cleanup, technical pronunciation and explicit
dictionary overrides still operate under their existing contracts. No public
API, oracle, snapshot, archive, version, or morphology policy is changed.

## Exact before/after observations

Text below applies to both output fields unless noted. `A` abbreviates
`AmbiguousNormalization`; `U` abbreviates `UnresolvedNumber`. All spans are
UTF-8 source bytes, not codepoint positions.

| Mode and input | Base `d565769` | Draft result | Ordered Draft diagnostics |
| --- | --- | --- | --- |
| Segment `OpenAI:−2 задачи` | `OpenAI:−два задачи`, only `A@0:24` | source preserved | `A@0:24;U@7:17` |
| Segment `OpenAI:+2 задачи` | `OpenAI:+два задачи`, only `A@0:22` | source preserved | `A@0:22;U@7:15` |
| RU `OpenAI:-2 задачи` | `OpenAI:-два задачи`, no warning | source preserved | `U@7:15` |
| EN `OpenAI:-2 задачи` | `OpenAI:-two задачи`, no warning | source preserved | `U@7:15` |
| Auto `OpenAI:-2 задачи` | `OpenAI:-два задачи`, only `A@0:22` | source preserved | `A@0:22;U@7:15` |
| Segment `OpenAI:-2 задачи /4` | source preserved, `A@0:25;U@8:17` | source preserved | `A@0:25;U@7:18` |
| RU `OpenAI:-2 задачи /4` | `OpenAI:-два задачи /четыре`, no warning | source preserved | `U@7:18` |
| Segment `OpenAI:-$2 задачи /4` | `OpenAI:-$2 задачи /четыре`, `A@0:26;U@7:3` | source preserved | `A@0:26;U@7:19` |
| Segment `OpenAI:-2 задачи%` | `OpenAI:-два задачи%`, only global ambiguity | source preserved | `A@0:23;U@7:16` |
| Segment `OpenAI:-2 задачи; 1 книга` | `OpenAI:-два задачи; одна книга` | `OpenAI:-2 задачи; одна книга` | `A@0:36;U@7:15` |

The raw base/current boundary probe records `original_text`, both output fields
and ordered codes/spans for 2,688 rows: four prefixes (`OpenAI:`, `OpenAI `,
`текст:`, `id:`), four signs including unsigned, three plain/grouped/currency
cores, fourteen tails, and EN/RU/Auto/SegmentCandidates. Exactly 1,182 rows
change intentionally inside the attached signed-colon slice. All 1,506 other
rows are byte-identical, including every unsigned and space-prefix neighbor.
No original-text changes, duplicate diagnostics or overlapping distinct
`UnresolvedNumber` spans were found. Additional exact probes cover numeric-left,
both currency/sign orders, scientific/hash overlap and protected neighbors.

The committed [direct tests](../tests/signed_colon_tests.cpp) independently
specify the preserved bytes, source spans, ordered warnings and independent
next-token output; their gold strings are not read from the production catalog.
The existing 720 signed-number observations remain unchanged.

## Ownership and excluded legacy cases

Core scanner prefix admission distinguishes a numeric-led left surface from
a lexical owner ending in digits (`id2`, `OpenAI2`, `строка2`). A clock/ratio or
malformed numeric-led surface does not gain a new independent sign after its
colon. In particular, `1.2:-2` retains full preservation with `U@0:6` in all
four modes. The inherited partial readings of `12:-2`, `1:+2`, `1:−2 задачи`
and `1.2a:-2 задачи` are explicitly outside this bounded corrective; this PR
does not claim to solve all colon syntax.

Common admission reuses the existing structural/grouping analysis and shared
continuation scanner. Technical ownership is established first: one catalog
scan builds both the unchanged ordinary technical ranges and admission
barriers. Numeric-owned generic scientific/hash tails are not opaque barriers;
actual URL/email/protocol/identifier ranges are. Barriers mask classification
points only, leaving original bytes and offsets unchanged. A technical neighbor
therefore cannot be absorbed into a numeric warning, and the next number after
`HTTP/2` remains independent.

The scientific-index fast prefilter is a separate performance-only commit.
Its committed differential test compares exact ranges with the previous
implementation on 1,152 deterministic signed/grouped/scientific/continuation
inputs. It avoids walking an ordinary connector chain's entire remaining tail
for every non-scientific start.

## Stacked dependencies and performance gate

This Draft is rebased onto #99 (`060af62`), which is itself stacked on #97
(`56325276`). Those independent email matcher correctives preserve their
previous exact ranges. Technical range collection must keep using
`scan_pattern_ranges()` after the rebase, not reintroduce recursive email
matching through a direct regex iterator.

`--signed-colon-scaling` times technical-index construction plus early common
admission for whitespace, a digit run, an unsigned connector chain, signed
semicolon-separated candidates and signed slash-separated candidates. Each
load uses 1,000/2,000/4,000/8,000 units, checks original text and mapping,
warning count and exact source spans, and rejects a fitted doubling slope of
3.25 or greater. It runs separately from the ASCII email matcher gate.
