# AveMotion Part 17 report — Repeater content groups and local solid paint

## Objective

Part 17 completes one bounded objective:

> Extend the proven direct Repeater seam from one solid-fill path set to a
> supported content group containing direct nested ShapeGroups and multiple
> solid Fill/Stroke paint applications, while preserving exact Telegram parity
> and sharing native-resource identities wherever the authored geometry is the
> same.

No new scheduler, serialization, masks, mattes, gradients, chained Repeaters or
Repeater+Trim semantics were introduced.

Version:

```text
0.17.0
```

Planned release tag:

```text
stage16-repeater-content-group-part17
```

## Baseline

Primary semantic oracle:

```text
TelegramMessenger/rlottie
67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d
```

Comparison/hardening lineage:

```text
Samsung/rlottie
2cab35db755b0e39df40b679969495e90d39c578
```

Telegram remains authoritative. Samsung behaviour is not substituted silently.

## Implemented pipeline

```text
canonical Repeater and content-group SourceNodes
+ canonical PropertyEvaluator output
        ↓
recursive supported ShapeGroup traversal
        ↓
ordered active path sets
+ authored solid Fill / dashless solid Stroke applications
        ↓
paint-local canonical geometry binding
+ authored paint binding
        ↓
per-copy Repeater matrix and opacity
        ↓
exact Telegram local/final path, matrix, paint and stroke parity
        ↓
EvaluatedScene local geometry/paint seam
        ↓
MotionRenderPlan cache identities and updates
```

## Recursive content-group binding

The projector now reproduces the supported subset of Telegram's paint-item
traversal instead of requiring a flat direct group with one Fill.

Accepted authored nodes:

- direct path-like nodes;
- direct nested ShapeGroups;
- solid Fill;
- solid Stroke without dash data;
- group Transform;
- exactly one controlling Repeater.

The traversal records one pending paint application for every accepted Fill or
Stroke. Each application receives the paths accumulated within its structural
group. Unsupported nodes invalidate the entire candidate binding and preserve
Telegram's established output.

Depth is bounded and all parent/child relations are validated.

## Geometry and paint identity separation

Part 17 separates two concepts that were coupled in the first Repeater seam:

```text
geometry binding
= Repeater + paint-space node + ordered source paths + fill rule

paint binding
= Repeater + authored Fill/Stroke node
```

This permits a Fill and Stroke over the same paths to share one canonical
geometry and future native path resource while retaining separate paint and
stroke-style resources.

A Direct2D-relevant detail was discovered during clean recompilation: fill mode
is part of native path geometry. A Stroke does not consume that mode, so it is
bound to the first compatible authored Fill rule for the same path set. A Fill
with a genuinely different rule receives a distinct geometry binding. This
keeps sharing correct rather than merely forcing equal cache keys.

## Coordinate-space model

Each accepted source path is materialized in the local coordinate space of its
paint application:

```text
path world matrix * inverse(paint-space world matrix)
= path-to-paint relative matrix
```

Per-copy final matrix:

```text
paint-relative matrix
* Telegram-compatible Repeater copy matrix
* Repeater parent world matrix
* root viewport matrix
```

The candidate is accepted only when:

- canonical local path equals Telegram local path;
- the candidate transformed by the computed matrix equals Telegram final path;
- the computed matrix equals Telegram `localToViewport`;
- all values are finite.

## Local Fill and Stroke evaluation

Paint values are evaluated from canonical property tables, not copied from the
final raster-oriented result.

Supported local values:

- solid color;
- paint opacity;
- stroke width;
- stroke cap;
- stroke join;
- miter limit.

Telegram-compatible RGBA byte conversion and stroke scale calculation are used.
For Stroke, the following final values are compared against Telegram before
publication:

- final color and alpha;
- scaled width;
- cap;
- join;
- miter;
- empty dash array.

Dash patterns remain fail-closed.

## Static and animated classification

Geometry is asset-scoped only when both source geometry and all transforms
relative to paint space are static. Repeater transform/opacity animation does
not change geometry identity.

Paint is asset-scoped only when its canonical color, opacity and stroke-width
properties are static. Animated paint applications are instance-scoped.

```text
static geometry + animated paint
→ shared geometry key
→ instance paint key
```

## Retained workspace

Part 17 reuses the existing prepared `SourceGeometryProjectionWorkspace`:

- active source paths;
- temporary local path streams;
- combined paint-local geometry;
- Repeater copy counters.

The exhaustive fixture verifies that `storageGeneration()` remains unchanged
after `prepare()` across all 61 frames and direct/sequential/repeated passes.

## New fixture

Added:

```text
tests/fixtures/repeater_content_group.json
```

It contains four independent groups:

1. direct Rectangle with Fill and Stroke;
2. nested Rectangle and Ellipse groups, each with Fill and Stroke;
3. two nested path groups with outer Fill and Stroke applications;
4. animated Fill and Stroke properties under one Repeater.

Every one of the 61 source frames must produce:

```text
Repeater groups:                     4
accepted paint/copy projections:    34
solid Fill applications:            17
solid Stroke applications:          17
nested paint applications:          16
animated paint applications:         6
shared geometry identities:          5
authored paint identities:          10
fallbacks/parity mismatches:          0
```

Additional invariants:

- every generated copy index is present exactly once per authored paint;
- Fill and Stroke over one path set share one complete `GeometryCacheKey`;
- all copies of one authored paint share one complete `PaintCacheKey`;
- direct seek equals sequential traversal for geometry, paint, matrix and
  opacity;
- repeated exact-time evaluation produces zero geometry and paint updates.

## Persisted source-geometry corpus

The golden now contains 70 rows across 14 assets/fixtures.

Aggregate values:

```text
draw items visited:                 774
source candidates:                  454
accepted projections:               479
asset-static projections:           431
instance-evaluated projections:      48
projected points:                  8,613

Repeater paint/copy projections:     290
visible / hidden:              279 / 11
asset-static Repeater geometry:      290
animated Repeater geometry:            0
transform-animated copies:            45
opacity-animated copies:              45
solid Fill applications:             205
solid Stroke applications:            85
nested paint applications:            80
animated paint applications:          30
Repeater failures/mismatches:           0
```

Golden file:

```text
tests/golden/source-geometry-telegram.tsv
SHA-256 d192c55dc4b9b54d8eca1f9a3e3b5f00fc45c7c9ae0681dd635301510fae63d0
```

Backward compatibility was checked by field name:

```text
pre-Part-17 rows:        65
missing rows:             0
changed old field values: 0
new diagnostic fields:    4
new fixture rows:         5
```

Clang Debug and GCC Release generated byte-identical manifests matching the
committed golden.

## Validation matrix

```text
Telegram + Clang Debug:          30/30 PASS
Telegram + GCC Release:          30/30 PASS
Telegram + Clang ASan:           30/30 PASS
Samsung + Clang Debug:           19/19 PASS
Samsung + GCC Release:           19/19 PASS
Samsung + Clang ASan:            19/19 PASS
Standalone/offline Clang:        10/10 PASS
Standalone ASan + UBSan:         10/10 PASS
CMake install/export:                 PASS
External find_package consumer:       PASS
```

Warnings printed by GCC from old vendored rlottie/pixman/stb code are preserved
upstream diagnostics. AveMotion-owned targets continue to use strict project
warnings.

## Packaging

The installed package version is now `0.17.0`. A clean external consumer
successfully configures with:

```cmake
find_package(AveMotion 0.17 REQUIRED CONFIG)
```

and links the exported Core, Model, Evaluation, Runtime and Rendering targets.

## Windows status

The product target remains Windows/Direct2D. The existing Direct2D backend can
consume the geometry and paint identities produced here:

```text
one shared path geometry
+ separate Fill/Stroke resources
+ per-copy matrix and opacity
```

The current execution environment is Linux without Windows SDK. Therefore this
stage does not claim native MSVC compilation or actual `ID2D1DeviceContext`
drawing. Windows presets and CI targets remain in the project; native execution
is the proposed next isolated gate.

## Deliberately deferred

- chained or nested Repeaters;
- Repeater + Trim/Merge combinations;
- gradients;
- dashed strokes;
- masks, mattes and effects;
- scheduler/player extraction;
- `.avm` serialization.

## Proposed Part 18

Before adding another semantic family, validate the already proven subset in a
real Windows host:

```text
MotionRenderPlan
→ MotionD2DBackend
→ Win32 preview
→ MSVC build
→ DPI/resize/device recreation
→ reference capture comparison
```

This keeps the next stage narrow and verifies the actual production platform
without expanding animation semantics.
