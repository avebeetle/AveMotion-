# AveMotion Part 12 report — generated Rectangle and Ellipse geometry

## Goal

Generate Rectangle, Rounded Rectangle and Ellipse paths from AveMotion's
canonical property tables, publish them through the existing source-geometry
and render-plan seam, and accept them only after exact Telegram parity.

## Result

Part 12 adds:

- backend-neutral `PrimitivePathGenerator`;
- centered Rectangle and Ellipse construction;
- sharp and rounded rectangles;
- clockwise and counter-clockwise traversal;
- Telegram-compatible radius clamping;
- Telegram-compatible quarter-arc endpoint arithmetic;
- static/animated primitive identity and revisions;
- primitive-specific projection diagnostics;
- a dedicated static/animated primitive fixture;
- a 45-row source-geometry golden;
- an additional Windows MSVC Release CI preset.

## Why the arc implementation is exact

A straightforward four-cubic rounded rectangle matched most samples but failed
at two animated intermediate radii. Telegram builds each corner through
`VRectF::arcTo()` and computes arc endpoints with its historical float math.
At those radii, the final point differs from the initial point by more than the
`VPath::close()` epsilon, so Telegram emits an extra closing line.

AveMotion now mirrors the relevant `VRectF`, ellipse-point and quarter-arc
arithmetic. The parity gate is unchanged; no tolerance was widened.

## Characterization

```text
assets / fixtures:             9
sampled states:               45
draw items visited:          374
source candidates:           344
accepted projections:         79
asset-static projections:     51
instance-animated projections:28
projected path points:      1,053
primitive candidates:         60
rectangle candidates:         35
ellipse candidates:           25
sharp rectangles:             17
rounded rectangles:           18
ellipses:                      25
primitive property failures:   0
primitive evaluation failures: 0
parity rejections:             22
```

The 22 remaining parity rejections are existing unsupported/nested Shape cases,
not generated primitive failures. The dedicated seven-primitive fixture has
zero parity rejections at all five samples.

Source-geometry golden SHA-256:

```text
e3e5bb6590a7c78ed08de86bc2db5132104a7f9a4916008138263e03bb83b0f3
```

## Windows-first gate

The project already exports a Windows-only Direct2D backend target. Part 12 adds
`windows-msvc-telegram-release` alongside Telegram Debug and Samsung Debug in
CMake presets and GitHub Actions. Generated primitive geometry enters the same
backend-neutral `MotionRenderPlan` consumed by Direct2D.

The current execution environment is Linux and does not contain the Windows
SDK. Therefore the C++ core, planner and parity suite are executed locally;
MSVC compilation and `ID2D1DeviceContext` execution remain explicit Windows CI
and host-machine gates.

## Regression boundaries

Unchanged:

- Telegram and Samsung CPU pixel goldens;
- evaluated-scene goldens;
- legacy render-plan goldens;
- canonical and parsed-model goldens;
- property/Shape/world-transform golden;
- vendor fingerprints and source corpus hashes.

Intentionally updated:

- `tests/golden/source-geometry-telegram.tsv`, because additional proven
  Rectangle/Ellipse geometry now replaces Telegram fallback in the source
  projection seam.
