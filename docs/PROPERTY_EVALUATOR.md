# AveMotion property evaluator — Part 10

`AveMotion::Evaluation` consumes immutable canonical authored tables and
publishes retained backend-neutral values without calling `rlottie` for its
supported subset.

## Inputs and outputs

```text
MotionAssetModel
    Property[] / Track[] / Segment[] / SourceNode[]
+ exact asset frame
+ per-instance workspace
    -> PropertyEvaluationView
       properties
       local transforms + opacity
       world transforms + inherited opacity
       revisions / dirty flags / diagnostics
```

## Temporal evaluation

Supported modes:

- static;
- hold;
- linear;
- temporal cubic Bezier using Telegram's 11-sample LUT, Newton iterations and
  binary subdivision fallback.

Segment lookup uses a cursor for same/adjacent motion and binary search for
random seeks. Cursor history never changes semantics.

## Spatial cubic position

Temporal progress is converted to a distance on the authored cubic path:

```text
temporal progress
-> requested approximate arc length
-> Telegram-compatible t-at-length search
-> cubic point and derivative
-> position and tangent angle
```

The path-length metric and subdivision match the pinned Telegram implementation.
The original unbounded search is bounded at 256 iterations; failure is reported
as `SpatialSearchDidNotConverge`. Real corpus coverage reaches 90 iterations.

`EvaluatedProperty` stores the spatial angle separately so auto-orient can use
it without coupling ordinary Vec2 consumers to transform semantics.

## Local transforms

The supported 2D sequence matches Telegram:

```text
translate(position)
-> rotate(authored rotation + auto-orient angle)
-> scale(scale / 100)
-> translate(-anchor)
```

Combined position and separated X/Y are supported. 3D rotations remain
unsupported.

## World hierarchy

A deterministic topological order is compiled when the evaluator is created.
For each node:

```text
worldMatrix  = localMatrix * selectedParentWorldMatrix
worldOpacity = localOpacity * structuralParentWorldOpacity
```

The selected matrix parent is `transformParent` when present, otherwise the
structural parent. Opacity always follows the structural parent. Cycles,
cross-composition hierarchy edges and invalid parent references are rejected.

## Retained revisions

Properties and transforms carry revisions and change flags. Revisions advance
only when canonical outputs change. Local and world revisions are independent:
a parent change may leave a child's local revision unchanged while advancing its
world revision.

Repeated exact-time evaluation is clean for properties, local transforms and
world transforms.

## Workspace model

`prepare()` sizes cursors, property values and transform buffers. Subsequent
evaluation does not resize them. One workspace belongs to one evaluation stream;
multiple workspaces can use one immutable evaluator concurrently.

## Telegram parity

The broad suite compares nine assets/fixtures over 213 source-frame samples,
including exact boundaries, random seeks, forward/reverse traversal and a
dedicated auto-orient fixture.

```text
property checks:        26,748
spatial checks:            406
local transform checks:  5,769
world transform checks:  5,769
unsupported transforms:      0
```

## Remaining limitations

- animated Shape and Gradient values are explicit `Unsupported`;
- world values are composition-local;
- nested precomposition time/instance mapping is not replaced;
- final integer canonical time is not selected;
- complete scene, modifiers, masks, mattes and pixels still use Telegram.


## Part 10 animated Shape extension

Animated Shape properties are now materialized into retained per-workspace
point buffers. See `docs/SHAPE_EVALUATION.md` for encoding, Telegram-compatible
interpolation, revisions and current limitations. Animated Gradient values
remain explicitly unsupported.
