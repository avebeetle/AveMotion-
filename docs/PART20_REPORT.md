# AveMotion Part 20 report — live Win32 Direct2D preview host

## Executive result

Part 20 adds the first real interactive host for the standalone AveMotion SDK.
It deliberately adds no new Lottie semantic family. The goal is to prove that
the existing canonical model, evaluator, geometry projector, render planner
and production Direct2D backend can be composed by an ordinary native Windows
application.

Version:

```text
0.20.0
```

## Scope

```text
Win32 HWND
+ QPC absolute clock
+ high-resolution waitable frame pulse
+ host-owned D3D11 device / DXGI flip swap chain
+ host-owned Direct2D device context
+ AveMotion runtime and instance
+ AveMotion property evaluator
+ AveMotion source-geometry projector
+ MotionRenderPlan
+ production Direct2D backend
```

No browser, Qt, WebView, Electron or private AveVoice subsystem is involved.

## Host/engine boundary

The host owns:

- window creation and message dispatch;
- input and layout decisions;
- the presentation clock;
- invalidation;
- D3D11/DXGI/Direct2D target ownership;
- `BeginDraw`, `EndDraw` and `Present`;
- resize, DPI and device recreation.

AveMotion owns:

- asset loading through the current Telegram import seam;
- immutable canonical model preparation;
- lightweight instance playback state;
- exact property and transform evaluation;
- source geometry projection;
- backend-neutral render planning;
- Direct2D native-resource caching and draw execution.

## Absolute-time playback

The preview converts QPC to nanosecond `MotionTime`. Animation state is sampled
for the current absolute time. It never performs `frameIndex++` as the source
of truth.

A high-resolution waitable timer requests approximately 60 presentation
opportunities per second. The deadline is maintained as an absolute QPC
counter. Late periods are skipped in one step and only one invalidation is
submitted, preventing catch-up playback and obsolete-frame queues.

## Interactive controls

The adapter demonstrates host-side orchestration for:

- play/pause;
- reverse retargeting;
- loop/once;
- normalized seek;
- playback-rate changes;
- forced graphics recreation.

All these operations use the public runtime instance state rather than mutating
Telegram or Direct2D objects directly.

## Windows graphics lifecycle

The preview uses:

```text
D3D11 hardware device, with WARP fallback
-> IDXGISwapChain1, flip-discard
-> ID2D1Factory1 / ID2D1Device / ID2D1DeviceContext
-> BGRA premultiplied target bitmap
-> AveMotion::Direct2D
```

`WM_SIZE` resizes the swap chain. `WM_DPICHANGED` applies the suggested window
rectangle and recreates the target at the new DPI. Device-loss results clear
the backend cache and recreate the graphics stack while preserving logical
animation state.

## Retained-state checks

The host records the evaluator and source-geometry workspace generations after
`prepare()`. Every live frame verifies that these generations remain stable.
Unexpected capacity growth is treated as a fatal validation error.

The Direct2D diagnostics exposed in the title and self-test include:

- draw calls;
- geometry-cache hits and misses;
- native geometry creation;
- graphics-domain resets.

## Hidden lifecycle test

`avemotion.win32.preview.selftest` runs on a hidden WARP-backed HWND. It renders
150 deterministic time samples and verifies:

- multiple unique source frames;
- exact pause hold;
- resume and direction changes;
- seek and playback-rate changes;
- resize and 144-DPI target recreation;
- explicit D3D11/Direct2D recreation;
- geometry-cache reuse;
- non-empty final capture artifact;
- a machine-readable `status=pass` report.

The CTest target receives the fixture through an explicit source path, while
the interactive executable receives a copied default asset beside the binary.

## CMake and automation

New option:

```text
AVEMOTION_BUILD_WIN32_PREVIEW
```

New preset:

```text
windows-msvc-win32-preview
```

New launcher:

```text
scripts/run_part20_windows.cmd
scripts/run_part20_windows.ps1
```

A dedicated GitHub Actions job runs the hidden preview gate on
`windows-latest` and uploads its image, report and CTest log.

## Validation performed in the implementation environment

The implementation environment is Linux and cannot execute Win32/D3D11.
The complete existing cross-platform matrix remains green after the new
Windows-only target and CMake wiring:

```text
linux-clang-telegram-debug:  34/34 passed
linux-gcc-telegram-release:  34/34 passed
linux-clang-telegram-asan:   34/34 passed
linux-gcc-samsung-release:   22/22 passed
linux-clang-offline:         13/13 passed
CMake install/export:        passed
external find_package:       compiled, linked and ran
```

The native preview self-test is prepared for MSVC/WARP and must be executed on
a Windows host; no claim is made that the interactive executable ran inside
the Linux implementation environment.

## Scope intentionally deferred

- central application-independent scheduler;
- bounded asynchronous evaluated snapshots;
- gradients and advanced compositing;
- masks and mattes;
- audio-driven parameters;
- `.avm` compiler;
- AveVoice adapter.

The extraction rule remains:

```text
characterize -> isolate -> implement -> compare -> preserve oracle
```
