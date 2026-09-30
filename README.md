# tts-front-cpp

TTS-agnostic C++17 text frontend для подготовки текста перед синтезом речи.

Версия 0.1 предоставляет детерминированную нормализацию русского и английского текста, подключаемый словарь произношения, semantic stress representation и диагностические предупреждения. В executable corpus зафиксированы числа, grouped/negative values, проценты, десятичные дроби, время, даты/годы, базовые валюты и единицы, а также protected technical spans (`RTX 4090`, `CUDA 13.3`, versions, IPv4, `C++`, `C#`, `HTTP/2`). Библиотека не зависит от Python, PyTorch, Qwen или конкретного TTS runtime.

## Быстрый старт

```cpp
#include <tts_front/tts_front.hpp>
tts_front::TextFrontend frontend;
tts_front::TextFrontendOptions options;
options.language = tts_front::Language::Russian;
const auto result = frontend.process("В 2026 г. API готов.", options);
```

Сборка: `cmake -S . -B build && cmake --build build`, тесты: `ctest --test-dir build`.

Установка экспортирует пакет `TtsFront::tts_front`; потребитель может использовать `find_package(TtsFront CONFIG REQUIRED)`.

Automatic context-sensitive Russian stress намеренно не включён: `StressMode::Automatic` выдаёт явное предупреждение до parity-проверки native/ONNX backend. Это не блокирует normalization и dictionary-only stress.

Намеренно не поддерживаются в v0.1: произвольная семантическая disambiguation дат/чисел, полноценная Unicode NFC normalization, phone/URL spoken rendering и neural context-sensitive stress. Неоднозначные/неподдержанные случаи сохраняются или сопровождаются warning, а не угадываются молча.

Исследовательские решения и provenance источников описаны в [docs/research/source-survey.md](docs/research/source-survey.md).
