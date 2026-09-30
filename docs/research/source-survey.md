# Source survey

Этот документ фиксирует provenance правил, использованных в `tts-front-cpp` 0.1. В core не включён код или runtime из перечисленных проектов; используются только публичные поведенческие идеи и адаптированные test vectors.

| Source | Revision / license | Используем | Не переносим |
|---|---|---|---|
| [shigabeev/russian_tts_normalization](https://github.com/shigabeev/russian_tts_normalization) (`rutextnorm`) | `722d564f1ef69592c24db6da67b57d623c6e0468` (HEAD на 2026-10-01); MIT | rule-based expansion чисел, дат, единиц, аббревиатур и принцип explicit uncertainty | Python package, regex tables без аудита, неявные глобальные настройки |
| [snakers4/silero-stress](https://github.com/snakers4/silero-stress) | `d38096cae9bf3ac846bbb88705428f3ed3801b96` (HEAD на 2026-10-01); MIT | разделение vocabulary lookup и context-sensitive homograph resolver; convention test vectors | PyTorch/LibTorch, веса и знак `+` в public semantic model |
| [NVIDIA/NeMo-text-processing](https://github.com/NVIDIA/NeMo-text-processing) | `ddadfb2a38d2bc6b8cc6232c4f915eb60f500688` (HEAD на 2026-10-01); Apache-2.0 | категории EN normalization: cardinal/ordinal, currency, percent, technical tokens | Python/Pynini/OpenFST runtime и generated grammars |

## Правила provenance

Встроенные C++ правила написаны заново и не являются механической копией исходников. Повторяемые vectors хранятся в `tests/text_frontend_tests.cpp`; при добавлении corpus fixture следует указывать `category`, `source` и дату адаптации. MIT/Apache attribution сохраняется в этом файле и не меняет лицензию самого проекта.

## Текущий scope

RU и EN deterministic normalization покрывают базовые числа, проценты, время, валюты, единицы, whitespace/punctuation cleanup и технические токены. Контекстные омографы Silero и ONNX feasibility study — следующий milestone; до него библиотека не утверждает parity или production-ready automatic stress.
