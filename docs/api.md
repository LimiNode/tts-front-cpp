# API design

`TextFrontend` не хранит mutable global state и может безопасно переиспользоваться из нескольких потоков. Все настройки передаются через `TextFrontendOptions`; словарь передаётся как const pointer и может быть общим неизменяемым объектом.

`TextFrontendResult` сохраняет оригинальный и нормализованный текст, rendered pronunciation text, semantic `WordPronunciation`, dictionary replacements и warnings. `stressed_vowel` — это zero-based ordinal гласной в произношении (`замок` со значением `1` означает вторую гласную), никогда не UTF-8 byte/code-point offset. `WordPronunciation::source_offset` связывает слово с byte offset в `normalized_text`.

Файл словаря принимает dependency-free JSON-массив:

```json
[{"pattern":"Qwen","pronunciation":"квен","match":"case_insensitive"},
 {"pattern":"замок","pronunciation":"замок","stressed_vowel":1}]
```

Те же записи можно добавлять через `add_token`, `add_case_insensitive_token` и `add_phrase`.

Pipeline выполняется один раз в фиксированном порядке: валидатор UTF-8 → optional whitespace/punctuation cleanup → language normalization с protected technical spans → dictionary → semantic stress diagnostics. При `cleanup_unicode=false` stage cleanup действительно пропускается; Unicode NFC normalization намеренно не заявляется. В core нет сетевых вызовов, Python runtime или зависимости от TTS engine.

Для `Language::Auto` наличие кириллицы выбирает Russian, отсутствие кириллицы — English; mixed Cyrillic/Latin получает `AmbiguousNormalization` и обрабатывается как Russian. Emoji и типографская пунктуация сами по себе язык не переключают.

Phrase entries — это exact token sequences: разделители между токенами могут отличаться, выбирается longest match, duplicate `(pattern, match)` отклоняется. Dictionary JSON загружается fail-closed: синтаксическая ошибка, duplicate keys, invalid UTF-8 или неверная schema не меняют уже загруженные записи. Programmatic `add_*` возвращают `bool` и не добавляют invalid/duplicate entry.
