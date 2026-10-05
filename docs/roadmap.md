# Roadmap

> Provenance clarification: PR #3 contains provenance-reviewed RU/EN vectors. Each
> fixture distinguishes `adapted_vector` (an upstream behavior/test reproduced at the
> pinned revision) from `category_derived` (a local vector selected from an upstream
> normalization category or grammar). The roadmap does not claim that every fixture is
> an exact upstream line. The numbered sequence below is the default order, not a hard
> dependency graph; focused functional PRs may move an item when its contract and gate
> criteria remain explicit.

> Status: invalid-span preservation and the first source-coordinate contract are now
> implemented in the source-mapping PR. The private mapping remains intentionally
> narrow; it is not a generic transformation framework.

Этот документ фиксирует порядок развития `tts-front-cpp` после закрытия baseline
v0.1. Этапы намеренно разделены: corpus и deterministic rules не должны незаметно
превратиться в source-map или neural-runtime рефакторинг.

## Текущее состояние

- **Baseline v0.1.0** — завершён в `main`: C++17 core, RU/EN deterministic
  normalization, pronunciation dictionary, semantic stress metadata и честный
  diagnostics contract.
- **Corpus expansion + provenance** — отдельный PR #3: расширение executable corpus
  реальными RU/EN vectors с pinned source revision и датой адаптации.
- **Remaining scaffolding cleanup** — отдельный PR #4: canonical public header,
  `.temp/` workspace policy и небольшие allocation/branch cleanup.
- **Silero Stress Phases 1–3** — reference receipt, graph-level и classifier-level
  parity зафиксированы в `docs/research/`.
- **Silero production path** — завершён: verified bundle loader, optional ONNX
  session owner, private sentence orchestration, production-vs-research parity,
  benchmark gate и `StressMode::Automatic` подключены за optional build boundary.
- **Silero distribution contract** — библиотека не поставляет и не скачивает
  model bundle; приложение передаёт bundle root через options или environment.

## Порядок этапов

### 1. Corpus expansion + provenance

Сначала расширяем executable corpus, не меняя API без необходимости:

- числа, даты и годы, валюты, единицы и проценты;
- protected technical spans;
- adapted upstream vectors record project, exact revision, and adaptation date;
- category-derived fixtures record the upstream project/revision and derived status;
- mixed-language и ambiguous cases;
- живые TTS-фразы, а не только изолированные токены;
- для адаптированных upstream vectors — проект, exact revision и `adapted` date.

Переход дальше возможен только при воспроизводимых fixture-тестах и зелёном CI.

### 2. Invalid-span preservation / source mapping — implemented

Контракт и минимальная реализация теперь зафиксированы:

1. invalid/ambiguous span сохраняется через последующие regex passes;
2. warning fragments разрешаются в UTF-8 byte ranges исходного `original_text`;
3. cleanup, technical protection, grouped-number collapse и length-changing replacements
   не публикуют промежуточные координаты;
4. generated text без надёжного source fragment mapping получает `0/0`.

Это намеренно узкая implementation detail, а не полноценный generic transformation map.

### 3. Более широкая deterministic normalization

Закрывать пробелы, найденные corpus-аудитом: новые правила, protected spans,
неоднозначности и edge cases. Не угадывать молча: неподдержанные случаи должны
сохраняться или диагностироваться. Каждое новое правило получает executable
regression fixture.

### 4. Silero Stress / ONNX production boundary

Это отдельный исследовательский слой, не изменение текущего `tts_front` core.
Рабочий research contract зафиксирован в
[docs/research/silero-stress-plan.md](research/silero-stress-plan.md).
Phase 1 receipt и deterministic vectors находятся в
[docs/research/silero-phase1.md](research/silero-phase1.md) и
[docs/research/silero-phase1-vectors.jsonl](research/silero-phase1-vectors.jsonl).
Graph-level ONNX parity зафиксирована в
[docs/research/silero-phase2-onnx.md](research/silero-phase2-onnx.md).
Classifier-level accentor parity зафиксирована в
[docs/research/silero-phase3-accentor-onnx.md](research/silero-phase3-accentor-onnx.md).
Цель формулируется как **reproduce the Silero Stress inference contract in native
C++**, а ONNX остаётся промежуточным/reference этапом:

```text
exact model + weights
        -> Python/PyTorch reference
        -> deterministic vectors
        -> ONNX export + parity
        -> native C++ prototype + parity
        -> CPU/Windows benchmark
```

Исследование должно отдельно зафиксировать:

- exact model/weights revision, preprocessing и tokenization;
- vocabulary lookup, context-sensitive homograph resolver и OOV accentor;
- convention перевода model output в model-neutral `stressed_vowel`;
- PyTorch vs ONNX parity на тех же vectors;
- размер артефактов, CPU latency и Windows compatibility;
- лицензионные ограничения весов и runtime.

В core не добавлять LibTorch и не включать ONNX Runtime по умолчанию. Optional
ONNX backend подключён после parity gate, regression vectors и принятого
runtime/dependency решения. Символ `+` и другие model-specific stress маркеры
не становятся public API.

### 5. Новые языки

После deterministic semantics и corpus для конкретного языка добавить
`Language::Ukrainian`, `Language::Belarusian` и т. п. Известный, но ещё не
реализованный enum value должен завершаться явным `UnsupportedLanguage`; не добавлять
спекулятивную частичную поддержку.

### Silero Stress Phase 4 — preprocessing boundary

The first Phase 4 boundary gate is implemented in
[docs/research/silero-phase4-boundary.md](research/silero-phase4-boundary.md).
It independently reproduces tokenization and n-gram embedding and checks
classifier argmax parity on all Phase 1 vectors. Homograph resolution and
full-call parity are covered by the production sentence parity gate.

The sentence-level orchestration gate is captured in
[docs/research/silero-phase4-fullcall.md](research/silero-phase4-fullcall.md):
the production path now matches all Phase 1 final sentences exactly.

The first native preprocessing prototype is documented in
[docs/research/silero-native-prototype.md](research/silero-native-prototype.md).
It matches the n-gram embeddings and pinned homosolver WordPiece IDs; native
ONNX Runtime execution and full sentence parity are now covered by the
production parity receipt.

### 6. Post-v0.1 benchmark-driven work

Только после функционального corpus pass измерять реальные hotspots:

- dictionary lookup и case folding;
- UTF-8 validation;
- regex pipeline;
- allocations и latency на representative sentences.

`v0.1.0` targets sentence- and message-sized TTS inputs. Document-scale
throughput, allocation reduction, broader normalization corpus and additional
language support are post-release `v0.2.0` work, accepted only with benchmark
evidence and regression coverage.

## Не смешиваем этапы

- source-map архитектура не входит в cleanup или corpus-only PR;
- Silero research не меняет normalization semantics и не тащит ML runtime в core;
- новые языки не добавляются «для задела» без работающего контракта;
- performance cleanup не оправдывает speculative abstractions;
- все сборки, тесты, coverage и временные файлы выполняются под `.temp/`.

Каждый этап заканчивается отдельным focused PR, exact-head CI verification и
обновлением этого документа.
