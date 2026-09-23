# AveMotion Part 14 report — one-path Trim Path geometry

## Goal

Evaluate and apply Lottie Trim Path to one canonical local path, reproduce the
pinned Telegram path-length/wrap/splitting semantics, publish the result through
the existing source-geometry and render-plan seam and preserve all established
pixel, scene, model, property and plan oracles.

Telegram `rlottie` remains the primary semantic oracle. Samsung `rlottie`
remains comparison/hardening only.

## Result

Part 14 adds:

- backend-neutral `TrimPathGenerator`;
- Telegram-compatible trim interval normalization;
- empty, full, partial and wrapped intervals;
- one-path Simultaneous mode;
- one-path non-wrapped Individual mode;
- historical line and cubic length measurement;
- line/cubic subdivision at measured distance;
- dash-style extraction into a new local path stream;
- exact Telegram verb/point parity before publication;
- deterministic content-derived geometry revisions;
- asset-scoped static and instance-scoped animated cache identities;
- direct-seek/sequential cache-key equivalence tests;
- repeated-time no-rebuild tests;
- standalone unit tests without rlottie;
- an eight-case fixture checked on all 61 source frames;
- Trim-specific structural diagnostics and golden fields.

Pipeline:

```text
canonical local path
+ canonical trim properties
-> AveMotion normalize/measure/split/extract
-> exact Telegram local-path parity gate
-> geometry identity / revision
-> MotionGeometryUpdate
-> MotionRenderPlan
-> headless backend / Windows Direct2D seam
```

## Compatibility details

The implementation intentionally follows the pinned Telegram/Samsung algorithm
family rather than substituting a new exact path-length library:

- trim segment normalization follows `LOTTrimData::segment()`;
- line length uses the historical max-plus-0.375-min approximation;
- cubic length recursively uses control-polygon/chord convergence at `0.01`;
- cubic split-at-length performs repeated parameter split and length checks;
- extraction follows `VPathMesure` / `VDasher` behaviour;
- partial trims omit Close;
- full trims preserve the original verb/point stream.

Defensive depth and iteration limits were added. Non-finite values,
non-convergence, corrupt path streams or unsupported modifier relationships fail
closed to Telegram geometry.

## Dedicated fixture

`tests/fixtures/trim_path_geometry.json` contains eight independent cases:

1. partial rectangle trim;
2. empty rectangle trim;
3. full rounded-rectangle trim;
4. positively wrapped/offset rectangle trim;
5. negatively wrapped ellipse trim;
6. partial ellipse trim;
7. animated ellipse trim;
8. one-path Individual star trim.

The exact authored labels are less important than the behavioural coverage. All
61 source frames are exercised exhaustively.

Fixture aggregate across the five persisted golden samples:

```text
visited draw items:          40
trim candidates:             40
Simultaneous:                35
Individual:                   5
accepted projections:        40
empty:                        5
full:                         5
partial:                     30
cubic/line split operations: 49
asset-static:                35
instance-animated:            5
binding/property errors:      0
input rejections:             0
Telegram parity mismatches:   0
```

Every individual frame also requires eight accepted projections with zero
errors or parity mismatches.

## Full characterization

```text
assets / fixtures:                 11
sampled states:                    55
draw items visited:               454
source-bound candidates:          424
accepted projections:             159
asset-static projections:         116
instance-animated projections:     43
projected path points:          2,338

primitive candidates:             140
rectangle candidates:              55
ellipse candidates:                40
polystar candidates:               45
stars / polygons:                  30 / 15
sharp / rounded rectangles:        27 / 28
ellipses:                           40

trim candidates:                   45
Simultaneous / Individual:         40 / 5
accepted trims:                    40
empty / full / partial:             5 / 5 / 30
trim split operations:             49
trim binding/property/eval skips:   0 / 0 / 0
trim input rejections:              0

nested composition candidates:    245
modifier fallback cases:            4
transform-unavailable fallback:   204
pre-existing parity rejections:    22
geometry update packets:          333
asset / instance geometry refs:   126 / 328
```

The 22 parity rejections are established unsupported/nested Shape cases. The
Trim Path fixture has zero parity rejections.

Persisted golden:

```text
tests/golden/source-geometry-telegram.tsv
SHA-256 f79f5087f1e61fdbf9d153a073a396975ab25e3195be9f164f70db34ee01fb8c
```

Clang 17 Debug and GCC 14 Release generate byte-identical manifests.

## Regression boundaries

Unchanged:

- Telegram and Samsung CPU pixel goldens;
- evaluated-scene goldens;
- legacy MotionRenderPlan goldens;
- canonical and parsed-model goldens;
- property/Shape/spatial/world-transform golden;
- vendor fingerprints and source corpus hashes.

Intentionally updated:

- `tests/golden/source-geometry-telegram.tsv`, because the proven one-path trim
  subset now replaces Telegram fallback in the source-projection seam;
- `tests/golden/README.md` and Part 14 documentation.

## Validation matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang 17 Debug | 29/29 passed |
| Telegram + GCC 14 Release | 29/29 passed |
| Telegram + Clang 17 AddressSanitizer | 29/29 passed |
| Samsung + Clang 17 Debug | 18/18 passed |
| Samsung + GCC 14 Release | 18/18 passed |
| Samsung + Clang 17 AddressSanitizer | 18/18 passed |
| Standalone/offline Clang | 9/9 passed |
| Standalone/offline Clang ASan + UBSan | 9/9 passed |
| CMake install/export | passed |
| External `find_package(AveMotion 0.14)` consumer | configure/build/run passed |
| Telegram Clang/GCC source-geometry manifest parity | byte-identical |


## Allocation boundary

The existing property workspace remains retained and stable after `prepare()`.
The new trim path currently uses temporary `std::vector` storage in the
projection/generation layer. Therefore Part 14 does not claim zero allocation
for Trim Path. This is an explicit implementation debt for Part 15.

## Windows-first gate

The trimmed path enters the existing backend-neutral `MotionRenderPlan` consumed
by the Windows-only Direct2D backend. Windows presets and CI entries remain:

```text
windows-msvc-telegram-debug
windows-msvc-telegram-release
windows-msvc-samsung-debug
```

The current environment is Linux without the Windows SDK. Core, evaluation,
Trim Path, source projection, render planning and headless tests are executed
locally. Native MSVC and actual `ID2D1DeviceContext`/`ID2D1PathGeometry`
execution remain explicit Windows gates.

## Next isolated goal

Part 15 should implement true multi-path Trim Path semantics and retained scratch
storage:

```text
several canonical local paths
+ one Simultaneous or Individual trim
-> ordered aggregate path length
-> Telegram-compatible interval distribution
-> retained scratch buffers
-> exact local-path parity
-> deterministic per-path revisions
```

Repeaters, merge/boolean paths, masks, mattes, scheduler and `.avm` remain out of
scope for that stage.
