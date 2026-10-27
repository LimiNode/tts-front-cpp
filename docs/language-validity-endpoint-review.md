# Language validity endpoint review

Review-only architecture note based on merge commit
`61db4abdd9533d7b070f337b28ea1533094157cc` (PR #48). This document changes
no production code, tests, working-corpus gold or snapshot, public API, CMake
metadata, or published release archives.

The purpose of this note is to choose a behavior-preserving interface for
language-owned validity questions before a future implementation PR. It does
not define new English or Russian semantics.

## Verified baseline and invariants

The current working corpus remains:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial      0
```

Published `v0.2.x` archives remain immutable. Post-merge workflow
`40470284202` completed on the exact merge SHA with all 12 jobs successful.
The existing warning-scaling and full document-scaling gates remain part of the
refactor baseline.

The future behavior-preserving PR must keep byte-for-byte identical:

- `normalized_text` and `pronunciation_text`;
- diagnostic codes, ordering, counts, offsets, and lengths;
- complete protected spans and non-overlapping `UnresolvedNumber` ranges;
- technical shielding order and source-mapped edits;
- all working-corpus and historical archive results.

No oracle or snapshot change is justified by this review.

## Current validity ownership

`AdmissionRules` is already supplied by the English and Russian language
modules, but the shared normalization pass still interprets its individual
fields:

- English comma-grouped values and comma-grouped percentages are matched by
  regexes held in `AdmissionRules`;
- `comma_grouping` controls whether a comma is treated as an English grouping
  observation;
- `dotted_dates` controls both dotted-separator observation and a later
  malformed-compound branch;
- `classify_surface` is a language-owned callback for unit/currency stems;
- Russian calendar validation is performed in the shared file using
  `try_parse_long()` and `valid_date()` after the candidate surface is built.

The last item is the remaining ownership leak: the Russian module owns the
date policy, but `normalization/malformed_admission.cpp` decides how to parse
and validate the dotted date surface.

## Candidate validity cases that must remain equivalent

The implementation PR must characterize at least these completed surfaces and
their adversarial neighbors before changing the interface:

| Surface family | Supported/observed case | Neighbor that must stay fail-closed or unchanged |
| --- | --- | --- |
| English grouping | `1,234`, `1,234.5`, `1,234%` | `12,34`, `1,234,56`, attached lexical tail |
| Russian dotted date | `01.02.2024` | `31.02.2024`, `01.13.2024`, signed/date-adjacent form |
| Shared separators | `1.5`, `1,5`, `1:05` | percent, connector, punctuation, independent next number |
| Language surface | recognized unit/currency suffix | combining mark, ASCII tail, Cyrillic lexical continuation |
| Technical neighbor | candidate next to URL/email/identifier | same bytes must not be reinterpreted by language validity |

Every row must be checked in RU, Auto, and SegmentCandidates, with EN added
when the shared scanner or admission endpoint is affected. Record exact source
text, normalized and pronunciation text, diagnostics, and UTF-8 byte spans.

## Interface alternatives

### A. One language-owned completed-surface classifier

The shared scanner returns one complete neutral candidate surface and the
language module receives that surface through a private endpoint. The endpoint
returns a typed result such as:

```text
NotApplicable | Supported(kind) | Malformed(reason)
```

English owns grouping and percentage syntax; Russian owns dotted-date parsing,
calendar validity, and language surfaces. The normalizer combines the typed
result with neutral structural facts and performs preservation/emission.

Advantages:

- one ownership endpoint per language;
- no regex evaluation or calendar parsing in the generic layer;
- easy to characterize because the endpoint sees a completed source span.

Risks:

- the private result must carry enough information for existing compound and
  currency branches without recreating boundary logic;
- language modules need a stable internal view of the neutral surface.

### B. Typed language observations consumed by a generic decision helper

The language module returns a private observation record containing fields such
as `valid_english_group`, `valid_russian_date`, `valid_currency_surface`, and a
reason code. A generic decision helper consumes that record together with
`StructuralFacts` and `PreservationFacts`.

Advantages:

- preserves the current decision shape with a small mechanical migration;
- makes each observation explicit and testable;
- allows multiple language-owned checks to share one completed surface.

Risks:

- the observation record can become a second fragmented `AdmissionRules` if
  it exposes too many policy-specific booleans;
- the generic layer may slowly regain language knowledge through reason flags.

## Recommendation

Choose **A** for the next design iteration: one private language-owned
completed-surface classifier returning a small typed result. The result should
describe applicability, validity, and the candidate kind needed by the existing
decision path; it must not expose regex pointers, calendar helpers, or boundary
offsets owned by the scanner.

Before implementation, write characterization tests for the matrix above and
compare all outputs with merge SHA `61db4ab`. If the endpoint cannot express an
existing branch without adding policy-specific flags, stop and document the
conflict instead of adding another generic conditional.

## Non-goals

This review does not:

- define new year, date, currency, or pronunciation policy;
- change accepted or rejected surfaces;
- modify the working oracle, snapshots, or release archives;
- expose a public admission API;
- replace fixed language patterns with a regex framework;
- split files before ownership and equivalence are proven.

## Exit criteria for a future behavior PR

The implementation PR must be one behavior-preserving hypothesis and must
include:

1. direct characterization and adversarial tests for the matrix above;
2. no duplicated boundary or UTF-8 span logic in language code;
3. no generic regex or calendar evaluation after the endpoint migration;
4. unchanged quality corpus, strict archive audit, checksums, and generators;
5. Debug/Release CTest, header self-containment, install-consumer smoke,
   warning scaling, and full document-scaling gates;
6. exact-head push and PR CI results before merge.

Any newly discovered semantic ambiguity remains a separate policy review and
is not folded into the ownership refactor.
