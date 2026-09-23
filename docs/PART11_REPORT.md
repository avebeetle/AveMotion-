# AveMotion Part 11 report — source Shape geometry projection

## Goal

Project AveMotion-evaluated, modifier-free source `ShapePath` properties into
backend-neutral local path geometry and feed accepted geometry into the existing
`MotionRenderPlan` update/cache seam without changing any established Telegram
pixel, scene, model, property or legacy plan oracle.

## Result

Part 11 adds:

- parsed `SourceNodeId` binding from Telegram model objects into runtime path
  and paint items;
- source path count and modifier-free metadata on the extraction seam;
- `SourceGeometryProjector` in `AveMotion::Rendering`;
- static and animated Shape resolution from canonical tables and retained
  property-evaluator buffers;
- composition-local path-to-paint transform resolution;
- zero-resize in-place local path publication after parity validation;
- authoritative source geometry revisions;
- asset-scoped static and instance-scoped animated cache identities;
- direct-seek, repeated-time, cross-instance and presentation-transform tests;
- a new 40-row source-geometry golden manifest.

## Corpus

```text
assets:                         8
sampled states:                40
draw items visited:           339
source-bound candidates:      309
projected states/items:        19
asset-static projections:      11
animated projections:           8
projected points:              385
nested candidates attempted:  245
modifier rejections:            9
multiple-path rejections:       0
transform-unavailable skips:  204
parity rejections:              22
```

Accepted paths match Telegram's local path before publication. Rejected paths
remain unmodified and continue through the legacy oracle path.

Source-geometry golden SHA-256:

```text
63525991cb5880cfceec5ef7389e73782ca9cb819d12858317339f9c1aee9f9a
```


## Deterministic native-resource identity

Animated projected geometry uses a content-derived authoritative revision. It
does not reuse the monotonic `Shape` or world-transform revision counters from a
particular evaluation history. Consequently, direct seek, forward traversal and
reverse traversal produce the same `GeometryCacheKey` for identical local path
content. A dedicated regression test compares those complete cache keys.

## Validation matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 27/27 |
| Telegram + GCC Release | 27/27 |
| Telegram + Clang AddressSanitizer | 27/27 |
| Samsung + Clang Debug | 16/16 |
| Samsung + GCC Release | 16/16 |
| Samsung + Clang AddressSanitizer | 16/16 |
| Standalone/offline Clang | 7/7 |
| Standalone/offline ASan + UBSan | 7/7 |
| CMake install/export | passed |
| External `find_package(AveMotion 0.11)` consumer | passed |

## Regression boundaries

Unchanged:

- Telegram and Samsung CPU pixel goldens;
- evaluated-scene goldens;
- previous render-plan goldens;
- canonical model and parsed-model goldens;
- property/Shape/world-transform golden;
- corpus file hashes.

Added:

- `tests/golden/source-geometry-telegram.tsv`;
- source binding and projection tests;
- source geometry characterizer.

## Upstream patch

`patches/telegram/0005-avemotion-source-geometry-binding.patch` is additive
metadata only. It carries canonical source path/paint IDs, path count and
modifier-free state into `LOTNode`. Telegram evaluation, final path generation,
CPU rasterization and compositing remain unchanged.

## Remaining boundary

The source projector currently consumes Telegram's exact local path as a
characterization gate and keeps Telegram's complete geometry pipeline for every
unsupported or mismatching candidate. Part 12 should replace the next isolated
geometry operation rather than broadening the projection unsafely.
