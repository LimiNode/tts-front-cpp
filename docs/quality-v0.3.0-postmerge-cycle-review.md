# v0.3.0 post-merge quality-cycle review

Review-only planning document for the next `v0.3.0` quality cycle. It is
based on merge commit `2e9740305d6350117db18e1f780146ac138f847e` (PR #30).
This document changes no production code, tests, working-corpus gold,
snapshots, public API, CMake metadata, or published release archives.

## Verified post-merge state

The current working corpus contains 655 cases. On the merge SHA the local
Debug audit reports:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial      0
```

The quality audit reports eight diagnostic exceptions covered by the working
snapshot and no semantic mismatches. The strict `v0.2.0` archive audit verifies
352 historical cases. Published `v0.2.x` archives remain immutable.

The merge was followed by push workflow `40129698397` on the exact merge SHA;
all 11 jobs succeeded, including Windows, ONNX, Debug/Release, native research,
and install-consumer checks.

This is a clean baseline, not evidence that every unrepresented neighbor has a
settled product meaning. The next cycle expands the reviewed surface before
adding any new oracle expectation.

## What is closed

The previous mismatch groups have explicit behavior and separate oracle
history:

- English spoken-year contexts and their fail-closed range behavior;
- Russian year agreement and canonical `ё` spelling in dates;
- mixed-language local ownership and independent global diagnostics;
- Russian decimal currency major/minor-unit formatting and unsupported-form
  admission.

No new corrective patch should be inferred from the zero-mismatch count. Any
new case must first demonstrate a real policy gap or an implementation defect.

## Next review tracks

The tracks below are review targets, not expected outputs or oracle changes.
They should be handled in separate review-only documents and then in focused
behavior PRs.

### 1. Mixed-language neighbor expansion

Confirm the local-head ownership policy on cases not represented by the ten
closed rows:

- foreign units and currencies next to Russian or English number heads;
- two valid candidates separated by punctuation, connectors, or a newline;
- a valid candidate beside malformed text, a URL, email, or technical token;
- no local lexical head, where the safe result should remain unchanged with
  `AmbiguousNormalization`;
- `Auto` and `SegmentCandidates` outputs with exact diagnostic spans.

The matrix must include ASCII and Unicode signs, grouped/decimal values,
combining marks, terminal punctuation, and independent following numbers. It
must verify that technical protection is applied before language ownership is
decided and that global ambiguity diagnostics are not suppressed by an
independent unresolved candidate.

### 2. English year/date regression boundary

The accepted spoken-year policy needs a regression-only neighbor audit:

- `in`/`from` with sentence-initial capitalization and mixed case;
- bare years versus explicit year contexts;
- `1900`, `1999`, `2000`, `2001`, `2024`, `2100`, and four-digit values with
  leading-zero or attached continuations;
- month-name dates, invalid calendar dates, and `from ... to ...` ranges;
- `%`, `#`, `$`, signs, spaces, Unicode connectors, and lexical suffixes.

The current range policy remains fail-closed until a separate product decision
approves spoken range semantics. No year oracle change is implied by this
track.

### 3. Russian agreement and date-context discovery

Before another grammar behavior PR, collect a review-only matrix for contexts
where gender or case is material:

- `после`, `до`, `из`, `в`, `за`, and `на` with masculine, feminine, and
  neuter heads;
- values `1`, `2`, `5`, `21`, `22`, `42`, `100`, and compound values;
- date phrases such as `дату первого января`, invalid dates, and attached
  continuations;
- forms where a safe formatter cannot establish agreement.

The default for an unresolved grammatical choice is source preservation plus a
diagnostic. A behavior PR must state the confidence boundary instead of using a
numeric threshold as a proxy for grammatical safety.

### 4. Russian currency follow-up

The implemented decimal policy still has intentionally deferred surfaces. A
separate review should decide, with examples and rounding consequences:

- grouped decimals and precision greater than two digits;
- `₽`, full noun forms, and foreign currency symbols;
- plus and Unicode-minus signs, negative zero, and sign pronunciation;
- terminal punctuation, connectors, attached lexical tails, and technical
  neighbors;
- mixed-language unit ownership and independent adjacent candidates.

Unsupported or ambiguous forms remain fail-closed until a policy is approved.

## Required sequence for each track

1. Publish a review-only policy document with current output, proposed meaning,
   alternatives, and explicit deferred cases.
2. Add positive, negative, continuation, and technical neighbors to a focused
   behavior PR without changing release archives.
3. Run the full mode matrix (`Russian`, `English`, `Auto`, and
   `SegmentCandidates` where applicable), checking normalized text,
   pronunciation text, diagnostics, and UTF-8 byte spans.
4. If the product decision changes expected output, update the working oracle
   in a separate, justified commit with old/new results and a regression test.
5. Re-run exact-head push and PR CI before merge; verify post-merge CI on the
   merge SHA before starting the next track.

Each boundary change must use the shared typed admission/scanner result. Do not
add wrapper-level exceptions, duplicate continuation regexes, or a corpus-only
gold change. After several corrective patches in one area, stop and perform a
simplification review before adding another special case.

## Recommended order

Start with mixed-language neighbor expansion, then English year/date boundary
regressions, Russian agreement/date discovery, and finally currency follow-up.
New languages and speculative scanner cleanup remain out of scope until these
tracks produce a reviewed contract and representative evidence.
