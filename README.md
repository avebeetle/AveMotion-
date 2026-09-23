# AveMotion — Part 24 private corpus laboratory

AveMotion is a standalone embeddable C++ vector-animation SDK developed from a
pinned Telegram `rlottie` semantic oracle while progressively replacing the
runtime with AveMotion-owned canonical data, evaluation, geometry, scheduling
and Direct2D rendering.

For ordinary lean internal builds and the no-reference installed package, see
[Module builds and offline package](docs/MODULE_BUILD.md). The Telegram-backed
module remains a build-tree integration; the offline package cannot load
animations through Runtime.
Normal CI also checks the lean builds and offline package. Samsung remains a
retained, manually dispatched comparison workflow; see the vendor integrity
section of the module guide for verification scopes and commands.

Part 24 adds a privacy-preserving laboratory for representative `.tgs` and
Lottie JSON collections. It deliberately adds no new visual family. The goal is
to choose the next implementation from measured feature frequency, native
coverage, timing and memory evidence rather than intuition.

## Current pipeline

```text
Lottie JSON or Telegram TGS
    -> hardened bounded TGS transport
    -> pinned Telegram parser/model
    -> immutable AveMotion canonical tables
    -> AssetValidator
    -> MotionInstance / centralized Player
    -> AveMotion evaluator and geometry subset
    -> MotionRenderPlan
    -> production Direct2D backend
```

## Private corpus laboratory

Directory:

```bash
python scripts/run_part24_corpus_lab.py \
  --input-dir C:/stickers \
  --output C:/reports/avemotion-corpus
```

ZIP with safe temporary extraction:

```bash
python scripts/run_part24_corpus_lab.py \
  --input-zip C:/stickers.zip \
  --output C:/reports/avemotion-corpus
```

The default privacy mode writes content-derived aliases and never copies source
assets or paths. Reports include:

```text
corpus_manifest.tsv
corpus_issues.tsv
corpus_features.tsv
corpus_benchmarks.tsv
corpus_decision.tsv
corpus_summary.txt
```

The committed 16-asset deterministic TGS corpus is used as a smoke test. Its
feature ranking is explicitly preliminary until a representative private corpus
is analyzed.

## Validation API

```cmake
target_link_libraries(my_tool PRIVATE AveMotion::Validation)
```

```cpp
#include <avemotion/validation/AssetValidator.hpp>

avemotion::validation::AssetValidator validator;
auto report = validator.validate(
    model,
    context,
    avemotion::validation::AssetValidationOptions::applicationAsset());
```

Profiles:

```text
ApplicationAsset  structurally valid application assets; report fallback
Direct2DNative    current fully native Direct2D subset only
TelegramSticker  TGS + Telegram authoring-profile constraints
```

## Windows validation

Part 20.4 was confirmed on a real Visual Studio 2022/MSVC/WARP/Direct2D setup,
including the 75-case CPU-reference capture corpus and interactive preview.
Part 24's lab itself is backend-neutral but builds against the Telegram
reference path for parsing and complete CPU comparison.

## Linux validation

```bash
cmake --preset linux-clang-telegram-debug
cmake --build --preset linux-clang-telegram-debug --parallel 4
ctest --preset linux-clang-telegram-debug --output-on-failure
```

Installable offline SDK:

```bash
cmake --preset linux-clang-offline
cmake --build --preset linux-clang-offline
ctest --preset linux-clang-offline --output-on-failure
cmake --install out/build/linux-clang-offline --prefix out/install
```

## Documentation

- `docs/CORPUS_LAB.md`
- `docs/ASSET_VALIDATION.md`
- `docs/TGS_LOADING.md`
- `docs/PLAYER_SCHEDULER.md`
- `docs/WIN32_PREVIEW.md`
- `docs/PART24_REPORT.md`
- [Part 25A evidence and limitations](docs/PART25A_PERSISTENT_SESSIONS_REPORT.md)
- [Reference-session correctness findings](docs/PART25A_REFERENCE_SESSION_FINDINGS.md)
- [Part 25B mapping-tree optimization and active-state evidence](docs/PART25B_REFERENCE_LIFECYCLE_REPORT.md)
- `docs/NEXT_STAGE.md`

Development rule:

```text
characterize -> isolate -> implement -> compare -> preserve oracle
```
