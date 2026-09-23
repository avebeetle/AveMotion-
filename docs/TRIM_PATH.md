# AveMotion Trim Path compatibility

## Current scope

Part 15 owns Trim Path evaluation for one or several direct canonical local
paths controlled by exactly one Trim Path node in a supported source group.

```text
ordered canonical local paths
+ start / end / offset
+ Simultaneous or Individual mode
-> retained multi-path trim workspace
-> trimmed path collection
-> paint-local combined path
-> exact Telegram parity gate
-> deterministic geometry identity
```

The result is published only after verb-for-verb and point-for-point comparison
with Telegram's evaluated local and final paths. Any unsupported relationship or
mismatch keeps the established Telegram geometry untouched.

## Binding subset

A direct group binding is accepted when:

- one enabled Trim Path exists;
- one or more direct path-like nodes occur before that trim;
- an enabled local solid fill after the trim consumes the same path count;
- no repeater or nested shape group is present;
- source properties and transforms are available in the same root composition;
- the complete local geometry/paint coordinate-space seam is available.

The previous Part 14 path-based single-path binding remains available so old
classification/fallback behaviour is preserved, including paint-before-trim
cases such as a stroked polystar.

## Mode semantics

### Simultaneous

The normalized interval is applied independently to each source path:

```text
path A -> trim(start, end)
path B -> trim(start, end)
path C -> trim(start, end)
```

### Individual

For a non-wrapped interval, Telegram distributes one absolute interval over the
aggregate approximate path length in authored order:

```text
L = length(A) + length(B) + length(C)
absoluteStart = start * L
absoluteEnd   = end   * L
```

The overlap with each path is converted back to that path's local normalized
interval. Paths before/after the aggregate interval become empty; fully covered
paths preserve their original streams.

### Wrapped Individual compatibility

The pinned Telegram implementation performs no trim operation when normalized
`Individual` has `start > end`. AveMotion preserves all source paths and records
`individualWrappedNoOp`. This is intentionally compatibility behaviour, not a
new mathematical interpretation.

## Normalization and measurement

`normalizeTrimSegment()` reproduces pinned `LOTTrimData::segment()` behaviour:

```text
startPercent / 100
endPercent   / 100
offsetDegrees / 360
-> normalized empty/full/partial/wrapped interval
```

Approximate line length:

```text
max(abs(dx), abs(dy)) + 0.375 * min(abs(dx), abs(dy))
```

Cubic length recursively compares control-polygon and chord lengths until the
Telegram-compatible `0.01` tolerance is reached, with defensive depth and
iteration bounds. Cubics are split through De Casteljau at the measured
position.

## Retained scratch model

`TrimmedPathCollection` owns prepared flattened buffers for:

- output verbs and points;
- per-path slices;
- source lengths;
- one reusable single-path trim scratch.

`SourceGeometryProjectionWorkspace` owns:

- source path streams;
- candidate records;
- trim inputs;
- retained trimmed collection;
- paint-local combined output.

`SourceGeometryProjector::prepare()` establishes conservative capacities from
the immutable canonical model. `storageGeneration` changes when any capacity
grows. Exhaustive tests assert that it remains unchanged for all 61 fixture
frames after preparation.

## Identity and revisions

```text
all source paths static + trim values static
-> asset-scoped geometry identity

any source path or trim value animated
-> instance-scoped geometry identity
```

The authoritative dynamic revision derives from final paint-local path content.
Cursor history and traversal order therefore do not affect backend cache keys.

## Fixture

`tests/fixtures/multi_trim_path_geometry.json` contains six groups and fifteen
source paths:

- two-path Simultaneous;
- two-path Individual with unequal lengths;
- three-path Individual mixing Rectangle, Ellipse and Star;
- animated three-path Individual;
- wrapped two-path Individual no-op;
- wrapped three-path Simultaneous;
- one non-identity group transform to exercise coordinate-space separation.

All 61 source frames must produce six accepted projections, fifteen visited and
projected source paths, zero parity mismatches and no retained-workspace growth.
The animated Individual group becomes wrapped at frame 33, exercising both
normal distribution and compatibility no-op in one asset.

## Deferred

- repeaters;
- merge/boolean paths;
- chained trims;
- nested path groups and unresolved precomposition attribution;
- local stroke/gradient seams not yet proven in the same coordinate space;
- masks, mattes and effects.
