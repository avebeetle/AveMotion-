# Part26A Qt Motion Lab — durable execution ledger

## 2026-09-24 initial checkpoint

- User explicitly resumed autonomous work every 15 minutes while asleep.
- New heartbeat `avemotion-avelabs-ui` ACTIVE; old avemotion-6 remains deleted.
- Architecture/design scope accepted by controller under earlier delegated
  decisions, not falsely recorded as the user reading this new document.
- Spec: `docs/superpowers/specs/2026-09-24-qt-motion-lab-design.md`.
- Plan: `docs/superpowers/plans/2026-09-24-qt-motion-lab.md`.
- Execution method: subagent-driven, one product writer, independent review.
- Initial AveMotion HEAD/remote: `98231d8fa1f7b20a1b6bd12be8a72bb4e34729c9`.
- Avelabs adopted C++20 in `dc09dce`; other task reports latest `59a46fa`.
- No product code, fixture, vendor, build setting or UI binary changed yet.
- Existing Avelabs task is writing its startup-only smoke tool/docs and requests
  no concurrent UI writes until its narrow checkpoint is handed off. Before
  first host edit coordinate with task `01a0c813-5b67-75e3-9d01-cda8cbb4a4bf`.
  Docs/design in AveMotion can proceed. Do not treat the other task's expected
  remote commits as ours or silently merge them; read their actual scope first.

## Decisions

1. First route is explicit experimental Telegram reference CPU bitmap. It is
   for host contract validation, not proof of native engine or GPU acceleration.
2. Runtime/Player/Instances all live on one worker/control thread; GUI gets owned
   bounded images. No Player threading or fallback policy change.
3. Qt code stays in Avelabs; engine remains Qt-free. Option default OFF; no
   installation of the experimental reference-linked host, including component
   or dependency-subdirectory install. Current accepted release stays untouched.
4. Corpus starts with existing fixtures. Real internet assets require a later
   source/hash/rights manifest; not downloaded or committed automatically.
5. Do not consume the whole night merely to fill time. Finish the bounded lab
   stage, record limitations, then pause this heartbeat.

## Tasks

- [x] Task 1 — opt-in static dependency and no-install boundary.
- [x] Task 2 — bounded serial worker and Qt controller.
- [ ] Task 3 — widget, controls and guarded shell entry.
- [ ] Task 4 — real-shell gates, measurements and handoff.

## Evidence and review

Read-only public-API audit confirms host-scheduled Player and caller-owned D2D
context, no existing Qt adapter. Read-only build audit confirms existing static
Qt/vcpkg dependencies and CMake 3.26.4. It identified vendor install rules not
covered by `AVEMOTION_ENABLE_INSTALL=OFF`; the controller verified local CMake
help and requires profile-wide `CMAKE_SKIP_INSTALL_RULES=TRUE` plus actual
default/component/subdirectory negative-install tests. Separate lab target name
was suggested by the auditor but is not necessary: the chosen same AvelabsUI
target lives exclusively in a fresh experimental build; default builds remain
unchanged. The approved option name is `AVELABS_ENABLE_MOTION_LAB`.

Self-review clarified replacement failure: preserving old Instances is not
enough, their result generation must advance to remain visible to the GUI.
Independent design review follows. Product tests have not run in this checkpoint;
historical test counts are not fresh Part26A evidence.

Independent review found a real navigation contradiction in the initial design:
setting the visible stacked page alone leaves service currentPage=voices, whose
same-key navigation then returns early. Root verified PageRoutingFlow and chose
the smaller integration: embed the lab widget inside existing Voices content,
without a new route or changing current-page state. Existing contents stay;
TTS/Sounds/Voices navigation and hidden-work behavior get explicit tests. This
is a deliberate bounded first host integration, not a new page-router feature.
Scoped reviewer re-check requested. Static Qt Test is absent; dynamic Qt Test is
present, so the isolated harness uses its own `/MD` dependency build while the
real host's separate `/MT` build/smoke remains mandatory.

Independent re-review by `/root/qt_lab_design_review`: Ready to implement,
no remaining Critical/Important design blockers. Minor minimum-window-size
usability check added to Task 3. Reviewer declined to judge future build,
shutdown timing and visual success; controller accepts these as execution gates,
not skipped work. Fresh docs title/placeholder checks and `git diff --check`
passed. Only four documentation paths are included in the initial checkpoint.

Avelabs coordinator reconfirmed source/remote `59a46fa` and clean tree, but this
is not the final handoff: it will explicitly report completion of its smoke
checkpoint and absence of live edits/build/GUI. Wait for that handoff before
host writes, then re-read Git and begin Task 1. No product implementer is active
for Part26A at this checkpoint; a heartbeat should not mistake this for one.

## 2026-09-24 Task 1 execution

SDD preflight and scratch ledger initialized at
`.superpowers/sdd/2026-09-24-qt-motion-lab/progress.md`. All four task-internal
checks and six shared-interface pairs recorded. Explicit main instruction
overrides generic worktree setup; user-required evidence preservation overrides
generic scratch cleanup. Scope/design unchanged.

While UI was still owned by its smoke task, sole implementer
`/root/qt_lab_build_boundary` prepared a test in AveMotion scratch. Actual host
OFF and ON configurations both succeeded, then the ON target-graph assertion
failed on missing avemotion_runtime (exit1). No host writes or GUI were needed.
Evidence: `out/part26a/task1-red/`, including complete commands/stdout and the
functional assertion. No GREEN or product completion is claimed yet.

The other task handed off clean main `32d77c3715f8d084e5eb9d8abc6017ad84c23420`,
matching remote, with edits/build/tests/GUI finished and its own heartbeat paused.
Root verified its six changed paths are smoke tooling/docs, not src/CMake.
The recorded RED is still applicable. Same Task 1 implementer is now released to
the five scoped host files from this BASE; root retains review/docs/push.

Protected working-byte snapshot before implementation: 157 existing src files
except main; SHA256 `762b9e7bc375d74a5b8606baf04486aa71ef10dc562397ea2b2f7628d29f365c`.
Accepted Release: 22 recursive files, aggregate SHA256
`d0fcbbba63574fa882d236b5d3a565e7bf2e89791d2eca1a525bf4ccf14ab0f2`.
Algorithm/details: plan workspace/protected-baseline.json. Main hook is the only
allowed existing src edit; adding motionlab files does not alter this baseline.
Upstream scheduling reference in spec now has a verified commit permalink;
no external source was copied, no license or dependency changed.

## Task 1 accepted and pushed

Host commits: `d0331a7` (static dependency), `c6330e6` (standalone no-install
guard), `7084563` (deferred root-scope enforcement). All five assigned paths;
no production Qt/source, engine or vendor edits. Ordinary push completed after
independent task review and two scoped fix reviews. Final scoped verdict:
all findings addressed, no new Critical/Important breakage.

Ruling: preserve/restore the seven known transitive forced cache entries,
including absence and metadata — normal function-local settings alone did not
isolate the caller — cost if wrong is reversible host-glue complexity. No new
host cache policy is forced. Functional cache RED/GREEN includes empty cache,
explicit sentinels, types and optional metadata; target stays static.

Review found the first standalone /MD compatibility harness generated vendor
install scripts, unlike the guarded host. First fix enforced local policy and
fresh-tree checks; second review identified nested-scope leakage. Actual RED
confirmed local TRUE/root FALSE still generated a root script. Ruling: validate
the root value at deferred configure completion before generation — cost if
wrong is a reversible false rejection — preserving caller policy, not forcing
it. Function-local/subdirectory-only and later-cleared flags are now rejected;
correct root-guarded nested calls succeed without any install script. No unsafe
RED-tree install was ever executed. Those directories remain labelled evidence.

Final commands (from U unless noted):

- `python tests/motionlab/test_build_boundary.py --output-root C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part26a/task1-review2-full`
  — exit 0, all 23 steps passed, including actual /MT and /MD fixture smokes,
  cache isolation, profile/path/variant refusals, stale-directory refusals and
  host/default/component/vendor plus standalone vendor install refusal.
- `ctest --test-dir C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part26a/task1-review2-full/on -C Release --output-on-failure`
  — exit 0, 1/1 passed. Root inspected full output and also independently ran
  the preceding-fix smoke and script-absence check. One controller check used
  the wrong directory name first; corrected after inventory, no product fault.
- `git diff 32d77c3..7084563 --check` — passed. Protected source and accepted
  Release hash checks passed after initial product edits; none of the later
  fixes touched those paths. Their exact original paths stay the final gate.

Raw logs: `out/part26a/task1-review2-full/`; complete execution/review reports:
`.superpowers/sdd/2026-09-24-qt-motion-lab/task-1-{report,review,fix1-review,fix2-review}.md`.
Engine HEAD throughout this task was `86e17e3` with documentation-only working
changes; engine/vendor compiled sources unchanged. Full real AvelabsUI static
link, GUI controls, visual/native DPI and measurements are Tasks 2-4, not claimed
by this smoke. Remaining minor for final review: expected skip-install warning,
unused overlay arguments and existing vendor compiler diagnostics; documented
without blanket suppression. The whole lab stage is still in progress.

## Task 2 execution checkpoint

Sole product implementer `/root/qt_lab_worker`, host BASE
`7084563835943e75d7c860fa160e0bb465c2cbdd`, is implementing the serial worker,
controller and bounded mailboxes. No duplicate writer or overlapping GUI/build
was started by the controller. Task 2 remains incomplete until final tests and
independent review; intermediate passing test counts are not a final gate.

Ruling: `seek(double)` and `FrameBatch.position` use normalized [0,1] positions,
matching public `Player::seekNormalized` — the brief omitted units and the
widget must not guess seconds/frame duration — cost if wrong is a reversible
adapter/widget contract adjustment. Spec/plan clarified and same information
sent to the implementer; no public engine API or playback semantics change.

Task 4 preparation is complete in the retained SDD workspace:
`task-4-measurement-protocol.md` records hashes of existing JSON/TGS fixtures
and a bounded sequential active/paused/hidden protocol for 1/4/16 instances.
No timings have yet been measured. Independent read-only
`task-4-existing-gates.md` identifies eight existing host CTest entries and
their scope. Those are historical baseline results, not fresh Part26A results.
The ordinary host EXE lacks full temporary data-root isolation; use the existing
temporary-INI harness and explicitly defer full-EXE manual acceptance. Never
bypass script policy or install dependencies to run a gate.

Task 2 implementation committed locally in host as
`b92c3bc253db95c5285ca2863c4016a1064f5eae` (nine assigned paths only; clean host
tree, no push). Functional RED/GREEN covered mailboxes and worker readiness,
diagnostics, then two self-review regressions: coalesced Stop -> Seek lost the
newer seek, and hidden count replacement lost automatic resume. Each was
reproduced before its fix. Final raw QtTest: 25 passed, 0 failed/skipped;
`ctest --test-dir D:/rvc/c++/DragonianVoice/Avelabs-UI/out/diagnostics/motionlab-2026-09-24/task2-md -C Release --output-on-failure`
passed 2/2 (2.90s). Root read the complete final outputs, not only the summary.
Evidence: `out/part26a-task2-*`, complete commands in SDD `task-2-report.md`.

Independent Task 2 review is active over the complete `7084563..b92c3bc` range;
this is not acceptance yet. The controller freshly verified protected source
aggregate `762b9e7bc375d74a5b8606baf04486aa71ef10dc562397ea2b2f7628d29f365c`
(157 files) and accepted Release aggregate
`d0fcbbba63574fa882d236b5d3a565e7bf2e89791d2eca1a525bf4ccf14ab0f2`
(22 files), both unchanged. Static host link/widget/native DPI are later gates.

Independent Task 2 review returned spec FAIL / quality Needs fixes. Four
Important findings (two P1, two P2): count/output state can diverge from retained
registrations when replacement fails, per-frame diagnostic events bypass frame
mailbox bounds, Seek -> Stop retains a superseded seek, and separate edge
clamping distorts requested aspect ratio. Root checked the cited changed code;
these are real spec gaps, not changes to the plan. Same implementer receives
fix round 1/5 from `b92c3bc`, with targeted functional RED before each fix and
fresh covering/full gates before scoped re-review. Task 2 is not accepted or
pushed. Existing configure skip-install warning remains a recorded Minor.

Cannot-verify items resolved by scope/evidence: UI activation/static host/DPI
are Tasks 3/4; protected bytes freshly matched above; root read retained
historical RED logs. Root also checked the current standalone Task 2 build
recursively: zero cmake_install.cmake files. Prior Task 1 helper negative-install
evidence remains applicable because the helper is unchanged. No unsafe install
or GUI command was executed to establish this check.

Ruling: Task 3 may extend `tests/motionlab/CMakeLists.txt` for its specified
widget/shell test target — the original Task 3 file list omitted that necessary
wiring although its tests were mandatory — cost if wrong is reversible test-
build glue only. Plan and prepared brief corrected before dispatch; original
UI harness and static-host/no-install policies are not changed by this ruling.

## Task 2 accepted

Scoped fix `ad608bf67636a885ccdd8dde3b55e821b3322bf0` addresses all four
Important findings; independent scoped review says all addressed and no new
Critical/Important breakage. Replacement state commits transactionally, diagnostics
use a separate bounded latest snapshot, Stop supersedes an earlier seek, and
output clamp derives one common scale. Each new regression failed before its
fix: actual 16,777,216 pixels over the cap, eight queued diagnostics, stopped
position 0.7, and landscape/portrait both incorrectly 1024x1024.

Final covering QtTest 8/8, full worker QtTest 30/30 and CTest 2/2 pass. Controller
fresh verification on committed `ad608bf`:
`ctest --test-dir D:/rvc/c++/DragonianVoice/Avelabs-UI/out/diagnostics/motionlab-2026-09-24/task2-md -C Release --output-on-failure --output-log C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part26a-task2-controller-ctest.log`
with existing dynamic Qt bin prepended only in that process: exit 0, 2/2,
3.45s. Full host range `7084563..ad608bf` passes `git diff --check`. No build or
test code exists outside the assigned Task 2 paths. Known skip-install configure
warning remains the only deferred review Minor; no blanket suppression.

Full reports and scoped diff remain in SDD workspace, raw outputs in
`out/part26a-task2-*`. Ordinary host push follows remote check (expected prior
remote `7084563`); no force/rewrite. Next Task 3 adds opt-in UI in the existing
Voices page; no claim of completed static host link, GUI/DPI or native playback.

Ordinary host push completed and remote main exact equality verified:
`ad608bf67636a885ccdd8dde3b55e821b3322bf0`. Engine documentation checkpoint
follows. No Task 3 product writer started before review acceptance.

## Task 3 and final-gate preparation

After both Task 2 pushes, sole writer `/root/qt_lab_page` started Task 3 from
host `ad608bf`; engine docs HEAD `0d4c793`. Ten explicit widget/entry/test/main/
CMake paths, no worker/core/vendor changes. Root found no competing GUI process
before releasing its sequential owned temporary-INI harness. Task 3 is not yet
accepted. Root performs no competing builds/tests/GUI while that writer works.

Independent read-only Task 4 measurement audit is retained in the plan workspace.
It confirms Loop default but requires observed wrap/advancement for valid active
samples. It distinguishes cumulative image/delivery/replacement counters from
last-batch render time and Player counters reset by replacement. Controls must
be serialized because acknowledgements share file generation; a post-Pause
transition redraw is recorded separately before measuring settled pause.
The prepared protocol/context now include these qualifications; no timing or
memory measurement has yet run.

Ruling: do not add a product API merely for scratch measurement metadata — the
existing controller does not expose actual Runtime asset duration/frame count,
which are not necessary for the agreed phase observations — retain authored
metadata labels and mark Runtime-only fields unavailable unless independently
observed. Cost if wrong is less metadata in this report, reversible in a later
instrument design. No GUI FPS, native/GPU speedup or zero-allocation claim.

Task 3 product commit `98841e0` is local only. Independent review confirms spec
scope, but requests one Important fix: shared DPR physical-size calculation
instead of duplicate canvas/page policies. Root verified the duplication;
same implementer handles fix round 1/5 from `98841e0`. No push or Task 4 execution
before scoped acceptance. Minimum-size containment/loaded-error coverage and
pre-release seek-negative coverage are deferred Minors for final-review triage.

Root read final retained worker30/30, page8/8, smoke and CTest3/3 output, viewed
the unloaded minimum-size screenshot, and freshly rechecked protected source157/
Release22 hashes unchanged. Final dynamic/static configure trees have no install
scripts. Historical functional RED and complete build stdout exist only in task
terminal responses; the implementation report explicitly does not relabel the
old timeout log as RED. Native DPI, actual static host link and runtime matrix
remain future gates; known original-source build warnings remain documented.

Task 3 fix1 `115f70b` independently accepted for the duplication finding. Root
fresh verification then caught an intermittent page-test failure: paused image
counter advanced from 5 to 6 after control acknowledgement. Reproduced on the
second bounded diagnostic run, raw `out/part26a-task3-repro-2.txt`. The existing
worker acknowledges controls before its deferred paused redraw; that signal is
not an idle fence. Same implementer handles fix2, limited to deterministic test
synchronization after confirming the ordering. Do not weaken paused steady-state
semantics or alter Player behavior. Task3 remains unaccepted/unpushed; positive
reruns are recorded but do not hide the earlier failure.

## Task 3 accepted

Host range `ad608bf..3b4742602ced90896232254b3e724834689bd5c2` contains
`98841e0` embedded page, `115f70b` shared DPR calculation and `3b47426` test-only
pause synchronization. Independent fix1/fix2 reviews accepted their findings;
no new Critical/Important breakage. The pause test snapshots delivery count at
the acknowledgement, waits for the subsequent paused batch and diagnostics,
then preserves its original no-growth interval and normalized seek/Stop checks.
No worker/engine/API behavior was changed for the test failure.

Retained focused25/25 fresh-process logs, full page9/9 and CTest3/3 passed.
Root fresh committed-head CTest3/3 (worker30/30,page9/9,9.19s) passed with PTY:
`ctest --test-dir D:/rvc/c++/DragonianVoice/Avelabs-UI/out/diagnostics/motionlab-2026-09-24/task3-page-md3 -C Release -V --output-log C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part26a-task3-fix2-controller-ctest.log`.
Dynamic Qt bin/plugin environment was process-local. Full Task3 diff check
passed; exactly ten assigned files changed, no normal/test GUI process remained.
Host remote was still `ad608bf`, engine remote `0d4c793` before scoped pushes.

Cross-task limits are resolved by explicit scope: generated OFF project has no
motion compile/link input; actual static /MT host link and broad host suites are
Task4. Native DPI/manual normal-EXE acceptance remains deferred as the plan
permits. Minimum-size containment/loaded-error and pre-release negative seek
coverage Minors go to the final whole-stage reviewer, alongside build warnings.
Historical RED raw-output limitations stay disclosed. Next Task4 uses the
prepared protocol/context and does not repeat the completed development loops.

Ordinary host push completed; remote main exact equality verified at
`3b4742602ced90896232254b3e724834689bd5c2`. No force/rewrite, no foreign changes.
Engine documentation checkpoint records this acceptance before Task4 dispatch.
