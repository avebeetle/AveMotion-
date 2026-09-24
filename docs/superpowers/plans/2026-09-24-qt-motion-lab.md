# Qt Motion Lab Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the current AveMotion CPU reference route in an opt-in Avelabs Qt
test page without changing the accepted application or claiming native playback.

**Architecture:** Static lean build-tree dependency, one serial worker owning
Runtime/Player/Instances, bounded latest-frame mailbox, QWidget presentation.
Qt integration lives in Avelabs, engine public API stays Qt-free.

**Tech Stack:** Windows x64, MSVC, C++20, Qt Widgets 6.10.0, static CRT `/MT`,
CMake >= 3.25, existing Python tools. No dependency installation.

**Spec:** `docs/superpowers/specs/2026-09-24-qt-motion-lab-design.md` in AveMotion.

## Global Constraints

- `AVELABS_ENABLE_MOTION_LAB` defaults OFF; explicit `--motion-lab` activates UI.
- Existing `build/Release`, docking, window animation, DPI and tray remain intact.
- Telegram reference-backed internal build-tree only; experimental install fails
  before writes. No license, redistribution, vendor or public fallback changes.
- One product writer at a time; scoped main commits and ordinary push only.
- Coordinate with task `01a0c813-5b67-75e3-9d01-cda8cbb4a4bf` before UI writes.
- A = `C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24` (engine/docs repo).
- U = `D:/rvc/c++/DragonianVoice/Avelabs-UI` (host/adapter repo).
- Unless prefixed A, implementation file paths below are relative to U.
- Preserve user changes and re-read HEAD/status before each task. No worktrees.

## Review Focus

1. Reference-linked host EXE install accidentally bypassing the engine guard:
   Task 1 performs real negative install into a fresh output and checks no files.
2. Late results after replace/close and Qt owning-thread destruction:
   Task 2 uses generation and blocked-worker handshakes, not only happy playback.
3. Receiver slower than producer and long render batch cancellation:
   Task 2 tests one pending notification and cancellation between renders.
4. User-paused state after hide/show, or hidden page still consuming CPU:
   Tasks 2/3 assert Player visibility, frozen position and stopped render counts.
5. `/MT`/Qt plugins, paths containing `c++`, DPR and source-image lifetime:
   Tasks 1/3/4 test the actual configured binary and detached image storage.

## Common commands and evidence

Use a fresh output directory for every experiment. Existing Qt prefix is
`U/out/ftfix/installed/x64-windows-static-release`; verify it before use.
Use installed Visual Studio CMake/CTest or the currently available matching
commands. Initial configure/build commands (from U):

```powershell
cmake -S . -B out/diagnostics/motionlab-2026-09-24/build `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_CONFIGURATION_TYPES=Release `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_INSTALLED_DIR="D:/rvc/c++/DragonianVoice/Avelabs-UI/out/ftfix/installed" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-release `
  -DVCPKG_HOST_TRIPLET=x64-windows-static-release `
  -DVCPKG_OVERLAY_TRIPLETS="D:/rvc/c++/DragonianVoice/Avelabs-UI/cmake/triplets" `
  -DVCPKG_OVERLAY_PORTS="D:/rvc/c++/DragonianVoice/Avelabs-UI/cmake/ports" `
  -DAVELABS_STATIC_BUILD=ON -DAVELABS_ENABLE_MOTION_LAB=ON `
  -DAVELABS_AVEMOTION_SOURCE_DIR="C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24" `
  -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF
cmake --build out/diagnostics/motionlab-2026-09-24/build --config Release --target AvelabsUI
```

Do not run `build_ui.bat static` or an install command into `build/Release`.
The installed dependency path and toolchain were confirmed read-only from the
host's tray/font build caches. CMake on PATH is 3.26.4. Keep both vcpkg manifest
flags OFF; never repair missing dependencies by installing them.

For every task record command, exit, baseline SHAs, RED and GREEN evidence under
`out/part26a/` in A or `out/diagnostics/motionlab-2026-09-24/` in U. Update A's
`docs/superpowers/ledgers/2026-09-24-qt-motion-lab.md` and
`docs/superpowers/STATE.md`; U's own lab report describes its actual linked
engine SHA.

### Task 1: opt-in static dependency and no-install boundary

**Files:** Modify `CMakeLists.txt`; create `cmake/AveMotionLab.cmake`,
`tests/motionlab/CMakeLists.txt`, `tests/motionlab/engine_smoke.cpp`,
`tests/motionlab/test_build_boundary.py`.

**Interfaces:** CMake function `avelabs_add_motion_engine()` defines the public
AveMotion::Runtime and AveMotion::Player targets in an isolated dependency
scope. Host variable `AVELABS_AVEMOTION_SOURCE_DIR` points to A. It must not
force host options into the cache. No Qt product code is written in this task.

- [x] Obtain UI-writer handoff; snapshot actual main/status and protected paths.
- [x] Write a functional Python test that configures a real scratch host twice:
  OFF rejects no missing engine path; ON requires a valid explicit engine path,
  and its generated target graph has Runtime/Player but no probe/characterizer.
  On the original code ON is ignored, so asserting the Runtime target fails:

```python
def assert_motion_engine_present(target_names):
    assert "avemotion_runtime" in target_names
    assert "avemotion_player" in target_names
    assert "avemotion_probe" not in target_names
```

- [x] Run that test against existing code and record the intended functional RED.
- [x] Implement default-OFF option, scoped `add_subdirectory` lean setup and
  Windows/Release/static-Qt/runtime/path checks. Call helper only when ON.
  Disable AveMotion tests/tools/install inside dependency scope. Use normal
  scoped variables with modern option policy, not cache FORCE.
- [x] In lab-enabled host builds, enforce `CMAKE_SKIP_INSTALL_RULES=TRUE` at the
  host root before generation and skip normal host install declarations. Verify
  no `cmake_install.cmake` exists anywhere in a fresh build tree, including the
  vendored dependency. Existing populated build dirs with stale install scripts
  must be refused for lab conversion; never delete arbitrary caller paths.
- [x] Add smoke executable using the existing public API:

```cpp
#include "avemotion/runtime/Runtime.hpp"
#include <filesystem>
int main(int argc, char** argv) {
    if (argc != 2) return 64;
    avemotion::runtime::Runtime runtime;
    auto loaded = runtime.loadTgsFile(std::filesystem::path(argv[1]));
    if (!loaded) return 1;
    auto made = runtime.createInstance(loaded.asset);
    if (!made) return 2;
    auto frame = made.instance->renderCpuFrame(0, 64, 64);
    return frame && frame.frame.argbPremultiplied.size() == 4096 ? 0 : 3;
}
```

- [x] Build/run actual `/MT` smoke. Test ON missing path, wrong variant, wrong
  CRT and attempted default/component/subdirectory install into fresh targets.
  Verify failed install created no files; OFF configuration remains valid.
- [x] Independent scoped review; fix findings; commit only Task 1 files and
  associated ledger/report. Reconcile remote before ordinary push.

### Task 2: bounded serial worker and Qt controller

**Files:** Create `src/app/motionlab/MotionTypes.h`, `MotionMailbox.h/.cpp`,
`MotionWorker.h/.cpp`, `MotionController.h/.cpp`, and
`tests/motionlab/tst_motion_worker.cpp`; extend test CMake only.

**Interfaces:** All types use namespace `Avelabs::MotionLab`.
`FrameBatch` owns `quint64 generation`, `QVector<QImage> images`,
`double position`, `quint64 renderedFrames` and `qint64 renderNanoseconds`.
`MotionMailbox::publish(FrameBatch)` returns true only when a notification is
newly needed; `takeLatest()` returns `std::optional<FrameBatch>` and clears it.
Mailbox has a mutex; publishing replaces, never appends.

`MotionController` is GUI-thread QObject, owns one QThread and shared mailbox.
Public methods: `loadFile(QString)`, `play()`, `pause()`, `stop()`,
`seek(double)`, `setInstanceCount(int)`, `setOutputSize(QSize)`,
`setVisible(bool)`, `shutdown()`. Signals: `frameReady(FrameBatch)`,
`errorChanged(QString)`, `loaded(QString)`, `stopped()`.
Register value types for queued connections. GUI generation changes immediately
on load/shutdown; stale worker messages are discarded in the controller.
`seek(double)` and `FrameBatch.position` are normalized [0,1] positions, not
seconds/frame indices, mapping to the existing Player::seekNormalized contract.
On replacement failure, the retained previous session is retagged with the
newest request generation before further publication; verify it still advances.
One latest-intent mailbox coalesces queued controls (transport/size/count/seek);
its independent notification flag bounds requests while the worker is busy.

`MotionWorker` is constructed/moved before use and initializes Runtime, Player
and timer in its worker thread. Corresponding control slots execute there only.
Shared cancellation state is atomic so shutdown need not wait for a queued
slot before preventing the next render in a batch. Generation-tag all requests.

- [x] Write deterministic mailbox tests first, initially using compilable empty
  behavior stubs so RED is an assertion failure, not a missing-header failure:

```cpp
MotionMailbox mailbox;
QVERIFY(mailbox.publish(FrameBatch{.generation = 1}));
QVERIFY(!mailbox.publish(FrameBatch{.generation = 2}));
QCOMPARE(mailbox.takeLatest()->generation, quint64(2));
QVERIFY(!mailbox.takeLatest().has_value());
```

- [x] Record RED, implement replacement/notification locking, GREEN. Add a
  producer/consumer handshake test covering publish during drain.
- [x] Write worker tests with explicit readiness and blocked-publication test
  seam: successful load/first frame, pause/seek, hidden Freeze, user pause then
  hide/show, generation A-to-B, load failure preserving prior usable session,
  stop/close while rendering and notification receiver destruction.
- [x] Start with stubs returning no frame; observe functional RED. Implement
  worker-owned Runtime/Player plus monotonic one-shot schedule callbacks; queue
  pump instead of reentering Player. Render CPU frames and deep-copy pixels:

```cpp
QImage owned(reinterpret_cast<const uchar*>(cpu.argbPremultiplied.data()),
             int(cpu.width), int(cpu.height), int(cpu.strideBytes),
             QImage::Format_ARGB32_Premultiplied);
owned = owned.copy();
```

- [x] Validate dimensions/stride with checked conversion before the QImage view.
  Test image bytes after CpuFrame destruction. Tick-frame spans are consumed
  before further mutating Player calls; never send Instance pointers to GUI.
- [x] Add actual limits tests: 2 MiB+1 file read cap; malformed TGS/CRC; zero or
  non-finite metadata; count only 1/4/16; edge <=1024 and total <=4194304 pixels.
  Coalesce latest seek/size/control intent to prevent queued mouse history.
- [x] GREEN with committed JSON/TGS fixtures and a deliberately slow mailbox
  consumer. Independently review thread/lifetime/queue contracts; commit/push.

### Task 3: widget, controls and guarded shell entry

**Files:** Create `src/app/motionlab/MotionCanvas.h/.cpp`, `MotionLabPage.h/.cpp`,
`MotionLabEntry.h/.cpp`, `tests/motionlab/tst_motion_page.cpp`; modify only the
guarded entry in `src/main.cpp`, opt-in target wiring in host CMake and
`tests/motionlab/CMakeLists.txt` for the new isolated widget/shell test target.

**Interfaces:** `MotionCanvas : QWidget` stores last images and paints them;
`void setBatch(const FrameBatch&)` takes ownership through QImage value semantics.
`MotionLabPage : QWidget` owns controller and controls. Entry:
`void installMotionLab(MainWindow&, const QStringList&)`; returns immediately
unless argument list contains `--motion-lab`. The reusable lab widget is a
parent-owned child section in the existing Voices page, not a new routed page.

- [ ] Functional RED Qt tests: route label is visible; load doesn't auto-play;
  Play/Pause/Stop and slider release send exact commands; hide/show and minimize
  update effective visibility; DPR/resize request bounded physical size.
- [ ] Implement controls with objectNames for stable tests, fixed route badge
  `Experimental — Telegram reference CPU`, error text and measured diagnostics.
  Slider tracking is false; status never claims Direct2D/GPU/native playback.
- [ ] Test usable controls and nonzero canvas at the host's minimum supported
  window size. Use scrolling/size policies inside the lab widget if necessary;
  do not alter or hide the existing Voices contents to obtain space.
- [ ] Add entry only behind compile definition supplied by ON option:

```cpp
#if defined(AVELABS_ENABLE_MOTION_LAB)
    Avelabs::MotionLab::installMotionLab(w, app.arguments());
#endif
```

- [ ] Entry uses existing component/layout access, with null guards and a stable
  objectName preventing duplicate insertion:

```cpp
auto* components = w.components();
auto* voices = components ? components->page_voices : nullptr;
if (!voices || !voices->layout()) return;
voices->layout()->addWidget(new MotionLabPage(voices));
```

- [ ] Do not modify menu/page service/docking or its current-page state. Test
  initial Voices click, Voices -> TTS -> Voices and Voices -> Sounds -> Voices;
  lab visibility/worker activity follows the existing page, original contents
  remain present, and model/view route keys remain consistent. No flag means no
  lab/controller/worker; OFF binary has no motion compile/link input.
- [ ] GREEN worker/widget tests, independent review; commit/push. Source hashes
  for unrelated shell/tray/docking remain unchanged.

### Task 4: real-shell gates, measurements and handoff

**Files:** Create `docs/testing/motionlab-2026-09-24/REPORT.md` and `STATE.md` in U;
update A's Part26A ledger/STATE and write `docs/PART26A_QT_MOTION_LAB_REPORT.md`.
Only fix independently reproduced defects with their own RED/GREEN cycle.

- [ ] Configure/build the experimental static host using the common commands.
  Record both exact source SHAs and any uncommitted state, EXE hash, CRT/Qt and
  compile/link provenance. Do not install/package into the accepted release.
- [ ] Configure isolated tests at `tests/motionlab`; CTest includes real fixture
  worker/canvas tests, build boundaries and explicit install refusal. Reuse
  existing dynamic Qt Test if static Qt Test is absent; label runtime separately
  and still require the actual `/MT` experimental host link/smoke.
  Static Qt Test was absent at planning; existing dynamic Qt Test config is
  `C:/vcpkg/installed/x64-windows/share/Qt6Test/Qt6TestConfig.cmake`. For that
  isolated harness compile both adapter and its AveMotion dependency with `/MD`
  in its own build tree; do not mix the `/MT` host archives into it. Static-only
  host option guards belong in the host root, not the reusable dependency helper.
- [ ] Run relevant existing UI/tray/settings/diagnostics suites once after the
  host change, not on every heartbeat. Re-run engine MSVC/CTest/preview gates
  only for actual engine changes; CPU lab is not a replacement for D2D/WARP gates.
- [ ] Coordinate one real GUI session with other tasks; use own process and
  explicitly temporary settings/log locations, never the installed accepted EXE.
  If the host has no clean data-root facility, use Qt test harness isolation and
  report full-host manual smoke as deferred instead of writing normal settings.
- [ ] Measure sequential warmed 1/4/16-instance active, paused and hidden phases
  over the same hashed fixtures; record actual times/counters/CPU/private bytes
  and stop reason. No universal FPS or no-leak claim from a brief run.
- [ ] Whole-stage independent review, resolve findings and fresh impacted gates.
  Report implementation, exact verification scope, manual/native-DPI limitations
  and remaining reference dependency. Preserve raw evidence and accepted bytes.
- [ ] Scoped final commits and ordinary pushes after remote reconciliation.
  Pause heartbeat `avemotion-avelabs-ui` after bounded stage completion, not after
  a normal intermediate task. Future native/GPU/corpus work needs its own design.

## Controller self-review and execution decision

All spec sections map to Tasks 1-4. Build/license boundary is tested before GUI;
worker lifetime before shell wiring; source generation and output limits before
many-instance measurement. The reference CPU label prevents confusing this
integration checkpoint with the long-term native engine. The controller selects
subagent-driven execution under the user's delegation; no human reply is needed
for ordinary reversible decisions. Current next action is coordination handoff
plus Task 1, not re-running Part25I or changing the accepted UI.
