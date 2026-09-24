# Part26A — экспериментальный Qt Motion Lab

Дата: 2026-09-24. Ограниченный Part26A принят: локальные gates, измерения,
whole-stage review и один scoped re-review финального исправления завершены.
Открытых Critical/Important замечаний нет. Это внутренний эксперимент,
не выпуск и не разрешение на распространение reference-linked приложения.

## Результат и границы

AveMotion подключён статически через build-tree к отдельной экспериментальной
сборке Avelabs-UI. Motion Lab находится внутри существующей страницы Voices,
не меняет маршрутизацию и сохраняет прежнее содержимое. Сборочная опция
`AVELABS_ENABLE_MOTION_LAB` по умолчанию OFF; при ON панель создаётся только
с аргументом `--motion-lab`. Обычная сборка не получает motion compile/link input.
Принятый каталог `build/Release` не заменён, новая DLL не поставляется.

Маршрут явно подписан `Experimental — Telegram reference CPU`: Telegram rlottie
готовит растровые кадры, QWidget показывает принадлежащие Qt QImage. Это не
самостоятельное полное Lottie/TGS-ядро, не новый SVG/SMIL renderer и не GPU/ANGLE.
QWidget автоматически не становится Direct2D target. Эти следующие задачи
требуют собственного дизайна и приёмки; текущий этап их не реализует.

Есть загрузка JSON/TGS, Play/Pause/Stop, нормализованный seek, 1/4/16 экземпляров,
ошибки и диагностика. Один serial worker владеет Runtime/Player/Instances и
таймером; GUI получает независимые изображения. Latest-value mailboxes ограничивают
очереди кадров, команд и диагностики. Поколения фильтруют старые результаты;
неудачная замена сохраняет предыдущую сессию. Скрытие использует Freeze, показ
не отменяет пользовательскую паузу. Закрытие отменяет работу между render calls
и безопасно присоединяет свой поток, но не прерывает чужой CPU render посередине.

Лимиты: чтение до 2 MiB+1, TGS 64 KiB/JSON 2 MiB/ratio128, положительные конечные
timing metadata, canvas до4096 и frames до18000; физический край вывода до1024,
суммарно до4,194,304 пикселей. Это ограничения эксперимента, не sandbox или
гарантия безопасности произвольного враждебного файла.

## Коммиты и происхождение

U = `D:/rvc/c++/DragonianVoice/Avelabs-UI`.
A = `C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24`.
База U: `32d77c3715f8d084e5eb9d8abc6017ad84c23420` после согласованной передачи
владения предыдущей UI-задачей. Работа выполнена прямо в main по просьбе пользователя.

| Изменение U | Коммиты |
| --- | --- |
| Статическое opt-in подключение и запрет install | `d0331a7`, `c6330e6`, `7084563` |
| Worker/controller, очереди, владение и исправления по ревью | `b92c3bc`, `ad608bf` |
| Панель Voices, единый DPR-расчёт, синхронизация теста паузы | `98841e0`, `115f70b`, `3b47426` |
| Восстановление таймера после раннего one-shot timeout | `fde867d` |
| Keyboard/wheel seek, release-only drag и защита от feedback | `7093c83` |

Все перечисленные product commits independently reviewed и отправлены обычным push.
Финальный product HEAD U: `7093c839b1a165ecb579b5264fb4481b670e3cea`;
итоговый docs/handoff HEAD U: `6568155f0337bcebe2d2ba5cfa4cb2c99e483bbd`.
Обычный push и точное равенство remote main подтверждены; tree чистый.
Предыдущие U docs: `035d4ff` и `9cb73f0`.
Точный U SHA при измерениях: `fde867d20d0014373235ea26506044efaabb6ea5`.
Engine/docs SHA при измерениях: `97fe85165a27e7bd8810a08ae961074e5fe6a296`.
Оба checkout были чистыми. В Part26A нет изменений engine/vendor, goldens,
Player threading, Direct2D ownership, public fallback policy или лицензий.
Последующие documentation commits не меняют происхождение измеренного EXE.

Экспериментальный статический host:

- `U/out/diagnostics/motionlab-2026-09-24/build/Release/AvelabsUI.exe`;
- 52,683,264 bytes; SHA256 `481cad66bd5fb7520122abd0e9d4d1ce6922e3fe21ae74b8c3645de3ee2723c3`;
- embedded identity `git=7093c839b1a165ecb579b5264fb4481b670e3cea state=clean config=Release Qt=6.10.0 compiler=19.44.35229.0`;
  U был чист при relink; A оставался `97fe851` с pending docs/report/STATE/ledger/plan,
  но без изменений компилируемого engine/vendor. Это не утверждение о чистом A;
- Qt6.10.0 из существующего `U/out/ftfix/installed/x64-windows-static-release`,
  MSVC19.44.35229.0, C++20, Release, static CRT `/MT`;
- CMake/vcpkg manifest mode/install OFF, новые зависимости не устанавливались;
- полный EXE не запускался: нет безопасного data-root для обычных настроек.
  Изолированный shell QtTest использует временные INI user/system scopes.

Старый pre-scheduler EXE SHA `8f891157...` не является результатом `fde867d`.
Post-scheduler/pre-widget EXE `4fe39d5defeda7092d2d636e10feb39f3a48fc5507dd4130386bf87f4b34366a`
сохранён как историческая идентичность, не подменяет финальный `481cad66...`.
В новом experimental build-tree нет `cmake_install.cmake`; reference-linked
установка запрещена на уровне всего дерева, включая vendor/component/subdir.
Старые RED-деревья с install scripts сохраняются только как evidence, не для install.

## Локальные проверки

| Проверка | Подтверждённый результат |
| --- | --- |
| Build boundary: OFF/ON, cache, CRT, stale tree, отрицательные install | 23 шага PASS |
| Отдельная post-fix `/MT` host сборка и engine smoke | build exit0; CTest1/1 |
| Final-widget-fix `/MD` Motion Lab QtTest | CTest3/3; worker31/31, page11/11, smoke |
| Fresh shell regression | CTest3/3 |
| Fresh tray regression | CTest2/2 |
| Fresh settings regression | CTest1/1 |
| Fresh logging regression | CTest2/2 |
| Измеритель: classifier/runner/provenance negative paths | 5 + 10 + 4 отказа, positive controls PASS |
| Итоговая последовательная матрица | 9/9 accepted, команда exit0 |

У указанных QtTest/CTest нет failures/skips. Worker/page числа включают lifecycle
entries QtTest, а не только пользовательские сценарии. Старые shell suites
прошли на `3b47426`; после adapter-only timer fix повторены затронутые lab gates,
а не неизменённые shell suites. Engine/D2D/WARP заново не запускались: их код
в этом этапе не менялся, и CPU-тесты не заменяют graphics gates.

Ключевые команды из U (полный configure с существующими путями — в
[плане](superpowers/plans/2026-09-24-qt-motion-lab.md) и U lab REPORT):

```text
cmake --build out/diagnostics/motionlab-2026-09-24/build --config Release --target AvelabsUI --parallel 4
ctest --test-dir out/diagnostics/motionlab-2026-09-24/build -C Release --output-on-failure --no-tests=error --parallel 1
ctest --test-dir out/diagnostics/motionlab-2026-09-24/tests-md -C Release -V
python -B tests/motionlab/test_build_boundary.py --output-root C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part26a/task4-boundary
```

Root independently ran final-widget-fix lab CTest3/3 (6.50s) и static smoke1/1
(0.03s), оба exit0, с process-local Qt environment, PTY, `--parallel 1 -V`.
Raw: `A/out/part26a/final-fix/root-final-md-ctest.log` и
`root-final-static-ctest.log`. Предыдущий scheduler gate3/3 (5.63s) остаётся в
`A/out/part26a/task4-scheduler-controller-ctest.log`. Остальные полные команды,
exit codes, RED/GREEN и независимые verdicts находятся в retained
`A/.superpowers/sdd/2026-09-24-qt-motion-lab/`; raw сборки/CTest — в
`A/out/part26a/task4-*.log`. Существуют configure/vendor/original-UI warnings
(включая skip-install, unused overlays, D9025/C4244/C4267/C4530/C4251/C4100/C4505);
сборка не объявляется warning-free, предупреждения не подавлялись для отчёта.

## Измерения: только отдельный CPU/controller harness

Это QCoreApplication без QWidget/painting, dynamic Qt6.10.0 и собственная `/MD`
сборка engine. Не смешивать её показатели со статическим host EXE, Direct2D или
ранними exact-scene benchmarks. Windows10 build19045, 12 logical processors.
Инструмент SHA256: `615c0b2d2cbe99248ad6c5b4970abe8feeccd85e90ad822b4f3608e320eae971`.
Manifest `out/part26a/task4-measure/provenance-audit3.json` привязывает исходники,
runner, EXE, cache/project, CRT и чистые repository SHAs; его SHA256
`6338761f5b0691314a3b99a353d2af8f9e1de8b667152b745f0fa18b8cab27d9`.
Матрица остаётся точным снимком чистых U`fde867d`/A`97fe851`. Последняя правка
затронула только widget/test, поэтому это применимое evidence неизменённых
worker/controller/engine, а не новый прогон на `7093c83` или измерение GUI.

Три имеющихся fixture, без загрузки чужих стикеров:

| Fixture | SHA256 |
| --- | --- |
| `tests/fixtures/telegram_sticker_basic.json` | `07d0e4dfecdfe878a4c77e5bdb10e256982f8d1d54c47df5cd80c919f9514c2c` |
| `tests/fixtures/repeater_content_group.json` | `57e61249ccd183f423433c280ee44812fc638ed7e321ab51ba217e5fd97ffe43` |
| `tests/fixtures/tgs/repeater_content_group.tgs` | `7129163eb83d0af7d25bfd2dc82ba38f0e485a9c3103362fb6ae0418060186ed` |

Каждый fresh process: load/first frame без autoplay, warm2s, active5s,
Pause/ack/settle/paused3s, resume, hide/ack/settle/hidden3s, show и shutdown/join.
Физический вывод256x256 на экземпляр. Замеры последовательные, без параллельных
build/test/GUI workers. CPU — kernel+user process time, память — PrivateUsage
на границах фаз, включая instrument overhead. Это не число allocations.

| Input | N | Active wall ms | CPU ms | CPU images | Private bytes start → end |
| --- | ---: | ---: | ---: | ---: | ---: |
| basic JSON | 1 | 5011 | 15.625 | 301 | 3837952 → 4362240 |
| basic JSON | 4 | 5007 | 578.125 | 1204 | 4894720 → 4677632 |
| basic JSON | 16 | 5008 | 1625 | 4816 | 8359936 → 8372224 |
| repeater JSON | 1 | 5008 | 15.625 | 301 | 4567040 → 4308992 |
| repeater JSON | 4 | 5007 | 953.125 | 1204 | 5734400 → 5476352 |
| repeater JSON | 16 | 5011 | 2281.25 | 4816 | 10510336 → 10510336 |
| repeater TGS | 1 | 5009 | 62.5 | 301 | 4268032 → 4268032 |
| repeater TGS | 4 | 5008 | 828.125 | 1204 | 5423104 → 5943296 |
| repeater TGS | 16 | 5008 | 2609.375 | 4816 | 10448896 → 10448896 |

Все строки:301 accepted delivered batches за active window, 0 replacements,
5 active wraps; maximum delivery gap19–31ms. Есть проверки каждого секундного
интервала и свежести последнего кадра; cutoff250ms — критерий валидности этого
наблюдения, не обещание frame deadline. Rendered images не равны GUI paints.
`renderNanoseconds` — последнее batch render-call sum, не cumulative CPU time.

В settled paused и hidden фазах во всех9 строках прирост rendered/received/
replaced равен0. Реальные окна3001–3016ms. Paused CPU delta0ms; hidden delta0ms,
кроме basic1/basic4 по15.625ms. Квантованные счётчики процесса и работа harness
не позволяют обещать нулевое потребление CPU. Shutdown/join0–1ms при округлении
до миллисекунд, не нулевая стоимость и не hard real-time гарантия.

Полные фазы, память/CPU, timing, boundary events и error/stop history сохранены в
`A/out/part26a/task4-measure/matrix-final1/`. `index.json`:
summary accepted9/total9, child exits0, stop reason `shutdown_join_returned`.
Время boundary сопоставляется с наблюдаемым main-thread acknowledgement,
а не недоступным внутренним моментом применения команды. Transition redraw
отделён от settled windows. Повторных замеров для выбора удачного числа нет.
Это короткий single-run current-route срез, не A/B speedup, не универсальный FPS,
не доказательство отсутствия утечек/гонок и не статистическая модель масштабирования.

## Найденные ошибки и сохранённые неудачи

1. Ранний CoarseTimer timeout потреблял one-shot до Player deadline. Player
   намеренно не повторяет callback для неизменённого deadline, поэтому playback
   мог остановиться. Функциональный RED имитирует consumed early expiry;
   `fde867d` возвращает outstanding nextDeadline неактивному таймеру. GREEN
   проверяет и rearm, и следующий реальный кадр. Player/vendor не менялись.
2. Тест паузы ожидал, что control acknowledgement является idle fence.
   Подтверждение предшествует deferred paused redraw. Исправлена синхронизация
   теста, не ослаблена проверка settled no-growth;25 focused повторов passed.
3. Первичная matrix-raw имела6 failures и3 apparent passes при слабом критерии
   activeEnd>warmEnd. Все9 сохранены, но НЕ приняты. Аудит измерителя потребовал
   continuity/wrap/phase/provenance/failure-retention и отрицательные проверки.
   Исправленный instrument прошёл независимый scoped audit до matrix-final1.
4. SliderReleased-only wiring пропускало keyboard/wheel seek. Реальный paused
   fixture дал RED: ползунок1000, но controller не достиг1.0 (2pass/1fail).
   `7093c83` использует tracking-disabled valueChanged и QSignalBlocker для
   программных обновлений. GREEN покрывает End/Home/wheel/page-step, отсутствие
   seek до release с control barrier и отсутствие feedback после frame/Stop.
   Raw RED/GREEN/build/CTest и новый static identity — `out/part26a/final-fix/`.

Исторический функциональный RED Task3 и полный build stdout сохранились только
в terminal/tool transcript, не отдельными raw файлами. Старый md2 timeout log
не выдаётся за функциональный RED. Финальные и fix test/build logs сохранены.

## Защищённые байты и оставшаяся приёмка

Root fresh check на U`9cb73f0` (product`7093c83`):157 исходных `src`-файлов, кроме разрешённого
main hook, aggregate SHA256 `762b9e7bc375d74a5b8606baf04486aa71ef10dc562397ea2b2f7628d29f365c`;
22 файла принятого Release, aggregate SHA256
`d0fcbbba63574fa882d236b5d3a565e7bf2e89791d2eca1a525bf4ccf14ab0f2`.
Оба совпадают с baseline; метод и результаты в retained protected-baseline.json
и final-widget-protected-check.json. Предыдущий final-protected-check.json
относится к `fde867d` и сохранён отдельно. Docking/tray/DPI/animations не переписаны.

Не выполнены manual acceptance полного EXE, physical mixed-DPI, GPU и визуальная
приёмка трея. Минимальный размер проверялся Qt harness и unloaded screenshot;
это не loaded/error containment proof. Coverage Minors переданы итоговому reviewer.
Static linking не отменяет reference-linked install/license ограничения.
GitHub Actions U не включались. Существующий интернет-корпус не расширялся.

Следующий отдельный дизайн: собственные raw-to-model bindings и независимое
Lottie/TGS playback; затем подходящий Qt/native composition и разрешённый
корпус с источниками/правами/хэшами. Полное собственное ядро сейчас не заявляется.

## Делегированные решения (Rulings) и цена ошибки

1. Main без worktree по выбору пользователя; цена ошибки — сверка/разбор чужой
   одновременной правки, поэтому ownership handoff и Git guards обязательны.
2. Подготовка Task1 RED только в scratch пока другой UI writer завершался;
   цена — повтор baseline, если бы CMake изменился до handoff.
3. Сохранение scratch/raw вместо generic cleanup; цена — место на диске.
4. Восстановление7 известных forced cache entries; цена — обратимая доработка
   CMake изоляции при неверном перечне.
5. Deferred root-scope no-install guard; цена — обратимое исправление ложного
   отказа, но не обход install prohibition.
6. Seek/position нормализованы[0,1] по Player; цена — обратимая корректировка
   adapter/widget контракта при ошибке.
7. Task3 test CMake wiring включено в scope; цена — test-build glue, не rewrite UI.
8. Не добавлять product API только для benchmark metadata; цена — меньшая
   полнота metadata в отчёте, недоступные поля помечены null.
9. Ограниченный timer repair после воспроизведения stall; цена — обратимая правка
   adapter scheduling с обязательными pause/hidden/cancel регрессиями.

## Итоговое ревью и завершение

Whole-stage review `32d77c3..035d4ff`: With fixes, no Critical, один Important —
keyboard/wheel меняют ползунок без controller seek. Root подтвердил источник:
был подключён только sliderReleased. Одна финальная волна `7093c83` исправила
этот путь с RED/GREEN, сохранила release-only dragging, исключила программную
обратную связь и добавила отрицательный drag test. Свежие page/lab gates и
новый static EXE hash получены. Scoped re-review `035d4ff..9cb73f0`:
I1 и M2 ADDRESSED, новых Critical/Important нет. Root прочитал полный verdict,
сверил code/raw evidence и повторил финальные затронутые gates на committed code.

Loaded/error containment при минимальном размере остаётся раскрытым Minor:
реальный clipping не установлен, layout rewrite не назначен. Существующие
warnings и все явно исключённые native/manual/security/license/performance
гарантии остаются ограничениями, а не положительными результатами проверки.
Reviewer независимо сверил хэши всех9 raw rows/index/manifest и подтвердил
соответствие таблицы исходным данным. Controller-only matrix повторять для
widget-only исправления не нужно; её исходные SHA сохраняются, не подменяются.

U final docs/push завершены (`6568155`); A final docs идут отдельным коммитом.
После guarded A push приостановить только heartbeat `avemotion-avelabs-ui`;
чужую automation `avelabs-ui` не менять. Весь согласованный ограниченный объём
выполнен, новый этап разработки этим handoff не запускается. Raw и SDD workspace
сохранены по требованию пользователя, а не удалены generic cleanup.
