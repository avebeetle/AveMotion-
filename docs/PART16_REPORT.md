# AveMotion Part 16 report — direct Repeater geometry

## Executive result

Part 16 adds the first Telegram-compatible Repeater path to AveMotion:

```text
supported canonical content group
+ copies / offset
+ Repeater transform and opacity
-> shared base geometry and paint identity
-> per-copy matrix and opacity
-> exact Telegram parity
-> deterministic MotionRenderPlan resources
```

Telegram remains the primary semantic, evaluated-scene and CPU pixel oracle.
Samsung remains comparison/hardening only.

## Architecture

Telegram materializes one runtime content tree per maximum copy. AveMotion does
not copy that ownership model into its render resources. It reconstructs the
canonical content path once, assigns one asset-scoped geometry identity and one
authored paint identity, then publishes transform and opacity per generated
copy.

```text
Canonical paths ----┐
Canonical fill -----+-> shared geometry/paint keys
Repeater properties ┘
                         ├-> copy 0 matrix/opacity
                         ├-> copy 1 matrix/opacity
                         └-> copy N matrix/opacity
```

## Telegram semantics reproduced

The implementation preserves `LOTRepeaterTransform::matrix()`,
`LOTRepeaterItem::update()` and `VMatrix` operation order:

- multiplier `copyIndex + offset`;
- position multiplied by the multiplier;
- anchor translate, exponential per-axis scale, rotation and inverse anchor;
- matrix composed with parent world and root viewport transforms;
- opacity interpolation using `copyIndex / copies`;
- `int(copies)` visible-copy classification;
- allocated copies beyond the visible prefix remain present with zero opacity;
- Telegram special-angle trigonometric branches.

## Binding and fail-closed boundary

A binding is accepted only for one direct Repeater content group with one or
more supported direct paths and one local solid fill. Required canonical
properties are `copies`, `offset`, position, scale, rotation, anchor, start
opacity and end opacity.

The result is published only after exact checks of:

1. local canonical path;
2. final transformed path;
3. full copy matrix;
4. separated copy opacity.

Nested/chained Repeaters, modifier combinations, incomplete local paint seams
and unresolved compositions remain on Telegram geometry.

## Cache identities

All copies of one group share:

- the same asset-scoped `GeometryCacheKey`;
- the same authored `PaintCacheKey`;
- the same canonical local path hash.

Each copy separately publishes:

- copy index;
- visible/max copy counts;
- transform content revision;
- opacity content revision.

Direct seek and sequential traversal produce identical geometry and paint cache
keys. Repeated exact-time evaluation produces no geometry or paint updates.

## Retained workspace

The existing `SourceGeometryProjectionWorkspace` now includes retained copy
counters per canonical Repeater. Its capacity is prepared before evaluation.
The exhaustive 61-frame test leaves `storageGeneration()` unchanged.

## Dedicated fixture

`tests/fixtures/repeater_geometry.json` contains six Repeater groups and 24
allocated copies per frame:

1. static integer copies;
2. fractional copies;
3. negative offset;
4. animated copies and offset;
5. animated transform and start/end opacity;
6. multiple paths under one fill.

Across all 61 source frames:

```text
copy projections:            1,464
asset-static geometry refs:  1,464
transform-animated copies:     549
opacity-animated copies:       549
binding/property/eval errors:    0
input rejections:                0
parity mismatches:               0
workspace growth:                0
```

## Persisted source-geometry oracle

The golden contains 65 samples across 13 assets/fixtures.

Aggregate metrics:

```text
draw items visited:               604
source-bound candidates:          454
accepted projections:             309
asset-static / animated:      261 / 48
projected points:                5,493

trim candidates:                   75
accepted trim projections:         70

Repeater candidates/copies:       120
accepted Repeater copies:         120
visible / hidden copies:      109 / 11
asset-static base geometry refs:  120
transform-animated copies:         45
opacity-animated copies:           45
Repeater fallback/parity errors:    0
```

Golden SHA-256:

```text
f0abb0e349e0831d31f3388ec2c8b76bf39a07b1dadadcf04510bbcc3878288d
```

Fixture SHA-256:

```text
8a7bbc1974d6d74002dca2e258d97e0d55dacfb49cbcecf8ccb56ea2e3d06dda
```

## Regression boundaries

Unchanged:

- CPU pixel manifests and reference frames;
- evaluated-scene manifests;
- legacy MotionRenderPlan manifests;
- canonical model and parsed-model manifests;
- property/spatial/world/Shape manifests;
- vendored source fingerprints.

Intentionally updated:

- source-geometry manifest and diagnostics;
- dedicated Repeater unit and exhaustive parity tests;
- current baseline/docs/version.

## Windows status

The project remains Windows-first and preserves Telegram/Samsung MSVC presets
and the Direct2D target. The Part 16 execution environment is Linux without the
Windows SDK, so the backend-neutral model/evaluator/projector/render-plan path
is locally verified while native `ID2D1PathGeometry` execution remains a
Windows CI/host gate.

## Deferred

- nested/chained Repeaters;
- Repeater + Trim/Merge paths;
- multi-paint and local stroke/gradient content;
- masks/mattes/effects;
- scheduler and `.avm` serialization.

## Validation matrix

The final Part 16 source tree passes:

| Configuration | Result |
|---|---:|
| Telegram + Clang 17 Debug | 30/30 |
| Telegram + GCC 14 Release | 30/30 |
| Telegram + Clang 17 ASan | 30/30 |
| Samsung + Clang 17 Debug | 19/19 |
| Samsung + GCC 14 Release | 19/19 |
| Samsung + Clang 17 ASan | 19/19 |
| Standalone/offline Clang | 10/10 |
| Standalone ASan + UBSan | 10/10 |
| CMake install/export | PASS |
| External `find_package(AveMotion 0.16)` consumer | PASS |
| Clang/GCC source-geometry golden | byte-identical |

Native Windows/MSVC/Direct2D execution remains an external Windows gate because
the build environment is Linux without the Windows SDK.

## Release identity

```text
version: 0.16.0
tag:     stage15-repeater-part16
```
