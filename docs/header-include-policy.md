# Header Include Policy

## Architecture

`tts-front-cpp` uses **Hierarchical Umbrella-Based Dependencies (HUBD)**.
The unit of dependency ownership is a domain, not necessarily an individual
header.

The domain dependency graph is directed and acyclic. A domain may depend on a
lower-level or sibling domain only when that edge is part of the documented
graph. Reverse and cyclic dependencies are forbidden.

The current domain inventory is maintained from the source tree. New domains,
subdomains, and dependency edges require an architecture review before code is
rearranged.

## Umbrella entry points

Each domain provides a domain umbrella when it has a meaningful public or
internal boundary. A subdomain umbrella is allowed when it prevents a reverse
dependency or keeps a domain facade coherent.

An umbrella is a deliberate aggregation point. It may include the supported
headers of its own domain and the umbrellas of its declared dependencies.
Transitive visibility inside that aggregation is intentional; it is not an
accidental include-order dependency.

The promised compilation points are:

- the public root header, when one exists;
- every domain or subdomain umbrella;
- explicitly registered standalone headers.

Internal leaf headers do not promise independent inclusion. They may receive
their domain dependencies transitively from the umbrella. A header that must
be independently includable—such as an optional module, plugin interface, or
standalone utility—must be listed as an exception and have its supported path
documented.

The root umbrella is optional. `src/tts_front.hpp` is the public facade for
the stable API and must not pull in optional runtime components merely to make
the internal implementation visible. Domain umbrellas remain the preferred
entry points for implementation code.

## Include paths

Project paths are canonical, rooted at the configured project include root,
and always use `/`:

```cpp
#include <tts_front/core/candidate_types.hpp>
#include "tts_front/core/text/codepoint_classification.hpp"
```

The exact delimiter follows the boundary: angle brackets are used for the
canonical public/domain boundary form, while quotes are used for local project
implementation includes. Both forms must use the canonical `tts_front/...`
path; delimiter choice does not permit a filesystem-relative path.

These forms are forbidden:

```cpp
#include "../core/candidate_types.hpp"
#include "./mapped_text.hpp"
#include "text\\codepoint_classification.hpp"
```

Standard-library and external dependency headers retain their normal
angle-bracket form. Public consumers include the installed facade as:

```cpp
#include <tts_front.hpp>
```

## Dependency visibility

Outside an umbrella, code must not rely on a declaration that is visible only
because some unrelated header happened to be included first. Within an
umbrella, deliberate aggregation is the dependency contract.

Every dependency edge must point toward an allowed lower-level or sibling
domain. A domain must not include a higher-level coordinator or its own
consumer. In particular, adding an umbrella must not make optional backends,
language implementations, or private runtime details mandatory for the public
facade.

## Verification

An include-architecture change must verify:

1. direct compilation of the public root and every promised domain umbrella;
2. direct compilation of every registered standalone exception;
3. absence of dependency cycles and forbidden reverse edges;
4. Debug/Release builds and the install-consumer smoke test;
5. unchanged behavior, API, quality oracle, and release archives unless a
   separate reviewed change explicitly says otherwise.

The migration is structural only. Include cleanup must not be used to hide a
behavioral, oracle, or release change.
