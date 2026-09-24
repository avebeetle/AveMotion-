# Native Ellipse Stream Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Emit the certified ellipse subset's ordinary model-applied scenes without per-frame reference work, with exact scene, history, ownership and render-plan evidence.

**Architecture:** A private serial Rendering stream consumes the immutable Part26C certificate. PropertyEvaluator and the existing primitive generator produce owned scene vectors; applyAssetModel supplies canonical model aliases. Independent test-only ordinary sources/models/planners provide the expected output. No production route or host is switched.

**Tech Stack:** C++20, MSVC x64, existing CMake/CTest Telegram reference, current Evaluation/Rendering libraries; no new dependencies.

**Spec:** docs/superpowers/specs/2026-09-24-native-ellipse-stream-design.md

## Global Constraints

- Work directly in main; scoped commits and ordinary push only, no history rewriting.
- Preserve Part25I admission grammar, codes, paths and resource limits.
- Keep D private and Telegram-only; none/Samsung must not acquire D sources or tests.
- No ordinary production caller selects native emission; do not change Runtime.cpp, public APIs/fallback, Player threading, Direct2D ownership or ANGLE/backend coverage.
- Do not modify vendor sources, versions, patches, licenses/notices, committed fixtures or goldens.
- Do not add dependencies, change Windows settings, write/build/run GUI in Avelabs-UI, or recreate UI/out.
- Preserve raw evidence and SDD records; do not create or resume an automation.
- Exact comparisons use no epsilon, fingerprint-only shortcut, cached reference geometry or weakened expected output.
- One product implementer at a time; TDD and independent task/whole-stage review before ordinary push.

## Review Focus

1. Tiny positive geometry and offscreen/edge motion must preserve empty-path hashes, local paths and exact float order, not assumed culling (Task1 matrix).
2. A reverse-order oracle sharing the candidate's mutable source or using frame+1 as sequence can validate the wrong implementation (Task1 source/model independence; Task2 history).
3. Failed viewport requests must consume an attempt sequence but not successful history; clamped frames must remain ordinary-compatible (Task1 boundary checks; Task2 history).
4. Equal-valued canonical copies are not model-owned aliases; retained plans must survive stream and external owner destruction (Task2 lifetime/owner checks).
5. Shared static resources must not merge animated per-instance geometry/history, and inactive/presentation transitions must produce correct updates (Task2 two-stream/full-plan tests).

## File map and verification environment

- src/render/NativeEllipseStream.hpp/.cpp: private stream/results, generation/materialization/history only.
- tests/support/NativeEllipseTestAssets.hpp: named in-memory transformations of the existing fixture, no parser/product helper.
- tests/support/NativeEllipseOracle.hpp: owned independent ordinary source/model, fresh ordinary sample and request history; explicitly counted oracle work.
- tests/native_ellipse_stream_tests.cpp: creation, full scene/record matrix, numeric and viewport boundaries.
- tests/support/ExactRenderPlanComparison.hpp: explicit exact field comparator, no production dependency.
- tests/native_ellipse_stream_lifecycle_tests.cpp: comparator mutations, plan/history, ownership, isolation and counters.
- CMakeLists.txt: Telegram-only Rendering source and test registration.
- Controller owns spec/plan/STATE/durable and SDD ledgers/reports. Workers own listed product/test files and their own raw/report artifacts.

Run every MSVC configure/build/CTest within an environment initialized with
`C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/Tools/VsDevCmd.bat -arch=x64`.
CTest itself needs this environment for the legacy-subproject test. Use a local
out/part26d/taskN/*.cmd runner written through apply_patch; retain separate RED,
GREEN and full-suite logs plus git identity. No execution-policy changes.

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake --preset windows-msvc-telegram-debug
cmake --build --preset windows-msvc-telegram-debug --parallel 4
ctest --preset windows-msvc-telegram-debug -R native_ellipse_stream --output-on-failure
ctest --preset windows-msvc-telegram-debug --output-on-failure
```

---

### Task 1: Native stream and independent scene parity

**Files:**
- Create src/render/NativeEllipseStream.hpp/.cpp.
- Create tests/support/NativeEllipseTestAssets.hpp and NativeEllipseOracle.hpp.
- Create tests/native_ellipse_stream_tests.cpp.
- Modify CMakeLists.txt only for Telegram private Rendering source/test registration.
- Raw evidence out/part26d/task1/; task-1-report.md in this plan's SDD workspace.

**Interfaces:**
- Consumes runtime::detail::NativeEllipseCertificate, its input/model/slot leases and matchesAsset; evaluation::PropertyEvaluator and PropertyEvaluationWorkspace; detail::generateEllipsePath; model::detail::applyAssetModel; runtime::computeSceneFingerprints.
- Produces the exact private spec interface in avemotion::render::detail: NativeEllipseStream::create(shared_ptr<const runtime::detail::NativeEllipseCertificate>, uint64_t) -> NativeEllipseCreateResult; emit(size_t,size_t,size_t) -> NativeEllipseFrameResult. Delete copy operations, private constructor.
- NativeEllipseCreateCode = Ready, InvalidCertificate, InvalidIdentity, EvaluationPreparationFailed. Result has code/message/unique_ptr stream and explicit bool for Ready plus non-null.
- NativeEllipseFrameCode = Emitted, InvalidViewport, EvaluationFailed, UnsupportedNumericOutput, ModelApplicationFailed. Result has code/message/optional<runtime::EvaluatedScene> scene and explicit bool for Emitted plus present scene.
- Shared test namespace avemotion::test: NativeEllipseTestAsset {std::string name,json}; nativeEllipseTestAssets(const std::string& seed) -> vector<NativeEllipseTestAsset>.
- Test NativeEllipseOracle(std::string json) owns independent bytes/source/model; freshScene(size_t frame,size_t width,size_t height) -> runtime::EvaluatedScene, independently increments attempt ordinal and computes successful fingerprint history. model() -> shared_ptr<const model::MotionAssetModel>; directSampleCount() -> size_t. It never accepts a candidate model/Asset as construction input.
- Test-only normalization function normalizeNativeOracleIdentity(runtime::EvaluatedScene&, const runtime::detail::NativeEllipseCertificate&, uint64_t instanceId, uint64_t sequence) changes ONLY scene assetHandle/invalid instanceHandle/instanceId/evaluationSequence, never model, paths, history or fingerprints.

- [ ] **Step 1: Declare contract and capture functional RED after successful build.**

Register `avemotion_native_ellipse_stream_tests` and CTest
`avemotion.render.native_ellipse_stream`. Link AveMotion::Rendering and the
existing rlottie::rlottie/Threads::Threads private test dependencies; private
includes src/render,src/runtime,src/model and existing Telegram internal include
variables; fixture-dir definition and avemotion_set_warnings. Add stream source
only inside `AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram"`. Rendering already
links Runtime and transitively Evaluation; never reverse that dependency.

With an initial unsuccessful create/emit stub, assert real baseline success:

```cpp
auto prepared = runtime::detail::prepareNativeEllipseCertificate(json);
require(static_cast<bool>(prepared), "baseline certificate prerequisite");
auto created = render::detail::NativeEllipseStream::create(prepared.certificate, 41);
require(static_cast<bool>(created), "native stream created");
auto emitted = created.stream->emit(30, 512, 512);
require(static_cast<bool>(emitted), "native baseline scene emitted");
require(emitted.scene->assetModelApplied, "native scene applies frozen model");
```

Retain the functional failing output. Do not use a compiler/link error as the RED.
Add desired differential/boundary assertions before their implementation changes.

- [ ] **Step 2: Build the independent ordinary test oracle and exact baseline comparison.**

Read tests/source_ownership_tests.cpp's preparedOrdinaryScene/source-lease helpers
and tests/support/FreshModelOracle.hpp. The existing hash-key oracle is an example,
not a safe drop-in: use a distinct test cache key, owned source text, separately
stamped ordinary source and separately frozen model. Each new ordinary Animation
must be explicitly stamped before sampling. Candidate/oracle must not share a
mutable LOTModel, Animation, vectors or workspace; retain ordinary source lease
throughout. Use current AnimationAccess mechanisms to assert identities. Do not
change global cache configuration concurrently. NativeEllipseOracle must not
construct its model from candidate-emitted frames or call NativeEllipseStream.

```cpp
NativeEllipseOracle oracle(json);
require(oracle.model().get() != prepared.certificate->model.get(), "independent model");
auto expected = oracle.freshScene(30, 512, 512);
auto actual = created.stream->emit(30, 512, 512);
require(static_cast<bool>(actual), "native frame success");
ExactSceneComparison comparison;
require(comparison.difference(expected, *actual.scene).empty(), "exact scene and record");
require(actual.scene->assetHandle == prepared.certificate->asset->handle(), "real asset lease");
require(!actual.scene->instanceHandle.valid() && actual.scene->instanceId == 41,
        "honest private identity");
```

Keep existing ExactSceneComparison unchanged. On mismatch print fixture/order/
frame/viewport/ordinal, first differing field and float bit representations of
relevant transform/path values. Count direct ordinary loads/samples explicitly.
No stored oracle-frame corpus or sampling shortcuts.

Concrete oracle construction: cache-disabled
`rlottie::Animation::loadFromData(json, "native-ellipse-oracle-" + uniqueSuffix, {}, false)`;
derive width/height/frameRate/totalFrame from that seed, sourceHash from exact bytes
with core::fnv1a64 and debugName=core::formatHash(sourceHash). Use test-only
AssetHandle{700001,1} in AssetModelDescriptor (not a claimed Runtime registration).
Retain AveMotionAnimationAccess::model(*seed), stamp that exact source once through
buildTelegramParsedModel(source,descriptor), then release seed. Build/update the
independent frozen model by N logical-size samples, each with a fresh ordinary
AveMotionAnimationAccess::fromModel(source); finalizeAssetModel after the full scan.
Each expected request also constructs fresh fromModel(source), not loadFromData
with the candidate cache key. All oracle Animations share only their OWN stamped
source lease; no separate descriptor-only Runtime Asset retaining a potentially
shared production source is needed. Track one parse plus model/request sample
counts. Test two simultaneously live ordinary Animations are distinct and point
to the oracle lease, and two cache-disabled parses have distinct source pointers;
do not compare freed allocation addresses, which can legitimately be reused.

- [ ] **Step 3: Implement creation, native generation, model application and history.**

Creation checks certificate/asset/model/input consistency through matchesAsset,
nonzero supplied identity and valid PropertyEvaluator, then prepares one owned
workspace. Store certificate, evaluator/workspace, attemptSequence, previous
fingerprints and hasPrevious; no Runtime/Instance/Animation/tree/oracle callbacks.
An allocation failure follows existing C++ allocation behaviour; no claim of
recovering all bad_alloc is introduced. Methods report the defined checked errors.

Use bound property and group/source node IDs, never constant table offsets.
For static Vec2 values resolve the checked AssetReference into model.vec2Values;
for animated properties use Materialized Vec2; reject invalid/type-mismatched
evaluation. Use the bound group's supported worldMatrix. Preserve pinned float
arithmetic and independent local/final path materialization:

```cpp
const float scale = std::min(float(width) / float(model.logicalWidth),
                             float(height) / float(model.logicalHeight));
const float tx = (float(width) - float(model.logicalWidth) * scale) / 2.0F;
const float ty = (float(height) - float(model.logicalHeight) * scale) / 2.0F;
runtime::AffineTransform transform{world.m11 * scale, world.m12 * scale,
    world.m21 * scale, world.m22 * scale, world.dx * scale + tx, world.dy * scale + ty};
auto primitive = generateEllipsePath(position, size, direction);
// Copy verbSpan/pointSpan into local path. Materialize final points with the
// bridge's transform arithmetic order; check finite transforms and points.
```

Implement small local helpers for property resolution, path/bounds/hash materializing,
layer/draw assembly and history, not a new generic rendering framework. Primitive
empty path hash is0; otherwise FNV count/verbs/x-y float bits match bridge. Slot
facts supply checked layer/layout/IDs/paint/default stroke/local flags. Root stays
visible; shape visible only in half-open active interval. Keep both layers and
child reference while inactive; zero draw/path/paint counts then. Final geometry
alone contributes scene path counts. No assumed offscreen culling. Retain generated
localPath for model-applied scenes; ordinary promotion's clearing is another route.

```cpp
auto applied = model::detail::applyAssetModel(certificate_->model, scene);
// On failure return ModelApplicationFailed without a scene or history update.
scene.fingerprints = runtime::computeSceneFingerprints(scene);
scene.changes.firstEvaluation = !hasPrevious_;
if (hasPrevious_) {
    scene.changes.topologyChanged = scene.fingerprints.topology != previous_.topology;
    scene.changes.geometryChanged = scene.fingerprints.geometry != previous_.geometry;
    scene.changes.paintChanged = scene.fingerprints.paint != previous_.paint;
    scene.changes.visualChanged = scene.fingerprints.scene != previous_.scene;
}
previous_ = scene.fingerprints;
hasPrevious_ = true;
```

Attempt sequence increments before viewport validation; clamp frame toN-1.
Zero width/height returns InvalidViewport and no scene. Nonfinite derived output
returns UnsupportedNumericOutput, no partial scene. Never prepareModel in create/
emit, or use SourceGeometryProjector/cached reference paths. Upstream change bits
remain SceneChangeNone, documented as excluded from ordinary equality.

- [ ] **Step 4: Run named exact matrix and boundary regressions.**

Build15 independent named inputs from the committed seed, with checked in-memory
replacements (missing/ambiguous target is a test error, not silent no-op):

| Name | Difference from seed |
| --- | --- |
| animated | unchanged |
| static-visible | replace ellipse position property with static [0,0] |
| static-far | static position [-32768,32768] |
| activity | layer ip10/op20, composition remains0/61 |
| fractional-rate | fr59.94 |
| nonsquare | logical w640,h360 |
| fractional-coordinates | layer translation[255.25,256.75,0]; position start[-75.5,.25], end and terminal[75.5,.25] |
| color-boundaries | color[.5,.25,.75,1] |
| linear-easing | outgoing(0,0), incoming(1,1) |
| nonlinear-easing | outgoing(.125,.875), incoming(.875,.125) |
| equivalent-names | equivalent 6e1 frame rate/6.1e1 end, escaped Unicode layer name, empty group/ellipse/fill names |
| tiny-collapsing | animated position, size[1e-20,1e-20] |
| edge-crossing | start[-400,0], end and terminal[400,0] |
| tiny-surviving | static[0,0], size[.0001,.0001] |
| boundary-translation | static[0,0], layer translation[.25,256,0] |

Use valid JSON numeric spelling0.5,0.25,0.75,0.125,0.875,0.0001 in actual strings.
For each require C certification before D creation; report C ineligibility distinctly
and escalate to controller, never skip or weaken raw admission/expected scenes.
Per viewport512x512,256x256,384x256,256x384: fresh stream/oracle, forward0..60,
reverse60..0, then[0,0,60,60,9,10,19,20]. This is130 requests per viewport.
Then eight frame30 requests alternating these four viewports twice on one stream.
Expected7920 comparisons =15*(4*130+8), with per-case/order/viewport planned and
executed counts. Each request uses fresh ordinary Animation and full comparator.
This is evidence for these requests only, not all numeric inputs/viewports.

```cpp
require(!NativeEllipseStream::create(nullptr, 41), "null certificate rejected");
require(!NativeEllipseStream::create(prepared.certificate, 0), "zero identity rejected");
auto first = stream->emit(0, 0, 512);
require(!first && !first.scene && first.code == NativeEllipseFrameCode::InvalidViewport,
        "failed viewport has no scene");
auto valid = stream->emit(std::numeric_limits<std::size_t>::max(), 512, 512);
require(valid && valid.scene->frameIndex == 60 && valid.scene->evaluationSequence == 2,
        "clamp and attempted sequence");
require(valid.scene->changes.firstEvaluation, "failure did not publish history");
```

Also test zero height, success/failure/repeat sequence and unchanged geometry/history
after failure; check every emitted transform/path coordinate finite. Do not forge
an immutable certificate solely to enter defensive error paths. If numerical
compatibility fails, retain mismatch evidence and diagnose before altering scope.

- [ ] **Step 5: GREEN, full suite, self-review, commit and report.**

Run focused stream tests during iteration, then one full Telegram suite. Inspect
private call path for absence of reference parser/bridge calls and constructor/
prepareModel/ordinary sampling. `git diff --check`; self-review all scoped files.
Commit only listed files as `feat: emit certified ellipse scenes natively`.
Do not push; controller reviews. Report RED/GREEN/full commands and raw paths,
exact executed counts, first mismatch/root cause if encountered, test helper/API
names, call-path inspection and limits. No worker subagents or UI writes.

### Task 2: Full plan, lifetime, history and reference-isolation gates

**Files:**
- Create tests/support/ExactRenderPlanComparison.hpp.
- Create tests/native_ellipse_stream_lifecycle_tests.cpp.
- Modify CMakeLists.txt only Telegram test registration.
- Amend Task1's private stream/helpers only when a functional regression demonstrates a contract defect; no unrelated refactor.
- Raw out/part26d/task2/; task-2-report.md in this plan's SDD workspace.

**Interfaces:**
- Consumes NativeEllipseStream::create(certificate,uint64_t), emit(size_t,size_t,size_t), checked codes/results from Task1.
- Consumes avemotion::test::NativeEllipseOracle, nativeEllipseTestAssets and normalizeNativeOracleIdentity exact signatures above.
- Produces avemotion::test::ExactRenderPlanComparison::difference(const render::MotionRenderPlan&,const render::MotionRenderPlan&) -> std::string; empty only when all contracted fields agree.
- Uses render::MotionRenderPlanner::build(shared_ptr<const runtime::EvaluatedScene>, const render::PresentationState& = {}); use actual RenderPlanner.hpp error names, not a new planner abstraction.

- [ ] **Step 1: Register lifecycle test and functional comparator RED.**

Register target avemotion_native_ellipse_stream_lifecycle_tests and CTest
avemotion.render.native_ellipse_stream_lifecycle with same private test dependencies.
Start comparator with deliberately unimplemented empty difference; tests must reject
mutated real plans, not synthetic all-default plans. Capture successful build and
failure on changed stamp, then implement the full comparator:

```cpp
ExactRenderPlanComparison compare;
const auto baseline = built.plan;
require(compare.difference(baseline, baseline).empty(), "identical plans agree");
auto changed = baseline;
++changed.stamp.evaluationSequence;
require(!compare.difference(baseline, changed).empty(), "wrong stamp detected");
changed = baseline;
changed.geometryUpdates.front().sourceDrawItemIndex ^= 1U;
require(!compare.difference(baseline, changed).empty(), "wrong update detected");
changed = baseline;
changed.firstPlan = !changed.firstPlan;
require(!compare.difference(baseline, changed).empty(), "wrong history detected");
changed = baseline;
changed.dirtyRegion.left += 1.0F;
require(!compare.difference(baseline, changed).empty(), "wrong bounds detected");
```

Check every field of RenderPlan.hpp explicitly: all7 stamp fields; sourceScene
via ExactSceneComparison plus excluded identity/sequence/change fields checked
explicitly; every draw/key/index/transform/opacity/bounds/feature field; geometry/
paint updates and vector lengths/order; presentedBounds/dirtyRegion, all9 statistics,
all5 fingerprints, firstPlan/visualChanged/reusableForRepaint. No struct memcmp or
fingerprint-only shortcut. Mutation matrix must cover each category (including
sourceScene changes, draw/cache key, paint update, statistics/fingerprint/repaint).
Value comparison doesn't claim model control-block equality; test owners separately.

- [ ] **Step 2: Exercise independent ordered histories and planners.**

For baseline, static-visible, activity and edge-crossing assets use independent
native/oracle planners with same requests: frames[0,0,10,19,20,20,60,30,10,9,0].
For frame30 also run all four viewports and repeat512x512. Apply separate matching
presentations: default, translation(dx3.25,dy-2.5), opacity0.5, visiblefalse/true,
layoutRevision1/2. Normalize only the four expected scene identity/sequence fields;
do not replace expected assetModel with candidate model. Compare plans completely.
Compute oracle scene changes from prior successful fingerprints and cross-check
the five flags against a separate sustained ordinary Instance::evaluateModelFrame
on the identical request sequence. Candidate/oracle source models remain distinct.

```cpp
normalizeNativeOracleIdentity(expected, *certificate, 91, actual.evaluationSequence);
auto nativePlan = nativePlanner.build(std::make_shared<const runtime::EvaluatedScene>(actual), presentation);
auto expectedPlan = oraclePlanner.build(std::make_shared<const runtime::EvaluatedScene>(expected), presentation);
require(nativePlan && expectedPlan, "corresponding plans built");
require(compare.difference(expectedPlan.plan, nativePlan.plan).empty(), "full plan parity");
```

Test stale sequence rejection using older retained scenes, repeated same frame with
new sequence/no resource updates, disappearance/reappearance, invalid viewport
between successes and clamp. Assert actual codes for stale build, not just false.
Do not mutate the oracle's model or rewrite mismatching fields to make plans equal.

- [ ] **Step 3: Two-stream identities, immutable outputs and lifetime.**

Create two streams with IDs101/102 from one certificate. Interleave distinct
forward/reverse frame/viewport sequences and keep prior snapshots/plans. Animated
geometry keys differ by instance; static paint keys match; static-visible geometry
keys share asset scope. Each repeat has no updates, previous snapshots remain
field-equal after the other stream advances. Run bounded two-thread work (each
thread owns one stream/planner,128 requests) and compare with sequential expected
results outside the concurrent interval. No shared global cache toggles, TSan claim
or concurrent calls on one stream.

For every canonical alias require address equals its own model record's staticValue
and identical shared_ptr owner (mutual !owner_before). Emit after one-argument
factory Runtime destruction, after external certificate/Asset reset, and keep
scenes/plans after stream destruction. Weak certificate/model/Asset expiry must
match real remaining owners; release plans/sourceScene/canonical aliases last.
Never require an owner to expire while an intentional lease still exists.

```cpp
require(!scene.assetModel.owner_before(draw.canonicalPaint)
     && !draw.canonicalPaint.owner_before(scene.assetModel), "paint aliases model owner");
require(draw.canonicalPaint.get() == &*scene.assetModel->paint(draw.modelPaint)->staticValue,
        "paint points into frozen record");
```

- [ ] **Step 4: Live counters and ordinary CPU isolation.**

Use prepareNativeEllipseCertificate(Runtime&,json) with a retained actual factory
Runtime. Record its preparation deltas (N model plus N scene samples, noCPU); keep
it quiescent and record actual diagnostics immediately before native creation/
emission and after. All metadata/model/scene/CPU session and sample counters, scene
evaluations and every available DiagnosticsSnapshot field remain unchanged in the native-only window.
Do not compare stored snapshots from a destroyed Runtime or unrelated Runtime.
Direct oracle calls occur outside this window and have explicit load/sample counts.
Print counters individually, not a guessed zero-allocation or speedup guarantee.

Use a separate ordinary Instance and ordinary CPU oracle: compare exact pixel
vectors at frames0,30,60 before/after native emission, with native work interleaved
between matching ordinary renders. Retain normal buffer ownership and compare
dimensions/stride as well. This is interference coverage, not native raster proof.
No CPU API is added to NativeEllipseStream.

- [ ] **Step 5: GREEN, full suite, self-review, commit and report.**

Run focused stream/lifecycle tests while iterating, then one full Telegram suite
and diff check. Commit only listed scoped files as `test: verify native stream plans and lifetimes`.
Any necessary production fix requires its own retained functional RED/GREEN first.
Report commands/raw logs, comparator field/mutation inventory, actual live deltas,
CPU/concurrency/lifetime results and limitations. Controller handles review/push.

## Controller gates and continuation

- [ ] Independent task review after each task; fix loop, STATE/ledger and ordinary push only after evidence and exact remote guard.
- [ ] Fresh full MSVC Telegram and windows-msvc-win32-preview (capture/WARP/device recreation), none/Direct2D; all-vendor and committed TGS16 checks.
- [ ] Fresh none and Samsung configure graphs contain no D implementation/tests; Telegram owns exactly one primitive-generator implementation and private stream in Rendering, no Runtime cycle or installed private header.
- [ ] Whole-stage independent review over6d154ca..HEAD; final report with product/docs SHA, exact matrix counts, cold-preparation versus zero reference-emission counters, comparison exclusions and remaining reference dependence.
- [ ] Continue separately designed bounded own ingress/model/none API, timeline, Direct2D pixel and isolated Avelabs integration. No claim that private stream completes the long user request.

## Controller self-review and approval

Spec ownership/interface/output ->Task1; all15 named inputs and request orders ->
Task1; full plans/history/alias/control-block/two-instance/CPU/live counters ->Task2;
private/platform/provenance and whole-stage review ->controller closure. All five
review-focus classes have concrete regression assertions. Task2 consumes exact
Task1 interfaces. No missing production route is implied: this is explicitly a
private proof boundary. Controllers decide any C-ineligible matrix case or exact
geometry mismatch from evidence; tests may not silently skip it.
Type self-review corrected RenderPlanBuildResult::plan as a value, not a pointer.
Read-only oracle audit was reconciled: descriptor comes from the independent
cache-disabled seed, not a second Runtime Asset with a possibly shared source.

Controller self-approved under the user's delegated design/plan/execution authority;
not a claim of user review of unseen text. Execute via SDD directly in main.
