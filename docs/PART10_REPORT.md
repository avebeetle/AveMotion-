# AveMotion Part 10 report — retained animated Shape evaluation

## Goal

Replace the `Unsupported` result for animated Shape/path properties with an
AveMotion-owned, retained, backend-neutral materialization path while preserving
exact Telegram semantics and all previous regression oracles.

## Result

Part 10 adds:

- fixed per-workspace Shape slots and point slices;
- static Shape asset references;
- endpoint, hold, linear and temporal-cubic Shape evaluation;
- pointwise interpolation of canonical `MoveTo + cubic triples` streams;
- Shape content hashes, revisions and dirty flags;
- topology-truncation diagnostics;
- exact Telegram Shape oracle extraction;
- point-level parity tests across forward, reverse and direct seek;
- synthetic tests for endpoints, hold, truncation, revisions and storage reuse.

## Broad Telegram parity

```text
assets/fixtures:            9
unique frame samples:     213
all properties compared: 27,243
animated Shape samples:    165
Shape points compared:   3,324
local transforms:        5,769
world transforms:        5,769
animated gradients deferred: 30
```

All supported values match Telegram.

## Persisted five-sample golden

```text
records:              40
properties:        1,545
compared:          1,540
animated Shapes:      30
Shape points:         525
animated gradients:     5
Shape truncations:       0
world transforms:      675
```

SHA-256:

```text
e2fd5678112a997bc7dd6f23a3ccdf1ee575199478c24dd6a246c8fd0debe5a4
```

## Memory and revisions

Evaluator construction computes one maximum point capacity per animated Shape
property. `prepare()` allocates all descriptors and point storage. The hot
`evaluate()` path does not resize vectors.

`EvaluatedShape::revision` changes only when the materialized Shape changes.
Repeated same-time evaluation reports zero changed Shapes and zero changed
properties.

## Compatibility nuance

The old Telegram `LottieShapeData::lerp` truncates mismatched paths to the
minimum point count and leaves the interpolated `closed` flag false. AveMotion
matches this for parity and sets `topologyTruncated` so future tooling can reject
such assets explicitly.

## Unchanged gates

The CPU pixel, full evaluated-scene, existing render-plan, parsed-model and
canonical-model goldens are unchanged. Part 10 changes only the property golden
and adds Shape-specific tests.

## Remaining boundary

The complete Telegram runtime still constructs final draw geometry after trim,
repeaters and other modifiers. Part 11 will begin projecting modifier-free
AveMotion-evaluated Shape paths into `MotionGeometryUpdate` and
`MotionRenderPlan`.


## Verification matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang 17 Debug | 24/24 |
| Telegram + GCC 14 Release | 24/24 |
| Telegram + Clang 17 AddressSanitizer | 24/24 |
| Samsung + Clang 17 Debug | 16/16 |
| Samsung + GCC 14 Release | 16/16 |
| Samsung + Clang 17 AddressSanitizer | 16/16 |
| Standalone/offline Clang 17 | 7/7 |
| Standalone/offline Clang 17 ASan+UBSan | 7/7 |
| CMake install/export | passed |
| External `find_package(AveMotion 0.10)` consumer | passed |

Clang and GCC produced byte-identical property manifests. The persisted file,
both generated manifests and the committed golden all have SHA-256:

```text
e2fd5678112a997bc7dd6f23a3ccdf1ee575199478c24dd6a246c8fd0debe5a4
```

## Platform limitation

The current environment is Linux without the Windows SDK. The backend-neutral
Shape evaluator, model, runtime, render-plan and public Direct2D header were
built and tested here. The Windows-only Direct2D implementation and real
`ID2D1DeviceContext` drawing remain an external MSVC/Windows gate; Part 10 does
not claim they were executed locally.
