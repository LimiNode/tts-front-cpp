# English year policy behavior change

This note records the reviewed working-corpus change for the English spoken-
year policy. It applies to the post-release working corpus only; the
published `v0.2.0`, `v0.2.1`, and `v0.2.2` archives remain immutable.

## Contract

Policy B from `docs/english-year-policy-review.md` is implemented:

- `in` and `from` followed by a four-digit year are explicit year contexts;
- month-name dates use the same spoken-year formatter;
- `1000–2099` are supported year values;
- `1000–1999` use paired-century forms (`nineteen ninety nine`);
- `2000–2099` use `two thousand ...` forms;
- bare numbers remain on the cardinal path;
- `between ... and ...`, values below `1000` or above `2099`, and ordinary
  non-year numeric contexts remain cardinal until a later policy decision;
- attached continuations (`#`, `/`, `%`, signs, lexical tails) are admitted
  before formatting and remain one fail-closed candidate with one warning.

`normalized_text` and `pronunciation_text` remain identical.

## Oracle change

Before this behavior change, the working snapshot recorded two English year
mismatches:

- `en-v030-year-0-a`
- `en-v030-year-0-b`

Both expected the spoken-year form for `1999`, while the implementation used
the cardinal form in `in`/`from` contexts. The implementation now follows the
existing month-name date convention, so these two mismatch entries are removed.

Working-corpus counts change from `533/88/0/34` to `535/88/0/32` for
`correct_transformation / justified_preservation / unnecessary_refusal /
incorrect_or_partial`. No other mismatch IDs, diagnostics, or release archive
contents are changed by this policy PR.

The direct regression matrix covers `1900`, `1999`, `2000`, `2001`, and `2024`
as bare numbers, `in`/`from` contexts, month-name dates, and unsupported
attached continuations.
