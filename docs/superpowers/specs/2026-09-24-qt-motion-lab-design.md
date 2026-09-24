# Part26A — experimental Qt Motion Lab in Avelabs UI

Date: 2026-09-24. Status: controller-approved under delegated execution;
implementation progress is tracked in the Part26A ledger and STATE.
This is an internal experiment, not a release.

## Intent and authority

The user identified `D:/rvc/c++/DragonianVoice/Avelabs-UI` as the foundation
for future AveVoice integration, accepted static inclusion rather than a DLL,
and then requested autonomous continuation every 15 minutes while asleep.
Earlier explicit delegation includes reversible design/plan decisions,
subagent-driven implementation/review, small main commits and ordinary pushes.
The controller accepts this bounded design on that basis; it does not infer
approval for licensing, distribution, dependencies, Windows changes or replacing
the accepted application. Written spec, plan, tests and independent reviews
remain required; repeated human checkpoints for ordinary decisions are delegated.

Long-term: our own Lottie/TGS loading/model/evaluation/playback, with rlottie only
in the comparison laboratory. Part26A proves an actual host boundary using the
current engine. It does not implement an independent parser or native playback.
Generic SVG/SMIL/CSS and WEBM are different scopes.

## Observed bases and concurrent ownership

- AveMotion main `98231d8fa1f7b20a1b6bd12be8a72bb4e34729c9` is the initial base.
  Part25A-I are closed. Runtime still depends on Telegram rlottie.
- Avelabs adopted C++20 in `dc09dce`; the inspected initial HEAD was `f84dc6c`.
  Its coordinating task subsequently reported main `59a46fa`. Re-read actual
  Git before implementation; these are snapshots, not pins to reset to.
- Host is Qt Widgets 6.10.0, Windows x64/MSVC, static Qt and `/MT`.
  Existing dependency tree: `out/ftfix/installed/x64-windows-static-release`.
- The Avelabs task is concurrently finishing a startup-only smoke tool under
  `tools/deployment` and `docs/testing/tray-2026-09-24`. Until its handoff,
  keep our writes outside that checkout. Coordinate before first UI write;
  do not stop or alter its work, stage its changes, or run overlapping GUI tests.
- Keep `build/Release`, docking, window animations, DPI, tray/Exit, settings and
  diagnostics behavior unchanged. No worktrees or history rewriting.

## Chosen approach and alternatives

Use a small opt-in Qt host adapter around public AveMotion Runtime/Player, with
one serial worker and a QWidget displaying owned CPU images. Mark the entire
surface `Experimental — Telegram reference CPU`. This gets the real shell,
visibility, ownership and scheduling tested without pretending a QWidget is a
Direct2D target. No new QPainter vector backend is introduced.

Direct2D-native embedding was considered but deferred: current Direct2D accepts
a caller-owned device context; Qt backing-store composition, clipping, alpha,
device lifetime and DPI require a separate proven graphics design. A standalone
Qt-only viewer was also considered; it is useful as a test harness but does not
alone satisfy embedding into the actual Avelabs shell.

## Boundaries and files

Canonical spec/plan/state live in AveMotion. Qt-dependent adapter and UI live
under Avelabs `src/app/motionlab`; tests under `tests/motionlab`, build glue in
`cmake/AveMotionLab.cmake`. No Qt includes/dependencies enter AveMotion public
headers, Runtime, Player, geometry or rendering libraries.

`AVELABS_ENABLE_MOTION_LAB` defaults OFF. When ON, require an explicit local
`AVELABS_AVEMOTION_SOURCE_DIR`, existing static Qt, Windows x64/MSVC, Release,
`/MT`, and CMake >= 3.25. Add AveMotion as a lean Telegram build-tree dependency:
no characterizers, preview, tests, downloads or install. Do not set global
`BUILD_TESTING` or other host options with FORCE. Scope dependency options and
restore host values. Test the actual static link, not only target names.

Execution clarification: isolation includes the existing engine wrapper's
transitive forced cache writes (`BUILD_SHARED_LIBS` and its six `LOTTIE_*`
switches). The host helper snapshots/restores those known entries, including
absence and cache metadata, around dependency construction. Restoring a caller's
previous state is allowed; imposing new host values through FORCE is not. Test
both an empty initial cache and supplied sentinel values. Engine/vendor files
remain unchanged in this host-integration task.

The experimental executable remains in a fresh `out/diagnostics/motionlab-*`
build directory. In lab mode, reject installation before any install write,
including component and subdirectory installs: do not route a reference-linked
EXE around AveMotion's existing install prohibition. Preserve all vendor
sources, licenses/notices and dependency identities. No redistribution approval.
Enforce `CMAKE_SKIP_INSTALL_RULES=TRUE` at the host root for the lab profile;
do not merely rely on a caller-supplied flag or on the dependency install option.
Use a fresh build directory without stale `cmake_install.cmake` files and verify
none are generated in the whole build tree. This covers vendor install rules
that exist even with `AVEMOTION_ENABLE_INSTALL=OFF`.

With the build option enabled, `--motion-lab` embeds the reusable MotionLabPage
widget as an extra section of the existing Voices page. Obtain that QWidget
through `MainWindow::components()->page_voices` and add to its existing layout;
do not hide/delete the original contents, add a new routed page, change page
indices or change PagePanelsService's current key. Only a small guarded call in
`src/main.cpp` is allowed. No menu/docking/page-service rewrite. Existing menu
navigation to TTS/Sounds hides and suspends the lab; Voices restores it. Clicking
Voices while already there is a harmless no-op. This deliberately avoids the
router's same-key early return that would strand an independently selected
QStackedWidget lab page. Without the argument, normal startup is unchanged and
no motion worker exists. Without the build option, no motion code is linked.

## Worker, scheduling and lifetime

One worker thread owns Runtime, Player, all Instances and its one-shot timer.
All Player methods, loading and renderCpuFrame calls execute serially there.
The existing Player thread contract permits a host-selected control thread;
it is not itself a GUI object. GUI code sends commands and displays QImages;
it never accesses an Instance. No raw ScheduledFrame span or Instance pointer
crosses a queued connection. Copy required values before another Player call.

Worker callbacks request a deferred pump, schedule one deadline or cancel it;
they must not synchronously re-enter Player. Use a monotonic clock. On a pump,
tick DueOnly, render due frames serially, and publish copied, independently owned
QImages using ARGB32 premultiplied. Do not retain a view of CpuFrame vector data.
QWidget repaint uses the cached image; expose does not force rerendering.

At most one frame-batch notification may be outstanding. A mutex-protected
latest-batch mailbox replaces pending frames; its queued notification carries
no growing image history. UI drains the mailbox and allows another notification.
Asset loads have monotonically increasing generations; stale completion, frame
or error delivery must not overwrite a newer selection. Resize/seek requests
coalesce; slider tracking is off so scrubbing submits on release. No unbounded
command/frame queue proportional to frame rate or mouse movement.
If the newest replacement fails, retain the previous usable session but publish
its frames/status under the newest request generation. Otherwise the GUI's
stale-result filter would permanently discard the preserved session's frames.
Coalesce controls through one latest-intent mailbox/notification, separate from
the frame mailbox. Preserve the most recent desired transport state, size,
count and absolute seek, not an accumulated list of mouse/timer actions.

Hidden/minimized page sends hidden state for all entries with Freeze policy:
no frame rendering/deadline timer while all entries are hidden. Showing resumes
only entries automatically paused by visibility, never user-paused entries.
Choose a small capped instance count (1, 4, 16) rather than unlimited spawning.

Closing the page invalidates delivery generations, cancels queued work and
requests worker shutdown. Check cancellation between individual renders and
before publication; join the owned worker safely before destroying its state.
Never terminate a thread or close a foreign process. Upstream CPU rendering is
not interruptible mid-call: tests measure shutdown for admitted fixtures, but
do not promise a hard real-time stop for arbitrary pathological input. A queued
signal alone is insufficient cancellation if a render batch is in progress.

## Input, limits and controls

Lab controls: Open JSON/TGS, Play, Pause, Stop, position slider, count 1/4/16,
status/error and actual route badge. Loading does not auto-play; successful load
shows a first frame. Existing session remains usable if a replacement fails.
One loaded Asset can back several independent Instances. No prepareModel scan
is needed for this explicit CPU reference route.

Use only explicit local file selection and committed/generated fixtures first;
no remote URL fetching, archive unpacking or automatic internet asset loading.
Read at most 2 MiB + 1 and reject oversize input; honor Runtime TGS limits
(64 KiB compressed, 2 MiB JSON, expansion ratio 128). Reject empty/invalid files
and non-finite/zero timing metadata. Canvas dimensions <= 4096 and frames <=
18000; these checks bound the lab workload, not the upstream parser's security.
No sandboxing or hostile-input safety guarantee is implied.

Maximum physical output edge is 1024 and aggregate output is <= 4,194,304 pixels
per batch. Calculate with checked integers; clamp requested pixel dimensions
before allocation, preserve aspect ratio, report any resolution clamp. Account
for devicePixelRatio in the widget/image presentation without changing shell
DPI code. Count changes and replacement loads release old registrations/images.

Errors appear on the lab page without modal loops. They include the operation
and Runtime error code, but never silently substitute another rendering route.
Diagnostics: selected file name, frame/position, count, CPU reference badge,
rendered/received/dropped batches, render durations and Player counters. These
are measured quantities, not a zero-allocation or GPU speedup assertion.

## Acceptance and verification

1. Default build lacks lab linkage and keeps normal startup/installation.
   Experimental static `/MT` consumer loads a fixture and renders a CPU frame;
   lab install fails before creating files. No dependency installation occurs.
2. JSON/TGS valid, invalid, truncated/CRC-invalid, oversize and boundary-size
   fixtures behave deterministically. Failure preserves the previous session.
3. Play/pause/stop/seek, 1/4/16 instances, hidden/visible, user pause, resize/DPR,
   load A then B, close during work and late result delivery are covered.
4. Frame bytes match Runtime CPU output for the same fixture/frame/size. Image
   storage survives destruction of its source frame and queued delivery.
5. Coalescing bounds pending notifications/images while the receiver is slow;
   hidden steady state stops render count growth. Tests must avoid arbitrary
   sleep-based correctness assertions where explicit handshakes can be used.
6. Relevant UI/tray/settings tests, lab tests and real experimental static build
   pass. Preserve protected UI/Release bytes except the explicitly allowed
   main/CMake additions. Re-run engine gates only if engine code/build changes.
7. Sequential active/paused/hidden measurements record actual executable and
   both repository SHAs, Qt/CRT, asset hashes, run length and raw results. Do not
   compare these CPU Qt timings to earlier Direct2D/exact-scene numbers as if
   they measured the same work. Manual appearance/native DPI remain explicit
   limitations when desktop inspection is unavailable.

No goldens, vendor versions, fallback policy, Player semantics or Direct2D
ownership change. Independent task and final review precede completion claims.

## Sources and next boundary

- `docs/PART25E_LEAN_MODULE_REPORT.md`: static build-tree vs offline package.
- `include/avemotion/player/Player.hpp`: host callbacks, control-thread contract.
- `include/avemotion/runtime/Runtime.hpp`: serial Instances and CPU frames.
- Avelabs `docs/development/maintenance.md`, `docs/ROADMAP.md` and tray STATE.
- https://doc.qt.io/qt-6.10/threads-qobject.html — GUI/main-thread separation.
- https://github.com/desktop-app/lib_lottie/blob/7d00b5048aff8dd93c0a9721abf50006d20682be/lottie/details/lottie_frame_renderer.cpp#L141-L172
  — previous read-only research on scheduling and buffered image preparation;
  no Telegram Desktop source is copied into the host adapter.

Upstream pin verified on 2026-09-24 (`git ls-remote` HEAD and pinned raw source).
`FrameRendererObject::queueGenerateFrames` coalesces render-queue work;
`generateFrames` posts weak-owner notifications to the main thread. The useful
principle here is bounded scheduling and owner-aware delivery, not importing
Telegram's queue/frame machinery. Our corresponding checks are slow-consumer
coalescing and destruction/late-delivery tests. Qt's thread-affinity contract
requires GUI work on the main thread and timer use on its owning thread; worker
readiness/shutdown tests cover that separation. Upstream behavior is precedent,
not evidence that our not-yet-implemented adapter is correct.

After this bounded stage, report and pause its heartbeat. Native raw-to-model
correspondence, independent playback, Direct2D/Qt composition and a rights-cleared
internet corpus remain separate designs; no claim that this stage completes them.
