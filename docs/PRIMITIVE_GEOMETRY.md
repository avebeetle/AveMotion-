# Canonical Rectangle and Ellipse geometry

Part 12 extends the AveMotion-owned source-geometry path from authored
`ShapePath` values to generated Lottie primitives:

```text
Rectangle position / size / roundness / direction
Ellipse position / size / direction
        -> AveMotion primitive path generator
        -> Telegram verb/point parity gate
        -> deterministic local path identity
        -> MotionGeometryUpdate / MotionRenderPlan
```

## Exact Telegram semantics

The generator intentionally follows the pinned Telegram `VPath` semantics,
including details that are easy to miss in a clean-room approximation:

- rectangle and ellipse positions are centered;
- `d=1` is clockwise and `d=3` is counter-clockwise;
- roundness is clamped against the effective `VRectF` width and height;
- a zero roundness produces four line segments plus `Close`;
- ellipses use `PATH_KAPPA = 0.5522847498f`;
- rounded rectangles use Telegram's quarter-arc construction;
- `VPath::close()` may insert an explicit final `LineTo` when the computed end
  point is not fuzzy-equal to the start point.

The last rule is observable for some animated intermediate radii. AveMotion
therefore reproduces the original `VRectF`, arc-endpoint and float-operation
order rather than raising the parity tolerance.

## Acceptance boundary

A primitive is published only when:

- the source node is a parsed Rectangle or Ellipse;
- all required canonical properties evaluate successfully;
- the source is modifier-free and bound to one local solid fill;
- path and paint transforms are available;
- generated verbs and transformed points match Telegram's extracted local path.

Any failure leaves the established Telegram geometry unchanged.

## Resource identity

Static primitive properties produce asset-scoped geometry. Animated position,
size or roundness produces instance-scoped geometry. The authoritative revision
is derived from the generated local path content, so direct seek and sequential
traversal produce the same backend cache key for the same path.

## Fixture

`tests/fixtures/primitive_geometry.json` explicitly covers:

- sharp clockwise rectangle;
- rounded clockwise rectangle;
- clamped counter-clockwise rounded rectangle;
- clockwise ellipse;
- counter-clockwise ellipse;
- animated rounded rectangle;
- animated counter-clockwise ellipse.

All seven primitives pass exact Telegram parity at five sampled times.

## Part 13 extension

Polystar and Polygon generation is documented separately in
`POLYSTAR_GEOMETRY.md`. It uses the same fail-closed source-geometry seam and
Telegram local-path oracle; the Rectangle/Ellipse implementation and goldens
remain unchanged.
