# AveMotion Part 6 report: immutable asset tables and host-driven playback

## Executive result

Part 6 moves AveMotion from lazily interned per-sample resources to an explicit,
immutable, typed asset model. Telegram remains the evaluator oracle, but stable
source identities are assigned before evaluation and compiled into contiguous
AveMotion tables. A lightweight instance facade now maps absolute host time to
forward/reverse/loop/once playback without owning a timer or renderer.

```text
Telegram source model
    -> stable source IDs
    -> immutable MotionAssetModel
    -> shared by N MotionInstances
    -> exact Telegram evaluation adapter
    -> EvaluatedScene with typed IDs
    -> MotionRenderPlan
```

## New public target

```text
AveMotion::Model
```

It contains no `rlottie`, Win32, Direct2D, COM, Qt or host-application types.

## Immutable model

The model contains contiguous tables for layers, nodes, geometry, paint, clips,
child references, per-layer node references, and draw order. All references use
asset-local typed IDs.

Corpus totals:

| Table | Records |
|---|---:|
| Layers | 291 |
| Nodes | 148 |
| Geometries | 148 |
| Paints | 148 |
| Child-layer references | 283 |
| Layer-node references | 148 |
| Default clips | 8 |
| Asset-static geometries | 11 |
| Asset-static paints | 137 |

No static-promotion conflict was observed.

## Stable Telegram IDs

The additive Telegram extension assigns deterministic layer/node/geometry/paint
IDs while constructing the runtime item tree. It also publishes declared table
counts at the root. The old render tree and CPU path are unchanged.

The exact patch is:

```text
patches/telegram/0003-avemotion-stable-source-ids.patch
```

Post-patch Telegram source fingerprint:

```text
5132c3efbf0e7046de0a03633d1eeefb9e1e30a4b4e0d90344bd9d1c2368b881
276 files
13,273,733 bytes
```

## Static ownership

Static local resources are copied into `MotionGeometryRecord` and
`MotionPaintRecord` values. Evaluated draw items use aliasing `shared_ptr`
references whose owner is the immutable model snapshot; instances do not own or
copy static values.

Dynamic values remain per-instance and receive instance-scoped cache keys.

## Playback facade

New API types:

```text
MotionTime
PlaybackStatus
PlaybackDirection
PlaybackLoopMode
PlaybackSnapshot
```

New instance controls:

```text
play
pause
resume
stop
seekNormalized
setControlledProgress
setDirection
setPlaybackRate
setLoopMode
playbackSnapshot
evaluateAt
```

The mapping uses absolute host time and re-anchors on state changes. Tests cover:

- forward playback;
- pause/resume;
- normalized seek;
- reverse without a jump;
- reverse start at the terminal position;
- controlled progress;
- playback-rate changes;
- once completion and holding;
- repeated exact-time evaluation.

## New regression boundary

`tests/golden/model-telegram.tsv` records one deterministic row per asset:

- schema and revision;
- model/topology/resource fingerprints;
- logical size, frame rate and frame count;
- declared/observed table counts;
- static-resource counts;
- mask, clip and adjacency counts.

SHA-256:

```text
90809e27a3cf6db9b64b8b4b5f3de76bdcf3ab48e187d302efad763da6a6a463
```

The manifest is byte-identical under Clang and GCC.

## Preserved regression boundaries

Unchanged:

- Telegram/Samsung CPU pixel manifests;
- evaluated-scene manifests;
- source corpus hashes;
- Part 5 canonical coordinate-space tests.

Render-plan aggregate work also remains unchanged:

| Metric | Part 5 | Part 6 |
|---|---:|---:|
| Source/visible draw items | 339 | 339 |
| Geometry updates | 290 | 290 |
| Paint updates | 152 | 152 |
| Asset-static geometry refs | 36 | 36 |
| Instance geometry refs | 303 | 303 |
| Asset-static paint refs | 290 | 290 |
| Instance paint refs | 49 | 49 |
| Unsupported items | 20 | 20 |

The typed model therefore changes identity ownership, not visual work.

## Final builder review

The final review also closed a copy-on-write edge case: changes limited to
declared table counts, mask metadata or dependency bits now mark the model
snapshot as changed and advance its revision. The corpus output was already
identical, so no golden file changed, but future assets can no longer lose that
metadata when no geometry or paint value changes in the same frame.

## Test matrix

Final Linux gates:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 18/18 |
| Telegram + GCC Release | 18/18 |
| Telegram + Clang ASan | 18/18 |
| Samsung + Clang Debug | 15/15 |
| Samsung + GCC Release | 15/15 |
| Samsung + Clang ASan | 15/15 |
| Offline/installable SDK | 6/6 |
| Clean external `find_package(AveMotion 0.6)` consumer | passed |

These counts were reproduced from clean final builds before packaging.

## Compiler determinism

For each upstream variant, Clang and GCC produced byte-identical:

- CPU manifests;
- evaluated-scene manifests;
- render-plan manifests.

The Telegram immutable model manifest is also byte-identical.

## Known limitations

- immutable model preparation currently scans every frame and creates a fresh
  Telegram runtime tree per exact sample;
- scan length is capped at 10,000 frames;
- only Telegram exposes stable typed model IDs;
- one default full-range clip is synthesized;
- property/track/keyframe tables are not yet extracted;
- playback maps to discrete Telegram frame indices;
- no player scheduler, marker events, ping-pong, parameter block, or state graph;
- model strings and optional values are not production-packed;
- Direct2D implementation is not locally compiled in this Linux environment;
- no `.avm` serializer or stable ABI.

## Decision

Part 6 is accepted as the new extraction baseline. The next replacement target
is immutable property/track data, not the CPU rasterizer or Telegram scheduler.
Telegram remains the production semantics oracle until extracted property
sampling has exact golden parity.
