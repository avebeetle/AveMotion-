# AveMotion Part 13 report — generated Polystar and Polygon geometry

## Goal

Generate Lottie Polystar and Polygon paths from AveMotion's canonical property
tables, publish them through the existing source-geometry and render-plan seam,
and accept them only after exact Telegram local-path parity.

Telegram `rlottie` remains the primary semantic oracle. Samsung `rlottie`
remains a comparison and hardening donor and does not silently replace Telegram
behaviour.

## Result

Part 13 adds:

- backend-neutral Polystar and Polygon generation;
- integer and fractional star point counts;
- Telegram-compatible polygon point flooring;
- sharp and rounded stars and polygons;
- inner/outer radius and roundness;
- clockwise and counter-clockwise traversal;
- static and animated position, point count, radius, roundness and rotation;
- asset-scoped static identity and instance-scoped animated identity;
- deterministic content-derived geometry revisions;
- bounded fail-closed generation for malformed or excessive complexity;
- direct generator unit tests independent of `rlottie`;
- an eight-primitive fixture checked on every source frame;
- a 50-row source-geometry golden;
- Polystar-specific diagnostics;
- standalone/offline packaging of the generator without an `rlottie` runtime.

The resulting path enters the same backend-neutral pipeline as existing authored
Shape, Rectangle, Rounded Rectangle and Ellipse geometry:

```text
canonical properties
-> AveMotion PropertyEvaluator
-> PrimitivePathGenerator
-> exact Telegram local-path parity gate
-> geometry identity / geometry revision
-> MotionGeometryUpdate
-> MotionRenderPlan
-> Headless backend / Windows Direct2D seam
```

## Exact pinned Telegram semantics

The implementation intentionally follows the pinned Telegram fork, including
observable historical behaviour that a new implementation might otherwise
"clean up":

- `K_PI = 3.141592f`;
- star roundness factor `0.47829f / 0.28f`;
- polygon roundness factor `0.25f`;
- fractional-star partial-point radius and first/last control scaling;
- `floor(points)` for Polygon;
- Telegram's historical second Polygon angle conversion;
- Telegram's `LOTPolystarItem` translation followed by the authored rotation
  applied twice;
- Telegram fuzzy-close behaviour that may append a final line before `Close`;
- Telegram float operation order and special handling of cardinal angles.

The existing verb-for-verb and point-for-point parity gate remains authoritative.
No comparison tolerance was widened to make the new implementation pass.

## Complexity and security boundary

Generation accepts at most 256 authored points. A rounded star can therefore
emit up to 512 cubic segments. Fixed retained storage is sized for that validated
profile, and any of the following conditions fails closed to the established
Telegram path:

- non-finite position, point count, radii, roundness or rotation;
- point count below one or above the configured bound;
- unknown Polystar subtype;
- property evaluation failure;
- fixed path-storage overflow;
- mismatch against Telegram's extracted local path.

The 256-point limit is a provisional security/performance profile, not a claim
about the complete Lottie format.

## Dedicated fixture and tests

`tests/fixtures/polystar_polygon_geometry.json` contains eight independent
cases:

1. sharp clockwise star;
2. rounded counter-clockwise star;
3. fractional clockwise star;
4. sharp clockwise polygon;
5. rounded counter-clockwise polygon;
6. animated star with animated topology, radii, roundness, position and rotation;
7. animated counter-clockwise polygon;
8. rounded counter-clockwise triangle polygon.

The fixture is checked in five persisted golden states and exhaustively on all
61 source frames. Every frame must produce:

```text
8 projected primitives
5 stars
3 polygons
0 property failures
0 evaluation failures
0 generation rejections
0 Telegram parity mismatches
```

Additional regression tests prove:

- direct seek and sequential traversal produce identical geometry cache keys;
- resetting track-cursor history does not change the result;
- repeated evaluation at the same time does not create a geometry update;
- retained evaluation storage does not reallocate after `prepare()`;
- the standalone generator works when `AVEMOTION_RLOTTIE_VARIANT=none`.

## Characterization

```text
assets / fixtures:                  10
sampled states:                     50
draw items visited:                414
source-bound candidates:           384
accepted projections:              119
asset-static projections:           81
instance-animated projections:      38
projected path points:            2,060

primitive candidates:              100
rectangle candidates:               35
ellipse candidates:                 25
polystar candidates:                40
star candidates / projected:        25 / 25
polygon candidates / projected:     15 / 15
sharp rectangles:                   17
rounded rectangles:                 18
ellipses:                            25

primitive property failures:         0
primitive evaluation failures:       0
polystar property failures:           0
polystar evaluation failures:         0
polystar input rejections:            0
modifier rejections:                  9
parity rejections:                   22
geometry update packets:            321
asset geometry references:           91
instance geometry references:       323
```

The 22 parity rejections are pre-existing unsupported/nested Shape cases. Every
candidate in the dedicated Rectangle/Ellipse and Polystar/Polygon fixtures
passes exact Telegram parity.

The persisted source-geometry golden is:

```text
tests/golden/source-geometry-telegram.tsv
```

SHA-256:

```text
a5b201633505dde12562d62a15ea264886258fa2d1e6ffc5e7ba64bb1741ae8d
```

Clang 17 Debug and GCC 14 Release generate byte-identical manifests.

## Regression boundaries

Unchanged:

- Telegram and Samsung CPU pixel goldens;
- evaluated-scene goldens;
- legacy render-plan goldens;
- canonical and parsed-model goldens;
- property/Shape/spatial/world-transform golden;
- vendor fingerprints and source corpus hashes.

Intentionally updated:

- `tests/golden/source-geometry-telegram.tsv`, because proven Polystar and
  Polygon geometry now replaces Telegram fallback in the source-projection seam;
- `tests/golden/README.md`, documenting the expanded manifest.

## Validation matrix

| Configuration | Result |
|---|---:|
| Telegram + Clang 17 Debug | 28/28 passed |
| Telegram + GCC 14 Release | 28/28 passed |
| Telegram + Clang 17 AddressSanitizer | 28/28 passed |
| Samsung + Clang 17 Debug | 17/17 passed |
| Samsung + GCC 14 Release | 17/17 passed |
| Samsung + Clang 17 AddressSanitizer | 17/17 passed |
| Standalone/offline Clang | 8/8 passed |
| Standalone/offline Clang ASan + UBSan | 8/8 passed |
| CMake install/export | passed |
| External `find_package(AveMotion 0.13)` consumer | configure/build/run passed |
| Telegram Clang/GCC source-geometry manifest parity | byte-identical |

The Samsung Release build continues to emit known warnings from Samsung's old
copy-on-write raster implementation. These are in the comparison branch, not in
new AveMotion code. The build and all tests pass.

In this shared Linux filesystem, two GCC parallel builds briefly attempted a
link before one newly produced object became visible. Immediate serialized
completion succeeded and the full GCC test suites passed. This was a transient
build-directory/filesystem event, not a compiler or source failure.

## Windows-first gate

The generated path is backend-neutral and enters the same `MotionRenderPlan`
consumed by the Windows-only Direct2D backend. Existing presets and CI entries
cover:

```text
windows-msvc-telegram-debug
windows-msvc-telegram-release
windows-msvc-samsung-debug
```

The current execution environment is Linux and has no Windows SDK. Core,
evaluation, geometry, planning, headless and parity paths are compiled and
executed locally. Native MSVC compilation and actual `ID2D1DeviceContext` /
`ID2D1PathGeometry` execution remain explicit Windows CI or host-machine gates.
No claim of local Direct2D execution is made in this report.

## Deliverable quality gates

Before delivery, the project is additionally verified through:

- a clean Git working tree;
- a tagged Git commit;
- fresh clone from the Git bundle;
- fresh extraction from the ZIP;
- vendor fingerprint verification;
- fresh Telegram build/test from the bundle;
- fresh Samsung and offline build/test from the ZIP;
- CMake install/export from the ZIP;
- external consumer compile/link/run through `find_package`;
- SHA-256 verification of all delivery artifacts.

## Next isolated goal

Part 14 is deliberately limited to one-path Trim Path geometry:

```text
canonical local path
+ trim start / end / offset
+ trim mode
-> path-length measurement
-> interval normalization and wrap
-> cubic subdivision
-> trimmed local path
-> Telegram exact parity
-> deterministic geometry revision
-> MotionRenderPlan
```

Repeaters, merge/boolean paths, masks, mattes, scheduler work and `.avm`
serialization remain outside that single stage.
