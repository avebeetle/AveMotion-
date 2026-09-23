# Pinned Telegram rlottie MSVC endpoint float variance

## Scope

The pinned Telegram `rlottie` baseline produces a platform-specific
least-significant-bit difference for exactly one persisted characterization
sample:

```text
asset:    polystar_line_clockwise_trim.json
sample:   p100
frame:    149
viewport: 128x128
```

The sample combines animated Polystar generation and Trim Path at the terminal
frame. The legacy implementation uses `float` trigonometry and recursive path
length/splitting arithmetic. MSVC's CRT and the Linux libm used to author the
persisted golden can therefore return different low-order float bits.

## Evidence from the native Windows run

The Windows run kept all semantic and structural observations identical:

- selected frame;
- viewport;
- scene topology;
- paint fingerprint;
- layer, draw-item, mask and path counts;
- source-geometry statistics;
- render-plan topology, paint identity and update counts.

Only float-derived complete/geometry/presentation fingerprints differed. The
same run passed:

- production Direct2D header and contract tests;
- D3D11 WARP smoke;
- hidden Win32 preview lifecycle test;
- 75-case Direct2D-vs-Telegram capture corpus;
- property, Shape, spatial, transform, primitive, Trim and Repeater parity.

This is therefore an oracle fingerprint portability issue, not a detected
visual or Direct2D regression.

## Policy

Exact golden comparison remains mandatory everywhere except this one pinned
Telegram sample under an MSVC build. For that row only:

- evaluated-scene goldens may differ in `scene` and `geometry` fingerprints;
- render-plan goldens may differ in `plan`, `geometry_identity` and
  `presentation` fingerprints;
- source-geometry raw hashes are handled by the independent portable-path policy described below.

Every other field must remain exact. Any different asset, frame, viewport,
topology, paint, count, update statistic or unsupported-feature count still
fails the suite.

The exception is implemented only in characterization tests. Runtime hashes,
resource cache keys, dirty tracking, evaluation and rendering are unchanged.
It should be removed once the legacy Telegram geometry path for this sample is
fully replaced by AveMotion-owned deterministic geometry.

## Part 20.4 refinement

The exact row policy above remains in force for evaluated-scene and render-plan
manifests. Source-geometry comparison no longer accumulates row-specific raw
hash pairs. It now requires an independently computed portable quantized path
fingerprint to match before accepting any MSVC-only raw-float difference. See
`SOURCE_GEOMETRY_RAW_FLOAT_PORTABILITY.md`.
