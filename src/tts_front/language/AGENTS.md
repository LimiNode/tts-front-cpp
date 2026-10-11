# `language` domain rules

Language modules own language semantics. Keep English and Russian knowledge in
their respective subdirectories and keep common code language-neutral.

- Keep one source of truth for a language's unit, currency, date, number, and
  mixed-candidate formatting. Do not maintain parallel ternary tables in the
  normalizer and mixed formatter.
- A language rule needs both a positive example and the nearest negative cases:
  malformed continuation, technical identifier, URL/email, sign, Unicode
  punctuation, and mixed-language mode where applicable.
- Date formatters must validate real calendar dates, including month lengths
  and leap years, before rewriting them.
- Grammar helpers must state their supported case, gender, numeric range, and
  noun-agreement contract. If the context does not provide enough information,
  preserve and diagnose instead of guessing.
- `Language::Auto` and `SegmentCandidates` are policy decisions. Do not infer
  that a single foreign token changes the language of surrounding text.
- Avoid language prefixes inside the matching language namespace unless they
  distinguish a genuinely cross-language concept.
