# v0.2.4 corpus mismatch ledger

The reviewed natural-sentence characterization contained one row that was
initially excluded from the working oracle. The bounded behavior fix and its
regression tests now establish the approved preservation contract:

| ID | Current observation | Approved policy | Treatment |
| --- | --- | --- | --- |
| `year-ru-04` | `Год 0999 указан неверно.` was normalized without diagnostics. | A leading-zero year is an unsupported candidate surface: preserve the atomic source span `0999` and emit one `UnresolvedNumber`. | Resolved by the bounded behavior PR; include in the working oracle with the new diagnostic contract. Release archives remain immutable. |

This was an explicit product-policy decision, not a characterization failure to
silently accept. The behavior PR defines the preservation and diagnostic
contract, and this separate oracle change records it. Published release
archives remain immutable.
