# Russian ordinal forms policy review

Status: review-only proposal. This document records the current boundary and
the decision required before any ordinal behavior change. It does not change
production code, tests, the working oracle, release snapshots, or archives.

The observations below are based on characterization PR #82, merged as
`7996f0a6c9a4fa266399ec7e70195ac6f995ea1a`. The executable inventory is
[`tests/characterization/russian_ordinal_inventory.tsv`](../tests/characterization/russian_ordinal_inventory.tsv).

## Policy boundary

| Surface | Current behavior | Policy status |
| --- | --- | --- |
| `7 книг` | `семь книг`, no diagnostic | Keep on the cardinal path. It is not an ordinal case. |
| `седьмая книга` | Preserved, no diagnostic | Already a valid lexical ordinal. No normalization is required. |
| `7-я книга` | Preserved; `UnresolvedNumber` is emitted for the numeric candidate | Fail-closed baseline. Do not infer an ordinal rewrite from the suffix in this policy PR. |
| `7-го издания` | Intended fail-closed boundary: preserve the complete source surface and retain the candidate `UnresolvedNumber` diagnostic | **To be verified.** This row is not in characterization inventory #82; pin its exact output and byte span in a separate characterization PR before behavior work. |
| `7 книга` | Current baseline is `семь книга` with no diagnostic | Separate `policy_gap`. Do not guess whether the number is cardinal or ordinal. |

The same baseline applies to `RU`, plain `Auto`, and
`SegmentCandidates` for standalone ordinal surfaces. A technical or mixed
neighbor may add its own ambiguity diagnostic, but it must not authorize a
partial rewrite of the ordinal candidate.

## Digital ordinal forms in scope

The characterization and future review matrix must treat these suffix families
as complete candidate surfaces:

- nominative and short forms: `7-й`, `7-я`, `7-е`;
- case forms: `7-го`, `7-му`, `7-ю`, `7-м`, `7-ми`, `7-ем`;
- plural/genitive forms such as `7-ых` and `7-ими`, when they occur in a
  supported lexical context.

The hyphen and suffix belong to one source surface. An implementation must not
normalize only the leading digits and leave a suffix or continuation behind.

## Required characterization matrix

Before a behavior PR, add direct rows for all four relevant mode views where
the API supports them (`RU`, `EN` control, plain `Auto`, and
`SegmentCandidates`):

- `7-я книга`, `7-го издания`, `7-му столу`, and representative plural/case
  suffixes;
- controls `7 книг` and `седьмая книга`;
- ambiguous bare form `7 книга`;
- punctuation, newline, `#`, `/`, `%`, `+`, ASCII `-`, Unicode minus, and
  combining-mark neighbors;
- URL/email and technical neighbors, including an independent next number.

Every row must pin `original_text`, `normalized_text`,
`pronunciation_text`, the complete ordered diagnostic signature, and
UTF-8 byte spans. The current preservation rows must remain complete source
surfaces; no marker may leak into either output.

## Product decision required for a behavior PR

The current fail-closed behavior is intentionally not a spoken-ordinal
contract. Product approval is required to choose one bounded direction:

1. keep digital ordinal surfaces unchanged permanently; or
2. add a bounded Russian ordinal formatter that maps an approved suffix and
   lexical context to a spoken ordinal, for example `7-я книга` to
   `седьмая книга` and `7-го издания` to `седьмого издания`.

Option 2 must define the supported noun lexicon or morphology, case and gender
agreement, unknown/ambiguous-noun fallback, and the distinction between
cardinal `7 книг` and ordinal `7-я книга`. It must not use the unresolved
`7 книга` surface as an implicit ordinal signal.

The behavior PR must be bounded to the approved suffix/context cluster. It
must not add isolated noun exceptions, change technical shielding, or alter
the diagnostic and source-span contract without a separately reviewed decision.

## Fail-closed and ownership rules

- Preserve the original bytes whenever the suffix, lexical owner, or case is
  unsupported or ambiguous.
- Do not reinterpret a bare number plus a singular noun as an ordinal.
- Do not consume a connector continuation (`#`, `/`, `%`, signs, or an
  attached lexical continuation) partially.
- A future behavior PR and its oracle update must be separate commits.
- Published release archives remain immutable.

## Exit criteria for this review

This proposal is ready for approval when product reviewers confirm whether
digital ordinal surfaces remain preservation-only or receive the bounded
formatter described above, and when the direct characterization row for
`7-го издания` pins its exact current warning span. Until then, no ordinal
behavior or oracle PR should be opened.
