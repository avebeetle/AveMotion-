# AveMotion Part 9 report — spatial motion and world transforms

Part 9 replaces two further pieces of Telegram runtime semantics with
AveMotion-owned, backend-neutral evaluation:

1. spatial cubic `Vec2` position, including tangent angle for auto-orient;
2. composition-local world-transform and inherited-opacity propagation.

Telegram `rlottie` remains the semantic oracle. Samsung remains a comparison and
hardening baseline; its behavior is not promoted into AveMotion automatically.

## Implemented pipeline

```text
canonical property / track / segment tables
        ↓
temporal interpolation
        ↓
Telegram-compatible spatial cubic position
        ↓
local transform + optional auto-orient
        ↓
compiled transform-parent / structural-parent order
        ↓
world transform + inherited opacity
        ↓
retained PropertyEvaluationWorkspace
```

The implementation remains independent of Qt, Win32, Direct2D and the upstream
runtime types.

## Spatial cubic evaluation

AveMotion now owns:

- cubic motion-path construction from canonical spatial tangents;
- Telegram-compatible approximate arc-length measurement;
- length-to-parameter search;
- position evaluation by traveled-length ratio;
- derivative/tangent-angle evaluation for auto-orient;
- bounded failure instead of an unbounded search loop.

The search retains Telegram's observable numerical behavior. A real committed
asset, `gradient_animated_background.json`, requires 90 search iterations for
one sample. An initial safety cap of 80 produced an incorrect point. The final
cap is 256 and exhaustion returns `SpatialSearchDidNotConverge` rather than
publishing the last approximation.

## Local transform and auto-orient

The local transform evaluator now combines canonical position, anchor, scale,
rotation and opacity with the spatial tangent angle when the authored node has
auto-orient enabled. A dedicated Lottie fixture was added because the original
eight-asset corpus did not exercise auto-orient.

## World hierarchy

World state is compiled and evaluated with two intentionally distinct
relationships:

- matrix parent: explicit transform parent when present, otherwise structural
  parent;
- opacity parent: structural parent only.

A deterministic topological order is built from both dependency sets. Cycles,
invalid cross-composition references and corrupt ranges are rejected.

For each source node the retained workspace now contains:

```text
local matrix
local opacity
world matrix
world opacity
local revision / changed
world revision / changed
state / worldState
```

Repeated evaluation at the same time leaves both local and world revisions
clean.

## Independent Telegram oracle

The reference seam captures Telegram's exact local `VMatrix`, including
spatial auto-orient behavior, and independently composes world matrices using
Telegram matrix operations. AveMotion results are compared field by field for
local matrix, local opacity, world matrix and world opacity.

## Broad parity result

```text
assets / fixtures:              9
unique source-frame samples:  213
property comparisons:      26,748
spatial property samples:      406
local transforms:            5,769
world transforms:            5,769
unsupported transforms:          0
spatial search iterations:     5,814
maximum search iterations:        90
```

Coverage includes random direct seek, forward traversal, reverse traversal,
repeated exact-time evaluation and concurrent evaluation of separate
workspaces.

## Persisted golden

The committed five-sample golden remains based on the original eight corpus
assets and now includes spatial/world fields and diagnostics:

```text
SHA-256:
c67ccd48ea6bc401be307be6a0a6353f967d6dfc624accededaba66e8961a20f

rows:                        40
spatial samples:             20
spatial search iterations:  586
maximum search iterations:   67
world transforms:           675
world changes:              284
```

Clang 17 Debug and GCC 14 Release produce byte-identical manifests.

## Regression boundary

Unchanged from Part 8:

- Telegram and Samsung CPU pixel oracles;
- evaluated-scene goldens;
- render-plan goldens;
- canonical model summary and detailed parsed-model goldens;
- corpus hashes;
- vendored source fingerprints.

Intentionally changed:

- `tests/golden/property-telegram.tsv`;
- property/world diagnostics and documentation.

The property golden changed because spatial values previously marked
`Unsupported` are now evaluated and world-transform fields are now published.

## Test matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 24/24 |
| Telegram + GCC Release | 24/24 |
| Telegram + Clang AddressSanitizer | 24/24 |
| Samsung + Clang Debug | 16/16 |
| Samsung + GCC Release | 16/16 |
| Samsung + Clang AddressSanitizer | 16/16 |
| Standalone/offline Clang | 7/7 |
| Standalone/offline ASan+UBSan | 7/7 |
| CMake install/export | passed |
| external `find_package(AveMotion 0.9)` consumer | passed |

Full logs are under `docs/build-logs/part9/`.

## Allocation and concurrency behavior

`prepare()` allocates cursor, property, local-transform, world-transform and
hierarchy storage. `evaluate()` does not resize these containers. Separate
workspaces can evaluate two instances of one immutable asset concurrently.
Shared mutable cursor state is not used.

## Security and hardening boundary

Part 9 validates finite spatial control points and hierarchy references. The
length-to-parameter search is explicitly bounded. Canonical hierarchy cycles
are rejected.

The recursive cubic-length subdivision still follows Telegram behavior and has
not yet been replaced by a separately budgeted geometry subsystem. It remains
inside the validated canonical property path and is covered by sanitizer and
malformed-model tests.

## Remaining Telegram ownership

Telegram still owns:

- animated shape/path materialization;
- animated gradient values;
- shape modifiers, trim paths and repeaters;
- masks and mattes;
- nested-composition time mapping;
- complete evaluated scene production;
- CPU reference rasterization.

## Direct2D status

Part 9 is backend-neutral. The existing Direct2D skeleton and public header are
kept build-compatible, but the implementation is not compiled in this Linux
environment because the Windows SDK is unavailable. Native MSVC/Direct2D
execution remains an external Windows gate.

## Next single goal

Part 10 should materialize topology-compatible animated shape paths from the
canonical shape/track/segment tables, retain their point buffers per workspace,
assign geometry revisions only when the result changes, and prove exact
point-level parity against Telegram before feeding those paths into the existing
`MotionGeometryUpdate` seam.
