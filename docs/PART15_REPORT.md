# AveMotion Part 15 report — multi-path Trim Path and retained scratch

## Executive result

Part 15 completes the first Telegram-compatible multi-path geometry modifier in
AveMotion:

```text
several direct canonical local paths
+ one Trim Path
+ Simultaneous or Individual mode
-> retained trimmed path collection
-> paint-local combined path
-> exact Telegram parity
-> deterministic MotionRenderPlan identity
```

The existing one-path Part 14 path remains compatible. Telegram is still the
primary semantic and pixel oracle; Samsung is comparison/hardening only.

## Implementation

### `TrimmedPathCollection`

A retained collection replaces by-value temporary vectors. It owns flattened
verb/point buffers, per-path slices, approximate source lengths and one reusable
single-path scratch result. It supports:

- `prepare()` for conservative capacity establishment;
- `reset()` without deallocation;
- safe per-path spans;
- storage-generation diagnostics;
- Simultaneous and Individual collection modes.

### `SourceGeometryProjectionWorkspace`

The projector now has an explicit move-only workspace prepared against one
canonical model fingerprint. It retains:

- temporary source path generation buffers;
- flattened source paths;
- candidate metadata and path-to-paint transforms;
- trim inputs;
- multi-path trim output;
- final paint-local combined geometry.

The old convenience `project(scene, properties)` API remains for one-shot tools
but is not the steady-state path.

### Binding model

Part 15 adds paint-group bindings for one direct Trim Path and one or more direct
paths before it, consumed by a local solid fill. The Part 14 single-path
path-based binding remains intact for backwards classification/fallback,
including paint-before-trim cases.

Nested groups, repeaters, chained trims and incomplete local-paint seams remain
fail-closed.

## Telegram semantics reproduced

### Simultaneous

The same normalized interval is independently applied to every source path.

### Individual

A non-wrapped interval is mapped onto the sum of approximate source path
lengths. The aggregate interval is distributed in authored path order, yielding
empty, full or partially trimmed per-path results.

### Wrapped Individual

The pinned Telegram implementation performs no operation when normalized
`start > end` in Individual mode. AveMotion preserves all paths and exposes a
separate diagnostic. The animated fixture crosses into this state at frame 33.

### Measurement and splitting

Part 14's Telegram-compatible line approximation, recursive cubic length,
iterative length-to-parameter search and De Casteljau splitting remain the
single source of truth. No visually similar replacement algorithm was added.

## Allocation result

After `SourceGeometryProjector::prepare()`:

- all source/candidate/combined vectors retain capacity;
- all multi-path trim buffers retain capacity;
- per-path slices and lengths retain capacity;
- exhaustive 61-frame evaluation leaves `storageGeneration` unchanged.

This proves no vector-capacity growth in the new steady projection path. It is a
stronger and more precise claim than saying the whole process performs no
possible runtime allocation: upstream Telegram evaluation and unrelated host
code remain outside this workspace diagnostic.

## New fixture

`tests/fixtures/multi_trim_path_geometry.json` contains six groups, fifteen
source paths and a non-identity group transform:

1. two-path Simultaneous;
2. two unequal paths in Individual mode;
3. three mixed Rectangle/Ellipse/Star paths in Individual mode;
4. animated three-path Individual;
5. wrapped two-path Individual no-op;
6. wrapped three-path Simultaneous.

All 61 source frames pass exact local/final Telegram parity.

Per frame:

```text
trim groups:                 6
multi-path groups:           6
source paths visited:       15
source paths projected:     15
Simultaneous groups:         2
Individual groups:           4
accepted projections:        6
asset-static:                5
instance-animated:           1
parity mismatches:            0
fallbacks:                    0
workspace growth:             0
```

Wrapped-Individual no-op count is one for frames 0–32 and two for frames 33–60
because the animated Individual group crosses the wrap boundary.

## Persisted source-geometry oracle

The golden now contains 60 samples across 12 assets/fixtures.

Aggregate metrics:

```text
draw items visited:             484
source-bound candidates:        454
accepted projections:           189
asset-static / animated:    141 / 48
projected points:              3,198

trim candidates:                 75
Simultaneous / Individual:  50 / 25
multi-path trim groups:          30
source paths visited:           120
source paths projected:         115
wrapped Individual no-ops:        7
accepted trim projections:       70
empty / full / partial:    5 / 12 / 53
split operations:               119
trim input/parity failures:       0
```

All 55 previously persisted rows preserve every prior semantic field. The only
intentional changes are four new diagnostic columns and five new fixture rows.

Golden SHA-256:

```text
b9d99319180465fab83244a3e2ef585c10738af3b172c18aecab89217a71f275
```

Fixture SHA-256:

```text
4bf3f3c1397bbeb6c5821a676eec82ca5299ba94be39fdad2e63e079d64b0dd3
```


## API additions

```cpp
class SourceGeometryProjectionWorkspace;

bool SourceGeometryProjector::prepare(
    SourceGeometryProjectionWorkspace&) const;

SourceGeometryProjectionResult SourceGeometryProjector::project(
    EvaluatedScene&,
    const PropertyEvaluationView&,
    SourceGeometryProjectionWorkspace&) const;
```

The workspace is one-stream/one-instance scratch and is not concurrently
shareable.

## Regression boundaries

Unchanged oracles:

- CPU pixel manifests and reference frames;
- evaluated-scene manifests;
- legacy MotionRenderPlan manifests;
- canonical model and parsed-model manifests;
- property/spatial/world/Shape manifests;
- vendored source fingerprints.

Intentionally updated:

- source-geometry manifest;
- dedicated trim and source-projection tests;
- current baseline/docs/version.

## Windows status

The project remains Windows-first and preserves MSVC Telegram/Samsung presets
and the Windows Direct2D target. The execution environment used for Part 15 is
Linux without the Windows SDK, so backend-neutral code and oracles are locally
verified while native `ID2D1PathGeometry` execution remains a Windows CI/host
gate.

## Deferred

- repeaters;
- merge/boolean paths;
- nested/chained modifiers;
- masks/mattes/effects;
- local stroke/gradient seams not yet proven in the same coordinate space;
- centralized scheduler and `.avm` serialization.

## Validation matrix

The final Part 15 source tree was rebuilt after the retained-capacity overflow
hardening and passed:

| Configuration | Result |
|---|---:|
| Telegram + Clang 17 Debug | 29/29 |
| Telegram + GCC 14 Release | 29/29 |
| Telegram + Clang 17 ASan | 29/29 |
| Samsung + Clang 17 Debug | 18/18 |
| Samsung + GCC 14 Release | 18/18 |
| Samsung + Clang 17 ASan | 18/18 |
| Standalone/offline Clang | 9/9 |
| Standalone ASan + UBSan | 9/9 |
| CMake install/export | PASS |
| External `find_package(AveMotion 0.15)` consumer | PASS |

The first aggregate Telegram-ASan orchestration command was externally stopped
while starting a later characterization test. The test passed independently,
and a clean full rerun completed with 29/29 passing. No sanitizer diagnostic was
reported.

The committed Clang and GCC source-geometry manifests are byte-identical. A
field-by-field compatibility check also proves that all 55 Part 14 records retain
all 48 pre-existing semantic fields; Part 15 only adds four diagnostic columns
and five fixture records.

## Release identity

```text
version: 0.15.0
tag:     stage14-multi-trim-part15
```

The final commit SHA is recorded in the external delivery report generated from
the tagged repository.

## Next isolated stage

Part 16 should implement direct-path Repeater geometry over an already proven
local path stream:

```text
canonical local path
+ repeater copies / offset / transform / opacity
→ ordered repeated local paths
→ Telegram parity
→ deterministic per-copy geometry identity
→ MotionRenderPlan
```

It should not simultaneously add merge/boolean operations, masks, mattes,
scheduling or `.avm` serialization.
