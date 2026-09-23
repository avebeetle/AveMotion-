# Part 3 extraction seams

Part 3 introduces the first AveMotion-owned runtime boundary before rlottie's CPU
rasterizer. The upstream source remains vendored and private, while consumers can
now load an immutable asset, create independent instances, request an exact
scene sample, inspect geometry/paint/layer state, and optionally render through
the unchanged CPU oracle.

## Public layers

```text
AveMotion::Core
  deterministic hashing utilities used by all characterization paths

AveMotion::Runtime
  Runtime
  Asset
  Instance
  EvaluatedScene
  RecordingBackend
  DiagnosticsSnapshot

AveMotion::Reference
  original CPU surface oracle and upstream provenance
```

Only the private runtime bridge includes `rlottie.h` and `rlottiecommon.h`.
Upstream types do not appear in the AveMotion runtime headers.

## Data flow

```text
Lottie JSON
    |
    v
Runtime::loadLottieJson
    |
    +--> immutable Asset (source bytes, metadata, source hash, cache key)
    |
    v
Runtime::createInstance
    |
    +--> private rlottie runtime item tree
    |
    v
Instance::evaluateFrame
    |
    +--> rlottie::Animation::renderTree
    |
    +--> deep copy into AveMotion EvaluatedScene
            layers
            child references
            draw items
            paths
            solid/gradient/image paints
            strokes
            masks
            mattes
            clip paths
            deterministic fingerprints
    |
    v
RecordingBackend / future MotionRenderPlan / future native backend

Separate path:

Instance::renderCpuFrame
    -> isolated rlottie::Animation
    -> unchanged CPU rasterizer
    -> ARGB32 premultiplied oracle frame
```

## Why scene and CPU instances are isolated

The public rlottie `renderTree()` view and the CPU renderer share mutable
`VDrawable` state. In particular, dashed paths are materialized in
`LOTDrawable::sync()`, while `VDrawable::preprocess()` later consumes and clears
that path for CPU rasterization. Reusing one upstream runtime object for both
inspection and rasterization can therefore make the result depend on call order.

Part 3 uses independent upstream runtime objects for:

- exact evaluated-scene extraction;
- CPU pixel rendering.

For exact scene sampling, each Instance owns one eager recording runtime tree.
Every sample resets its recording state before evaluation and immediately
deep-copies the result. Model preparation owns a separate temporary recording
tree across its ascending scan; CPU rendering keeps its own lazy ordinary tree.
Telegram Assets now retain the exact parsed `LOTModel` returned by their metadata
Animation and construct their scene, model-preparation and lazy CPU Animations
from that same source. The model cache may evict or be disabled without losing
authored path and paint IDs on prepared scenes. Samsung continues to use its
ordinary loader. The source lease itself retains no mutable evaluator.
Access to each Instance remains serial; separate Instances may run concurrently.
Source binding epochs refresh local recording IDs after model extraction;
unchanged-epoch frames acquire no source binding lock.

See `docs/known-issues/RLOTTIE_RENDER_TREE_STATE.md`.

The earlier cache-dependent loss of Telegram authored IDs was accidental
metadata loss. The prepared-scene oracle explicitly stamps its own fresh
ordinary source before comparison, so it still checks every scene field while
using the Asset's frozen model for this source-ownership test. The independent
model-session test additionally builds a whole model using fresh ordinary
Animations for every ascending frame, then compares complete model-applied scenes.

## EvaluatedScene contract in Part 3

`EvaluatedScene` is a deep, self-owned snapshot. No pointer in it refers into the
upstream tree. The snapshot contains viewport-space evaluated paths, so it is an
**evaluated recording boundary**, not yet the final local-space canonical model.

It captures:

- flattened layer hierarchy with parent and child-index ranges;
- stable depth-first draw order;
- visible state, layer alpha and matte mode;
- clip paths and masks;
- path verbs (`MoveTo`, `LineTo`, `CubicTo`, `Close`) and points;
- solid, linear-gradient, radial-gradient and image paint metadata;
- stroke width, cap, join, miter and any exposed dash data;
- scene statistics and four deterministic fingerprints.

Before publication, the bridge rejects non-finite path, stroke, dash, gradient
and image-transform data and applies explicit structural limits. This is a
containment boundary around the legacy public C tree, not a substitute for the
future offline asset validator.

The fingerprints are:

- `topology`: hierarchy, draw order, verb streams, paint kinds and matte/mask modes;
- `geometry`: evaluated coordinates and conservative control-point bounds;
- `paint`: solid/gradient/stroke values;
- `scene`: combined visible visual state, excluding instance IDs and evaluation
  sequence numbers.

## Change classification

Each `Instance` retains only the previous fingerprints. A subsequent exact
sample reports whether topology, geometry, paint or the total visual scene
changed. Upstream dirty flags are recorded for diagnostics but are not trusted as
AveMotion semantic dirty state.

This is deliberately simple. Future canonical evaluation will produce revisions
at property/node/geometry granularity.

## Transitional limitations

- scene paths are already transformed to the requested viewport;
- local transforms and canonical source geometry are not yet separately exposed;
- exact scene evaluation still deep-copies every sample from its recording tree;
- images record dimensions and matrix but do not copy image pixels;
- bounds are conservative control-point bounds, not exact cubic extrema;
- the API is source-stable only for this laboratory stage, not a published ABI;
- masks and mattes are recorded, not reimplemented.

These limitations are explicit gates for later extraction rather than hidden
behavior.

## Part 5 canonical promotion

The exact scene snapshot now optionally carries Telegram-proven local geometry,
a separate affine transform, base solid paint, separated opacity and explicit
static-candidate flags. `Runtime` interns proven resources into immutable
asset-owned registries and attaches stable generation handles to every scene.

The legacy final viewport path and paint remain in the snapshot and retain their
original fingerprints. This preserves the Part 3 oracle while allowing the Part
4 planner and Direct2D seam to select canonical local resources when available.

## Part 6 typed asset-model seam

Part 6 adds stable source IDs to the Telegram runtime item tree before frame
evaluation. Those IDs become dense `LayerId`, `NodeId`, `GeometryId`,
`PaintId`, and `DrawItemId` values in an immutable AveMotion model. The model is
shared across instances and contains no upstream pointers.

The legacy render tree remains authoritative for evaluated values. Each exact
scene is validated against the model before typed IDs and static table-owned
resources are applied. Any identity mismatch fails the model-aware path instead
of silently falling back to traversal ordinals.

## Part 7 direct parsed-model seam

Part 7 moves authored topology and timeline ownership before runtime evaluation.
A private Telegram-only builder reads `LOTModel` and compiles reusable
compositions, source nodes, typed properties, tracks, keyframe segments and
value storage directly into AveMotion tables.

This seam is read-only and narrow. The Telegram evaluator and CPU rasterizer are
unchanged and remain the behavior oracle. Direct source-table identity no longer
depends on frame sampling or evaluation history.

The older render-facing compatibility tables still depend on exact Telegram
runtime evaluation. This is intentional: Part 7 isolates authored data first,
without claiming that modifiers, masks, evaluated geometry or compositing have
already been replaced.

## Part 8 independent property-evaluation seam

Part 8 adds the first AveMotion-owned evaluator above the direct parsed tables.
The evaluator supports static, hold, linear and temporal cubic tracks for
scalar, vector, color and matrix values plus a constrained local 2D transform.

Telegram remains available only through a private exact-property oracle used by
characterization and parity tests. Public/installable evaluation code depends on
`MotionAssetModel`, not `LOTModel` or any upstream runtime object.

Spatial tracks, animated shapes/gradients, auto-orient, world hierarchy and full
scene composition remain on the Telegram compatibility path until separately
characterized and replaced.

## Part 9 spatial and world-transform seam

Part 9 consumes spatial tangents already present in canonical segments and
materializes Telegram-compatible spatial Vec2 values plus tangent angles. Local
transforms use the angle only for authored auto-orient layers. A precompiled
composition-local topological order then publishes independent world matrices
and inherited opacity. The private Telegram oracle supplies exact local matrices;
world composition uses the pinned `VMatrix` implementation as a separate
field-level parity boundary.
