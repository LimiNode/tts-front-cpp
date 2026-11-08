# Header Include Graph Inventory

This inventory records the include graph after the first HUBD umbrella phase.
Edges below mean "the left-hand domain includes the right-hand domain".
It is a structural inventory, not a promise that every leaf header is a
standalone public entry point.

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

The source tree now uses canonical project paths for every internal include:

- project headers use `tts_front/...` or the public `tts_front.hpp` facade;
- no `../`, `./`, backslash, or bare project-header include remains;
- umbrella headers use deliberate aggregation for their documented domain
  dependencies;
- optional Silero implementation headers remain explicit internal exceptions,
  so the public facade does not pull in ONNX or backend details.

This was checked against tracked source files with `git ls-files`, the explicit
CMake source lists, and a recursive include-path scan. The cleanup preserved
behavior, public API, quality oracle, snapshots, release archives, and optional
backend boundaries.

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

## Workspace cleanup result

The historical paths `include/` and `src/detail/` are absent from the current
tree and have zero tracked files. There are no tracked `external/` or build
directories either; generated build trees and temporary probes remain confined
to the allowed `.build/` and `.temp/` workspaces. No deletion was required in
this pass.

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
