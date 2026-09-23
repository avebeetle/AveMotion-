# AveMotion autonomous work state

Updated: 2026-09-23

## Repository

- Branch: `main`
- Remote: `https://github.com/avebeetle/AveMotion-.git`
- Baseline commit: `cda415c`
- Baseline provenance: all 2,478 files matched
  `C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24.zip` byte-for-byte.
- Prior Git history: unavailable locally and absent from the previously empty
  remote; `cda415c` intentionally begins new history.

## Current stage

The user approved Part 25A and delegated autonomous planning/execution:

`docs/superpowers/specs/2026-09-23-persistent-reference-sessions-design.md`

Implementation plan:
`docs/superpowers/plans/2026-09-23-persistent-reference-sessions.md`

Execution method: subagent-driven implementation with a fresh reviewer per
task, directly on `main` as the user authorized. No routine approval is pending.

Task 0 reproduced both seek and complete ascending-scan corruption in Telegram
and Samsung. Durable findings: `docs/PART25A_REFERENCE_SESSION_FINDINGS.md`;
raw scratch evidence: `out/part25a-probe/findings.md`. Direct scene and model
reuse are rejected. The independent trim-cache investigation and broader
recording-lifecycle assessment are complete in `out/part25a-trim/findings.md`
and `out/part25a-recording-design/proposal.md`. A general fix requires more than
dash/trim invalidation. This run takes the approved safe branch: fresh sampling
stays, persistent reuse and its speedup target remain incomplete.

Task 1 diagnostics complete: `bcde48b`, with reviewed Samsung-test correction
`f9b1e4e`. Full Telegram debug 57/57 passed; the controller freshly reran focused
seams tests on Telegram and Samsung, both passed. Independent task review and
scoped fix review passed. No session lifetime or visual behavior changed.
Task 2 measurement instrumentation and report-directory safety are complete at
`94b08bf` plus reviewed correction `c484f4b`. Independent review found first-sample
timing after warm-up and a safety-test dependency on preset binary paths; both
were fixed with RED/GREEN tests and passed scoped re-review. The controller
freshly rebuilt and reran the focused corpus gate: 5/5 passed in 9.25 seconds.
The preceding full Telegram suite passed 59/59 at `94b08bf`; final full gates
remain Task 5. `MEASUREMENT_BASELINE=c484f4b`, corrected Release sanity 16/16
assets validated. Raw context: out/benchmarks/part25a/MEASUREMENT_BASELINE_REVIEWED.txt.
Original pre-review timing observations are superseded, not valid evidence.

Task 2 removes observed unconditional recursive output-directory deletion in
the corpus CLI and runner; its regression tests preserve unrelated files and
inputs while regenerating the tool's named reports.

Task 3 tests complete at `7b33c03`: independent task review passed; controller
freshly rebuilt/reran the new reference-session target on Telegram (16.86 s)
and Samsung (15.69 s), both 1/1 passed. The temporary naive-reuse mutation failed
on actual dash geometry in both variants and is fully restored. No production
runtime/vendor/golden changes. Full scene fields, nine fixture families, viewport,
CPU and two-instance functional isolation are covered; TSan remains unavailable.

The existing Samsung scene-golden test fails Polystar p100/frame150. Independent
clean `cda415c` MSVC build reproduces the exact same hashes, so this is not a
Task 3 regression. Raw baseline proof: out/part25a-samsung-golden/clean-baseline-build-test.txt.
The numerical investigation traced the exact hashes to a sine-rounding boundary:
one ULP in sine becomes two ULP in one coordinate and its derived left bounds.
Full trim does not split the path at this endpoint. Findings and reproduced
hash reconstruction: out/part25a-samsung-golden/findings.md. No golden/allowance
is changed; full Samsung validation remains unresolved rather than claimed green.

Task 4 complete at `4ab196d`: independent spec/quality review passed, controller
fresh asset-model test passed on Telegram (7.49 s) and Samsung (0.03 s). Tests pin
fresh model-session counts, idempotent publication, scene-role isolation and
failed retry counters, including the preparation timeline limit and Samsung's
unsupported capability. No production/golden change; RED mutation restored.

Task 3 integration fix complete at `b3bb269`, independently reviewed: moving the
identical dash fixture into tests/fixtures/reference_sessions preserves the
auto-enumerated 16-asset corpus. Both test consumers pass on both variants.
Earlier full-gate failures at bbfad2d were this fixture-placement regression;
they are corrected without changing corpus, generator or goldens.

Task 5 full verification at b3bb269: all four configurations/builds PASS.
Telegram debug 60/60 (52.43 s); Win32 preview 54/54 (46.59 s), including explicit
capture/WARP/device recreation; no-reference Direct2D 29/29 (2.47 s).
Samsung debug 38/40 (14.05 s): scene and plan golden fail on the same Polystar
endpoint. Both exact failures reproduce in clean cda415c, independently rerun by
the controller. No Samsung all-green claim. Local-prefix installation and the
external find_package consumer configure/build/run also passed, exit 0.
Logs: out/part25a-final/*-b3bb269.txt and out/part25a-samsung-golden/.

Repeated baseline observations are recorded in
`docs/PART25A_PERSISTENT_SESSIONS_REPORT.md`: aggregate exact median 22.125 us,
full CPU pipeline 34.625 us; 1000 fresh scene sessions for 1000 steady samples.
Evaluator/projector storage counters stayed stable; planner allocations and TSan
remain unverified. No optimized candidate and no A-B speedup claim.

Task5 independent whole-change review complete over cda415c..ee25b71: no
Critical/Important issues, one Minor stale comment corrected at7b5221d and
approved by a separate scoped reviewer. No review finding remains open.
Controller post-review full configure/build/CTest rerun at7b5221d: Telegram60/60
(73.60s), preview54/54 (62.28s), no-referenceDirect2D29/29 (22.77s), Samsung38/40
(66.08s), with the same two baseline-proven hashes. Logs *-7b5221d.txt.

Safe partial delivery is finalized; persistent reuse/2x target remain NOT
implemented and Samsung is NOT fully green. Next is only final documentation
push and stopping avemotion-6. Preserve raw evidence and the clean baseline
scratch checkout for the unresolved Samsung/recording-lifecycle follow-up.
Do not redispatch Tasks0–5 or reopen lifecycle implementation in this run.

The plan has been corrected for explicit WARP/preview presets, actual CTest
names, measured workspace growth, corpus schema changes, model-result errors,
and a safe deferred-reuse branch. The spec contains the execution amendment.

SDD ledger:
`.superpowers/sdd/2026-09-23-persistent-reference-sessions/progress.md`

Deadline: 2026-09-23 08:32 UTC / 11:32 Moscow. Start no major block after
08:12 UTC. Reserve the final block for review, verification and handoff.

## Fresh baseline verification

- Configure: `cmake --preset windows-msvc-telegram-debug` — passed with
  MSVC 19.44.35229.0.
- Build: `cmake --build --preset windows-msvc-telegram-debug --parallel 4`
  — passed. The first parallel run stopped while linking one test executable
  without a linker diagnostic; an isolated verbose reproduction and the full
  incremental build both passed, so the initial failure's cause is unconfirmed.
- Tests: `ctest --preset windows-msvc-telegram-debug --output-on-failure` —
  57/57 passed in 65.55 seconds.

## Duplicate-run rule

Scheduled and interactive runs read this file and the plan-scoped ledger first.
Resume the next incomplete task; do not regenerate the plan or redispatch
completed work. A heartbeat in this same task is normally a continuation after
the preceding turn ended. A stale "running" note alone is not proof of a second
controller. Check live agents and ledger ownership before deciding to skip;
collect a finished worker's report and continue its review. Skip only a proven
duplicate controller or already-running identical worker.

Automation `avemotion-6` is ready to stop after this safe-scope handoff.
Final closeout must verify the ordinary main push and pause it through the app
tool; no remaining ordinary clarification or repeated execution is needed.
