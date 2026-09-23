# Canonical Polystar and Polygon geometry

Part 13 extends AveMotion-owned generated geometry to Lottie `sr` source nodes:

```text
Polystar/Polygon canonical properties
        -> AveMotion PropertyEvaluator
        -> Polystar/Polygon path generator
        -> Telegram verb/point parity gate
        -> deterministic local geometry identity
        -> MotionGeometryUpdate / MotionRenderPlan
```

## Supported properties

Stars use:

- position;
- point count, including fractional values;
- inner and outer radius;
- inner and outer roundness;
- authored rotation;
- clockwise/counter-clockwise direction.

Polygons use:

- position;
- point count (Telegram floors it to an integer);
- outer radius;
- outer roundness;
- authored rotation;
- clockwise/counter-clockwise direction.

All properties may be static or animated. Static paths use asset-scoped
identity; any animated input makes the generated path instance-scoped.

## Pinned Telegram semantics

The implementation intentionally preserves observable behavior from the pinned
Telegram baseline rather than replacing it with a visually similar clean
formula:

- `K_PI = 3.141592f`;
- star roundness uses `0.47829f / 0.28f`;
- polygon roundness uses `0.25f`;
- fractional stars use Telegram's partial-point radius and first/last-control
  scaling;
- polygon point count is floored;
- clockwise direction advances positive angles and counter-clockwise direction
  negative angles;
- `VPath::close()` may append an explicit final line when the generated endpoint
  is not fuzzy-equal to the start;
- the pinned `LOTPolystarItem` applies the authored rotation twice after
  translating to the authored position;
- the pinned polygon implementation performs its historical second -90-degree
  conversion before generating the first point.

The last two rules look unusual, but they are part of the baseline's real
rendered behavior. AveMotion does not silently “correct” them while Telegram is
the selected semantic oracle.

## Complexity bound

This laboratory accepts at most 256 authored star/polygon points. The largest
accepted rounded star can therefore emit up to 512 cubic segments. The path
storage is bounded and generation fails closed if the limit or any finite-value
invariant is violated.

The limit is a provisional security/performance boundary, not a Lottie format
claim. A future offline compiler may choose a different validated profile.

## Acceptance boundary

A generated Polystar/Polygon is published only when:

- the source node is a parsed `Polystar`;
- its subtype is known (`Star` or `Polygon`);
- every required canonical property evaluates successfully;
- point count and all numeric inputs are finite and within the laboratory bound;
- the source is modifier-free and connected to one local solid fill;
- source/paint transforms are available;
- generated verbs and transformed points match Telegram's extracted local path.

Any failure leaves Telegram's established geometry untouched.

## Dedicated fixture

`tests/fixtures/polystar_polygon_geometry.json` covers:

- sharp clockwise star;
- rounded counter-clockwise star;
- fractional clockwise star;
- sharp clockwise polygon;
- rounded counter-clockwise polygon;
- animated star topology/radii/roundness/position/rotation;
- animated polygon topology/radius/roundness/position/rotation;
- fully rounded counter-clockwise triangle star.

All eight primitives are checked on every one of the fixture's 61 source
frames, plus direct-seek/sequential and repeated-time cache-key tests.
