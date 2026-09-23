# MotionRenderPlan boundary

AveMotion uses a backend-neutral rendering contract:

```text
MotionAssetModel + EvaluatedScene
    -> MotionRenderPlanner
    -> immutable MotionRenderPlan
    -> headless or native backend
```

`MotionRenderPlan` is not a raster frame and contains no COM object. It owns a
shared immutable evaluated-scene snapshot and compact draw items.

## Stable ownership stamps

A plan records:

```text
asset source hash
packed AssetHandle (index + generation)
legacy monotonic instance ID
packed InstanceHandle (index + generation)
evaluation sequence
plan sequence
layout revision
```

Generation identities prevent stale planner/cache state from being reused after
a registry slot is destroyed and allocated to another object.

## Typed topology

When an immutable Part 6 model is attached, every draw item carries:

```text
NodeId
DrawItemId
GeometryId
PaintId
```

The same IDs are stable across instances and across separately loaded copies of
identical source bytes. Transitional source keys remain only for legacy
compatibility diagnostics.

## Geometry and paint identity

Each draw item has independent `GeometryCacheKey` and `PaintCacheKey` values.

### Asset scope

```text
scope = Asset
asset identity
GeometryId / PaintId
canonical content hash
revision = 0
no instance identity
```

The same static resource can be reused by multiple instances.

### Instance scope

```text
scope = Instance
asset identity
instance identity
evaluated resource slot
content hash
monotonic content revision
```

A changed path or paint replaces one bounded instance slot. Layout-only changes
do not advance these revisions.

## Transform separation

```text
geometryTransform      canonical/local geometry -> composition/viewport
presentationTransform  host layout/placement
```

A transform-only animation can reuse the same native path. The Direct2D backend
composes both transforms with the host transform at draw time.

## Plan ownership

A plan owns `shared_ptr<const EvaluatedScene>`. The scene retains
`shared_ptr<const MotionAssetModel>`. Static canonical values are owned by model
table records. A backend may read plan/snapshot/model storage during `draw`, but
must not retain raw pointers beyond their lifetime.

## Dirty region

The planner exposes a conservative hint:

```text
dirty = union(previousPresentedBounds, currentPresentedBounds)
```

If the visual result is unchanged, the region is invalid. Hosts may ignore the
hint and repaint more broadly.

Fundamental separation:

```text
repaint does not imply reevaluation
repaint does not imply geometry rebuild
repaint does not imply paint rebuild
```
