# Source mapping audit

This note records the deliberately small mapping implementation used by the
invalid-span/source-coordinate stage.

## Coordinate contract

`TextWarning::offset` and `TextWarning::length` are UTF-8 byte coordinates into
`TextFrontendResult::original_text`. They never refer to cleanup output, protected
markers, grouped-number intermediates, or `normalized_text`.

`length == 0` means that a diagnostic is global and has no concrete source span.
Whole-input invalid UTF-8 keeps the known range `0..original_text.size()`.

## Pipeline audit

| Stage | Length-changing? | Provenance handling |
|---|---:|---|
| UTF-8 validation | no | invalid input is directly the whole original byte range |
| spacing cleanup | yes | warning fragments are resolved against the original bytes |
| language detection | no | mixed-language diagnostic is explicitly whole-input |
| technical protection | yes internally | protected spans are restored; warnings resolve against original bytes |
| grouped-number collapse | yes | invalid fragments remain source-identical and are located in original bytes |
| normalization replacements | yes | warnings are emitted only for unchanged source fragments |
| dictionary/stress metadata | no warning source mapping | global diagnostics remain no-range |

The private `MappedText` representation stores sorted output runs. Directly copied
runs retain exact byte correspondence; generated replacements retain the complete
source interval they consumed. Cleanup whitespace, grouped-number collapse, technical
markers, and regex replacements all create or copy runs rather than searching the
original text. `WarningSink` receives the current match range and converts it through
those runs. Invalid warning ranges are also registered as preserved spans, so later
regex passes skip them instead of relying on incidental regex exclusions.

This is sufficient for the current fixed pipeline without introducing a generic rope,
transformation graph, byte-per-byte public map, or serialization API.

## Regression coverage

The test matrix covers warnings after:

- length-changing percent expansion;
- protected technical spans;
- whitespace cleanup;
- grouped-number collapse;
- a non-ASCII UTF-8 prefix (byte, not character, offset);
- multiple warnings and a warning at byte offset zero;
- mixed-language whole-input diagnostics coexisting with a span warning.

If a future normalizer emits a warning for generated text rather than an unchanged
source fragment, it must add an explicit provenance rule or remain a no-range
diagnostic. Intermediate `std::smatch::position()` values must never be published as
public source coordinates.
