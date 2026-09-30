# API design

`TextFrontend` не хранит mutable global state и может безопасно переиспользоваться из нескольких потоков. Все настройки передаются через `TextFrontendOptions`; словарь передаётся как const pointer и может быть общим неизменяемым объектом.

`TextFrontendResult` сохраняет оригинальный и нормализованный текст, rendered pronunciation text, semantic `WordPronunciation`, dictionary replacements и warnings. Ударение — это optional индекс Unicode-символа в слове, а не знак конкретной модели.

Файл словаря принимает dependency-free JSON-массив:

```json
[{"pattern":"Qwen","pronunciation":"квен","match":"case_insensitive"},
 {"pattern":"замок","pronunciation":"замок","stressed_vowel":1}]
```

Те же записи можно добавлять через `add_token`, `add_case_insensitive_token` и `add_phrase`.

Pipeline выполняется один раз в фиксированном порядке: UTF-8/whitespace cleanup → language normalization → dictionary → semantic stress diagnostics. В core нет сетевых вызовов, Python runtime или зависимости от TTS engine.
