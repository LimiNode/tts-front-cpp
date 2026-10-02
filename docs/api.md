# API design

`TextFrontend` has no mutable global state and is safe to reuse from multiple
threads. Configuration is passed through `TextFrontendOptions`; dictionaries are
non-owning `const` pointers and must outlive the call.

`TextFrontendResult` contains the original and normalized text, rendered
pronunciation text, semantic `WordPronunciation` metadata, dictionary replacements,
and warnings. `stressed_vowel` is a zero-based vowel ordinal, never a byte or
Unicode code-point offset.

## Warning source coordinates

`TextWarning::offset` and `TextWarning::length` use UTF-8 byte coordinates into
`TextFrontendResult::original_text`. They are not offsets into `normalized_text`,
cleanup intermediates, protected-marker text, or regex-stage inputs.

For a warning tied to a concrete source fragment, `length > 0` and the pair selects
the exact original byte range. A zero `length` means that the diagnostic is global
and has no concrete source span, for example `UnsupportedLanguage` or unavailable
automatic stress. Whole-input diagnostics such as invalid UTF-8 may cover the full
original input, including a range beginning at byte offset zero.

The normalization pipeline preserves source provenance through cleanup, protected
technical spans, grouped-number collapse, and length-changing replacements before
emitting span-based warnings.

## Dictionary

Dictionary files use a dependency-free JSON array:

```json
[{"pattern":"Qwen","pronunciation":"квен","match":"case_insensitive"},
 {"pattern":"замок","pronunciation":"замок","stressed_vowel":1}]
```

The same entries can be added with `add_token`, `add_case_insensitive_token`, and
`add_phrase`. Loading is transactional and fail-closed: syntax errors, duplicate
keys, invalid UTF-8, and invalid schema do not mutate an already loaded dictionary.

Phrase entries match complete token sequences, choose the longest match, and do not
rescan dictionary output.

## Pipeline

Processing runs once in a fixed order: UTF-8 validation, optional spacing cleanup,
language detection, language normalization with protected technical spans, dictionary
replacement, and semantic stress diagnostics. The core has no network, Python
runtime, or TTS-engine dependency.

For `Language::Auto`, Cyrillic selects Russian and input without Cyrillic selects
English. Mixed Cyrillic/Latin input uses the Russian policy and emits an
`AmbiguousNormalization` warning.
