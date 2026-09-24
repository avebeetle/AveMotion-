# Part26C durable ledger

Spec: docs/superpowers/specs/2026-09-24-native-ellipse-binding-design.md
Plan: docs/superpowers/plans/2026-09-24-native-ellipse-binding.md
Base: d212a49d69d75b665914a75f25210f4f42602a72

Controller approves this reversible design/plan under user delegation, not as
an assertion that the user has read it. User requests long execution; continue
C -> D -> independent supported-subset ingestion -> isolated host integration,
each with its own written correctness gate. No scheduler changed.

Ruling: use a private cold factory, not a production model-scan observer — avoids
publication/cache changes before proof — costs one extra cold full-timeline scan
in this opt-in test path; later integration may remove it after correctness.

Ruling: certificate covers observable ID/model correspondence, not hidden
upstream-ID provenance — bridge erases fallback origin — stronger activation
proof requires a private seam later; claiming more would be unsound.

Ruling: numeric eligibility follows checked double-then-float (including frame
rate), without changing exact raw admission — matches pinned parser storage —
some admitted inputs remain ineligible and descriptor precision stays exact.

Ruling: main execution and preserved SDD/raw records follow explicit user choice
over generic worktree/cleanup workflow — enables recoverable continued work —
requires scoped staging and remote checks before each ordinary push.

## Preflight

| Pair/task | Interface/shared surface | Finding |
|---|---|---|
| Task1 -> Task2 | Named IDs in NativeEllipseModelBinding | Same signatures; Task2 adds source lease proof, not assumed by Task1. |
| Task1 / Task2 | CMakeLists Telegram blocks | Sequential writers only; no conflict. |
| Task1 | RED, binder, test matrix | Functional stub RED; exact float interpretation explicitly separate from raw grammar. |
| Task2 | Factory, audit, failure/lifetime tests | No production route; double scan cost and hidden provenance limit explicit. |

Design commit cfe4d14 pushed and remote equality verified. Task1 writer
/root/native_binding_implementation active; functional RED captured after clean
target build: baseline authored binding published fails, 0/1, expected.

Ruling: add a private supplied-Runtime factory overload before Task2 — the
one-argument factory destroys its Runtime, preventing later real diagnostic
deltas — slightly larger private API, but no public change or retained Runtime;
both forms load their own exact bytes, snapshots are before/after and measured
with a quiescent supplied Runtime. Future D audit confirmed this requirement.

User reconfirmed host destination D:/rvc/c++/DragonianVoice/Avelabs-UI. Engine
tests precede real host acceptance; long-work destination is statically embedded
own supported playback in isolated Motion Lab, not a separate AveMotion DLL.

Task 1: complete (cfe4d14..3f8bf0c), independent spec/quality review Approved;
no Critical/Important findings. Functional RED and scalar-alias matrix RED/GREEN
retained. Focused3/3, full Telegram71/71 in79.97s; root fresh rebuild+focused3/3
at committed3f8bf0c,0.71s. Initial70/71 run was outside VsDevCmd; compiler
discovery failure corrected by proper invocation, no test/product workaround.
Task1 minor (deferred): long bindNativeEllipseModel function, line213; whole-stage
review will triage. Environment-log minor is resolved, failed raw log retained.
Cannot-verify items: none/Samsung actual graph gates belong to controller closure;
source lease/slot proof is explicitly Task2, not a missing Task1 feature.
Task2 may proceed after handoff commit/push. No code changes after root focus.
