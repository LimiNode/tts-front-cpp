## Intent

What user-visible behavior or maintenance goal does this PR address?

## Contract

- Owning domain:
- Behavior that must change:
- Neighboring behavior that must remain unchanged:
- Ambiguous cases and fail-closed policy:

## Change class

- [ ] behavior
- [ ] refactor
- [ ] documentation
- [ ] quality-oracle change
- [ ] release preparation

Keep these classes separate when possible. If more than one is checked, explain
why they cannot be split.

## Validation

- Focused regression:
- Adversarial neighbor matrix:
- Debug/Release build and CTest:
- Quality corpus/archive/checksum (when applicable):
- Install-consumer/header checks (when applicable):
- `git diff --check`:

## Oracle and release safety

Choose one oracle status:

- [ ] No expected output or release archive changed.
- [ ] An expected output changed; the old result, new contract, and reason are
      stated above and covered by a direct regression.

- [ ] Published release archives remain unchanged.
- [ ] The exact pushed HEAD was checked.
- [ ] This PR does not claim merge/release readiness from push CI alone.

## Architecture review

List new dependencies, moved ownership, duplicated policy removed, and any
deferred simplification or performance work.
