# AveMotion Part 19 report — native Direct2D capture corpus

## Executive result

Part 19 deliberately adds no new Lottie semantic family. Its single purpose is
to validate the already proven AveMotion-owned Shape, primitive, Trim Path and
Repeater subset through the actual Windows Direct2D backend and compare native
D3D11 WARP pixels with the pinned Telegram CPU rasterizer.

Version:

```text
0.19.0
```

The user has already closed the prerequisite Part 18.1 Windows gate on Visual
Studio 2022 / MSVC:

```text
focused Direct2D suite: 3/3 passed
full standalone suite: 13/13 passed
native WARP smoke: passed
```

Part 19 builds on that confirmed production-backend baseline.

## Scope

The native capture corpus covers five representative assets:

```text
animated Shape
Rectangle / Rounded Rectangle / Ellipse
Polygon / Polystar
Trim Path
Repeater content group with solid Fill / Stroke
```

Each asset is sampled at three timeline positions and five presentation
profiles:

```text
5 assets × 3 samples × 5 size/DPI profiles = 75 capture cases
```

Profiles include:

- 64×64 at 96 DPI;
- 128×128 at 96 DPI;
- 192×128 at 96 DPI;
- 128 logical DIPs rendered to 192×192 pixels at 144 DPI;
- 128 logical DIPs rendered to 256×256 pixels at 192 DPI.

## Pipeline under test

```text
Lottie JSON
→ pinned Telegram parser / model
→ AveMotion immutable canonical model
→ AveMotion property evaluator
→ AveMotion source-geometry projection
→ MotionRenderPlan
→ production AveMotion Direct2D backend
→ D3D11 WARP BGRA texture
→ CPU readback
```

The comparison side is independent:

```text
same Lottie JSON + same selected source frame
→ pinned Telegram rlottie CPU rasterizer
→ premultiplied ARGB/BGRA frame
```

The two premultiplied surfaces are then compared with a documented tolerant
policy. Bit identity is not expected because Direct2D and rlottie use different
scan converters and antialiasing implementations.

## New tests

### `avemotion.capture.metrics`

Portable tests for the pixel comparison layer. They prove that the policy:

- accepts identical pixels;
- accepts small antialias-like edge perturbations;
- rejects missing geometry;
- rejects colour-channel swaps;
- detects shifted bounds.

### `avemotion.direct2d.capture_preflight`

Cross-platform Telegram-only preflight. It runs all 75 planned cases through:

```text
canonical properties
→ source geometry projection
→ MotionRenderPlan
```

It verifies:

- every case contains AveMotion-owned projected geometry;
- every visible item belongs to the currently supported Direct2D subset;
- no property or projection workspace grows after `prepare()`;
- every plan retains a valid evaluated scene;
- no visible draw item is silently dropped.

Deterministic result under Clang and GCC:

```text
cases:            75
draw items:      900
projected items: 870
unsupported:       0
```

### `avemotion.direct2d.capture`

Windows-only native capture:

```text
D3D11 WARP
→ ID2D1Device / ID2D1DeviceContext
→ MotionRenderPlan
→ AveMotion::Direct2D
→ BGRA texture readback
→ Telegram CPU comparison
```

For every case it also draws the unchanged plan a second time and requires:

- identical pixels on repaint;
- no new native geometry creation;
- no native geometry replacement;
- geometry-cache hits on the repeated draw.

The artifact directory is deleted at the start of every run, so stale files
from a previous or interrupted corpus cannot satisfy the launcher checks.

## Pixel comparison policy

The comparison records:

- active-pixel intersection over union;
- relative total-alpha error;
- full-surface mean absolute channel difference;
- active-region mean absolute channel difference;
- full-surface RMS difference;
- fraction of active pixels whose maximum channel difference exceeds 64;
- maximum channel difference;
- nontransparent bounds and maximum bounds delta;
- hashes of both full premultiplied surfaces.

Initial acceptance policy:

```text
minimum active IoU:                     0.78
maximum relative alpha error:           0.12
maximum mean absolute difference/all:   12.0
maximum mean absolute difference/active:30.0
maximum >64-difference fraction:         0.18
maximum bounds delta:                    4 pixels
```

The limits are intended to tolerate edge antialiasing differences while still
rejecting missing paths, double DPI scaling, lost opacity, channel swaps and
large transform errors. The native Windows corpus is the authority for whether
these provisional thresholds are appropriately strict.

## Artifact contract

```text
direct2d-capture-artifacts/
├── capture_manifest.tsv
├── README.txt
└── <75 case directories>/
    ├── telegram.bmp
    ├── direct2d.bmp
    └── diff.bmp
```

The BMP files are 24-bit visual evidence composited over a checkerboard. The
comparison itself always operates on the original premultiplied BGRA buffers,
not on the BMP visualization.

The launcher verifies:

```text
75 manifest rows
225 BMP files
0 rows ending in FAIL
focused Direct2D suite passed
full Telegram + Direct2D suite passed
```

## Dedicated Windows preset

```text
windows-msvc-direct2d-capture
```

Run from a Visual Studio 2022 Developer Command Prompt:

```bat
scripts\run_part19_windows.cmd
```

The preset enables:

- pinned Telegram `rlottie`;
- canonical model/evaluator/projector;
- production Direct2D backend;
- portable Direct2D contract;
- native WARP smoke;
- capture metrics and 75-case preflight;
- native 75-case WARP capture corpus.

## CI

A dedicated GitHub Actions job named `windows-direct2d-capture` runs the Part
19 script on `windows-latest` and uploads:

- the complete capture corpus;
- the Part 18 WARP smoke artifact;
- CTest `LastTest.log`.

Artifacts are uploaded even after failure so parity problems remain
inspectable.

## Local validation performed

Current implementation environment:

```text
Linux x86-64
CMake 3.31.6
Ninja 1.12.1
Clang 17
GCC 14.2
Python 3.13
```

Validated from the Part 19 working tree:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 34/34 passed |
| Telegram + GCC Release | 34/34 passed |
| Telegram + Clang ASan | 34/34 passed |
| Samsung + GCC Release | 22/22 passed |
| Standalone/offline Clang | 13/13 passed |
| Capture metrics | passed under Clang/GCC/ASan |
| 75-case capture preflight | passed under Clang/GCC/ASan |
| Vendor fingerprint verification | passed |
| CMake install/export | passed |
| External `find_package(AveMotion 0.19)` consumer | built and ran |

The full existing oracle chain remains green:

- Telegram and Samsung CPU pixel goldens;
- evaluated-scene goldens;
- render-plan goldens;
- immutable canonical-model goldens;
- parsed-model and property/evaluator goldens;
- Shape, primitive, Trim Path and Repeater parity tests;
- playback and generation-handle tests;
- Direct2D production-code contract.

## Honest Windows boundary

The current implementation container has no Windows SDK. Therefore this report
does **not** claim that the new `direct2d_capture_tests.cpp` has already been
compiled by MSVC or that all 75 WARP captures have passed.

What is already proven on Windows by the user is the Part 18.1 production
backend gate. What Part 19 prepares as the next external gate is:

```text
MSVC compilation of the capture executable
75 native WARP renders
225 evidence BMPs
Telegram-vs-Direct2D metrics
unchanged-plan cache reuse across the corpus
```

A failed comparison keeps the manifest and all images, then fails CTest. It
does not hide a mismatch or silently fall back to Telegram pixels.

## Deferred

Part 19 does not add:

- gradients or animated gradient topology;
- dashed strokes;
- clips, masks, mattes or isolation groups;
- effects or offscreen composition;
- new path modifier families;
- scheduler/player extraction;
- `.avm` serialization;
- interactive Win32 preview host.

## Next isolated stage

After the native 75-case capture gate is run and reviewed, Part 20 should add a
minimal real Win32 preview host:

```text
HWND
→ host-owned D3D11 / Direct2D device and presentation target
→ absolute host clock
→ AveMotion Runtime / MotionRenderPlan / Direct2D backend
→ resize / WM_DPICHANGED / device recreation
→ one visible animated fixture
```

This remains a host adapter and validation application, not a dependency of
AveMotion Core.
