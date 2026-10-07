# Changelog

## 0.2.0

- Добавлена opt-in политика `MixedLanguagePolicy::SegmentCandidates` для локальной нормализации поддержанных числовых, валютных и единичных конструкций в смешанном тексте.
- Технические диапазоны строятся единым индексом, а восстановление вложенных technical markers выполняется одним sweep без повторной пересборки всего текста.
- Укреплён fail-closed admission: malformed foreign candidates сохраняются целиком с единым `UnresolvedNumber`, включая Unicode punctuation, signed currency и attached lexical suffixes.
- Проектная версия и CMake package version обновлены до `0.2.0`.

## 0.1.0

First stable release of `tts-front-cpp`.

- deterministic Russian and English normalization, including dates, times,
  decimals, currencies, units, grouped numbers and protected technical spans;
- fail-closed numeric and technical admission with UTF-8-aware boundaries;
- UTF-8 source-mapped diagnostics and preservation of unsupported input;
- pronunciation dictionary, phrase matching and explicit rewrite precedence;
- semantic stress representation with conservative Russian initialism support;
- optional, parity-verified Silero automatic stress backend through ONNX Runtime;
- verified bundle provenance/hash validation and persistent private sessions;
- CMake install/export package and install-consumer support;
- cross-platform GCC, Clang and MSVC CI coverage.

The Silero/ONNX backend is optional. The verified model bundle is not included
in this release and is not downloaded by the library; applications configure its
root through `TextFrontendOptions::silero_bundle_path` or
`TTS_FRONT_SILERO_BUNDLE`.

Document-scale throughput, allocation reduction, broader normalization and
additional languages remain planned post-v0.1 work.
