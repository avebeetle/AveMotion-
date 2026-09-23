# AveMotion Part 21 report — centralized player/scheduler

## Executive result

Part 21 moves frame scheduling out of the Win32 sample and introduces an
installable, application-independent `AveMotion::Player` target.

```text
many MotionInstance registrations
+ one host-supplied absolute clock
        -> centralized due selection
        -> one nearest deadline
        -> one coalesced host repaint request
```

No new Lottie feature family was added. Telegram parsing/evaluation oracles,
canonical model, geometry projection, render-plan semantics and Direct2D output
remain unchanged.

## New public module

```text
include/avemotion/player/Player.hpp
src/player/Player.cpp
CMake target: AveMotion::Player
```

The module exports stable player handles, host callbacks, entry options,
frame-reason flags, due-frame views and diagnostics.

## Scheduling model

- one registry coordinates many instances;
- one absolute deadline is published to the host;
- no thread or timer is owned by the SDK;
- frame requests are coalesced until `tick()`;
- late periods are skipped mathematically rather than enqueued;
- the due list is retained and bounded by registration count;
- visible repaint and logical timeline advancement are distinct reasons;
- completion produces one terminal state notification and no continuing
  periodic deadline;
- stable generations reject stale registration handles.

## Visibility model

- `KeepUp`: logical time advances while scheduling is suppressed;
- `Freeze`: hidden playback is automatically paused/resumed without a visual
  jump;
- user pause is never mistaken for an automatic visibility pause;
- hiding requests one host frame so old presentation pixels can be cleared.

## Win32 integration

The Part 20 preview no longer owns a private fixed-rate animation scheduler.
It now:

```text
Player::scheduleWakeup(deadline)
-> one-shot Win32 waitable timer
-> host invalidation
-> WM_PAINT
-> Player::tick(currentQpcTime, AllVisible)
-> existing evaluate/project/plan/Direct2D path
```

The preview's HWND, message loop, swap chain, DPI handling and final present
remain outside AveMotion core. Minimize/restore maps to the player's `Freeze`
visibility policy.

The hidden WARP self-test records and validates:

- player tick count;
- frames returned;
- wakeups scheduled;
- skipped deadline count;
- existing Direct2D cache/device/DPI lifecycle metrics.

## Regression coverage

The central player test covers:

- multiple instances with different rates;
- nearest-deadline selection;
- no early frame;
- bounded late-frame skipping;
- pause and completion frames;
- direct instance revision detection;
- KeepUp and Freeze visibility;
- explicit invalidation coalescing;
- full host repaint selection;
- stable handle generation on slot reuse;
- hidden pre-playing containment;
- callback replacement with a pending removal repaint;
- old-host deadline cancellation, new-host deadline republication and final
  scheduler shutdown cancellation;
- retained storage stability.

A source-level integration test proves the Win32 preview consumes
`AveMotion::Player` and no longer contains the previous private fixed-rate
scheduler symbols.

## Validation status

The release process validates:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 40/40 PASS |
| Telegram + GCC Release | 40/40 PASS |
| Telegram + Clang ASan | 40/40 PASS |
| Samsung + GCC Release | 28/28 PASS |
| Standalone/offline Clang | 18/18 PASS |
| Install/export of `AveMotion::Player` | PASS |
| External `find_package(AveMotion 0.21)` consumer | PASS |

Clean ZIP and Git bundle rebuilds are performed as delivery gates.

The Part 21 Windows runner additionally validates the real MSVC/WARP preview,
Direct2D contract/capture gates and player diagnostics. Part 20.4 is the last
confirmed Windows baseline before this scheduler integration; the new Part 21
Windows gate must be run on a Windows SDK/MSVC host.

## Deliberately deferred

- worker evaluation and bounded snapshot publication;
- a Telegram-style bitmap frame ring;
- priority scheduling;
- a deadline heap (linear scan remains benchmark-gated);
- markers/events;
- clip/state graph;
- TGS decompression;
- simplified product facade;
- AveVoice or Qt-specific adapters.

## Next isolated goal

Part 22 should add a hardened `.tgs` transport loader:

```text
TGS gzip bytes
-> size/ratio/CRC validation
-> bounded decompression
-> UTF-8 JSON handoff
-> existing Telegram parser / AveMotion asset path
```

This directly advances the original Telegram-sticker product goal without
mixing another rendering feature family into the scheduler stage.
