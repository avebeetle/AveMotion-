# AveMotion Part 3 report: evaluated-scene extraction seams

## 1. Executive result

Part 3 creates the first real AveMotion-owned runtime layer on top of the pinned
rlottie baselines. It no longer exposes only metadata and CPU pixels. It can now
extract a self-owned evaluated vector scene before scan conversion and
compositing.

The new path is:

```text
immutable AveMotion Asset
    -> independent AveMotion Instance
    -> exact upstream render-tree sample
    -> AveMotion EvaluatedScene
    -> RecordingBackend

and, independently:

Instance
    -> unchanged rlottie CPU renderer
    -> pixel oracle
```

This is the first replaceable seam required for a future `MotionRenderPlan` and
Direct2D backend.

## 2. New AveMotion-owned targets

```text
AveMotion::Core
AveMotion::Runtime
AveMotion::Reference
```

`AveMotion::Core` currently provides deterministic explicit-field hashing with
canonical little-endian integer/IEEE-754 encoding and canonical signed zero.
`AveMotion::Runtime` owns the new asset/instance/evaluated-scene API.
`AveMotion::Reference` remains the original CPU oracle and provenance facade.

Project version is now `0.3.0`.

## 3. Runtime API introduced

Public headers:

```text
include/avemotion/core/Hash.hpp
include/avemotion/runtime/Runtime.hpp
include/avemotion/runtime/EvaluatedScene.hpp
include/avemotion/runtime/RecordingBackend.hpp
include/avemotion/runtime/Diagnostics.hpp
```

Primary types:

```cpp
avemotion::runtime::Runtime
avemotion::runtime::Asset
avemotion::runtime::Instance
avemotion::runtime::EvaluatedScene
avemotion::runtime::RecordingBackend
avemotion::runtime::DiagnosticsSnapshot
```

An `Asset` is immutable and shared. An `Instance` owns independent mutable
upstream runtime state. The source JSON and stable hash are stored once per
asset. rlottie's pinned model cache shares parsed models between exact samples
and independent instances.

## 4. Evaluated scene contents

The bridge deep-copies the public `LOTLayerNode`/`LOTNode` tree into AveMotion
structures:

- layer hierarchy and keypaths;
- draw order;
- visibility, opacity and matte mode;
- masks and clip paths;
- path verbs and evaluated viewport-space points;
- fill rule;
- solid paints;
- linear and radial gradients with stops;
- stroke metadata;
- image dimensions and transform metadata;
- structural statistics and deterministic fingerprints.

No upstream pointer escapes the bridge. The bridge validates finite geometry,
stroke, dash, gradient and image-transform values and enforces explicit depth,
layer, node, mask, path, gradient-stop and dash limits before publishing a scene.

## 5. Recording backend

`RecordingBackend` does not generate pixels. It produces explicit hashes for:

```text
topology
geometry
paint
combined visual scene
```

Hashes are built field-by-field rather than hashing C++ object memory, avoiding
padding-dependent results. Instance IDs and evaluation sequence numbers are not
part of visual identity.

## 6. Exact-sample determinism issue found

While implementing direct-seek tests, Part 3 found that rlottie's public
`renderTree()` is coupled to mutable `VDrawable` state normally consumed by the
CPU preprocess stage. Reusing one runtime object for inspection made some
render-tree results history-dependent, particularly around dashed/static paths.

Containment:

- a fresh runtime item tree is created for each exact evaluated sample;
- parsed models remain shared;
- CPU rendering uses a different runtime object;
- the tree is deep-copied immediately.

This restored direct-seek and repeated-frame determinism across the full corpus.
The issue and future replacement choices are documented in
`docs/known-issues/RLOTTIE_RENDER_TREE_STATE.md`.

## 7. Tests added

### Core determinism test

- verifies the standard FNV-1a `hello` vector;
- verifies canonical little-endian integer encoding;
- verifies that positive and negative floating-point zero hash identically.

### Runtime seam tests

For every pinned corpus asset:

- load immutable asset;
- create two independent instances;
- evaluate first, middle and last frame;
- evaluate the same frame twice;
- seek backward to frame zero;
- compare independent-instance scene fingerprints;
- map normalized position `1.0` to the last valid frame;
- render through the isolated CPU oracle;
- compare CPU hash with the original `ReferenceRuntime`;
- reject invalid viewport dimensions;
- reject cross-runtime asset ownership.

### Evaluated-scene golden tests

Two committed manifests cover:

```text
2 rlottie lineages
x 8 assets
x 5 samples
= 80 evaluated-scene goldens
```

Each row records scene/topology/geometry/paint fingerprints and structural
counts. Both Clang and GCC builds reproduce the same committed manifests.

### Offline tests

The installable variant without rlottie now exports `AveMotion::Core`,
`AveMotion::Reference` and `AveMotion::Runtime`. A clean external consumer uses
`find_package(AveMotion 0.3)` and verifies the explicit
`ReferenceUnavailable` error path.

## 8. Build and test matrix

Validated locally in Linux x86-64:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 8/8 tests passed |
| Telegram + GCC Release | 8/8 tests passed |
| Telegram + Clang ASan | 8/8 tests passed |
| Samsung + Clang Debug | 8/8 tests passed |
| Samsung + GCC Release | 8/8 tests passed |
| Samsung + Clang ASan | 8/8 tests passed |
| Offline Clang package | 4/4 tests passed |
| External `find_package` consumer | passed |

MSVC presets and CI remain configured, but this Linux environment cannot execute
MSVC or Direct2D.

## 9. Telegram versus Samsung at the scene boundary

Across 40 normalized samples:

| Field | Identical | Different |
|---|---:|---:|
| Combined evaluated scene | 12 | 28 |
| Topology | 27 | 13 |
| Geometry | 20 | 20 |
| Paint | 35 | 5 |
| Selected frame index | 21 | 19 |
| Path-verb counts | 27 | 13 |
| Path-point counts | 35 | 5 |

The comparison is stored in
`docs/characterization/scene-telegram-vs-samsung.md`.

This localizes many previously pixel-only differences: some are endpoint/frame
selection, some are path topology, some are coordinates, and a smaller set are
paint values. Samsung still cannot be adopted wholesale without semantic review.

## 10. Diagnostics

`DiagnosticsSnapshot` records:

- asset load attempts/success/failure;
- instance creation;
- evaluated scene count/failure count;
- CPU oracle renders;
- copied layers, draw items, masks and path points;
- cumulative scene-evaluation and CPU-render time.

The scene characterizer reports these counts after each corpus run.

## 11. Transitional limitations

- scene geometry is already transformed into the requested viewport;
- local/canonical transforms are not separately exposed;
- exact scene samples rebuild the upstream runtime item tree;
- bounds are conservative control-point bounds;
- image pixels are not copied into scene snapshots;
- masks/mattes are recorded only;
- no `MotionRenderPlan` or native backend exists yet;
- no public ABI promise is made.

## 12. Delivery rule

The extraction rule remains:

```text
characterize -> isolate -> replace -> compare -> remove old path
```

The CPU renderer remains intact. The new evaluated-scene goldens are the gate
for replacing geometry, paint, hierarchy or evaluation subsystems.
