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
reuse are rejected. A bounded independent trim-cache root-cause investigation
continues in `out/part25a-trim/findings.md` when available.

Task 1 diagnostics complete: `bcde48b`, with reviewed Samsung-test correction
`f9b1e4e`. Full Telegram debug 57/57 passed; the controller freshly reran focused
seams tests on Telegram and Samsung, both passed. Independent task review and
scoped fix review passed. No session lifetime or visual behavior changed.
Next block is Task 2 measurement instrumentation and report-directory safety.

Task 2 also removes observed unconditional recursive output-directory deletion
in the corpus CLI and runner. Regression tests must preserve unrelated files
and inputs while regenerating the tool's named reports.

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

Automation `avemotion-6` remains active every 15 minutes until the deadline.
Routine unchanged status remains quiet; report completion, material failure or
required user action. At completion/deadline, deliver evidence and stop this
automation through the app tool.
