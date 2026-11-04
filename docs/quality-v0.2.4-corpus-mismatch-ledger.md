# v0.2.4 corpus mismatch ledger

The reviewed natural-sentence characterization contains one row that is not
eligible for the working oracle expansion:

| ID | Current observation | Approved policy | Treatment |
| --- | --- | --- | --- |
| `year-ru-04` | `Год 0999 указан неверно.` is normalized to `Год девятьсот девяносто девять указан неверно.` with no diagnostics. | A leading-zero year is an unsupported candidate surface: preserve the atomic source span `0999` and emit one `UnresolvedNumber`. | Keep as `policy_mismatch`; exclude from the working oracle and from all release archives. |

This is an explicit product-policy decision, not a characterization failure to be
silently accepted. A future behavior PR must define the preservation and
diagnostic contract, add a focused regression test, and update the working
oracle in a separate change. Published release archives remain immutable.
