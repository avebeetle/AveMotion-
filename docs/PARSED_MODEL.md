# Direct parsed-model extraction — Part 7

## Purpose

Part 7 introduces a read-only bridge from Telegram's parsed `LOTModel` to
AveMotion-owned immutable authored tables. This removes the requirement to
sample frames merely to discover source topology, property types or keyframes.

```text
Telegram LOTModel
    -> TelegramParsedModelBuilder
    -> MotionAssetModel schema 2
         Composition[]
         SourceNode[]
         Property[]
         Track[]
         Segment[]
         typed value arrays
```

The bridge is private. No upstream type appears in installed public headers.

## Reusable compositions

The root composition and every precomposition asset receive a dense
`CompositionId`. Precomposition layers store `referencedComposition` rather
than recursively copying the referenced children.

```text
Composition 0: root
Composition 1..N: sorted precomposition assets

SourceLayer(kind=Precomposition)
    -> referencedComposition
```

References are validated and the composition graph must be acyclic. Dimensions
are taken from asset metadata, then referencing-layer hints, then root fallback.

## Source graph

`MotionSourceNodeRecord` stores:

- structural parent;
- transform parent scoped to the same composition;
- composition and optional referenced composition;
- authored layer/node type;
- contiguous child and property ranges;
- names/debug metadata;
- layer timeline metadata;
- static/timeline dependency flags;
- non-animatable fill, stroke, gradient, mask, matte, blend, primitive, trim and
  repeater semantics.

Root nodes and child order follow source order. Precomposition registration is
sorted by source reference ID so unordered-map iteration cannot affect IDs.

## Properties and tracks

Every property has a typed `PropertyId`, owner node, semantic, optional component
index, value type, flags and either:

```text
staticValue
```

or:

```text
TrackId -> contiguous Segment[] range
```

Repeated authored components such as dash entries use `semanticIndex` rather
than ambiguous duplicate semantics.

Supported typed storage:

```text
float
Vec2
Color
Matrix3x2
Shape (point range + closed flag)
Gradient (float range)
```

Animated segments record:

- start/end frame;
- start/end typed values;
- hold/linear/temporal cubic interpolation;
- authored temporal control points;
- separate spatial interpolation and tangents.

Temporal and spatial interpolation are intentionally separate. A spatial path
segment can still have hold, linear or cubic temporal easing.

## Read-only Telegram introspection patch

`patches/telegram/0004-avemotion-parsed-model-introspection.patch` exposes:

- static transform matrix and opacity;
- animated transform component data;
- temporal Bezier control points;
- `ExtraLayerData::mHasTimeRemap`, set only when an authored `tm` property exists.

The explicit time-remap flag prevents masks/precompositions that merely allocate
`ExtraLayerData` from producing false `LayerTimeRemap` properties.

## Determinism

The model fingerprint is computed field by field with canonical numeric
encoding. It excludes allocation addresses, pointers and host debug labels.

Tests prove:

- duplicate loads are identical;
- changing host debug name does not change the parsed fingerprint;
- arbitrary evaluations before model preparation do not change any parsed table
  or fingerprint;
- Clang and GCC emit byte-identical summary and detailed manifests.

## Validation

The builder rejects:

- missing root compositions;
- duplicate precomposition IDs;
- cycles in layer or composition graphs;
- invalid IDs/ranges/back-references;
- cross-composition transform parents;
- unsorted/overlapping keyframe segments;
- mismatched property/track/value types;
- non-finite values/easing/tangents;
- invalid shape/gradient ranges;
- excessive nodes/properties/segments/value storage/dash components.

These are laboratory containment limits, not the final untrusted-input policy.

## Transitional boundary

The parsed source tables are independent of evaluated-frame sampling. The
legacy render-facing geometry/paint compatibility tables are not. Telegram still
owns exact scene evaluation, modifiers, path construction, masks/mattes and CPU
rasterization.
