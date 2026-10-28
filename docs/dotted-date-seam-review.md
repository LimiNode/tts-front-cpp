# Dotted-decimal admission seam review

This review-only note records the remaining `dotted_dates` seam after the
completed-surface validity refactor (merge commit `31f8765`). It does not
change production behavior, tests, the working oracle, public API, CMake
metadata, or published release archives.

## Baseline and invariants

The current 655-case working corpus remains at:

```text
correct_transformation  567
justified_preservation   88
unnecessary_refusal       0
incorrect_or_partial      0
```

The future cleanup must preserve byte-for-byte normalized and pronunciation
text, diagnostic ordering and spans, protected ranges, technical shielding,
and all historical archive results. The `v0.2.x` archives are immutable.

## Current seam

`AdmissionRules::dotted_dates` is currently `false` for English and `true` for
Russian. In the common malformed-admission pass it does two things:

1. enables a policy observation when the neutral structural scan sees a dot
   followed by digits;
2. enables a later continuation branch for dotted percentages, keeping forms
   such as `1.5% 123`, `1.5%/4`, and `1.5%RTX-4090` atomic in Russian.

English dotted decimals and percentages are handled by the English normalizer;
the common pass must not apply the Russian continuation branch to English.
The flag therefore carries language policy through a common coordination type,
even though regex and calendar ownership already live in language modules.

## Proposed cleanup contract

The implementation PR should remove the boolean from `AdmissionRules` and keep
the distinction in a language-owned typed result. The neutral structural scan
may report that the completed surface contains a dotted decimal and a percent;
the Russian admission endpoint then classifies that surface as a malformed
dotted-percentage kind. English must return `NotApplicable` for that kind.

If a provisional surface is needed to decide continuation, the endpoint may be
called once for that provisional surface and once for the final expanded
surface. Both calls must use the same source span and typed scanner facts; no
language regex or calendar parser may be added to common code.

The common layer may use only neutral states and kinds, for example:

```text
NotApplicable | Supported(kind) | Malformed(kind)
```

No `English`, `Russian`, or `dotted_dates` field should remain in the common
admission contract after the behavior-preserving implementation.

## Required characterization matrix

Before and after the cleanup, direct tests must compare normalized text,
pronunciation text, warning code/count, byte offset, and byte length for:

| Family | Cases |
| --- | --- |
| Russian dotted percent | `1.5%`, `1.5% 123`, `1.5%/4`, `1.5%#4`, `1.5%$4` |
| Russian technical neighbors | URL, email, `RTX-1.5%`, `1.5%RTX-4090` |
| Russian boundaries | punctuation, newline, terminal input, combining mark, Cyrillic tail |
| English controls | `1.5%`, `1.5% 123`, `1.5%RTX-4090` and independent next numbers |
| Modes | explicit RU, explicit EN, Auto, and SegmentCandidates |

All unchanged neighbors must retain their complete source spans and existing
diagnostics. No oracle update is expected or permitted.

## Exit criteria

The behavior PR may proceed only when it has:

1. one private language-owned typed endpoint for the dotted-percentage result;
2. no `dotted_dates` boolean in common admission state;
3. unchanged corpus and archive audits;
4. Debug/Release CTest, format, install-consumer, warning scaling, and full
   document-scaling checks;
5. exact-head push and PR CI before merge.

This document intentionally does not choose a new pronunciation or grammar
policy. It only constrains the ownership-preserving implementation.
