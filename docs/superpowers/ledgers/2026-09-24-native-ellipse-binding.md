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

Task2 product9ba9376: complete implementation, independent review pending.
Functional RED and final focused3/3/full72/72,85.53s retained. User input ended
the original worker before its report/commit; inventory then proved no live agent
or compiler/test, so a bounded finalizer inspected existing work and recovered
handoff without repeating implementation. Root fresh scoped4/4,0.91s and full
Telegram72/72,83.15s pass at9ba9376. Preview/none/provenance gates follow serially.
The sole dirty spec refinement distinguishes default timeline clip from visual
clipping. An interim static-path assumption was corrected: evaluateFrame canonical
promotion deliberately clears a duplicate local path; this is not culling proof.
No production route/native playback claim or UI change is made.

Root pre-fix cross-preset gates at9ba9376 pass: preview66/66,85.73s;
none/Direct2D31/31,3.39s; vendor all and TGS16. Fresh none40 source entries have
zero reference/privateC; Samsung92 entries/36reference have zero privateC.
Independent Task2 review: Needs fixes, I1 instanceId/instanceHandle omitted from
per-frame identity. Root verified omission/public stamping; bounded fix round1/5
dispatched with functional mixed-real-instance RED and invalid/drift checks.
Final platform checks must cover fixedcode; no push while I1 remains open.

Task2 fix round1/5: I1 addressed at0fa0508, scoped independent re-review accepts
with no new Critical/Important or out-of-scope observations. Root fresh scoped
4/4,0.89s. Task2 implementation/review complete(e63ffe3..0fa0508); final stage
cross-preset gates and whole-stage review remain open. Pre-fix25 evidence files
archived under out/part26c/pre-fix-9ba9376 before new runs; none were discarded.

Post-fix root full Telegram72/72,87.76s at0fa0508 passes. Exact remote main
e63ffe3 remains unchanged before task handoff. Scope docs include explicit
instance identity clarification; report still marks final stage closure pending.

Reviewed handoffe7b2c32 ordinarily pushed, remote exact equality/clean tree verified.
Final post-fix gates allpass: Telegram72/72,87.76s; preview66/66,85.48s;
none31/31,3.37s; vendorall/TGS16 and fresh compile/build.ninja boundaries. None40
entries/0reference/0private; Samsung92/36reference/0private. Identitylogs record
Telegram/preview source0fa0508 with docsdirty, none/provenance docsonlye7b2c32.
Whole-stage reviewer native_binding_final_review(astra high) active over full
d212a49..e7b2c32 range. No final acceptance or own playback claim yet.

Whole-stage review With fixes: sole Important I1 is missing render-layer self-ID
equality in finalRowsAgree; accessors only check index/presence. Root verified
predicate and accessor. One final fix wave dispatched to certificate finalizer,
BASEe7b2c32: two row comparisons, four malformed-row regressions, poison outcome
and preserved coherent reindexing. Existing25 gate evidence files hash-verified
into out/part26c/pre-final-fix-0fa0508 before subsequent reruns.

Ruling: defer nonblocking binder-length refactor(M1) until subset expansion —
linear validation already has tested guards and no associated correctness defect;
unrelated restructuring would widen this bounded final fix — costs future review/
maintenance effort and a later independently verified helper extraction.

Final review declined items resolved by controller: geometry/history/plans remain
D; erased ID provenance remains unproved; render diagnostic name hashes/dependency
summaries are outside metadata certificate (not complete model/fingerprint parity);
existing Asset's internal reference lease is intentional, not own ingestion;
production/nativepixels/noneingress/host remain separate; Samsung/ANGLE/TSan/
zeroallocation/speedup claims are absent; rawgrammar expansion is prohibited.
No declined item is silently considered completed. No additional product scope.

Final fix6b0be0f accepted by independent scoped final review: I1 addressed,
no new breakage; M1 deferred. Six malformed root/shape self-ID tests have live
functional RED0/1 and focusedGREEN4/4,0.97s. Root final fullgates at6b0be0f:
Telegram72/72,94.94s; preview66/66,97.92s; none31/31,3.45s; all-vendor/TGS16,
fresh none/Samsung source/build boundaries (same graph hashes verified). No
product edits after these gates. No active writer/reviewer/build. Bounded C
accepted, report complete. Final docs seal/push next, then D design continuation;
no goal-complete or own playback claim, no schedule mutation or scratch cleanup.
