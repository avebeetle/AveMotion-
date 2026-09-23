# Canonical local-space geometry and static provenance

## Purpose

Part 5 separates reusable source content from per-instance presentation state:

```text
immutable local path + immutable paint
    + evaluated affine transform + inherited opacity
    -> legacy viewport-space visual result
```

This is the first resource boundary that allows a native backend to share one
static path between multiple animation instances instead of rebuilding an
already transformed path for every instance and frame.

## Why the Part 4 heuristic was insufficient

The public `renderTree()` output exposes final viewport-space paths. Fresh
Telegram runtime trees also set `ChangeFlagPath` on nearly every drawable. Thus:

- an upstream dirty bit does not prove animation;
- equality at two or five sampled frames does not prove lifetime staticness;
- final paths cannot distinguish path animation from transform-only animation.

Part 5 therefore adds an explicit metadata seam inside the pinned Telegram
runtime rather than inferring provenance from sampled output.

## Telegram metadata seam

The additive extension exposes:

```text
local path after path modifiers but before the paint-group transform
paint-group affine transform
base solid color
separated inherited opacity
staticness of source path
staticness of trim/modifier chain
staticness of internal transform chain
staticness of solid-paint properties
```

The legacy final path, brush and CPU rasterizer inputs remain unchanged. The
original pixel and evaluated-scene fingerprints deliberately exclude the new
metadata, so those oracles remain byte-for-byte stable.

The extension is compiled only for the Telegram baseline. Samsung remains an
independent comparison implementation and makes no `AssetStatic` claim.

## Staticness rule

A geometry is promoted to asset scope only when all of the following are true:

1. the authored path/basic shape is static;
2. all path modifiers affecting it are static;
3. all transforms internal to the path-to-paint-group relationship are static;
4. the local path and transform are finite and structurally valid;
5. a source key that was already promoted resolves to exactly identical
   canonical content.

The paint-group/root presentation transform may still animate. That transform
is stored separately and does not invalidate the local native geometry.

A solid paint is promoted only when its source color/opacity properties are
static. Runtime dynamic-property overrides are not yet exposed through the
AveMotion API; adding them must invalidate or bypass this proof.

For the Part 5 native-render path, local geometry is selected only when the
matching local solid-fill paint/opacity seam is also present. Telegram's legacy
stroke widths, dash lengths and gradient coordinates are already expressed for
the final transformed path. Pairing those values with a local path would apply
transform semantics twice. Stroke and gradient items therefore stay on the
legacy final-path representation until their complete local paint contracts are
extracted and characterized.

## Asset-owned resources

`AssetData` owns canonical registries:

```text
sourceGeometryId -> shared_ptr<const CanonicalGeometry>
sourcePaintId    -> shared_ptr<const CanonicalPaint>
```

The first proven sample creates the immutable object. Later samples and other
instances reuse it only after exact content verification. A disagreement is
reported as `canonicalResourceConflicts` and the item remains instance-scoped.

The current source key is transitional:

```text
asset source hash + layer index + local draw-item ordinal
```

It is stable for the pinned Telegram runtime and characterized corpus. A future
compiled canonical asset will replace it with explicit `GeometryId`/`PaintId`
values assigned during import.

## Instance-local resources

Any geometry or paint that is animated, uncertain, unsupported by the metadata
seam, or conflicting remains instance-scoped:

```text
InstanceHandle + source slot + content hash + revision
```

Repeated identical evaluation preserves the revision. A content change
increments it. Presentation transform and layout changes do not increment
geometry or paint revisions.

## Stable handles

Assets and instances use index+generation handles:

```cpp
struct AssetHandle    { uint32_t index; uint32_t generation; };
struct InstanceHandle { uint32_t index; uint32_t generation; };
```

Released slots may be reused, but the generation advances. Cache keys and
planner state use the packed generation identity so stale work cannot target a
new object that inherited an old registry index.

## Direct2D mapping

```text
AssetStatic CanonicalGeometry
    -> one cached ID2D1PathGeometry per compatible graphics/factory domain

InstanceEvaluated geometry
    -> bounded instance/slot/revision native geometry entry

geometryTransform
    -> applied at draw time

presentationTransform
    -> host layout/placement transform, also applied at draw time
```

This makes transform-only animation reuse native geometry. No Direct2D object
appears in the canonical resource or render-plan model.

## Characterization result

Across 40 Telegram plan samples:

| Metric | Part 4 | Part 5 |
|---|---:|---:|
| Geometry update packets | 295 | 290 |
| Asset-scoped geometry references | 0 | 36 |
| Instance-scoped geometry references | 339 | 303 |
| Asset-scoped paint references | 0 | 290 |
| Instance-scoped paint references | 339 | 49 |
| Canonical resource conflicts | n/a | 0 |

The dedicated runtime test examines all eight corpus assets at five times,
validates local-path/transform parity with the legacy final path, verifies
cross-instance pointer sharing, checks resize reuse, and checks handle
generation advancement.

## Explicit limitations

- canonical resources are currently discovered lazily while exact Telegram
  runtime samples are extracted; they are not yet compiled asset tables;
- gradients, strokes with changing properties, masks/mattes and dynamic paths
  remain conservative;
- source IDs are transitional, not final serialized IDs;
- exact scene evaluation still creates a fresh Telegram runtime tree per sample
  to avoid the known `renderTree()` history dependency;
- Direct2D native runtime validation remains a Windows gate.
