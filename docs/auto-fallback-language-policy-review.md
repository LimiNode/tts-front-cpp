# Configurable `Language::Auto` fallback review

Status: review-only proposal. This document does not authorize a production
change, an oracle update, or a release-archive change.

## Scope

`Language::Auto` currently uses English behavior when a technical token has no
reliable lexical language context. A caller may know the assistant's primary
language even when the input is only a technical token, so the next behavior
PR may make that fallback configurable.

The change is limited to spoken technical rendering. It must not change
`normalized_text`, technical shielding, source mapping, diagnostics, or byte
spans.

## Contract

Language ownership is resolved in this order:

1. An explicit `Language::Russian` or `Language::English` request has highest
   priority.
2. For `Language::Auto`, reliable lexical language evidence wins over any
   fallback. Technical identifiers alone are not lexical evidence.
3. If `Auto` has no language evidence outside technical spans, the configured
   fallback may select the technical pronunciation. Without a configured
   fallback, the technical surface is preserved in spoken output.
4. `MixedLanguagePolicy::SegmentCandidates` uses the local lexical segment
   owner. A global fallback must not override reliable local ownership.
5. Conflicting or ambiguous language evidence is never resolved by this
   fallback. The technical surface is preserved and the existing ambiguity
   diagnostic remains, even when a fallback is configured. A separate policy
   option would be required for deliberate ambiguity resolution.
6. A `PronunciationDictionary` entry always overrides a built-in technical
   default.

Preservation has three independent dimensions:

- the exact normalized spelling and source bytes;
- the existing diagnostics and UTF-8 byte spans;
- the spoken `pronunciation_text` when no reliable technical reading is
  selected.

An unknown technical token is not itself a warning. Existing malformed or
ambiguous candidate diagnostics remain unchanged.

## API options

| Option | Shape | Compatibility | Assessment |
| --- | --- | --- | --- |
| A | `Language auto_fallback_language = Language::English` | Existing aggregate callers keep the current default | Uses `Language::Auto` as an implicit preservation sentinel; easy to misuse. |
| B | `std::optional<Language> auto_fallback_language = Language::English` | Existing callers keep the current default; field can be appended to `TextFrontendOptions` | Explicit `std::nullopt` means preservation, with no new public enum. **Selected.** |
| C | `AutoFallbackPolicy { English, Russian, Preserve }` | Existing default can remain English | Clearer names, but adds a public enum for one option and a new validation surface. |
| D | Separate `AutoLanguagePolicy` object | Existing default can remain unchanged | More flexible, but unnecessary until more than one Auto policy needs configuration. |

### Selected minimal variant

Add one field at the end of `TextFrontendOptions`:

```cpp
std::optional<Language> auto_fallback_language = Language::English;
```

Only `Language::Russian` and `Language::English` are valid configured values.
`std::nullopt` requests preservation when no reliable owner exists. A value of
`Language::Auto` is invalid for this field and is treated fail-closed as
`std::nullopt`; it must not silently select a language. The behavior PR must
cover this with a direct test.

The field is consulted only when `language == Language::Auto`. Explicit RU/EN
requests ignore it. In `SegmentCandidates`, local ownership is resolved first;
the fallback is considered only for a candidate with no local owner.

Appending the field preserves source compatibility for existing aggregate
initializers that omit trailing members and preserves runtime behavior through
the English default. It changes the public struct layout, so binary/ABI
compatibility is not promised by this proposal and would require a separate
compatibility review.

## Observable examples

The following are behavior-PR targets, not current gold changes:

| Input and options | Required owner | Spoken technical result |
| --- | --- | --- |
| `Auto("RTX 4090")`, fallback RU | Russian fallback | Russian `RTX 4090` reading |
| `Auto("RTX 4090")`, fallback EN/default | English fallback | Current English reading |
| `Auto("RTX 4090")`, fallback unset | No owner | Preserve `RTX 4090` |
| `Auto("Тест RTX 4090")`, any fallback | Reliable Russian context | Russian reading; fallback is ignored |
| `Auto("Test RTX 4090")`, any fallback | Reliable English context | English reading; fallback is ignored |
| `Auto("Тест Test RTX 4090")`, fallback RU | Conflicting context | Ignore fallback; preserve the technical token and keep existing ambiguity diagnostic |
| `SegmentCandidates("Тест RTX 4090 and C++")` | Local RU then EN owners | Russian `RTX 4090`, English `C++`; global fallback cannot override either |
| `SegmentCandidates("RTX 4090")`, fallback RU | No local owner | Russian fallback |

The same matrix must be run for `C++` and `HTTP/2`. A dictionary entry for
each token must win in RU, EN, Auto, and SegmentCandidates, regardless of the
fallback setting.

## Required behavior matrix

The behavior PR must compare exact results for:

- explicit RU and EN;
- Auto with Russian fallback, English fallback, and no fallback;
- reliable Russian and English lexical governors;
- mixed and ambiguous Auto input;
- configured fallback on ambiguous input (must be ignored);
- invalid `auto_fallback_language = Language::Auto` (must fail closed as unset);
- SegmentCandidates with independent local owners and with no local owner;
- `RTX 4090`, `C++`, and `HTTP/2`;
- dictionary overrides for every token and mode;
- punctuation, whitespace/newline, URL/email shielding, attached suffixes,
  continuations, and unknown neighboring technical identifiers.

Every row must assert `original_text`, `normalized_text`,
`pronunciation_text`, ordered `automatic_rewrites`, dictionary replacements,
diagnostic codes, diagnostic counts, and exact UTF-8 byte offsets and lengths.
Fallback must never leak internal technical markers or rewrite only part of an
unknown compound identifier.

## Separation from oracle and releases

This review-only PR changes no production code, tests, working oracle,
snapshots, or published archives. The behavior PR must keep the default
English fallback and add focused regressions before any oracle change. An
oracle update, if needed, belongs in a separate commit with explicit old/new
results. Published `v0.2.x` archives remain immutable.

## Exit criteria

The policy is ready for implementation when reviewers confirm:

- the selected field and its `std::nullopt` preservation meaning;
- fail-closed treatment of an invalid `Language::Auto` value in the field;
- the rule that fallback is ignored for conflicting or ambiguous evidence;
- preservation for conflicting ownership;
- local ownership precedence in SegmentCandidates;
- dictionary precedence and unchanged diagnostics/spans.
