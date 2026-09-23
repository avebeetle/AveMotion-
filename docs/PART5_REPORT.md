# AveMotion Part 5 report: canonical local-space geometry and static provenance

## 1. Executive result

Part 5 introduces the first proven immutable asset-resource seam while keeping
Telegram `rlottie` as the primary semantic baseline.

```text
Telegram parser/model/evaluator
    -> exact runtime tree
    -> local path + separate transform + static metadata
    -> AveMotion canonical geometry/paint registry
    -> MotionRenderPlan
    -> headless or Direct2D backend
```

The existing CPU-pixel and evaluated-scene oracles are unchanged. Render-plan
goldens are intentionally revised because cache identities now include stable
generation handles and Telegram-proven asset-static resources.

Project version: `0.5.0`.

## 2. Delivered capabilities

- stable index+generation handles for assets and instances;
- additive Telegram runtime metadata for local geometry, affine transform,
  staticness, solid paint and separated opacity;
- asset-owned immutable `CanonicalGeometry` and `CanonicalPaint` registries;
- a conservative complete-seam rule: local geometry is rendered only with a
  matching local solid-fill paint; stroke/gradient items retain Telegram's
  final-path semantics until their local paint contracts are extracted;
- exact conflict detection before cross-instance promotion;
- a complete-seam regression test proving that local paths are selected only
  when matching local-space paint is available, preventing double transforms
  for Telegram strokes and gradients;
- `AssetStatic` and `InstanceEvaluated` cache identities;
- geometry/paint revisions unaffected by layout-only changes;
- Direct2D selection of canonical/local/final geometry and paint;
- transform separation in draw execution;
- canonical diagnostics and a dedicated full-corpus test;
- installed-package consumer test in CI.

## 3. Why Telegram remains primary

Telegram behavior remains the production semantic target because this project
started from Telegram animated-sticker requirements and the Telegram fork is the
characterized source of truth. Samsung remains useful for newer fixes and
hardening, but its frame counts, evaluated geometry, paint and raster output
differ on the corpus. Part 5 therefore applies the provenance seam only to the
Telegram baseline and does not pretend that Samsung metadata proves Telegram
semantics.

## 4. Upstream modification

The new patch is recorded as:

```text
patches/telegram/0002-avemotion-local-geometry-metadata.patch
```

It is additive. Legacy render-tree fields and CPU rasterization remain intact.
The vendored Telegram source fingerprint is recorded in
`third_party/rlottie/UPSTREAM.json` and verified by `scripts/verify_vendor.py`.

## 5. Stable ownership identities

`AssetHandle` and `InstanceHandle` pack `index + generation` into a 64-bit
identity. Registry slots are reused only after generation advancement. The
planner and cache keys use these identities, so stale instance state cannot be
mistaken for a newly created object at the same index.

## 6. Canonical promotion

`AssetData` lazily interns canonical resources under a mutex. Promotion requires
explicit Telegram metadata and exact content equality. On success, the
per-evaluation duplicate local path is discarded and the immutable asset-owned
object becomes authoritative.

On any disagreement:

```text
canonicalResourceConflicts++
item remains instance-scoped
```

No conflict occurred in the eight-asset/five-sample corpus.

## 7. Planner and backend effects

The render planner now separates:

```text
geometryTransform      local/canonical path -> viewport/composition
presentationTransform  host layout/placement
```

Static cache keys contain asset identity and no instance identity. Dynamic keys
contain instance generation and revision. The Direct2D skeleton composes the
transforms and can share `ID2D1PathGeometry` for static resources.

## 8. Golden boundaries

Unchanged:

- CPU pixel manifests and frame hashes;
- evaluated-scene manifests and fingerprints;
- source corpus hashes.

Intentionally changed:

- render-plan manifests, because Part 5 adds generation identities, transform
  separation and real static provenance.

New committed manifest SHA-256 values:

```text
Telegram 8784dcdebb5d43b7d4ad1644b313cea10a8a5dba45de707a19857438614ffaf1
Samsung  bffd0e5630ab5fe59ea6e851cb2a8f3a0b19a9853b17130970751b799af50a1a
```

## 9. Corpus statistics

| Metric | Telegram Part 4 | Telegram Part 5 | Samsung Part 5 |
|---|---:|---:|---:|
| Geometry update packets | 295 | 290 | 295 |
| Paint update packets | 152 | 152 | 155 |
| Asset-scoped geometry refs | 0 | 36 | 0 |
| Instance-scoped geometry refs | 339 | 303 | 339 |
| Asset-scoped paint refs | 0 | 290 | 0 |
| Instance-scoped paint refs | 339 | 49 | 339 |
| Unsupported items | 20 | 20 | 20 |

The dedicated canonical test reports, at its selected middle samples:

```text
Telegram: local=122, static geometry=8, static paint=115
Samsung:  local=0,   static geometry=0,  static paint=0
```

## 10. Build and test matrix

Validated in the current Linux environment:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 13/13 |
| Telegram + GCC Release | 13/13 |
| Telegram + Clang AddressSanitizer | 13/13 |
| Samsung + Clang Debug | 13/13 |
| Samsung + GCC Release | 13/13 |
| Samsung + Clang AddressSanitizer | 13/13 |
| Offline/installable package | 6/6 |
| Clean external `find_package(AveMotion 0.5)` consumer | passed |

Exact logs are stored under `docs/build-logs/part5/`.

## 11. Windows/Direct2D status

The Direct2D implementation was updated for canonical static resources and
separate transforms, but the current environment has no Windows SDK. Therefore:

- backend-neutral code was compiled and tested;
- the public Direct2D header portability test passed;
- Windows/MSVC targets and CI are configured;
- actual MSVC compilation and runtime drawing remain an explicit external gate.

No claim of executed Direct2D rendering is made in this report.

## 12. Known limitations

- source resource IDs are derived from asset hash, layer index and local ordinal;
- canonical tables are populated lazily instead of being compiled at asset load;
- only solid paint provenance is exported;
- runtime keypath/property overrides are not part of the AveMotion API yet;
- exact evaluation still constructs a fresh Telegram runtime item tree;
- Samsung remains comparison-only;
- public API/ABI is laboratory-stage;
- derivative-work licensing must be reviewed before public distribution.

## 13. Next stage

Part 6 should move ownership one level earlier:

```text
Telegram parsed model
    -> AveMotion immutable canonical asset tables
         NodeId / GeometryId / PaintId / static metadata
    -> independent MotionInstance playback state
    -> existing evaluated-scene and render-plan oracles
```

This removes the transitional layer/ordinal source identity and prepares a
standalone player API without replacing Telegram evaluation prematurely.
