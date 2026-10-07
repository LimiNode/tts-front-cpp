# tts-front-cpp

TTS-agnostic C++17 text frontend для подготовки текста перед синтезом речи.

Версия 0.1.0 предоставляет детерминированную нормализацию русского и английского текста, подключаемый словарь произношения, semantic stress representation и диагностические предупреждения. В executable corpus зафиксированы числа, английские порядковые числительные, grouped/negative values, проценты, десятичные дроби, время, даты/годы, базовые валюты и единицы для RU/EN, а также protected technical spans (`RTX 4090`, `CUDA 13.3`, versions, IPv4, `C++`, `C#`, `HTTP/2`). Библиотека не зависит от Python, PyTorch, Qwen или конкретного TTS runtime.

## Быстрый старт

```cpp
#include <tts_front.hpp>
tts_front::TextFrontend frontend;
tts_front::TextFrontendOptions options;
options.language = tts_front::Language::Russian;
const auto result = frontend.process("В 2026 г. API готов.", options);
```

Для смешанных фраз безопасный default — `MixedLanguagePolicy::DominantLanguage`.
Если нужно локально озвучивать явно распознанные foreign units/currency, можно
включить `MixedLanguagePolicy::SegmentCandidates`; технические идентификаторы и
остальной текст при этом остаются в языке основной фразы.

Сборка: `cmake -S . -B build && cmake --build build`, тесты: `ctest --test-dir build`.

Установка экспортирует пакет `TtsFront::tts_front`; потребитель может использовать `find_package(TtsFront CONFIG REQUIRED)`.

Automatic context-sensitive Russian stress is available through the optional, parity-verified ONNX/Silero backend when it is built
and a verified Silero bundle is configured through `TextFrontendOptions::silero_bundle_path` or
the `TTS_FRONT_SILERO_BUNDLE` environment variable. If no bundle is configured, the frontend
preserves deterministic output and emits `AutomaticStressUnavailable`; it never searches a
working-directory default, downloads a model, or accesses an external cache.

Encoding contract: all public `std::string`/`std::string_view` text is UTF-8, and project source files are UTF-8. MSVC targets compile with `/utf-8`.

`StressMode::Disabled` и `resolve_stress=false` полностью отключают semantic stress; dictionary pronunciation replacement при этом продолжает работать. `StressMode::Automatic` использует доступные dictionary decisions и optional Silero backend, а при отсутствии ONNX backend или verified bundle добавляет `AutomaticStressUnavailable`.

Намеренно не поддерживаются в v0.1.0: произвольная семантическая disambiguation дат/чисел, полноценная Unicode NFC normalization и phone/URL spoken rendering. Неоднозначные/неподдержанные случаи сохраняются или сопровождаются warning, а не угадываются молча.

Silero/ONNX является optional-компонентом: verified bundle модели не входит в поставку библиотеки и не скачивается автоматически. Приложение передаёт путь через `TextFrontendOptions::silero_bundle_path` или `TTS_FRONT_SILERO_BUNDLE`.

Исследовательские решения и provenance источников описаны в [docs/research/source-survey.md](docs/research/source-survey.md).

Silero Stress Phase 1 reference receipt: [docs/research/silero-phase1.md](docs/research/silero-phase1.md).

Порядок следующих функциональных этапов и критерии перехода зафиксированы в [docs/roadmap.md](docs/roadmap.md).

## Разработка

`clang-format` — canonical formatter для C++-кода. Локальные команды:

```text
cmake -S . -B build-format -DTTS_FRONT_CLANG_FORMAT=clang-format
cmake --build build-format --target format
cmake --build build-format --target format-check
```

Перед отправкой PR запускайте `format-check`.
