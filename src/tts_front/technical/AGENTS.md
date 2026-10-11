# `technical` domain rules

- Technical protection is conservative: URLs, email addresses, known
  identifiers, versions, protocols, and protected composites remain intact.
- Build and merge protected ranges once per pipeline where possible. Restoration
  must be ordered, source-mapped, and marker-free.
- A technical match overlapping a numeric surface must not hide a numeric
  continuation or produce a partial rewrite. Resolve the overlap through the
  shared admission policy.
- Do not copy English/Russian identifier knowledge into admission code. Put
  known-identifier predicates and patterns in this domain.
- Add explicit overlap tests for ordinary identifiers, URLs, emails, and known
  technical tokens before changing restoration or admission.
