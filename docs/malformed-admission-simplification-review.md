# Malformed numeric admission simplification review

Review-only architecture note based on merge commit
`cbe14fb54f98d6c0c0581b8f9e6d6d0146f6e161` (PR #38). This document changes
no production code, tests, working-corpus gold or snapshot, public API, CMake
metadata, or published release archives.

The purpose of this review is to define a behavior-preserving route through the
remaining complexity in `normalization/malformed_admission.cpp`. It does not
approve a new language or pronunciation policy.

## Verified baseline and invariants

`protect_malformed_numeric_candidates()` is a shared normalization-stage pass
used by both English and Russian normalizers. In each pipeline, typed
continuations and technical numeric candidates are protected before this pass,
and general language formatting follows it.

The current working corpus contains 655 cases:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial      0
```

The working snapshot contains eight reviewed diagnostic exceptions and no
semantic mismatch. Published `v0.2.x` archives remain immutable. The full
document scaling gate added by PR #38 exercises `TextFrontend::process()` and
is part of the performance baseline for any later refactor. Post-merge workflow
`40194592242` completed on the exact base SHA with all 12 jobs successful.

The following contracts must remain unchanged throughout the simplification:

- valid supported candidates are transformed exactly as before;
- an unsupported or ambiguous candidate is preserved as one complete source
  span with a byte-accurate diagnostic;
- distinct non-empty `UnresolvedNumber` spans do not overlap, identical
  diagnostics are not duplicated, and diagnostics from different categories
  may overlap;
- technical ranges are established before malformed numeric admission and are
  never reinterpreted as language candidates;
- all edits remain source-mapped and no internal marker reaches public output;
- `normalized_text`, `pronunciation_text`, diagnostics, and UTF-8 byte spans
  remain identical to the base commit.

## Current responsibility inventory

The implementation is 776 lines and its main loop currently performs five
different jobs.

### Candidate-start recognition

The loop decides whether a point begins an unsigned, ASCII-signed, Unicode-minus,
positive, currency-prefixed, phone-like, or parenthesized candidate. This stage
also distinguishes positive numeric expressions from phone-like surfaces.

### Structural scanning

The same loop consumes digit groups, whitespace grouping, decimal and date
separators, range connectors, percent signs, exponent tails, long-number tails,
and phone punctuation. During that scan it mutates both the candidate end and
flags such as `malformed_grouped` and `malformed_compound`.

### Continuation expansion

Several later blocks extend the same boundary for combining marks, lexical
suffixes, nonterminal ellipses, technical-looking tails, and attached or spaced
numeric continuations. The file calls the shared scanner entry point, but also
calls lower-level helpers directly:

- `scan_candidate_continuation()`;
- `scan_spaced_numeric_continuation()`;
- `scan_numeric_continuation_points()`;
- `scan_lexical_suffix()`;
- `scan_currency_prefix_begin()`;
- `scan_candidate_surface()`.

Because each call can update `end_index` independently, it is difficult to
prove that all connector, combining-mark, punctuation, and end-of-string
neighbors reach the same final boundary.

### Language-dependent validity

The generic pass interprets `AdmissionRules` supplied by the language modules.
Those rules expose English comma-grouping regexes and a `comma_grouping` flag,
a Russian `dotted_dates` flag, and a language-owned unit/currency surface
classifier. The callbacks live in the correct language directories, but the
normalization implementation still knows which language-specific questions to
ask and combines their answers with generic boundary state.

### Decision and emission

The final compound condition combines separator counts, embedded status,
currency-prefix validity, percent state, English grouping, Russian date
validity, lexical suffixes, and the accumulated malformed flags. It then
chooses `NumericCandidateKind`, checks preservation overlap, emits a diagnostic,
and advances the loop cursor.

This organization is behaviorally covered, but the boundary, validity, and
emission decisions are interleaved. A new neighbor rule can therefore require
editing multiple stages and can accidentally change either the protected span
or the reason for preservation.

## Ownership target

The intended dependency direction is:

```text
core numeric scanner
    -> complete neutral surface and typed AdmissionResult
language admission
    -> classify language validity of that complete surface
normalization admission
    -> coordinate analysis, preservation, diagnostics, and edits
technical protection
    -> remain established before language interpretation
```

Shared UTF-8 traversal, numeric boundaries, connectors, combining marks, and
source spans belong in `core/`. English grouping and Russian date or lexical
surface validity belong in their language modules. `normalization/` should
coordinate those results without reproducing either policy.

This review does not prescribe a public abstraction. Any analysis types should
remain private until their contracts are proven by the existing consumers.

## Proposed behavior-preserving sequence

Each step is a separate PR or commit-sized hypothesis. A failed equivalence
check stops the sequence; it is not resolved by changing the oracle.

### 1. Characterize the current boundary matrix

Add direct characterization tests before moving code. Record exact normalized
and pronunciation text, diagnostic code/count, and byte span for all rows. The
tests establish current behavior; they do not introduce a new expected policy.

### 2. Introduce private analysis state

Inside the current translation unit, replace loosely related local booleans
with a private state that separately records:

- the input surface and current admitted boundary;
- observed structural facts;
- language-validity results;
- preservation reasons;
- the final candidate kind and source span.

Use reason flags or another inspectable private representation rather than a
single `malformed` boolean. This is an organizational change only: the final
predicate and emitted diagnostics must remain equivalent.

### 3. Separate the phases without moving ownership

Extract private functions for start classification, neutral structural scan,
continuation expansion, final decision, and emission. Their inputs and outputs
must make boundary changes explicit. Do not create helpers whose only purpose
is reducing line count, and do not split files while the dependency direction
is still changing.

### 4. Consolidate continuation expansion

Route complete-candidate expansion through the typed `AdmissionResult` wherever
the shared scanner already owns the rule. Direct calls to low-level continuation
helpers should remain only when the caller has a distinct contract that the
typed endpoint cannot express. Every remaining exception must be documented
with a characterization test.

If the typed result lacks information, extend its private/core metadata in a
separate refactor. Do not add a wrapper-level connector, suffix, or sign guard
to compensate.

### 5. Move validity questions behind language-owned endpoints

Replace the generic layer's knowledge of English comma regexes and Russian
dotted-date switches with language-owned internal classification results. The
normalization layer may consume a neutral result such as supported, malformed,
or not-applicable plus the classified candidate kind; it should not evaluate a
language regex or calendar surface itself.

The exact internal interface is a developer architecture decision. Before
implementation, compare at least these alternatives:

- classify a completed neutral surface through one language-owned endpoint;
- have language admission return typed observations consumed by a generic
  decision function.

Choose based on dependency direction and testability, not on the fewest changed
lines. No speculative public interface is needed.

### 6. Split implementation files only after equivalence

Once analysis, continuation, language validity, and emission have stable
boundaries, consider splitting the translation unit along those responsibilities.
File splitting before that point would only distribute the existing coupling.

## Required characterization matrix

The matrix must cover English, Russian, Auto, and SegmentCandidates whenever a
shared boundary is involved. At minimum it includes:

- unsigned, `+`, ASCII `-`, and Unicode-minus starts;
- plain, grouped, decimal, currency-prefixed, percent, date, phone-like, and
  exponent surfaces;
- attached and spaced `#`, `/`, `%`, `$`, plus, minus, and Unicode connectors;
- a known suffix followed by a combining mark;
- a combining mark followed by an ordinary ASCII or Cyrillic lexical word;
- punctuation, ellipsis, newline, an independent following number, and
  end-of-string;
- URL, email, and technical-identifier neighbors;
- supported candidates next to malformed candidates and two independent valid
  candidates.

For every row, compare against
`cbe14fb54f98d6c0c0581b8f9e6d6d0146f6e161` and record:

- exact `normalized_text` and `pronunciation_text`;
- diagnostic code, count, byte offset, and byte length;
- the complete protected source span;
- whether the next token is part of the candidate or independent.

## Non-goals

This simplification must not:

- define new English or Russian semantics;
- change accepted or rejected surfaces;
- change the working oracle, snapshot, or diagnostic exceptions;
- modify published release archives;
- redesign the full UTF-8 phase pipeline;
- replace fixed language patterns with a new regex framework;
- expose a new public admission API;
- combine architecture, behavior, oracle, and release work in one PR.

## Verification and exit criteria

Every implementation step must pass:

- focused characterization and adversarial boundary tests;
- Debug and Release CTest;
- quality corpus and strict archive audit;
- snapshot/checksum/generator checks;
- header self-containment and install-consumer smoke;
- format and `git diff --check`;
- warning-scaling and full document-scaling gates.

The refactor is complete only when behavior is byte-for-byte equivalent on the
matrix and existing corpus, direct low-level continuation calls have either
been removed or individually justified, language-specific validity no longer
leaks into generic normalization decisions, and the final worktree contains no
unrelated or generated changes.

Any newly discovered semantic ambiguity is deferred to a separate review-only
policy document. Any newly discovered defect requires a separate behavior PR;
neither is folded into this simplification series.
