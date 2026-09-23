# AveMotion Part 7 — direct parsed-model tables

## Executive result

Part 7 removes evaluated-frame sampling from the authored canonical source
model. Telegram remains the accepted parser/evaluator/CPU oracle, but AveMotion
now owns immutable reusable compositions, source nodes, typed properties,
tracks, segments, authored temporal easing, spatial tangents and typed value
storage built directly from Telegram's parsed `LOTModel`.

```text
Lottie JSON
    -> Telegram LOTModel
    -> narrow read-only introspection seam
    -> MotionAssetModel schema 2
         Composition[]
         SourceNode[]
         Property[]
         Track[]
         Segment[]
         typed value storage
    -> MotionInstance
    -> Telegram evaluator adapter (oracle)
    -> EvaluatedScene
    -> MotionRenderPlan
```

## Direct source-model result

Across eight committed assets:

```text
compositions:           15
source nodes:          135
properties:            309
static properties:     266
animated properties:    43
tracks:                  43
segments:                71
scalar values:          185
Vec2 values:            112
color values:            27
matrix values:           48
shape values:            28
gradient values:          8
source child refs:      120
source property refs:   309
```

Seven precomposition assets are represented once and referenced from 24
precomposition layers. Earlier exploratory flattening duplicated those source
graphs; the final Part 7 model does not.

## Extracted authored semantics

The direct bridge records:

- root and reusable precomposition tables;
- structural and transform parenting;
- layer type, visibility, timeline and time stretch;
- fills, strokes, gradients, masks, mattes and supported blend metadata;
- rectangle, ellipse, shape, polystar, trim and repeater metadata;
- static transform matrices and animated transform components;
- typed static values;
- typed animated tracks and contiguous keyframe segments;
- hold, linear and temporal cubic-Bezier data;
- spatial path interpolation/tangents independently from temporal easing;
- repeated semantic component indices for dash arrays.

## Narrow Telegram patch

`patches/telegram/0004-avemotion-parsed-model-introspection.patch` exposes
read-only data already held by Telegram `rlottie`:

- static transform matrix and opacity;
- animated transform component storage;
- authored temporal Bezier controls;
- whether a time-remap property was actually parsed.

The last field avoids false `LayerTimeRemap` properties when `ExtraLayerData`
exists only for masks or precomposition metadata.

The full 0001–0004 patch series was reapplied to the original uploaded Telegram
archive and reproduced the vendored source tree exactly.

## Determinism and structural validation

Dedicated tests validate:

- dense typed IDs and valid ranges;
- reusable composition references and acyclic composition graph;
- structural and transform-parent consistency;
- property/track/segment back-references;
- static/animated partition;
- type-correct value references;
- sorted non-overlapping segment ranges;
- separate temporal and spatial interpolation metadata;
- repeated dash component indices;
- finite authored values, easing controls and tangents;
- duplicate-load determinism;
- host debug-name independence;
- evaluation-history independence.

Compiler parity:

```text
summary SHA-256:
3ff5d980520c93c520e88eaf42f5313b08edb9626f2c61c58907e3842d7943c4

detailed parsed-model SHA-256:
de29245024b8c57a3a70aad6ae3fde6ded35b0eba22a1b19c3f32b750c164e00

Clang 17 == GCC 14: PASS
```

The detailed structural oracle contains 990 lines including its header.

## Preserved prior oracles

The following remain byte-identical to the Part 6 commit:

```text
scene-telegram.tsv
scene-samsung.tsv
plan-telegram.tsv
plan-samsung.tsv
```

The existing CPU-pixel corpus is also still exercised by all reference builds.
Changed intentionally:

- `MotionAssetModel` schema 1 -> 2;
- Telegram model summary golden;
- new detailed parsed-model golden.

## Build and test matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 20/20 |
| Telegram + GCC Release | 20/20 |
| Telegram + Clang AddressSanitizer | 20/20 |
| Samsung + Clang Debug | 15/15 |
| Samsung + GCC Release | 15/15 |
| Samsung + Clang AddressSanitizer | 15/15 |
| Offline/installable SDK | 6/6 |
| CMake install/export | PASS |
| External `find_package(AveMotion 0.7)` consumer | PASS |
| Telegram patch reproduction | PASS |
| Vendor fingerprint verification | PASS |

Environment:

```text
CMake 3.31.6
Ninja 1.12.1
Clang 17
GCC 14.2
Linux x86-64
```

## Honest boundary

Part 7 does not replace Telegram property evaluation. The older render-facing
geometry/paint compatibility tables still use exact Telegram evaluation across
the asset timeline. Telegram still owns modifiers, evaluated geometry,
masks/mattes, render-tree construction and CPU rasterization.

The direct authored tables are ready for an AveMotion evaluator, but are not yet:

- a final `.avm` binary layout;
- a stable public ABI;
- a complete untrusted-input sandbox;
- an independent Lottie implementation.

Direct2D implementation was not compiled or executed locally because this stage
ran in Linux without the Windows SDK. Windows/MSVC remains a required gate.

## Next single goal

Part 8 should implement an AveMotion-owned evaluator for a deliberately small
property subset—static/hold/linear/temporal-cubic scalar, `Vec2`, color and 2D
transform values—while comparing exact results against Telegram at boundaries,
random seeks and reverse traversal. Telegram remains the oracle until parity is
proved.
