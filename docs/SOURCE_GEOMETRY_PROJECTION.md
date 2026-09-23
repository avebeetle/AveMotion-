# AveMotion source geometry projection

AveMotion progressively replaces Telegram final geometry with independently
evaluated local paths and paints while preserving Telegram as an exact parity
oracle.

```text
Telegram parsed model
-> canonical SourceNode / Property / Track / Segment tables
-> AveMotion PropertyEvaluator
-> source Shape or generated primitive local paths
-> optional one-/multi-path Trim Path
-> optional Repeater content-group projection
-> prepared SourceGeometryProjector
-> EvaluatedScene local geometry/paint
-> MotionRenderPlan updates
```

## Supported source geometry

- authored ShapePath;
- Rectangle / Rounded Rectangle;
- Ellipse;
- Polystar / Star;
- Polygon;
- one direct path with one Trim Path;
- several direct paths with one Trim Path and one local solid fill;
- one direct Repeater over supported direct/nested ShapeGroups;
- multiple local solid fills and dashless local solid strokes inside that
  Repeater content group.

## Conservative acceptance contract

A draw item is published through the AveMotion-owned source path only when:

- supported relationships are available from parsed-model metadata;
- all required properties evaluate successfully;
- geometry and paint share a proven local coordinate-space seam;
- modifier relationships match the current subset;
- generated storage is valid and finite;
- transformed verbs, points, matrices and paint values exactly match Telegram.

Any failure leaves Telegram geometry and paint untouched.

## Coordinate spaces

```text
source-local path
-> path-to-paint relative transform
-> paint-local combined path
-> Repeater copy transform when present
-> local-to-viewport transform
-> Telegram final-path parity
```

## Retained preparation

`SourceGeometryProjectionWorkspace` is prepared once per canonical model and
evaluation stream. It retains path candidates, flattened source streams,
multi-path trim outputs, combined paint-local geometry and per-Repeater copy
counters. Tests and the golden characterizer reject capacity growth after
preparation.

## Current persisted characterization

```text
70 samples across 14 assets/fixtures
774 draw items visited
454 source-bound candidates
479 accepted projections
431 asset-static / 48 instance-animated
8,613 projected points

290 Repeater copy/paint projections
279 visible / 11 hidden
205 solid fill applications
85 solid stroke applications
80 nested paint applications
30 animated paint applications
0 Repeater binding/property/evaluation/input/parity failures
```

The Part 17 fixture is additionally checked on all 61 source frames:

```text
4 Repeater groups
34 accepted paint/copy projections per frame
17 fills + 17 strokes
16 nested applications
6 animated applications
5 shared geometry identities
10 authored paint identities
0 parity mismatches or fallback projections
0 workspace growth after prepare
```

## Deferred

- chained or nested Repeaters;
- Repeater combined with Trim/Merge operations;
- merge/boolean operations;
- gradients and dashed strokes;
- ambiguous nested/precomposition source attribution;
- masks and mattes.
