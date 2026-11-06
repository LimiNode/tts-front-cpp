# Technical-token pronunciation policy (review-only)

Status: proposal. This document does not authorize a production change.

The next technical pronunciation change must be a separate bounded behavior PR.
It may change only spoken rendering in `pronunciation_text`; `normalized_text`,
technical shielding, source mapping, diagnostics, and public API remain fixed.

## Non-negotiable boundary

- Technical identifiers are recognized and protected before language formatting.
- A recognized token keeps its exact normalized spelling and UTF-8 source span.
- A spoken rewrite is allowed only for an approved token, language, and context.
- `PronunciationDictionary` is the user override and has priority over any
  built-in default. A built-in default must never overwrite a user entry.
- An unknown, ambiguous, malformed, or partially matched technical surface is
  fail-closed: preserve the complete surface and do not invent a pronunciation.
- An unknown technical token by itself is not a normalization error. Existing
  candidate diagnostics still apply when the same bytes are independently a
  malformed numeric candidate.

This is a policy review, not an oracle update. No normalized or pronunciation
gold is changed here.

## Modes and ownership

| Mode | Candidate owner | Technical pronunciation rule |
| --- | --- | --- |
| `RU` | Russian language policy | Use only an approved Russian spoken form. |
| `EN` | English language policy | Use only an approved English spoken form. |
| `Auto` | Detected dominant language | Do not infer a language from the token alone; preserve when detection is missing or tied. |
| `SegmentCandidates` | Candidate segment language | Apply the segment's approved language rule; this is not equivalent to plain `Auto`. |

The same protected token may therefore have different spoken forms by mode,
but its normalized bytes and source span do not change.

## Primary review candidates

The spelling sources below establish the identifier, not a canonical TTS
pronunciation. Spoken forms are product alternatives and require explicit
approval.

| Token | Normalized form | RU candidate | EN candidate | Alternatives to compare | Confidence / source |
| --- | --- | --- | --- | --- | --- |
| `RTX 4090` | `RTX 4090` | `эр ти икс сорок девяносто` | `R-T-X forty ninety` | `эр ти икс четыреста девяносто`; `R-T-X four hundred ninety` | Medium. NVIDIA product name and model spelling: [GeForce RTX 4090](https://www.nvidia.com/en-us/geforce/graphics-cards/40-series/rtx-4090/). No official spoken contract. |
| `C++` | `C++` | `си плюс плюс` | `C plus plus` | Russian/English language variants; never read the plus signs as a number continuation | Medium. ISO C++ naming and standard references: [isocpp.org](https://isocpp.org/), [C++ draft](https://eel.is/c++draft/). Spoken form remains a product decision. |
| `HTTP/2` | `HTTP/2` | `аш ти ти пи два` | `H-T-T-P two` | `аш ти ти пи слэш два`; `H-T-T-P slash two` | Medium. Protocol spelling and version semantics: [RFC 9113](https://www.rfc-editor.org/rfc/rfc9113). Slash wording is not standardized. |

The initial recommendation is to keep the first candidate in each language as
the default proposal and retain every alternative in the review ledger. The
cardinal reading for `4090` is intentionally not silently selected by this
document.

## Future token inventory

These entries are inventory only. They are not approved defaults.

| Family | Normalized examples | Candidate readings to review | Source / open decision |
| --- | --- | --- | --- |
| CUDA | `CUDA`, `CUDA 12` | `куда` / `C-U-D-A`; English `CUDA` / `C-U-D-A` | NVIDIA [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/). Decide whether the product name is lexicalized or spelled. |
| GPU | `GPU`, `GPU 2` | `джи пи ю`; `G-P-U` | NVIDIA technical documentation. Decide whether RU and EN use the same letter names. |
| API | `API`, `API v2` | `эй пи ай`; `A-P-I`; Russian lexical `апи` remains a separate product option | The term is widespread but has no single pronunciation contract. Preserve unknown compounds. |
| C# | `C#`, `C# 12` | `си шарп`; `C sharp`; `си хэш` is a separate alternative, not a fallback | Microsoft [C# documentation](https://learn.microsoft.com/en-us/dotnet/csharp/). Choose one language-owned form. |
| IPv4 | `IPv4`, `IPv6` | `ай пи ви четыре`; `I-P-v-four`; `ай пи версия четыре` | Address-family spelling: [RFC 791](https://www.rfc-editor.org/rfc/rfc791). Do not rewrite a suffix of an unknown identifier. |
| Versions | `v2`, `v2.1`, `HTTP/3`, `RTX 50xx` | letter-name, cardinal, or version-word forms | Version syntax can be identified from [Semantic Versioning](https://semver.org/) where applicable; exact speech is product policy. |
| Compound identifiers | `CUDA-12`, `C++17`, `GPU/CPU`, `RTX-4090` | whole-token rules only; no partial rewrite around `-`, `/`, or attached digits | Requires a bounded technical-token grammar and negative neighbors. |

## Required context matrix for a future behavior PR

Every approved token needs direct cases for all four modes and both language
policies where applicable. The matrix must include:

- standalone token and punctuation-adjacent token;
- leading/trailing whitespace, newline, and combining-mark neighbors;
- attached and spaced numeric continuations (`C++17`, `HTTP/2 200`);
- ASCII `+`/`-`, Unicode minus, slash, hash, percent, and dot neighbors;
- ordinary lexical neighbors, URL/email spans, and protected technical tokens;
- an unknown model/version and a malformed continuation;
- `Auto` with a reliable language governor, with mixed text, and with no
  reliable governor;
- `SegmentCandidates` with an explicit foreign-language segment.

For every row, record exact `original_text`, `normalized_text`,
`pronunciation_text`, ordered diagnostics, UTF-8 byte spans, and marker-leakage
checks. `auto_segment` must not be used as a substitute for plain `Auto`.

## Dictionary and fail-closed behavior

The future implementation must resolve in this order:

1. user `PronunciationDictionary` entry;
2. an approved built-in token/language/context rule;
3. unchanged spoken text when no rule is reliable.

The third case is not a warning merely because the token is unfamiliar. A
warning is appropriate only when the existing diagnostic contract identifies a
malformed or ambiguous candidate with a complete byte span. Partial rewrites
such as reading `HTTP/2` while leaving an attached unknown suffix are forbidden.

## Provenance and confidence

Each future ledger row must record:

- exact source URL or standard and access date;
- whether the source proves spelling, semantics, or spoken usage;
- language, mode, context distance, and technical protection status;
- proposed normalized and spoken forms;
- confidence (`high`, `medium`, or `low`) and the unresolved alternatives;
- a negative example showing why the rule must not apply more broadly.

Official product pages and standards are strong evidence for spelling and
scope, but not by themselves for TTS pronunciation. A spoken default requires
an explicit product decision or a reproducible corpus source.

## Exit criteria for this review-only stage

This document is ready for product approval only when reviewers choose one
spoken form per approved language/context row, mark alternatives as rejected
or deferred, and confirm the fail-closed boundary. A later behavior PR may then
implement one bounded cluster at a time. It must keep this document's
normalized-text and shielding invariants, add focused regressions and
adversarial neighbors, and update the working oracle only in a separate
follow-up commit. Published release archives remain immutable.
