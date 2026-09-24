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
- [ ] Task 2 — bounded serial worker and Qt controller.
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
