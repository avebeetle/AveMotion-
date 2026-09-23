# Direct2D capture corpus

Part 19 compares the proven AveMotion solid-vector subset against the pinned
Telegram `rlottie` CPU rasterizer on a real Windows D3D11 WARP + Direct2D
surface.

## Pipeline

```text
Lottie JSON
  -> Telegram parsed model (oracle/import seam)
  -> AveMotion canonical property evaluation
  -> AveMotion source geometry projection
  -> MotionRenderPlan
  -> AveMotion Direct2D backend
  -> WARP BGRA readback

same asset/frame
  -> Telegram rlottie CPU ARGB frame

WARP BGRA <-> Telegram ARGB
  -> premultiplied-pixel metrics
  -> BMP evidence
  -> capture_manifest.tsv
```

The comparison does not demand bit-identical antialiasing from two different
scan converters. It does require consistent geometry, transform, opacity,
colour, DPI mapping and bounds.

## Corpus

Five assets represent the currently proven Direct2D subset:

```text
dynamic_path_test.json             animated authored Shape
primitive_geometry.json            Rectangle / Rounded Rectangle / Ellipse
polystar_polygon_geometry.json      Star / Polygon
trim_path_geometry.json            one-path Trim Path
repeater_content_group.json         nested Repeater Fill / Stroke group
```

Samples:

```text
p000
p050
p100
```

Profiles:

```text
64x64 pixels at 96 DPI
128x128 pixels at 96 DPI
192x128 pixels at 96 DPI
128x128 DIPs -> 192x192 pixels at 144 DPI
128x128 DIPs -> 256x256 pixels at 192 DPI
```

Total:

```text
5 assets x 3 samples x 5 profiles = 75 capture cases
```

Each case emits:

```text
telegram.bmp   Telegram CPU oracle over a checker background
direct2d.bmp   Direct2D WARP capture over the same background
diff.bmp       amplified colour/alpha difference
```

## Comparison policy

Metrics are calculated from the original transparent premultiplied BGRA
surfaces, not from the checkerboard visualizations.

The initial policy is deliberately rasterizer-aware:

```text
minimum active-pixel IoU:             0.78
maximum total-alpha relative error:   0.12
maximum full-surface mean abs error: 12.0
maximum active-region mean abs error: 30.0
maximum >64-level active fraction:    0.18
maximum nontransparent bounds delta:  4 pixels
```

These limits are expected to catch missing items, channel swaps, double DPI
scaling, incorrect opacity, wrong transforms and large geometry deviations.
They may be tightened only after collecting real Windows evidence across
multiple GPU/driver paths. A threshold failure always retains all images and
metrics for diagnosis.

## Cache gate

Every unchanged plan is drawn twice. The second draw must:

- produce byte-identical WARP pixels;
- create no new geometry resource;
- replace no existing geometry resource;
- use the existing native geometry cache.

## Windows command

From a Visual Studio Developer Command Prompt:

```bat
scripts\run_part19_windows.cmd
```

Artifacts:

```text
out/build/windows-msvc-direct2d-capture/direct2d-capture-artifacts/
```

The dedicated preset includes Telegram `rlottie`, the standalone AveMotion
pipeline and the native Direct2D backend. It is intentionally separate from
the lightweight Part 18 WARP smoke preset.
