# AveMotion Reference Laboratory — отчёт по Part 2

Дата фиксации: 25 июля 2026 года.

## 1. Результат этапа

Part 2 превратил bootstrap из первой части в реально работающую лабораторию с
двумя полными исходными деревьями `rlottie`:

- **TelegramMessenger/rlottie** — основной поведенческий baseline, от которого
  начинается постепенное выделение AveMotion;
- **Samsung/rlottie** — более поздняя сравнительная линия и источник
  hardening/modernization-решений.

Оба варианта собираются одним AveMotion CMake-проектом, доступны через один
AveMotion-owned facade, проходят одинаковый test corpus и генерируют
воспроизводимые manifests и изображения кадров.

На этом этапе внутренние классы `rlottie` намеренно не переименованы и не
переписаны. Сначала сформирована защитная характеристическая граница, после
которой подсистемы можно отделять по одной.

## 2. Зафиксированные исходники

### Telegram — основной baseline

| Поле | Значение |
|---|---|
| Repository | `https://github.com/TelegramMessenger/rlottie` |
| Commit | `67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d` |
| Исходный архив | `rlottie-master.zip` |
| SHA-256 архива | `c60add305b5c4251bc031941de917579b9517de39b6c864f2ad57f4f3e15b6d0` |
| Post-patch tree fingerprint | `770a7457b295c5f873e45a318f655f277410efb46626b098ba6e5738ba1669a9` |
| Файлов / байт | `276 / 13,263,902` |
| Роль | основной поведенческий baseline и первая линия extraction |

Из исходного ZIP при нормализации дерева исключён только случайно попавший Vim swap-файл `src/binding/.lottieplayer.cpp.swp`. Он не является исходным кодом; исключение явно записано в `UPSTREAM.json`, тогда как SHA-256 исходного архива сохранён без изменений.

В Telegram tree внесён один cumulative build-only patch:

1. удалён глобальный `-Werror`, превращавший новые предупреждения GCC/Clang в
   ошибки внутри старого vendored-кода;
2. в `src/vector/vrle.cpp` явно добавлен отсутствующий `<limits>`;
3. backport'нуты MSVC compile/link/install guards из
   `desktop-app/rlottie@b9d5f532cdaf97b77dc3fe5b50317493b7da0864`;
4. Unix-only visibility flag optional image-loader'а закрыт от MSVC.

Точный cumulative diff хранится в
`patches/telegram/0001-modern-toolchain-build-only.patch`.

Ни evaluator, ни geometry, ни rasterization/compositing намеренно не менялись.

### Samsung — comparison/hardening baseline

| Поле | Значение |
|---|---|
| Repository | `https://github.com/Samsung/rlottie` |
| Commit | `2cab35db755b0e39df40b679969495e90d39c578` |
| Исходный архив | `rlottie-master_samsung.zip` |
| SHA-256 архива | `1b8b8d5cd69e9da43512f21f922463db049adb502c519033f8f1d3fb4ff07570` |
| Post-patch tree fingerprint | `f710dd5f7fcbe789d88438d8d0a876d7bdf0bc55831548286d064cb3e7f18b84` |
| Файлов / байт | `322 / 12,189,463` |
| Роль | comparison, security hardening и modernization donor |

Samsung source содержит один узкий hardening patch, сохранённый в
`patches/samsung/0001-fix-arena-null-pointer-arithmetic.patch`. Он:

1. убирает `nullptr + offset` до первого heap allocation;
2. не использует поля allocator после явного завершения его lifetime в `reset()`;
3. проходит полный AddressSanitizer corpus;
4. даёт 40/40 pixel- и metadata-identical samples относительно оригинального
   Samsung source.

Первоначальные `invalid free` при эксперименте оказались связаны не с самим
arena patch, а с upstream ELF version-script, ошибочно перенесённым из static
library в sanitizer executables. После исправления CMake static-link boundary
patch повторно прошёл ASan и corpus parity.

Наблюдательный UBSan-профиль после patch всё ещё показывает signed overflow в
FreeType-derived raster boundary arithmetic. Полная трассировка и политика
описаны в `docs/known-issues/SAMSUNG_UBSAN_RASTER.md`.

## 3. Новая CMake-архитектура лаборатории

Проект поддерживает варианты:

```text
AVEMOTION_RLOTTIE_VARIANT=telegram   # default
AVEMOTION_RLOTTIE_VARIANT=samsung
AVEMOTION_RLOTTIE_VARIANT=none       # offline/installable facade
```

Основные targets:

```text
avemotion_reference
avemotion_probe
avemotion_characterize
avemotion_reference_tests
avemotion_manifest_tests
avemotion_offline_tests
```

Upstream загружается через `add_subdirectory`, но его include paths и types не
проникают в публичные AveMotion headers.

### Статическая линковка и ELF version script

Обе upstream CMake-конфигурации добавляют ELF `--version-script`, предназначенный
для shared library. При принудительной статической сборке CMake переносил этот
link item в финальные test executables. Это ломало symbol interposition
AddressSanitizer и приводило к ложным/искажённым bad-free симптомам даже в
минимальном `std::filesystem` коде.

`cmake/AveMotionRlottie.cmake` теперь удаляет **только** propagated
`version-script` из static consumer interface. Upstream target и его runtime
поведение в остальном не изменяются. После этого ASan корректно перехватывает
allocation API и весь Samsung corpus проходит.

## 4. AveMotion-owned reference facade

Внешний код лаборатории работает не с `rlottie::Animation` напрямую, а через:

```cpp
avemotion::reference::ReferenceRuntime
avemotion::reference::ReferenceAnimation
avemotion::reference::AnimationMetadata
avemotion::reference::RenderedFrame
```

Facade умеет:

- загрузить Lottie JSON из памяти;
- получить width, height, frame rate, duration и total frames;
- преобразовать normalized position в frame index;
- синхронно отрендерить выбранный кадр;
- вернуть ARGB32 premultiplied surface;
- вычислить FNV-1a 64 hash полного кадра;
- вычислить alpha sum, число непрозрачных пикселей и bounds содержимого.

`rlottie.h` является private dependency. Это первая реальная граница, через
которую позже можно заменить loader/model/evaluator/renderer по частям.

## 5. Characterization corpus

В `tests/corpus/` закреплено восемь assets:

1. `1667-firework.json`;
2. `ModernPictogramsForLottie_LoudMute.json`;
3. `StickAndBall.json`;
4. `dynamic_path_test.json`;
5. `gradient_animated_background.json`;
6. `mask.json`;
7. `matte_two_item_with_lowerlayer.json`;
8. `polystar_line_clockwise_trim.json`.

Для каждого файла хранятся SHA-256 значения. `verify_vendor.py` проверяет corpus
и оба source trees перед тестами.

Characterizer рендерит пять точек:

```text
p000
p025
p050
p075
p100
```

Для каждой точки записываются:

- фактический frame index;
- metadata asset;
- frame hash;
- alpha sum;
- non-transparent pixel count;
- non-transparent bounds;
- PPM reference frame.

Матрица из двух variants и двух compilers создаёт 160 зафиксированных кадров.

## 6. Тесты

### Reference tests

Проверяются:

- отказ на malformed/empty JSON object;
- корректная metadata каждого corpus asset;
- рендер первого, среднего и последнего кадров;
- repeated same-frame determinism;
- random seek назад к frame 0;
- равенство двух независимых instances;
- параллельные независимые load/render operations;
- размер и целостность surface.

### Manifest/provenance tests

Проверяются:

- выбранный variant;
- repository URL;
- полный commit SHA;
- соответствие `UPSTREAM.json` compile-time pin.

### Characterizer smoke

CTest реально запускает renderer на `StickAndBall.json` и создаёт manifest/frame,
то есть конфигурация не может пройти только за счёт успешной линковки.

### Offline facade

В режиме `variant=none` проверяется:

- отсутствие скрытой зависимости от `rlottie`;
- корректное объяснение недоступности renderer;
- install/export CMake package.

## 7. Проверенная build matrix

Среда:

```text
CMake 3.31.6
Ninja 1.12.1
Clang 17.0.0
GCC 14.2.0
Python 3.13.5
Linux x86-64
```

| Конфигурация | Configure | Build | CTest |
|---|---:|---:|---:|
| Telegram + Clang Debug | PASS | PASS | 4/4 PASS |
| Telegram + GCC Release | PASS | PASS | 4/4 PASS |
| Samsung + Clang Debug | PASS | PASS | 4/4 PASS |
| Samsung + GCC Release | PASS | PASS | 4/4 PASS |
| Telegram + Clang ASan | PASS | PASS | 4/4 PASS |
| Samsung + Clang ASan | PASS | PASS | 4/4 PASS |
| Offline facade + Clang | PASS | PASS | 2/2 PASS |
| Offline install/export | — | PASS | external consumer PASS |

Windows/MSVC и Direct2D в текущем Linux-контейнере не запускались. Для них
подготовлены presets и GitHub Actions matrix с MSVC developer environment.

## 8. Детерминизм compilers

Внутри каждого variant Clang и GCC дали полностью одинаковые manifests:

| Variant | Samples | Pixel-identical | Metadata differences |
|---|---:|---:|---:|
| Telegram Clang vs GCC | 40 | 40 | 0 |
| Samsung Clang vs GCC | 40 | 40 | 0 |

Это важный baseline: выбранные corpus outputs не зависят от этих двух Linux
compiler configurations.

## 9. Telegram и Samsung не являются drop-in-equivalent

Сравнение variants дало:

```text
40 normalized samples
10 pixel-identical
30 pixel-different
8/8 assets с metadata differences
0 missing samples
```

Во всех восьми assets Samsung сообщает на один total frame больше, а duration
соответственно длиннее на один frame interval. Из-за этого часть normalized
samples попадает на соседние frame indices. Однако существуют различия и на
`p000`/одинаковых индексах, поэтому расхождение не сводится только к endpoint
mapping.

Полные таблицы находятся в:

- `docs/characterization/comparison-clang.md`;
- `docs/characterization/comparison-gcc.md`.

Вывод: Samsung нельзя автоматически объявить «исправленной Telegram-версией».
Для каждого будущего semantic subsystem нужно отдельно решить, какое поведение
считается целевым AveMotion contract.

## 10. Characterization artifacts

В архив включены:

- configure/build/CTest logs для основной матрицы;
- ASan logs for both lineages and non-gating Samsung UBSan observation logs;
- manifests TSV;
- PPM frames;
- comparison reports;
- compiler parity reports;
- contact sheets Telegram и Samsung.

Build directories и object files в пользовательский архив не входят.

## 11. Лицензирование

Telegram core — LGPL-2.1-or-later с отдельно лицензированными bundled
компонентами. Samsung snapshot идентифицирует core как MIT и также содержит
отдельные licenses.

Part 2 не выбирает окончательную лицензию нового AveMotion-owned кода. До
публичного распространения требуются:

- file-level license audit;
- решение, остаётся ли AveMotion derivative fork или заменяет upstream код;
- сохранение всех notices;
- проверка требований LGPL при static/dynamic distribution;
- отдельное решение о C/C++ ABI и packaging.

## 12. Что сознательно не сделано

- массовое переименование upstream файлов и классов;
- изменение Lottie semantics;
- canonical AveMotion model;
- отделение evaluator от render tree;
- Direct2D backend;
- production player/scheduler;
- `.avm` compiler;
- public stable ABI.

Это сознательная защита от ситуации, когда после большого rename/rewrite уже
невозможно определить источник pixel regression.

## 13. Следующий этап

Part 3 должен работать поверх Telegram baseline и постепенно ввести seams:

```text
upstream JSON loader/model
        ↓
AveMotion Asset facade
        ↓
AveMotion Instance facade
        ↓
exact evaluated scene / recording backend
        ├── existing CPU pixel oracle
        └── future Direct2D backend
```

Первая semantic replacement допускается только после появления целевого golden
набора для соответствующей функции.

Подробный scope находится в `docs/NEXT_STAGE.md`.
