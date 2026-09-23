# AveMotion Part 25A — Persistent Reference Sessions Design

**Status:** Approved for autonomous execution; persistent reuse gated by correctness investigation

**Date:** 2026-09-23

**Baseline:** `cda415c` (`chore: import AveMotion Part 24 baseline`)

## Execution Amendment — 2026-09-23

The user approved the design, delegated detailed planning and routine technical
decisions, and authorized ordinary commits/pushes to `main`. The five-to-six-hour
run retains its deadline of 08:32 UTC / 11:32 Moscow.

Plan review surfaced `docs/known-issues/RLOTTIE_RENDER_TREE_STATE.md`: fresh
per-sample trees are an intentional Part 3 correctness containment, not merely
an overlooked allocation. A current MSVC Telegram probe has reproduced scene
history differences. Therefore the lifetime described below is a target, not a
safe mechanical substitution. Both arbitrary scene seeks and ascending model
scans must pass independent parity gates before persistent reuse can ship.

The execution plan first diagnoses the mutable-tree lifecycle. A bounded fix
to existing upstream integration requires a written ownership/provenance/test
addendum and regression evidence; it must preserve visual semantics, existing
licenses and dependency versions. If such a fix cannot be proved within the
run, retain fresh sampling on affected paths and deliver verified diagnostics,
stronger regression coverage, baseline evidence and a precise deferred-work
report. This outcome is partial completion, never a claim that Part 25A's
persistent-session target has been achieved.

The measurement patch is neutral to runtime and visual semantics. Corpus
report schema 2 may change the sampled frame distribution and telemetry totals,
with those changes documented and shared by baseline/candidate. Asset
classifications and semantic goldens remain unchanged. Workspace growth is
measured through actual available counters; absent planner allocation telemetry
must be reported as unverified, not inferred to be zero.

## Intent

AveMotion is intended to become a Windows-native animation module embedded in
AveVoice. AveVoice will own its window, D3D11 device, DXGI swap chain,
`ID2D1DeviceContext`, render loop, and final presentation. AveMotion will own
animation assets, instances, playback policy, evaluation, render planning, and
backend resource caches. It must not introduce ANGLE, a second swap chain, a
per-instance timer, or a hidden rendering thread.

Part 25A is the first production-foundation slice. It removes avoidable
per-frame construction of the reference animation runtime while preserving the
Part 24 visual and behavioral oracle. It does not claim that AveMotion is
independent of `rlottie`; that larger migration is split into later stages.

Success means that scene sampling reuses one reference scene session per
`runtime::Instance`, model preparation uses one temporary reference session per
successful eligible asset build, CPU oracle rendering remains isolated, and
every existing golden, parity, playback, planning, Direct2D, and corpus result
remains unchanged.

## Why This Is the First Slice

The current pipeline is structurally suitable for host integration:

```text
TGS / Lottie JSON
    -> Runtime / immutable Asset
    -> Instance / centralized Player
    -> PropertyEvaluator / SourceGeometryProjector
    -> MotionRenderPlanner
    -> Direct2D Backend
    -> host-owned EndDraw / Present
```

The hot path is not yet production-grade. `Instance::evaluateAt()` delegates to
`evaluateModelFrame()`, which delegates to `evaluateExactFrame()`. Every exact
sample calls `extractExactScene()`, and that function constructs a new
`rlottie::Animation` before calling `renderTree()`. Stable model preparation
repeats the same construction for every source frame. At the same time,
`Runtime::createInstance()` already creates and retains
`InstanceData::sceneAnimation`, but exact scene sampling does not use it.

This makes persistent sessions a small, measurable change with a strong safety
boundary: it removes redundant work without changing Lottie semantics, public
playback behavior, canonical tables, geometry generation, render plans, or
Direct2D drawing.

## Scope Decomposition

Part 25A includes only:

- persistent scene-evaluation sessions;
- one-session stable model preparation;
- explicit session diagnostics;
- access-order and CPU-isolation regression coverage;
- before/after performance evidence on the committed corpus;
- documentation of the resulting lifetime and threading contract.

The following are separate designs and are not part of Part 25A:

- replacing `renderTree()` with a fully AveMotion-owned scene topology and
  evaluator for native-ready assets;
- defining whole-asset versus layer-level reference fallback presentation;
- gradients, dashes, masks, mattes, blend modes, images, text, and effects;
- a public `MotionService` facade or a stable DLL/C ABI;
- cache eviction policies for future bitmap and offscreen resources;
- shipping policy and final licensing for an external SDK.

This decomposition keeps the first implementation reviewable and lets its
performance result guide the next stage.

## Considered Approaches

### 1. Reuse the existing per-instance reference session — selected

`InstanceData::sceneAnimation` becomes the sole reference scene runtime for
that instance. Stable model preparation constructs one local session before
its frame scan and reuses it for the scan. The CPU oracle keeps its separate
lazy `cpuAnimation`.

Advantages:

- removes the known redundant construction from every frame;
- uses ownership already present in the code;
- adds no new steady-state allocation to an instance;
- preserves isolation between scene inspection and CPU rasterization;
- requires no new dependency or platform abstraction.

Risk: a mutable upstream tree could be history-dependent. Part 25A therefore
cannot ship on timing evidence alone; direct, sequential, reverse, repeated,
and interleaved access must match fresh-instance results exactly.

### 2. Cache complete evaluated scenes for every frame

This avoids repeated reference evaluation but scales memory with animation
length, viewport, and potentially DPI. It also duplicates mutable presentation
state and does not help arbitrary viewport sizes. It is rejected for the
runtime path.

### 3. Replace the complete reference scene evaluator immediately

This is the long-term target, but the current canonical model does not yet own
all topology, composition timing, masks, mattes, gradients, images, and effects.
Combining that migration with host integration would make regressions hard to
localize. It is deferred to a sequence of native feature slices after Part 25A.

## Detailed Design

### Internal session boundary

The exact-scene helper will receive an already-created animation session:

```cpp
Instance::SceneResult extractExactScene(
    rlottie::Animation& animation,
    const detail::AssetData& asset,
    AssetHandle assetHandle,
    InstanceHandle instanceHandle,
    std::uint64_t instanceId,
    std::uint64_t evaluationSequence,
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight);
```

The helper remains responsible for frame clamping, viewport validation,
`renderTree()`, deep-copying the result, typed errors, and publishing handles.
It no longer decides session lifetime or constructs an animation.

`evaluateExactFrame()` passes `*InstanceData::sceneAnimation`. A missing scene
session is an `EvaluationFailed` invariant violation; evaluation must not hide
the defect by allocating a replacement in the hot path.

### Instance lifecycle

`Runtime::createInstance()` continues to create the scene session before
returning the instance. Creation remains fail-fast: if the session cannot be
created, no partially usable instance is published.

One instance owns exactly:

- one eager scene session for `renderTree()`;
- zero or one lazy CPU session for `renderSync()`;
- its playback state, previous fingerprints, and evaluation sequence.

Both sessions are destroyed with the instance. Two instances of one asset have
independent mutable sessions. They may be evaluated on separate host workers
only after the shared upstream model-cache behavior passes the explicit
two-instance concurrency gate defined below; each individual instance must
still remain confined to one worker at a time.

### Model preparation lifecycle

`prepareStableAssetModel()` remains serialized by `AssetData::modelMutex` and
idempotent through the stored immutable model. After direct parsed-model
extraction succeeds, it creates one temporary model-preparation animation and
uses it for all observed frames. The session is destroyed when preparation
finishes, whether preparation succeeds or fails.

Model preparation does not borrow an instance session and does not retain a
mutable upstream object on the shared asset. This prevents preparation from
changing any instance's access history.

### Metadata and CPU sessions

Asset loading may still create a short-lived metadata session to validate the
JSON and obtain width, height, frame rate, duration, and frame count. Removing
that parse is not required for Part 25A because it is not per-frame work.

CPU rendering remains deliberately isolated. `renderSync()` may mutate
drawable state in ways that affect `renderTree()`, especially for dashed paths.
No scene session is shared with the CPU oracle, and no new shared-session
optimization is allowed in this stage.

### Threading and reentrancy

Part 25A makes the existing effective contract explicit:

- a `runtime::Instance` is serially accessed by one execution stream;
- concurrent scene evaluations on the same instance are unsupported;
- scene evaluation and CPU rendering on the same instance are serialized;
- separate instances may run concurrently;
- `Asset::prepareModel()` remains safe under concurrent calls because model
  publication is protected by `modelMutex`;
- `Player` stays control-thread-only and creates no worker or timer;
- AveMotion creates no platform thread in this stage.

No mutex is added around every frame. The host is responsible for instance
confinement, which avoids turning a local animation object into a contended
global service.

### Diagnostics

`runtime::DiagnosticsSnapshot` will add resettable cumulative counters for one
diagnostic epoch:

```text
referenceMetadataSessionsCreated
referenceSceneSessionsCreated
referenceModelSessionsCreated
referenceCpuSessionsCreated
referenceSceneSamples
referenceModelSamples
```

All increments use relaxed atomics, consistent with the existing diagnostics.
`resetDiagnostics()` starts a new counter epoch but does not recreate live
sessions. Exact-count tests call reset only while the runtime is quiescent.
These are observability counters, not synchronization primitives.

The counters make the performance contract deterministic:

- successfully loading one asset creates one metadata session;
- creating N instances creates N scene sessions;
- successfully preparing one eligible Telegram asset model creates one model
  session; variants without the stable Telegram extensions create none;
- evaluating any number of frames creates no additional sessions;
- first CPU rendering on an instance creates one CPU session, and later CPU
  frames reuse it.

Wall-clock benchmarks supplement these invariants but do not replace them.

### Errors

The public error categories remain unchanged.

- scene-session creation failure: `InstanceCreationFailed`;
- model-session creation failure: `AssetModelPreparationFailed` with a precise
  message;
- null or rejected tree during sampling: `EvaluationFailed`;
- CPU-session creation failure: `CpuRenderFailed`.

Part 25A introduces no new exception-translation policy: expected engine and
validation failures use the existing typed results, while allocation failures
retain the current C++ behavior. A failed model preparation records its error
message and is retried by a later call, matching current behavior; every retry
may therefore create a new temporary model session. There is no silent switch
back to per-frame session construction.

## Compatibility and Test Design

Tests compare observable results, not only counters.

### Access-order parity

For representative fixtures covering animated paths, Trim/Repeater content,
dashes, masks/mattes, and nested composition timing:

1. evaluate each target frame once on a newly created instance;
2. evaluate the same targets on one persistent instance in sequential order;
3. repeat in reverse order;
4. use a deterministic non-monotonic order with repeated frames;
5. compare topology, geometry, paint, and scene fingerprints plus every
   recorded visual field.

Every result must be identical to the fresh-instance result for the same frame
and viewport. Instance IDs, evaluation sequence numbers, and history-derived
change flags are tested separately because they intentionally differ between a
fresh instance and a reused instance. The test must include viewport changes
because `renderTree()` is also parameterized by output dimensions.

### Separate-instance concurrency

Two instances of the same asset are evaluated concurrently on two host threads
using deterministic frame orders. Their visual results must match sequential
fresh-instance results, ThreadSanitizer coverage where available must report no
AveMotion race, and neither instance may be touched by both threads. If this
gate cannot run in the active Windows toolchain, the portable concurrency test
still runs and the sanitizer limitation is reported explicitly.

### CPU isolation

The test evaluates a scene, renders a CPU frame, then evaluates the same scene
again. Scene fingerprints and recorded content must remain identical. Repeated
CPU rendering must not create another CPU session.

### Model preparation

Existing model and golden tests remain authoritative for canonical output. New
diagnostic assertions require one model session rather than one session per
source frame, and repeated `prepareModel()` calls create none.

### Existing gates

The complete applicable suite must pass, including runtime seams, asset model,
playback, player, source geometry, render plans, scene/model/validation goldens,
TGS, Direct2D contract, vendor integrity, and the Windows WARP gates when the
MSVC environment is available. Tests are not weakened and golden data is not
updated unless an independently explained semantic correction requires it;
Part 25A itself expects no golden changes.

## Performance Evidence

The baseline is commit `cda415c`, measured on the same machine, compiler,
configuration, assets, viewport, warm-up, and repetition count as the Part 25A
candidate.

Required evidence:

- zero reference-session creations during a 1,000-sample steady loop after
  instance creation and model preparation;
- no growth of prepared evaluator, projector, player, or planner storage in a
  steady loop;
- per-asset median and p95 for exact scene extraction and the complete
  evaluate/project/plan path;
- session construction counts for load, first frame, and steady frames;
- peak working-set observation for 1, 16, and 64 live instances;
- all visual/golden gates unchanged.

The performance target for the committed smoke corpus is at least a 2x lower
median exact-scene time than `cda415c`, with no asset's p95 more than 10% worse.
If the timing target is not met, the deterministic allocation improvement may
still be retained, but the stage is reported as incomplete and no
Telegram-class performance claim is made.

The 16 committed fixtures are not representative product evidence. Final
AveVoice targets require the private 10–30 asset corpus and separate timings
for independent evaluation, projection, planning, and Direct2D drawing.

## Windows and Graphics Contract

Part 25A does not alter graphics ownership:

- AveVoice owns D3D11, DXGI, Direct2D device/context, target, `BeginDraw`,
  `EndDraw`, and `Present`;
- AveMotion Direct2D receives a borrowed device context plus graphics-domain
  identity and generation;
- device recreation invalidates backend resources but preserves assets,
  instances, playback state, and reference sessions;
- no ANGLE, D3D9, D3D11on12, OpenGL, or second graphics device is introduced.

The current Win32 preview remains the executable integration example and must
continue to survive explicit device recreation and device-loss recovery.

## Packaging and Licensing

Part 25A adds no third-party source and does not copy another algorithm. It
changes lifetime around the existing pinned `rlottie` public API. The existing
LGPL notices, patches, source snapshots, and redistribution obligations remain
unchanged.

This stage does not solve external SDK packaging: the installable
`AVEMOTION_RLOTTIE_VARIANT=none` package still cannot load and evaluate general
Lottie/TGS assets. AveVoice integration should therefore initially link the
same source tree statically inside the application while licensing and native
coverage are resolved. A stable DLL/C ABI is deferred until the public surface
and dependency policy stop changing.

## Delivery Shape

Part 25A should land as small commits:

1. regression tests and diagnostic contract;
2. persistent instance scene session;
3. one-session model preparation;
4. benchmark/report evidence and documentation.

Each commit must build and pass its focused tests. The final commit must pass
the complete locally available suite before push to `main`. Windows-only gates
that cannot run in the active shell are listed explicitly rather than inferred
from portable tests.

## Follow-on Stages

After Part 25A is measured, the next specifications are ordered as follows:

1. native scene topology and evaluation for Direct2D-native assets, removing
   per-frame `renderTree()` from that path;
2. explicit render completeness and whole-asset fallback policy so successful
   drawing can never silently omit unsupported content;
3. AveVoice `MotionService` adapter with asset/instance registries, host
   scheduling hooks, device-generation handling, and bounded caches;
4. packaging, license decision, external consumer gate, and only then a stable
   binary boundary if AveVoice actually needs one;
5. corpus-driven feature families such as gradients, masks, and mattes.

This order improves the measured hot path first, then removes the reference
engine from native frames, then freezes an integration API around proven
behavior.
