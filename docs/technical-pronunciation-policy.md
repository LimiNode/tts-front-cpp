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

## Class inventory and status ledger

The inventory is intentionally broader than the first behavior scope. `approved`
means approved for the initial behavior scope, not that every listed reading is
already a finalized product default. `candidate` means a possible follow-up
cluster. `deferred` is inventory-only until a separate decision is recorded.

Every row has a negative neighbor so that a future rule cannot silently consume
an ordinary word, an unknown suffix, or only part of a compound identifier.

| Class | Token / normalized spelling | RU / EN readings to review | Status | Negative example | Separate behavior PR |
| --- | --- | --- | --- | --- | --- |
| Initialism | `CPU` | `си пи ю` / `C-P-U` | candidate | `CPU-bound` with an unknown suffix | yes |
| Initialism | `GPU` | `джи пи ю` / `G-P-U` | candidate | `GPU/CPU` must remain one compound until its rule is approved | yes |
| Initialism | `API` | `эй пи ай` or `апи` / `A-P-I` | candidate | `APIv2` must not be split by an unapproved version rule | yes |
| Initialism | `URL` | `ю ар эл` / `U-R-L` | deferred | URL/email protected range with punctuation | yes |
| Initialism | `URI` | `ю ар ай` / `U-R-I` | deferred | `URIref` unknown compound | yes |
| Initialism | `JSON` | `джейсон` or `джей сон` / `J-S-O-N` | deferred | JSON-like ordinary lexical text | yes |
| Initialism | `XML` | `икс эм эл` / `X-M-L` | deferred | XMLnamespace unknown compound | yes |
| Initialism | `SDK` | `эс ди кей` / `S-D-K` | deferred | `SDKMAN` unknown compound | yes |
| Initialism | `ABI` | `эй би ай` / `A-B-I` | deferred | `ABI2` attached version | yes |
| Initialism | `TTS` | `ти ти эс` / `T-T-S` | deferred | `TTS-front` project identifier | yes |
| Initialism | `ASR` | `эй эс ар` / `A-S-R` | deferred | `ASR2` unknown model | yes |
| Protocol | `HTTP/2` | `аш ти ти пи два` / `H-T-T-P two` | approved (scope) | `HTTP/2.0` must not be partially rewritten | yes |
| Protocol | `HTTP/3` | `аш ти ти пи три` / `H-T-T-P three` | deferred | `HTTP/300` numeric continuation | yes |
| Protocol | `HTTPS` | `аш ти ти пи эс` / `H-T-T-P-S` | deferred | `https://` URL protected range | yes |
| Protocol | `TCP` | `ти си пи` / `T-C-P` | deferred | `TCPdump` unknown compound | yes |
| Protocol | `UDP` | `ю ди пи` / `U-D-P` | deferred | `UDP2` unknown version | yes |
| Protocol | `TLS` | `ти эл эс` / `T-L-S` | deferred | `TLS1.3` attached version | yes |
| Protocol | `SSL` | `эс эс эл` / `S-S-L` | deferred | `SSLv3` version family | yes |
| Protocol | `SSH` | `эс эс эйч` / `S-S-H` | deferred | `ssh://` URL protected range | yes |
| Protocol | `DNS` | `ди эн эс` / `D-N-S` | deferred | `DNSSEC` unknown compound | yes |
| Hardware/model | `RTX 4090` | `эр ти икс сорок девяносто` / `R-T-X forty ninety` | approved (scope) | `RTX 4090 Ti` requires a whole-token rule | yes |
| Hardware/model | `RTX-4090` | same family, hyphen-aware / `R-T-X four zero nine zero` | deferred | `RTX-4090x` unknown model suffix | yes |
| Hardware/model | `VRAM` | `ви рам` / `V-R-A-M` | deferred | `VRAM2` unknown model | yes |
| Hardware/model | `RAM` | `рам` or `эр эй эм` / `R-A-M` | deferred | ordinary word `рам` in Russian text | yes |
| Hardware/model | `NVMe` | `эн ви эм и` / `N-V-Me` | deferred | `NVMe-oF` unknown compound | yes |
| Hardware/model | `USB-C` | `ю эс би си` / `U-S-B-C` | deferred | `USB-C2` attached version | yes |
| Hardware/model | `PCIe 4.0` | `пи си ай экспресс четыре ноль` / `P-C-Ie four point zero` | deferred | `PCIeGen4` unknown compound | yes |
| AI/ML | `CUDA` | `куда` or `си ю ди эй` / `CUDA` or `C-U-D-A` | deferred | `CUDA-12` must be handled as a whole compound | yes |
| AI/ML | `cuDNN` | `куда эн эн` / `C-U-D-N-N` | deferred | case-folded ordinary `cudnn` identifier | yes |
| AI/ML | `NCCL` | `эн си си эл` / `N-C-C-L` | deferred | `NCCL2` unknown version | yes |
| AI/ML | `ONNX` | `он икс икс` / `ONNX` or `O-N-N-X` | deferred | `ONNXRuntime` unknown compound | yes |
| AI/ML | `ORT` | `о ар ти` / `O-R-T` | deferred | `ORT2` unknown runtime version | yes |
| AI/ML | `Qwen3` | `кьювэнь три` / `Qwen three` | deferred | `Qwen3.5` requires a version policy | yes |
| AI/ML | `GPT-5` | `джи пи ти пять` / `G-P-T five` | deferred | `GPT-5-mini` unknown model suffix | yes |
| AI/ML | `Llama 3` | `лама три` / `Llama three` | deferred | `Llama.cpp` compound identifier | yes |
| Language/tool | `C++` | `си плюс плюс` / `C plus plus` | approved (scope) | `C++17` must not rewrite only `C++` | yes |
| Language/tool | `C++17` | whole-token language/version form | deferred | `C++17x` unknown suffix | yes |
| Language/tool | `C#` | `си шарп` / `C sharp` | deferred | `C#12x` unknown suffix | yes |
| Language/tool | `CMake` | `си мейк` / `CMake` | deferred | `cmake-format` tool compound | yes |
| Language/tool | `.NET` | `дот нет` / `dot net` | deferred | hidden filename `.NETrc` | yes |
| Language/tool | `STL` | `эс ти эл` / `S-T-L` | deferred | `stl2` unknown version | yes |
| Version/compound | `v2.1` | `версия два точка один` / `vee two point one` | deferred | ordinary decimal `2.1` | yes |
| Version/compound | `HTTP/2.0` | version-aware whole form | deferred | `HTTP/2.0-beta` unknown suffix | yes |
| Version/compound | `CUDA-12` | whole-token CUDA version | deferred | `CUDA-12x` unknown model | yes |
| Version/compound | `GPU/CPU` | two protected initialisms with explicit slash policy | deferred | URL-like `GPU/CPU/path` | yes |
| Version/compound | `RTX 50xx` | model-family spelling, cardinal deferred | deferred | `RTX 5090 Ti` unapproved extension | yes |

The three `approved (scope)` rows are the only proposed first behavior cluster.
`GPU`, `API`, and `CPU` are the only candidates that may be considered for the
next small cluster; all other rows remain inventory-only.

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
