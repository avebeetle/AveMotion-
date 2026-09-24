# Part26C — native ellipse input/model and slot correspondence

2026-09-24. Intermediate verification report; final gates and whole-stage review
are still pending. This stage does not activate native playback or change UI.

## Delivered boundary

The private authored binder interprets the exact admitted descriptor against the
pinned float model, resolves semantic node/property IDs, and validates topology,
values, references and tracks. It accepts coherent renumbering rather than assuming
fixed table offsets. Raw admission and numeric/model eligibility remain distinct.

The private cold factory owns the accepted source bytes, loads those same bytes,
prepares and binds the frozen model, then audits every source frame on a separate
instance. It publishes a const certificate only after the complete scan succeeds.
Its asset lease is pointer/handle-generation/hash/model bound; another same-byte
asset cannot substitute. Mutated scenes, incomplete/repeated/skipped scans, resource
or layout disagreement and unsupported defaults fail closed. The reviewed identity
fix additionally rejects mixing frames from different instances of the same asset.

The certificate owns fixed slot metadata and immutable leases, not per-frame paths,
scene trees, an Instance, Animation or Runtime. Two factory overloads support a
local Runtime or a supplied, quiescent Runtime with actual before/after diagnostics.
No ordinary production caller selects this API; none/Samsung do not compile it.

## Commits and review

- cfe4d14: C design and implementation plan.
- 3f8bf0c: exact authored-model binder; Task1 independent spec/quality Approved.
- e63ffe3: reviewed Task1 handoff and private factory refinements.
- 9ba9376: full-timeline certificate and streaming audit.
- 0fa0508: reject cross-instance observations; functional mixed-instance RED
  followed by focused GREEN. Scoped independent re-review accepts the fix,
  with no new findings.

Task1 minor: the binder's long validation function remains for whole-stage review
triage. Task2 initial review found the instance-identity omission; it was not
dismissed because the factory happened to use only one instance.

## Evidence

Baseline has N=61 frames. Successful preparation measures 61 model samples plus
61 additional scene samples and zero CPU-rendered frames. These are cold setup
costs, not speedup evidence. A nonempty supplied Runtime verifies deltas rather
than assuming counters start at zero. No emission performance claim is made.

TDD: initial binder and certificate builds succeeded before their functional
baseline-publication tests failed. Matrix failures and corrected results remain
under out/part26c/task1 and task2. The initial Task1 full test failed solely because
legacy_subproject could not discover MSVC outside VsDevCmd; the corrected full run
passed without a product or test workaround.

Root focused tests at 0fa0508:4/4,0.89s and full Telegram72/72,87.76s.
Remaining final full gates are running; do not read
the following earlier counts as validation of the later fix:

| Pre-fix preset at9ba9376 | Result |
|---|---|
| windows-msvc-telegram-debug |72/72,83.15s|
| windows-msvc-win32-preview |66/66,85.73s|
| windows-msvc-direct2d (none) |31/31,3.39s|

Those raw pre-fix logs were preserved in out/part26c/pre-fix-9ba9376 before new
gate runs. Gate commands, each under VsDevCmd -arch=x64, are cmake --preset PRESET,
cmake --build --preset PRESET --parallel4, and ctest --preset PRESET
--output-on-failure. Actual command scripts/logs are out/part26c/run-preset-gate.cmd
and root-PRESET-{identity,environment,configure,build,ctest}.log.

Provenance commands: python scripts/verify_vendor.py --variant all;
python scripts/generate_tgs_compatibility_corpus.py --check. Generated compile
graphs for freshly configured none and Samsung are also checked. The Samsung
graph check is not a full Samsung suite pass; historical Polystar failures remain
separate. Fixtures/goldens/vendor/licensing are unchanged.

## Interpretation and limitations

The bridge does not preserve whether a render ID was upstream or an index
fallback. C proves observable consistency on the exact leased source, not hidden
ID-origin provenance or production-route eligibility. Geometry emission parity
is a subsequent D gate, not implied by a complete C metadata scan.

MotionAssetModel::clips contains a default timeline clip, not visual clipping.
The audit validates that row and still prohibits evaluated clip paths. Similarly,
empty static localPath from evaluateFrame can mean canonical promotion discarded
a duplicate: Runtime.cpp:508-556. It is not evidence of off-screen culling.
evaluateModelFrame/applyAssetModel has a different local-path ownership contract.

No own loader, own pixels, no-reference playback, zero-allocation, TSan, generic
Lottie coverage, speedup or final host acceptance is claimed by this stage.
Avelabs remains unchanged at712d454; UI/out remains absent. Its accepted main
build/Release is not replaced. Existing Motion Lab still uses reference CPU.

## Decisions made under delegated authority

1. Private cold factory instead of a production scan observer: avoids publication
   changes, costs an extra N cold samples; wrong choice costs later integration
   refactoring, not silently altered production behaviour.
2. Certify observable IDs, not erased upstream origin: stronger activation needs
   a later seam; overestimating this proof would invalidate route eligibility.
3. Checked double-to-float numeric interpretation, including frame rate: matches
   pinned storage while preserving exact raw admission; some admitted values stay
   ineligible and broader numeric support would require another design.
4. Work directly in main and preserve SDD/raw evidence per user choice: reversible
   scoped commits and exact remote checks replace branch isolation/cleanup.
5. Add private supplied-Runtime overload: enables real live diagnostic deltas;
   costs one private API overload but no retained Runtime/public API change.

## Continuation

After final C acceptance: private native scene stream with exact ordinary scene,
history and complete render-plan comparison; own bounded ingestion/model creation
in none; reference-neutral playback timeline; native Direct2D raster validation;
static integration into isolated Avelabs Motion Lab for host acceptance. No separate
AveMotion DLL is planned. Each subsystem has its own design, TDD and review gate.
No automation is resumed or created by this interactive long-work request.
