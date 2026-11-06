# Header Include Graph Inventory

This inventory records the include graph at the start of the HUBD cleanup.
Edges below mean "the left-hand domain includes the right-hand domain".

## Observed domain edges

The current source tree has these direct or transitive domain relationships:

```text
core           -> public API types
technical      -> core
normalization  -> core, technical, public API types
language/en    -> core, normalization, technical
language/ru    -> core, normalization, technical
dictionary     -> core, serialization, public API types
backend/silero -> core, serialization, optional ONNX runtime
```

The public root header `src/tts_front.hpp` currently contains the stable API
types and does not include implementation domains. That keeps optional backend
headers out of the normal consumer path. The dependency from internal support
headers to these public API types is an existing API-type seam, not a license
to make the public root include the implementation tree.

## Current include-path findings

The graph is already mostly rooted at `tts_front/...`, but several local
includes still depend on the source file's directory or include search order:

- core implementation files include bare names such as `mapped_text.hpp`,
  `source_range_index.hpp`, and `utf8.hpp`;
- Silero implementation files include bare sibling names such as
  `silero_bundle.hpp` and `silero_sentence.hpp`;
- language implementation files include bare `numbers.hpp` and
  `formatters.hpp`;
- no domain umbrella headers currently exist.

These are structural seams only. The cleanup must preserve behavior, public
API, quality oracle, snapshots, release archives, and optional backend
boundaries.

## Promised entry points after cleanup

The first cleanup phase introduces these domain facades:

```text
src/tts_front/core.hpp
src/tts_front/technical.hpp
src/tts_front/normalization.hpp
src/tts_front/language/english.hpp
src/tts_front/language/russian.hpp
src/tts_front/language.hpp
src/tts_front/serialization.hpp
```

`src/tts_front.hpp` remains the public API facade. The Silero implementation
does not receive a general umbrella because its optional ONNX owner must not
become a mandatory dependency. Silero headers remain explicitly registered
standalone/internal exceptions until a separate optional-backend facade is
designed.

## Verification points

The cleanup must compile:

1. `tts_front.hpp`;
2. each domain facade listed above;
3. the documented Silero standalone headers through their existing optional
   tests;
4. the normal Debug/Release and install-consumer targets.

No behavior or oracle assertion is part of this inventory. Any change to a
domain edge that requires a public API or optional-runtime decision must be a
separate reviewed change.
