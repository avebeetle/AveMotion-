# AveMotion Part 4 report: backend-neutral render plan and Direct2D seam

## 1. Executive result

Part 4 converts the deep-copied `EvaluatedScene` from Part 3 into an immutable,
backend-neutral `MotionRenderPlan` and proves that a non-pixel backend can
consume the plan independently of rlottie's CPU rasterizer.

The new path is:

```text
Telegram/Samsung exact evaluated tree
    -> AveMotion EvaluatedScene oracle (unchanged)
    -> MotionRenderPlanner
    -> MotionRenderPlan
    -> HeadlessPlanBackend
    -> future/native backend
```

The original CPU pixel oracle and evaluated-scene oracle remain unchanged and
continue to gate every configuration.

Telegram remains the primary behavioral baseline. Samsung remains a comparison
and hardening source only.

## 2. New targets

```text
AveMotion::Rendering
AveMotion::Direct2D       (Windows only)
avemotion_plan_characterize
avemotion_render_plan_tests
avemotion_plan_golden_tests
```

Project version is now `0.4.0`.

## 3. New public API

```text
include/avemotion/render/RenderPlan.hpp
include/avemotion/render/RenderPlanner.hpp
include/avemotion/render/HeadlessBackend.hpp
include/avemotion/backends/direct2d/Direct2DBackend.hpp
```

Primary types:

```cpp
MotionRenderPlanner
MotionRenderPlan
MotionDrawItem
GeometryCacheKey
PaintCacheKey
HeadlessPlanBackend
PresentationState
```

No rlottie type, Qt type, COM object, Win32 window, or Direct2D object appears in
backend-neutral render headers.

## 4. Plan structure

A plan contains:

- request/asset/instance/evaluation/plan stamps;
- a shared immutable backing evaluated scene;
- compact draw items;
- geometry and paint identities;
- geometry and paint update packets;
- presentation transform and effective opacity;
- conservative presented bounds;
- conservative dirty-region hint;
- backend-neutral feature flags;
- deterministic plan fingerprints and statistics.

The plan is reusable for repaint. Repainting the same plan does not require
reevaluating rlottie or rebuilding geometry.

## 5. Explicit provenance

Part 4 adds `EvaluatedValueOrigin` to evaluated draw items:

```text
Unknown
AssetStatic
InstanceEvaluated
```

The current exact-sample rlottie bridge conservatively uses
`InstanceEvaluated`. This is necessary because a fresh Telegram runtime tree
sets `ChangeFlagPath` on every extracted draw item; upstream dirty bits cannot
prove asset-level staticness.

Future canonical assets may provide explicit static geometry and paint IDs.
Synthetic tests already prove cross-instance sharing and transform-only reuse
when this provenance is available.

## 6. Revisions and update packets

Instance-scoped source items retain independent revisions:

```text
geometry content changed -> geometryRevision increments
paint content changed    -> paintRevision increments
presentation changed     -> neither revision increments
```

The planner emits update packets only when content changes. Repeated same-scene
planning produces no updates and no dirty region.

## 7. Headless plan backend

`HeadlessPlanBackend` records, without pixels:

```text
plan fingerprint
topology fingerprint
geometry-identity fingerprint
paint-identity fingerprint
presentation fingerprint
statistics
bounds and dirty validity
```

Fingerprints are generated field-by-field with the existing canonical FNV-1a
encoding; raw structure memory and padding are never hashed.

## 8. Plan golden corpus

Two committed manifests cover:

```text
2 rlottie lineages
x 8 assets
x 5 normalized samples
= 80 render-plan goldens
```

Each row includes plan fingerprints, selected frame, visible draw-item count,
update counts, static/evaluated identity counts, unsupported-feature count, and
dirty-region validity.

Clang and GCC reproduce the manifests byte-for-byte for both lineages:

```text
Telegram: 40 / 40
Samsung:  40 / 40
```

## 9. Telegram versus Samsung at the plan boundary

Across 40 samples:

| Field | Identical | Different |
|---|---:|---:|
| Complete plan | 19 | 21 |
| Topology | 40 | 0 |
| Geometry identity | 23 | 17 |
| Paint identity | 33 | 7 |
| Presentation | 27 | 13 |
| Selected frame | 21 | 19 |
| Visible item count | 40 | 0 |
| Geometry update count | 40 | 0 |
| Paint update count | 37 | 3 |

The planner itself is deterministic. Differences are inherited from the
selected frame and evaluated scene. This reinforces the decision to preserve
Telegram semantics as primary rather than replacing them wholesale with
Samsung behavior.

## 10. Direct2D backend skeleton

A Windows-only backend target accepts a host-provided `ID2D1DeviceContext` and
does not own the final target or presentation loop.

Implemented code paths:

- build `ID2D1PathGeometry` from AveMotion path verbs;
- solid fills;
- simple solid strokes;
- fill rules;
- presentation transform and opacity;
- bounded native geometry cache;
- stroke-style cache;
- graphics-domain invalidation.

Unsupported features are skipped and counted rather than rendered with wrong
semantics.

The Linux environment cannot compile or execute Windows SDK code. Windows/MSVC
CI builds the backend target and runs a no-device smoke executable. Runtime D2D
render validation remains a future Windows gate.

## 11. Tests added

### Synthetic render-plan tests

- explicit asset-static geometry and paint identities;
- cross-instance static-resource sharing;
- transform-only reuse without geometry rebuild;
- geometry-only revision changes;
- paint-only revision changes;
- repeated identical plan produces no dirty region;
- stale evaluation sequence rejection;
- instance-state reset through `forgetInstance()`;
- deterministic headless fingerprints.

### Header portability test

The Direct2D public backend header compiles without Windows SDK headers on a
non-Windows host and does not leak Direct2D into core/render-plan headers.

### Windows smoke test

When the Direct2D target exists, CI constructs the backend and validates its
initial diagnostics without requiring a graphics device.

## 12. Build matrix

Validated locally on Linux x86-64:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 12/12 |
| Telegram + GCC Release | 12/12 |
| Telegram + Clang ASan | 12/12 |
| Samsung + Clang Debug | 12/12 |
| Samsung + GCC Release | 12/12 |
| Samsung + Clang ASan | 12/12 |
| Offline/installable package | 6/6 |
| External `find_package(AveMotion 0.4)` consumer | passed |

The final exact counts are regenerated into delivery logs before packaging.

## 13. Transitional limitations

- extracted paths remain viewport-space rather than canonical local-space;
- current rlottie scenes cannot prove asset-static provenance;
- draw-item identity is temporarily derived from asset/layer/local ordinal;
- masks, mattes, gradients, images, dash, clips, and effects are plan metadata
  but not D2D-rendered;
- bounds are conservative control-point bounds plus stroke expansion;
- plan building still allocates vectors;
- Direct2D runtime execution is not validated in the Linux environment;
- stable index+generation instance handles are not yet implemented.

## 14. Gate preserved

Part 4 was accepted only after:

- pixel oracle tests remained unchanged;
- Telegram and Samsung scene goldens remained unchanged;
- plan goldens were deterministic across Clang and GCC;
- transform-only synthetic samples reused asset-static geometry identity;
- no upstream type appeared in public plan/backend-neutral headers.

## 15. Extraction rule

```text
characterize -> isolate -> replace -> compare -> remove old path
```

The CPU renderer and exact evaluated-scene bridge remain active regression
oracles. No old path has been removed prematurely.
