# Russian and English noun-agreement policy draft

Status: **review-only proposal**. This document does not approve a behavior
change and is not part of the working oracle.

## Problem boundary

The generic number paths currently rewrite a number without interpreting an
adjacent noun. This is observable in both languages:

```text
RU: 2 книги  -> два книги
RU: 1 книга  -> один книга
EN: 1 books  -> one books
EN: 2 book   -> two book
```

The existing `ошибка` rule and English unit, currency, time, decimal, ordinal,
and technical paths are specialized controls. They are not evidence that a
general noun-agreement mechanism exists. Adding more word-specific regex
exceptions is explicitly out of scope.

## Proposed bounded policy

The recommended product shape is a bounded lexical layer, implemented and
reviewed separately for each language:

- Russian entries carry a lemma, gender, supported number/case forms, and an
  explicit confidence/provenance record. Only entries with an approved form
  set may rewrite a number+noun candidate.
- English entries carry approved regular or irregular singular/plural forms.
  The generic path may pluralize or singularize only an exact approved entry.
- Unit, currency, time, decimal, ordinal, and protected technical candidates
  retain their current owning formatter. Generic noun agreement must not
  intercept or partially rewrite them.
- Dictionary and explicit caller overrides retain priority over built-in
  lexical defaults.

For a known entry, the behavior PR may change the complete candidate span, for
example `2 книги` to `две книги` or `2 book` to `two books`. For an unknown,
conflicting, or ambiguous noun, the recommended behavior is fail-closed:
preserve the complete source span and emit one honest `UnresolvedNumber`
diagnostic with UTF-8 byte coordinates. No number-only partial rewrite is
allowed.

## Language and mode ownership

- Explicit `Language::Russian` or `Language::English` has highest priority.
- `Auto` uses reliable lexical evidence; a configured fallback is considered
  only when no reliable evidence exists. Conflicting or ambiguous evidence
  preserves the candidate.
- `SegmentCandidates` uses the local lexical segment. A language or noun owner
  from another segment must not silently supply a rewrite.
- Missing or ambiguous local ownership preserves the complete candidate unless
  a separately approved explicit fallback enables it.

The same ownership contract applies to RU and EN, but the lexical and
inflection data remain language-specific. One shared morphology implementation
must not be introduced merely to reduce code duplication.

## Required behavior-PR evidence

Russian and English behavior PRs must remain separate. Each one must provide:

1. a bounded inventory with lemma/forms, source revision, and adaptation date;
2. positive and negative cases for all supported modes;
3. unknown, ambiguous, punctuation, continuation, technical-neighbor, and
   mixed-language cases;
4. exact `original_text`, `normalized_text`, `pronunciation_text`, diagnostics,
   and non-overlapping UTF-8 byte spans;
5. a behavioral commit first, followed by a separate working-oracle update;
6. no changes to published release archives.

Until this proposal is approved, the characterization inventories remain the
source of truth for current observations and no generic noun-agreement behavior
should be enabled.
