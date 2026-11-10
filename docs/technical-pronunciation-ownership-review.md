# Technical pronunciation ownership review

Status: characterization only. This document records the current behavior after
the approved defaults for `RTX 4090`, `C++`, and `HTTP/2`. It does not
authorize a pronunciation-policy change.

## Observed behavior

The approved token defaults are language-specific:

| Input | RU | EN |
| --- | --- | --- |
| `RTX 4090` | `эр ти икс сорок девяносто` | `R-T-X forty ninety` |
| `C++` | `си плюс плюс` | `C plus plus` |
| `HTTP/2` | `аш ти ти пи два` | `H-T-T-P two` |

Plain `Auto` currently uses the existing dominant-language detector. A
technical token without a lexical language governor therefore follows the
English fallback:

`Auto("RTX 4090")` -> `R-T-X forty ninety`.

A mixed `Auto` input emits the existing global
`AmbiguousNormalization` diagnostic. For example, `Тест RTX 4090` uses
the Russian technical form, while `Test HTTP/2` uses the English form.

## SegmentCandidates observation

The current pronunciation pass receives the already selected dominant language
and applies that language to every recognized technical surface. The
characterization matrix records this explicitly:

| Input | Mode | Current spoken output |
| --- | --- | --- |
| `Тест RTX 4090 and C++` | Auto + SegmentCandidates | `Тест эр ти икс сорок девяносто and си плюс плюс` |
| `Test RTX 4090 и C++` | Auto + SegmentCandidates | `Test R-T-X forty ninety и C plus plus` |

Both rows preserve `normalized_text` and produce two automatic technical
rewrites. Mixed input retains one global `AmbiguousNormalization` warning with
the full source span.

This is a characterization of the current implementation, not a claim that
dominant-language pronunciation is the final product contract.

## Contract question

The technical pronunciation policy document describes
`SegmentCandidates` as applying the approved language rule for each candidate
segment. The current pipeline does not yet expose a segment-language decision to
the technical pronunciation pass; it uses the dominant language selected during
mixed normalization.

A behavior change therefore needs an explicit decision between:

1. keep dominant-language technical pronunciation for the current release line;
2. assign technical surfaces to the nearest/local lexical segment;
3. preserve technical surfaces when local language ownership is unavailable or
   tied.

The choice affects only `pronunciation_text` and automatic rewrite metadata.
`normalized_text`, source spans, technical shielding, diagnostics, and
dictionary precedence must remain unchanged.

## Required follow-up behavior PR

Before changing implementation:

- approve one ownership policy for `Auto` without a lexical governor;
- define local ownership boundaries for `SegmentCandidates`;
- keep user dictionary entries ahead of built-in defaults;
- add RU/EN/Auto/SegmentCandidates cases for all three approved tokens;
- cover punctuation, URL/email shielding, attached version suffixes, mixed
  lexical heads, and no-governor inputs;
- compare exact pronunciation output, automatic rewrites, diagnostics, and
  source offsets against this characterization baseline;
- keep any oracle update in a separate commit.

No release archive or published baseline may be changed by this review-only stage.
