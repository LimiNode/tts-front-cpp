# Selective automatic stress

## Contract and ownership

`StressMode::AutomaticHomographsOnly` is an opt-in addition. Existing enum
values, `Automatic`, and the default `DictionaryOnly` remain unchanged; no
public struct field or runtime dependency is added.

The private Silero backend uses its existing homograph-route flag (phrase
rules or homosolver), not a new frontend word list. Only exact lexical
homographs receive automatic pronunciation/stress decisions. Ordinary words,
including ordinary Silero exception entries, remain unchanged in this mode.
This is selective stress, not general Russian morphology.

PronunciationDictionary token/phrase matching and precedence are unchanged.
Explicit U+0301 accents are protected through the existing rewrite mechanism;
the shared tokenizer can optionally include combining marks when collecting
these protection spans, without changing its default boundaries. An existing
dictionary rewrite takes precedence over an overlapping explicit span.
Hyphenated composites are not truncated into a catalog word. Repeated
homographs retain offsets into the original normalized source, not the
backend's intermediate text containing inserted `+` markers.

Without a usable backend/bundle, deterministic dictionary output survives and
one global `AutomaticStressUnavailable` diagnostic is emitted at `0:0`, just
as for `Automatic`. `resolve_stress=false` does not invoke the backend.

Filtering only frontend metadata was rejected: it would leave ordinary
backend pronunciation changes in place. Changing the legacy tokenizer or the
existing automatic mode was also excluded from this batch.

## Text-only Qwen check

The existing Qwen adapter was compiled unchanged from
`qwen3-tts-bridge-cpp@a1429b6d4ed0676f14e4dfa4355926ae9480664b` against this
frontend, using the explicitly selected `ExplicitCombiningStress` capability.
No Qwen dependency pin or application profile was changed.

| Input | DictionaryOnly | Automatic | AutomaticHomographsOnly |
| --- | --- | --- | --- |
| `дверной замок` | `дверной замок` | `дверно́й замо́к` | `дверной замо́к` |
| `старый замок` | `старый замок` | `ста́рый за́мок` | `старый за́мок` |
| `документы` | `документы` | `докуме́нты` | `документы` |

Selective rendering also preserves explicit `докуме́нты`. A dictionary entry
forcing vowel 0 in `замок` yields `дверной за́мок`; a dictionary entry selecting
vowel 2 in `документы` yields `докуме́нты`. These checks have no warnings.

Selective routing is not a confidence or grammatical-correctness guarantee.
In the additional input `Проверь дверной замок. Покажи старый замок и документы.`,
the same verified Silero homosolver selects vowel 1 for both occurrences of
`замок`, in both backend modes. The selective Qwen text is therefore
`Проверь дверной замо́к. Покажи старый замо́к и документы.` without warnings.
This inherited contextual prediction differs from the isolated `старый замок`
check above; no phrase exception or model change is introduced to conceal it.
Application dictionary overrides remain the deterministic way to request a
specific reading.

The local text check used the verified Silero bundle at source revision
`d38096cae9bf3ac846bbb88705428f3ed3801b96`, model SHA-256
`aecb207df9db34a079de2ba91edba2a9333839ad24de0cf17e7bc82424876786`, and
ORT 1.30.0, runtime revision `f2c39fe2f838cf35ce7da92824f5a5e3ee6e88a7`.
The Windows runtime DLL SHA-256 was
`7e39e2bdbba836d98071ef28620735ba36a47c554cf794585269aecc50fab0da`.

## Verification and remaining gate

Direct tests cover unavailable-backend fallback and stress disabling. Verified
bundle tests cover the two contextual readings, ordinary words, explicit
accents including final-vowel stress, dictionary replacement before a later
homograph, overlapping explicit/dictionary spans, repeated homographs and
hyphenated neighbors. Existing automatic/parity tests remain in place.

No normalization policy, working oracle, published archive or version changes
are included. Corpus baseline remains `776 / 672 / 104 / 0 / 0`.

This receipt is text verification only: no Qwen worker/model or Ephi voice was
used and no audio was generated or listened to. Ephi must remain on
`DictionaryOnly` until an identified authorized voice/profile is compared on
the same utterances and the exact audio artifacts receive a listening verdict.
