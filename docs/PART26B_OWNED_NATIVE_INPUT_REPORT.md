# Part26B — точное собственное описание входной анимации

2026-09-24. Product: `32381913dc24474a78c8de4067e7b6bb28edc8f2`.
Design/plan: `edd3ab19acca47a2f23152b43f73aa2764351490`.
Статус: ограниченный этап завершён. Task review и итоговый whole-stage review
приняли изменение без Critical/Important/Minor замечаний; свежие локальные gates
пройдены. Обычный push следует за этим финальным documentation seal; точный
remote SHA проверяется после отправки, CI этим отчётом не объявляется зелёным.

## Что сделано

Частный `decodeNativeEllipseInput` теперь возвращает неизменяемое собственное
описание строго ограниченного профиля с одним эллипсом. Сохранены размеры,
частота/интервалы кадров, layer ID/перенос, размер эллипса, static/animated
position, easing, RGBA и необязательные имена/version. Все данные принадлежат
результату, не DOM или исходной строке.

Числа сохраняются в точной нормализованной десятичной форме. Например,
положительное `1e-9999` не превращается в ноль, а эквивалентные записи `60`
и `6000e-2` дают одинаковые значения. Структурные целые извлекаются без
округления. Старые whitelist, коды/пути ошибок и лимиты не изменены.

Используется тот же проверенный DOM и таблица точных чисел; второго
семантического парсера нет. Audit-only вызов не строит лишний descriptor.
Публикация происходит только после полного успешного аудита. На отказе
результат пуст; между независимыми вызовами нет общего изменяемого состояния.

Затронуты только пять файлов: новый private header, существующий admission
implementation, новый test executable, проверки parity в существующей
admission matrix и Telegram-only регистрация теста в CMake.

## Проверки

Функциональный RED: тест собрался и упал именно на
`accepted input publishes owned descriptor`; fixture admission уже прошёл.
После реализации GREEN2/2. Полная проверка исполнителя70/70 за85.59s.
Независимое task review: spec compliant / quality approved, без замечаний.
Отмеченный ревьюером внешний пункт о границах сборки проверен контроллером.

Свежие последовательные проверки контроллера на committed product3238191:

| Preset | Результат CTest | Время |
| --- | --- | --- |
| `windows-msvc-telegram-debug` | 70/70 | 82.41s |
| `windows-msvc-win32-preview` | 64/64 | 88.64s |
| `windows-msvc-direct2d` (`none`) | 31/31 | 4.86s |

Для каждого выполнены configure/build/CTest, все exit0:

```text
cmake --preset PRESET
cmake --build --preset PRESET --parallel 4
ctest --preset PRESET --output-on-failure
```

`PRESET` — каждая точная строка из таблицы; процесс предварительно настроен
существующим VS2022 BuildTools `VsDevCmd.bat -arch=x64`. Полные команды/лог
идентичности — `out/part26b/run-preset-gate.cmd` и `root-*-identity.log`.
Preview включает Direct2D capture, WARP smoke, второй graphics device и
инвалидацию/rebuild ресурсов; это не ручная проверка DPI или Qt-композиции.
В configure/build логах этих gates compiler warnings не обнаружены.

Дополнительно прошли:

```text
python scripts/verify_vendor.py --variant all
python scripts/generate_tgs_compatibility_corpus.py --check
cmake --preset windows-msvc-samsung-debug
ninja -C out/build/windows-msvc-direct2d -t commands avemotion_runtime_offline_tests
git diff --check
```

Vendor/corpus неизменны; все16 детерминированных TGS assets проверены.
Свежие compile graphs: none40 записей, ноль rlottie/private parser;
Samsung92 записи, ноль Telegram/private parser и37 ожидаемых Samsung sources.
В none Runtime compile/link commands нет reference libraries/objects/includes;
единственный текстовый rlottie token — выключающий define
`AVEMOTION_HAS_RLOTTIE=0`. Samsung CTest здесь не запускался: старые известные
Polystar scene/plan failures не устранены и Samsung не объявляется зелёным.

Тесты покрывают все поля seed literal-значениями, статический/анимированный
tag, изменённые interval/ID/translation/color/size, точные числа и границы,
перестановку ключей и absent/empty/UTF-8 имена, все прежние admission mutations,
ошибки/лимиты, JSON/TGS equality, уничтожение исходного буфера и два
параллельных независимых вызова. Это не TSan/race-detector доказательство.

Raw: `out/part26b/`; review/TDD reports:
`.superpowers/sdd/2026-09-24-owned-native-ellipse-input/`.
Две инфраструктурные ошибки проверки сохранены в ledger: shell quoting до
старта Samsung CMake и слишком широкий grep, поймавший выключающий define.
Обе установлены и исправлены в способе проверки; продукт/тесты не ослаблялись.

Whole-stage review диапазона350888f..894ffeb: Ready to merge, no findings.
Его явно отложенные области (полная Samsung runtime correctness, последующее
native correspondence/ingestion, ручные Qt/DPI/race-detector/UI-cleanup checks)
сверены с текущим scope; это не скрытые дефекты Part26B и не заявленные здесь
успешные проверки. Code после свежих gates не менялся, final seal — только docs.

## Чего этот этап не делает

Это ещё не полноценное собственное Lottie-ядро. Descriptor не является
certificate соответствия модели/слотам и не переключает playback. Runtime
по-прежнему зависит от Telegram rlottie. Parser остаётся Telegram-private;
none package всё ещё не умеет полноценную загрузку Lottie. Ни public fallback,
ни Player threading, ни Direct2D ownership, ни ANGLE/backend coverage не менялись.
Нет нового лицензионного решения, dependency install или внешнего корпуса.
Не заявляются speedup, zero-allocation, GPU/Qt или broad SVG/Lottie coverage.

Следующий самостоятельный этап: сопоставить точный descriptor с frozen model
и render-slot ownership, определить exact-to-model conversion eligibility,
проверить source identity и active/inactive intervals. Только затем — native
emission с полным scene/plan/history parity и доказательством отсутствия
per-frame reference sampling; независимый parser/package — отдельная граница.

## Координация UI и принятые решения

Этот этап не менял UI. Другая задача переносит его зависимости/сборки по
отдельному указанию пользователя. До её удаления UI/out архивированы112
диагностических файлов (1,707,923 байт), SHA256+size112/112 совпали:
`out/part26a/ui-evidence-archive-2026-09-24/manifest.json`, SHA256
`1f6c9185eabd8429f96c2fce2aa9862d174b4f148e5a74d174823c585b0aae72`.
Исторические Part26A пути в отчёте — provenance, не обещание существующей EXE.
Compiled products/dependency trees не архивировались; scope указан в README.

Новый handoff UI получен отдельно: clean HEAD712d454, `UI/out` отсутствует;
не создавать его снова. Qt теперь `UI/build/dependencies/installed`, будущие
промежуточные сборки — `UI/build/cmake` или `UI/build/diagnostics`. Основной
EXE — `UI/build/Release/AvelabsUI.exe`; SHA256
`c92f26ee8feb4ff03f6dc7f4afff4b11fb48142743d1edccb4e213aaa81f82c2`.
Контроллер подтвердил только HEAD/clean status, наличие/отсутствие путей и hash;
новые UI build/tests/GUI этой задачей не выполнялись. Reference-only Motion Lab
по-прежнему отдельная opt-in сборка, не этот основной EXE.

Контроллер по делегированным полномочиям выбрал отдельный exact descriptor
(цена: ещё один этап до correspondence), одобрил spec/plan и SDD/main
(цена ошибки: обратимая переработка), оставил private parser/playback без
переключения (цена: независимое воспроизведение ещё впереди), сохранил raw/SDD
и не возобновлял завершённую автоматизацию (цена: место на диске и отсутствие
автоматического следующего запуска). Это не утверждение, что пользователь
прочитал новые документы. Полный журнал — durable ledger этого этапа.
