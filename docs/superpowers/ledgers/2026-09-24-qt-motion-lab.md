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

- [ ] Task 1 — opt-in static dependency and no-install boundary.
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
