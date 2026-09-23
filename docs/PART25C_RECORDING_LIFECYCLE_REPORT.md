# Part25C — retained recording lifecycle validation

Status: Part25C complete. Platform gates ran at `aec482f2f310240402aaa04b46dc06a1a242af1c`; independent whole-change review through `3f8a06e419e547e4fab91b3ff431fea0ce6bced7` approved spec compliance and integration with no Critical or Important findings. Stage base: `45a21a4724bc69f290c805187ba6b42fd78bc10a`. Product implementation HEAD before controller gate metadata: `8748dae3c8608989ab198f13a58b230037641c06`. This stage supplies a private vendored recording lifecycle to both pinned rlottie variants. AveMotion `Runtime` and `ReferenceRuntime` still use their existing fresh sampling; persistent Runtime integration belongs to Part25D.

## Contract and ownership

Both variants add `Animation::enableRecordingLifecycle()` and const `renderTreeForRecording(size_t frameNo, size_t width, size_t height)`. Enable is one-way, idempotent and available only while the object is pristine; metadata queries do not consume the latch. Ordinary tree, sync/async CPU render dispatch and property override consume it. Calling the wrong tree entry returns null. On a recording object, CPU sync/async render and all property setter routes throw `std::logic_error` before changing the surface, running callbacks or scheduling work. The narrow exception-unwind override applies only to each vendor's `lottieanimation.cpp`; evaluator and raster translation units retain their existing flags. Single-object access remains serial.

Every recording sample resets mutable evaluation state in the existing composition, all layers including skipped descendants, masks/clips, groups, maximum repeater copies, shapes, trims, paints, typed drawable payloads and conditional C publication fields, then runs the original evaluation algorithms. Parsed model, object topology, bindings, source IDs, brush/gradient resources and allocated publication owners remain retained. Local shape and mask paths clear before evaluation; publication derives an owned dashed path from the original undashed path. Telegram recording mask/clip work updates geometry without enqueueing raster jobs; Samsung recording performs no CPU preprocess. Returned tree pointers are borrowed until the next sample or destruction; AveMotion copies the scene before either event. Invalid viewport dimensions and the global constructor-frame `-1` no-build sentinel return null without publishing stale state. Original frame mapping, variant-specific visibility (Telegram exclusive/Samsung inclusive out frame) and valid nested local `-1` semantics remain intact.

The common test compares deep-copied complete `EvaluatedScene` values with unchanged `ExactSceneComparison` against a separately loaded fresh ordinary Animation for each expected sample. It covers 33 JSON histories per variant, including the 16 direct smoke corpus inputs, nested dash/epsilon and new skipped/negative-child cases, ascending/reverse timelines, seeks/repeats, viewport changes, invalid-dimension recovery, guard ownership, ordinary CPU isolation and copied-scene lifetime. Telegram reports 4,584 full-scene comparisons; Samsung reports 4,650. Each variant also exercises 320 exact comparisons across two independently constructed host-thread sessions. These are functional concurrency checks, not a race-detector result.

## RED, GREEN and platform gates

The initial temporary retained-ordinary adapters failed functionally before vendor production edits: Telegram 12 of 26 fixture histories differed (`out/part25c-telegram/red.txt`, 13.13 s); Samsung 15 histories plus the negative-skipped concurrent history differed (`out/part25c-samsung/red.txt`, 10.24 s). Samsung's direct gapped `PathData` case separately failed before reset (`out/part25c-samsung/path-red-2.txt`); the earlier `path-red.txt` followed a failed build and is not evidence for that case. Callback-owner lifetime initially failed under disabled exception unwinding in both variants and passed after the source-local flag correction. Final focused Debug lifecycle tests passed Telegram 1/1 and Samsung 1/1; see the Task 1/2 reports and raw GREEN logs for the intermediate sequence.

The controller ran fresh configure, full build and CTest from Visual Studio 2022 `VsDevCmd.bat -arch=x64 -host_arch=x64` through `out/part25c-final/run-gate.ps1`. Each raw log records the full command, revision and result. These final gates used checkout `aec482f`:

Reproduction from the repository root in a Windows `cmd.exe` shell. First run `call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64`; every subsequent CMake/CTest command uses that initialized shell. The stage-owned scripts under ignored `out/part25c-final/` orchestrated and logged the original runs, but a new checkout can use these underlying commands directly:

```bat
cmake --preset windows-msvc-telegram-debug
cmake --build --preset windows-msvc-telegram-debug --parallel 4
ctest --preset windows-msvc-telegram-debug --parallel 4 --no-tests=error --output-on-failure
cmake --preset windows-msvc-samsung-debug
cmake --build --preset windows-msvc-samsung-debug --parallel 4
ctest --preset windows-msvc-samsung-debug --parallel 4 --no-tests=error --output-on-failure
cmake --preset windows-msvc-win32-preview
cmake --build --preset windows-msvc-win32-preview --parallel 4
ctest --preset windows-msvc-win32-preview --parallel 4 --no-tests=error --output-on-failure
cmake --preset windows-msvc-direct2d
cmake --build --preset windows-msvc-direct2d --parallel 4
ctest --preset windows-msvc-direct2d --parallel 4 --no-tests=error --output-on-failure
cmake --preset windows-msvc-telegram-release
cmake --build --preset windows-msvc-telegram-release --parallel 4
ctest --preset windows-msvc-telegram-release --parallel 4 --no-tests=error --output-on-failure -R avemotion.runtime.recording_lifecycle
cmake --preset windows-msvc-samsung-debug -B out/build/part25c-samsung-release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/part25c-samsung-release --target avemotion_recording_lifecycle_tests --parallel 4
ctest --test-dir out/build/part25c-samsung-release -R avemotion.runtime.recording_lifecycle --no-tests=error --output-on-failure -V
cmake --install out/build/windows-msvc-direct2d --prefix "%CD%\out\part25c-final\install-reproduction"
cmake -S tests/consumer -B out/part25c-final/consumer-reproduction -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%CD%\out\part25c-final\install-reproduction"
cmake --build out/part25c-final/consumer-reproduction --parallel 4
out\part25c-final\consumer-reproduction\avemotion_installed_consumer.exe
ctest --test-dir out/build/windows-msvc-direct2d -N -R avemotion.runtime.recording_lifecycle
```

| Gate | Final CTest result | Raw evidence |
| --- | --- | --- |
| `windows-msvc-telegram-debug` | 63/63 passed, 84.19 s; lifecycle passed | `out/part25c-final/windows-msvc-telegram-debug-aec482f-20260923T082715904Z.txt` |
| `windows-msvc-samsung-debug` | 41/43 passed, 17.90 s; lifecycle passed; two known golden failures | `out/part25c-final/windows-msvc-samsung-debug-aec482f-20260923T082902606Z.txt` |
| `windows-msvc-win32-preview` | 57/57 passed, 68.99 s; lifecycle and preview/capture coverage passed | `out/part25c-final/windows-msvc-win32-preview-aec482f-20260923T082716110Z.txt` |
| `windows-msvc-direct2d`, variant `none` | 30/30 passed, 2.36 s | `out/part25c-final/windows-msvc-direct2d-aec482f-20260923T082902849Z.txt` |

Samsung's two failures are `avemotion.scene.golden` and `avemotion.plan.golden`, both row 39, `polystar_line_clockwise_trim.json`, p100, frame 150, 128x128. The mismatching scene hash remains expected `0cca4a4d4395c248` versus actual `d16b938d33790784`, and geometry `c52135375f4b4fd9` versus `4c0fb2275b3068ab`. The plan hash remains expected `4e7835d97cd1de98` versus actual `43cc77e5cb56f0e1`, geometry identity `f392889bad747440` versus `4992a725d8c4bd2c`, and presentation `83bafa6dc131d21b` versus `38cbdf9a188f397b`. The Task 2 literal comparison against `out/part25b-final/windows-msvc-samsung-debug-58236a9.txt` found the same rows; no golden or whitelist changed. The Samsung gate helper's PowerShell wrapper raised `NativeCommandError` after CTest reported its complete 41/43 result, so its raw log has no final `GATE_EXIT` marker. This gate is a known-failure result, not all green.

The controller independently compared all four complete Samsung expected/actual rows byte for byte with that Part25B log. The same audit lists the preview CTest's actual `avemotion_win32_preview.exe --self-test --warp` command, plus capture and preflight test registration: `out/part25c-final/graphics-and-baseline-aec482f.txt`.

Release-specific recording checks used the same checkout and Visual Studio environment. Telegram's official `windows-msvc-telegram-release` configured and built the full preset, then passed the focused lifecycle CTest 1/1 (test 1.49 s, total 1.50 s; `GATE_EXIT=0`): `out/part25c-final/windows-msvc-telegram-release-aec482f-20260923T082927999Z.txt`. Samsung used a fresh stage-specific `out/build/part25c-samsung-release`, configured with `-DCMAKE_BUILD_TYPE=Release`, built `avemotion_recording_lifecycle_tests`, and passed the verbose focused CTest 1/1 (test 2.13 s, total 2.15 s; `RELEASE_EXIT=0`): `out/part25c-final/samsung-release-aec482f-20260923T082928322Z.txt`. Release checks cover the recording test, not full Release suites; Telegram's nonverbose log does not restate comparison counts.

The no-reference external consumer installed into new `out/part25c-final/install-aec482f-20260923T082928559Z`, then configured `tests/consumer` with that prefix, built and executed successfully (`INSTALLED_CONSUMER_EXIT=0`): `out/part25c-final/installed-consumer-aec482f-20260923T082928559Z.txt`. `ctest -N` for the no-reference preset reports zero recording lifecycle tests, as intended (`out/part25c-final/controller-integrity-aec482f.txt`). These are MSVC/Windows results; no new GCC/Clang or sanitizer execution is asserted.

## Provenance and preserved bytes

The canonical verifier passes for Telegram and Samsung in the live checkout and the corrected index snapshot. `UPSTREAM.json` retains original upstream commit/archive values and records the local patches and current source fingerprints: Telegram SHA-256 `7166aaeedcc6ceb5b49388cdc119a9fe0fc917f6f706ee50e239f5b30b01d184`, 276 files, 13,284,156 bytes; Samsung SHA-256 `77ab5814b88590936f4b33ef09114d3757a1d91dcde349d5e70c4b40926c549f`, 322 files, 12,196,477 bytes. Telegram patch `0006-avemotion-recording-lifecycle.patch` reproduces all five changed vendor files byte for byte from `45a21a4`; Samsung patch `0002-avemotion-recording-lifecycle.patch` reproduces all six. The controller freshly matched all 11 patch replay SHA-256 rows against current files. Raw per-file hashes and commands remain in `out/part25c-telegram/patch-reapply.txt` and `out/part25c-samsung/patch-reapply.txt` and the corresponding task reports.

Repository-local `.gitattributes` `-text` rules protect `third_party/**`, `patches/**`, `tests/corpus/**` and `tests/compatibility/tgs/**`. Under the installed `core.autocrlf=true`, the disposable mini-repository add/checkout test failed before and passed after these rules; it also passed with `core.autocrlf=false`. The nine pre-existing vendor blob newline discrepancies were re-indexed while all 642 protected working-file hashes stayed unchanged, and a Git-free checkout of the corrected index passed vendor/corpus verification. The controller's final integrity records confirm unchanged Runtime/ReferenceRuntime production code, 16-asset corpus, goldens and all 15 tracked license/notice paths (14 under `third_party` plus `NOTICE.md`) against the stage base. No global Git setting or upstream license text changed. The intended nine-file index diff is line-ending review noise, documented in Task 3.

The preserved Part25B-compatible Release baseline is `out/benchmarks/part25c/baseline/avemotion_corpus_lab.exe`, SHA-256 `be9b98b42fd0f610665163da3c676316bc14725a6cdf862c87ee2208f418da69`. The controller's final integrity check reconfirmed this hash and the other retained baseline hash. This baseline is the Part25B Runtime with the common instrument, not the original `cda415c` Runtime; no timing comparison is made here.

## Decisions, limits and Part25D handoff

The SDD ledger's chronological rulings were: honor the user's continuous-work delegation despite older deadlines (cost if wrong: reversible work/resource use); approve the Part25C spec/plan under delegated authority (architecture rework); separate vendor lifecycle from Runtime integration so both variants pass independent parity first (an extra handoff); choose recursive constructor-state reset over baseline projection/epochs (redesign if differential parity fails); retain raw scratch evidence (disk use); interpret the smoke set as eight direct `tests/corpus` plus eight direct `tests/fixtures` files (missing coverage if mistaken); enable exception unwinding only in Telegram's animation wrapper after the owner-lifetime failure (wrapper codegen or cross-compiler regression); and add a bounded protected-byte task after the Git newline audit (attribute/re-index churn). Samsung's actual disabled-exception flags required the same narrow wrapper correction. Each ruling is recorded with its cost in `.superpowers/sdd/2026-09-23-recording-lifecycle/progress.md`.

Build logs retain MSVC option-override D9025 diagnostics and upstream C4251, C4530, conversion/deprecation and similar warnings; the build is not warning-free. The test proves exact retained-versus-fresh output for covered cases, not Runtime reuse, speedup, zero allocations, thread-sanitizer safety, or cross-compiler behavior. Part25D must settle parsed-model/source-ID stamping order and refresh for retained sessions, and ensure cache eviction cannot substitute a different LOTModel under the same cache key. It must also handle shared-source stamping races and failed-sample recovery under actual Runtime ownership.

Implementation commits: Telegram `bd2724d`; Samsung `0ebd660`; protected Git bytes `8748dae3c8608989ab198f13a58b230037641c06`; verified gate report `3f8a06e`. Task-level reviews and final whole-change review were approved. Raw independent review: `.superpowers/sdd/2026-09-23-recording-lifecycle/final-review.md`. It independently checked the four unchanged Samsung rows, all eleven patch/source deltas, nine newline-only blocks and 642 unchanged protected hashes. Since the tested product revision, only controller documentation changed; no product fix wave was required.

Final review triaged captured Git stderr and intentional EH-option warning noise as nonblocking diagnostic debt. The controller accepts that bounded deferral (cost if wrong: slower failure diagnosis or obscured warnings) without removing byte checks or exception support. Its declined-to-judge areas remain explicit: Runtime integration and performance/memory acceptance are Part25D obligations; cross-compiler/sanitizer behavior remains unverified; known Samsung failures/upstream warning repair remain separate; new licensing/platform decisions remain outside current authority. Carrying these obligations forward rather than claiming them complete costs further validation work and preserves the limits of this stage. Automation remains active and proceeds directly to Part25D.
