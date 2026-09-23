# AveMotion animated Shape evaluation — Part 10

## Canonical representation

A `MotionShapeValueRecord` references a contiguous range in
`MotionAssetModel::shapePoints`. The accepted stream is empty or:

```text
point 0: MoveTo
points 1..3: cubic control 1, control 2, end
points 4..6: next cubic
...
```

Therefore a non-empty Shape must contain `1 + 3n` points. All coordinates must
be finite. These invariants are checked when `PropertyEvaluator` is built.

## Immutable and mutable data

```text
Immutable asset:
  MotionShapeValueRecord[]
  MotionVec2Value[] source points
  Track/Segment references

Per workspace / instance:
  EvaluatedShape[]
  retained MotionVec2Value[] point storage
  revisions and dirty flags
```

Static Shape properties remain `AssetReference` values. Only animated Shape
properties allocate an `EvaluatedShape` slot.

## Storage preparation

For every animated Shape property, evaluator construction determines the
maximum endpoint point count across all segments. `prepare()` allocates one
fixed slice of the workspace point buffer. `evaluate()` never resizes the
buffer.

```text
property -> shapeSlot -> [firstPoint, pointCapacity)
```

## Sampling semantics

- before the first segment: copy the first endpoint and its `closed` flag;
- after the last segment: copy the last endpoint and its `closed` flag;
- active hold segment: use progress zero in Telegram's interpolation path;
- active linear/cubic segment: interpolate each point after temporal easing;
- mismatched point counts: use the minimum count and set
  `topologyTruncated=true`.

The active interpolation path intentionally publishes `closed=false`, matching
the pinned Telegram `LottieShapeData::lerp` implementation.

## Revisions

`EvaluatedShape::revision` increments only when the materialized visual Shape
changes: point count, point values, closed state or truncation state. Cursor
movement and segment identity changes do not independently advance it.

A property containing that Shape advances its property revision when the Shape
revision changes, even though `MotionPropertyValue::shapeSlot` stays constant.

## Safety

Evaluation rejects:

- invalid Shape references;
- out-of-range point ranges;
- non-finite points;
- invalid `1 + 3n` encoding;
- workspace slices outside retained storage;
- spatial interpolation attached to a Shape track.

## Current boundary

Part 10 owns source Shape-property materialization. It does not yet apply shape
modifiers or replace Telegram's final draw geometry. Part 11 will project safe,
modifier-free Shape nodes into AveMotion local path geometry and
`MotionGeometryUpdate` packets.
