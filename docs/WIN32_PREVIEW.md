# AveMotion Win32 preview host

The preview is a validation host and integration example. It is not a dependency
of AveMotion Core and does not introduce a second animation engine.

## Pipeline

```text
Win32 message loop + QPC absolute clock
    -> AveMotion centralized Player
    -> AveMotion Runtime / Instance playback state
    -> AveMotion PropertyEvaluator
    -> SourceGeometryProjector
    -> MotionRenderPlanner
    -> production AveMotion Direct2D backend
    -> host-owned D3D11 swap chain / Direct2D device context
    -> Present(1)
```

The preview continues to use the pinned Telegram import/evaluation seam for the
parts of Lottie that AveMotion has not replaced yet.

Part 22 reads assets as bytes and calls `Runtime::loadAssetData()`. Plain JSON
and gzip `.tgs` therefore share the same validated runtime path after bounded
decompression. The copied default asset and hidden WARP self-test asset are now
`preview-assets/repeater_content_group.tgs`.

## Build and run

From a Visual Studio 2022 Developer Command Prompt:

```bat
scripts\run_part22_windows.cmd
```

Launch a different Lottie JSON or Telegram TGS:

```bat
scripts\run_part22_windows.cmd -Asset "C:\path\sticker.tgs"

scripts\run_part22_windows.cmd -Asset "C:\path\animation.json"
```

Useful switches:

```text
-SkipCapture   do not rerun the Part 19 75-case capture corpus
-SkipFullSuite run only the focused player/preview/Direct2D gate
-NoLaunch      do not open the interactive window after validation
```

## Controls

```text
Space       pause / resume
R           reverse direction without a visual jump
L           loop / once
Left/Right  seek by 5%
Home/End    seek to an endpoint
+ / -       change playback rate
F5          recreate D3D11 / Direct2D resources
Escape      close
```

## Scheduling policy

The old preview-local fixed 60 Hz scheduler is removed. The application now
maps the portable `PlayerHostCallbacks` to Win32:

```text
Player publishes nearest MotionTime deadline
-> SetWaitableTimerEx(one shot)
-> host requests a repaint
-> WM_PAINT calls Player::tick(QPC time, AllVisible)
-> host evaluates and draws the selected instance
```

There is still exactly one host timer, never one timer per instance. The player
advances missed deadlines directly and does not queue obsolete frames.

Minimize/restore maps to `HiddenTimePolicy::Freeze`, so rendering work stops
while hidden and the animation resumes from the same visual position.

## Resize, DPI and graphics recreation

The host is Per-Monitor-V2 DPI-aware and handles:

- `WM_SIZE` and visibility scheduling;
- `WM_DPICHANGED`;
- physical-pixel swap-chain resizing;
- logical DIP viewport calculation;
- explicit graphics recreation with `F5`;
- Direct2D/DXGI device-loss errors.

Logical assets, registrations and instances survive graphics recreation. The
Direct2D cache is rebuilt lazily under a new graphics generation.

## Hidden self-test

CTest runs:

```text
avemotion.win32.preview.selftest
```

The WARP test exercises 150 samples and validates player ticks, returned frames,
published wakeups, playback controls, resize, DPI, device recreation and native
geometry-cache reuse.

Artifacts:

```text
out/build/windows-msvc-win32-preview/win32-preview-artifacts/
├── avemotion-win32-preview-selftest.ppm
└── avemotion-win32-preview-selftest.txt
```

The Part 22 Windows runner uses the TGS fixture and requires the report to contain `status=pass`,
`finalDpi=144`, at least 150 player ticks/frames and at least one scheduled
wakeup.

## Deliberate boundaries

The preview owns its HWND, QPC conversion, waitable timer, swap chain and final
presentation. These concepts do not enter AveMotion core/player headers.

Part 22 adds only hardened TGS transport; it does not add gradients, masks, mattes, effects, `.avm`, audio parameters
or AveVoice-specific code.
