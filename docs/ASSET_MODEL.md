# Immutable canonical asset model — Part 7

## Purpose

`MotionAssetModel` now contains two explicit layers of data:

```text
1. Direct authored source model (Part 7)
   Composition[] / SourceNode[] / Property[] / Track[] / Segment[]

2. Render-facing compatibility model (Parts 5–6)
   Layer[] / Node[] / Geometry[] / Paint[] / DrawOrder[]
```

The first is built directly from Telegram's parsed model and is independent of
which frames were evaluated. The second remains a compatibility oracle for the
existing render planner until the canonical evaluator/geometry pipeline
replaces it.

## Typed IDs

Asset-local 32-bit typed IDs now include:

```text
CompositionId
SourceNodeId
PropertyId
TrackId
SegmentId
LayerId
NodeId
GeometryId
PaintId
DrawItemId
ClipId
```

`UINT32_MAX` is invalid. IDs are dense table indices and contain no pointers,
strings, backend objects or registry generations. Outer asset/instance handles
provide lifetime generations.

## Direct authored tables

```text
MotionAssetModel
├── MotionCompositionRecord[]
├── MotionSourceNodeRecord[]
├── sourceChildIds[]
├── sourcePropertyIds[]
├── MotionPropertyRecord[]
├── MotionTrackRecord[]
├── MotionSegmentRecord[]
├── scalarValues[]
├── vec2Values[]
├── colorValues[]
├── matrixValues[]
├── shapeValues[] + shapePoints[]
└── gradientValues[] + gradientFloats[]
```

The model is immutable after publication and shared by every instance of the
asset. It contains no `rlottie`, Win32, Direct2D or host-window types.

## Precomposition ownership

Precomposition assets are stored once as separate composition tables. A source
layer references them by `CompositionId`. This preserves reuse and avoids the
large duplication produced by recursively flattening each use.

On the current corpus:

```text
root compositions:            8
reusable precompositions:     7
precomposition references:   24
```

## Property storage

Static properties reference immutable typed values. Animated properties
reference tracks, and tracks own contiguous segment ranges. A segment stores
source-frame bounds, start/end values, temporal easing and optional spatial
path data.

Track cursors and evaluated values do not belong in `MotionAssetModel`; they are
future per-instance runtime state.

## Ownership

```text
MotionAssetStore / AssetData
    owns shared_ptr<const MotionAssetModel>

MotionInstance
    references Asset
    does not copy source tables

Telegram evaluator adapter
    reads the same source JSON/model as semantic oracle

Direct2D backend
    owns native resources outside MotionAssetModel
```

Logical assets therefore survive graphics-device recreation.

## Deterministic fingerprints

The model has:

- `parsedModelFingerprint` for authored source tables;
- topology/resource/full fingerprints for compatibility tables.

All are computed field by field with canonical integer and IEEE-754 encoding.
Pointers, struct padding, registry generations and host debug labels are
excluded.

## Corpus totals

```text
assets:                  8
compositions:           15
source nodes:          135
properties:            309
static properties:     266
animated properties:    43
tracks:                  43
segments:                71
scalar values:          185
Vec2 values:            112
color values:            27
matrix values:           48
shape values:            28
gradient values:          8
```

## Current limitations

- Telegram still performs exact visual evaluation;
- render-facing geometry/paint tables are still compatibility tables;
- source frame times remain `double` until the canonical time study is applied;
- one full-range default clip is synthesized;
- markers, parameter bindings and compiled easing IDs are not yet modeled;
- debug names are owning strings, not packed string-table IDs;
- no `.avm` serialization/memory mapping exists;
- Samsung has no direct parsed-model table path in this stage.
