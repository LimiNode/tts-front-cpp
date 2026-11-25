# Linear ASCII email matching

## Intent and invariant

This performance-only corrective is stacked on the GCC 14 UTF-8 email fix
(#97, `56325276`). The ASCII email pattern still backtracked from each byte
of a long digit-only local part, causing quadratic scans even when no `@`
existed. The same path is used by technical protection and pronunciation
candidate collection, so fixing a downstream numeric pass would leave the
cause in place.

The existing iterative email matcher now handles both catalog patterns. Its
domain matching is unchanged; only local-byte and start predicates differ.
Other technical patterns keep their existing regex implementation. No cache,
new dependency, public API, language rule, or email standard is introduced.

## Exact compatibility boundary

The source of truth remains the two existing fixed byte grammars:

```text
ASCII: [A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}
UTF-8: \b[A-Za-z][A-Za-z0-9.,_%+\-\x80-\xFF]*@[A-Za-z0-9.-]+\.[A-Za-z]{2,}
```

The ASCII grammar has no word-boundary requirement and allows a local part
starting with a digit, underscore, dot or sign. It does not acquire the UTF-8
grammar's comma/high-byte support. The domain remains greedy, including its
existing unusual acceptance/rejection of repeated dots and trailing bytes.
These are compatibility requirements, not claims of RFC validation.

A failed maximal local run cannot contain a later valid ASCII local start
whose `@` was missed. Successful matches resume at the exact greedy match
endpoint. Failed domains resume after `@`; a domain/local run is scanned at
most a constant number of times. Runtime is linear in source bytes and uses
constant working memory apart from returned ranges.

## Regression evidence

The committed differential matrix compares ordered byte ranges against the
unchanged regex grammars for 15,120 short inputs across both patterns. It
varies lexical/Unicode prefixes, local punctuation, repeated `@`, multi-dot
domains, invalid TLDs, combining marks and subsequent independent addresses.
An independent fixed-seed probe compared another 92,256 ordered range lists
without a difference.

The long ASCII regression covers a 100,000-digit bare local part, an invalid
`@localhost` suffix and a valid `@example.com` suffix. The valid address must
remain one exact protected range, produce no technical pronunciation
candidate and round-trip through marker protection/restoration byte-for-byte.
The existing long UTF-8 regression remains intact.

`--ascii-email-scaling` measures those three shapes at 1,000/2,000/4,000/8,000
local bytes. Each sample checks exact ranges; the median of three timed
samples feeds a fitted doubling slope. The gate rejects slopes above 3.25
and runs in the existing normalization-scaling CI job. It does not measure
the full frontend or claim to make every remaining regex linear.

Local GCC 16.1 Release measurements were 1.95x (bare local), 1.93x
(`@localhost`) and 1.86x (`@example.com`) fitted growth per doubling. At
8,000 local bytes the respective scans took 7.51/7.53/7.68 microseconds.
These timings describe this machine, not a cross-platform latency guarantee.

No working oracle, published archive, snapshot or version is changed.
