# AveMotion Part 20.4 report — portable source-geometry identity

## Windows evidence

The complete Part 20.3 MSVC run confirmed the intended product pipeline:

```text
Win32 manifest wiring                 PASS
Direct2D header / contract / WARP     PASS
Win32 preview hidden lifecycle        PASS
75-case Direct2D capture corpus       PASS
Capture preflight                     PASS
Vendor verification                   PASS
Telegram scene golden                 PASS
Telegram render-plan golden           PASS
```

The capture stage rendered all 75 cases through the production Direct2D
backend and compared them with Telegram CPU output:

```text
cases=75
drawCalls=150
geometryCacheHits=1657
geometryCacheMisses=143
```

Only `avemotion.render.source_geometry_golden` failed. The first mismatch was:

```text
polystar_polygon_geometry.json / p000 / frame 0
```

All 68 non-fingerprint fields matched exactly. The raw aggregate geometry hash
changed from `7eee63a3feae85a5` to `c2aef5615ae7f285`.

## Diagnosis

Part 20.3 had acknowledged one exact final Polystar + Trim hash pair. The new
failure occurred in a different Polystar fixture and proved that enumerating
individual low-order raw-float results would create an endless platform hash
allowlist.

The raw source-geometry fingerprint includes exact path float bits and raw
content-derived revisions. These remain useful as local characterization but
are not a reliable cross-CRT semantic identity for legacy trigonometric path
generation.

## Fix

Part 20.4 adds `projected_portable_fingerprint` to the source-geometry TSV.
It hashes exact topology and source identities while quantizing path
coordinates to 1/64 asset unit with deterministic fixed-point conversion.

The MSVC comparator may ignore a difference in `projected_fingerprint` only
when:

- both rows are Telegram rows;
- `projected_portable_fingerprint` is present and equal;
- the raw fingerprint is the only differing field.

The old command-line flag is retained as a compatibility alias, but CMake uses
`--allow-telegram-msvc-raw-float-variance`.

## Regression protection

`avemotion.golden.source_geometry_policy` now proves that:

- strict comparison rejects a raw difference;
- MSVC policy accepts it only with an exact portable fingerprint;
- a changed portable fingerprint fails;
- a changed point count fails;
- the policy does not apply to Samsung;
- an old manifest without the portable field fails.

Clang and GCC independently regenerate the new portable field and must produce
byte-identical manifests.

## Unchanged product behavior

Part 20.4 does not modify:

- Telegram/Samsung sources;
- parser or evaluator semantics;
- primitive, Polystar, Trim or Repeater algorithms;
- local path coordinates;
- runtime exact hashes or revisions;
- `MotionRenderPlan` construction;
- Direct2D backend or resource caches;
- Win32 preview timing/lifecycle;
- native capture metrics or thresholds.

This is a characterization-harness correction only.

## Validation before delivery

```text
Linux Clang Telegram Debug: 38/38 PASS
Linux GCC Telegram Release: 38/38 PASS
Linux Clang Telegram ASan: 38/38 PASS
Linux GCC Samsung Release: 26/26 PASS
Linux Clang standalone/offline: 17/17 PASS
Clang/GCC source manifest parity: byte-identical
CMake install/export 0.20.4: PASS
External find_package consumer: PASS
Captured MSVC raw-hash simulation: PASS
```

New source-geometry golden SHA-256:

```text
bd42276cf5e098b8219b5cb6c919d7eeb817bd1cbeac70ece4fde733dfb8628f
```

A real MSVC rerun remains the final confirmation that its newly generated
portable fingerprint matches the persisted Linux value while its raw hash may
retain the observed CRT-specific bits.
