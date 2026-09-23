# Centralized application-independent player/scheduler

## Purpose

`AveMotion::Player` coordinates many `runtime::Instance` objects without owning
a platform event loop, timer, window, render target or worker thread. It is the
portable policy layer between instance playback state and a host's presentation
loop.

```text
MotionInstance A --\
MotionInstance B ----> Player::tick(hostTime)
MotionInstance C --/        |
                            +--> bounded due-frame view
                            +--> nearest absolute deadline
                            +--> coalesced requestFrame callback
```

This deliberately differs from Telegram's CPU-bitmap player layer. AveMotion
does not need a QImage frame ring because the production path evaluates a live
vector state and draws it through a native backend.

## Ownership

The player registry owns a `shared_ptr<runtime::Instance>` for every active
registration. A registration therefore remains valid until explicit
`removeInstance()` or `clear()` even when the host releases its own reference.

The external identifier is:

```cpp
struct PlayerHandle {
    std::uint32_t index;
    std::uint32_t generation;
};
```

A reused slot receives a new generation, so an old handle cannot address a new
registration.

## Threading contract

Part 21 is intentionally synchronous and control-thread-only:

- no hidden thread is created;
- no platform timer is created;
- callbacks execute on the thread that called the player;
- callbacks must not re-enter the same `Player`;
- evaluator and renderer ownership remains with the host;
- separate instances may still be evaluated on separate workers in a future
  layer because assets are immutable and workspaces are instance-local.

This makes embedding predictable for Win32, Qt, game/editor render loops and
headless tests.

## Host callbacks

```cpp
struct PlayerHostCallbacks {
    void* userData;
    void (*requestFrame)(void*) noexcept;
    void (*scheduleWakeup)(void*, MotionTime deadline) noexcept;
    void (*cancelWakeup)(void*) noexcept;
};
```

`requestFrame` is coalesced until the next `tick()`. `scheduleWakeup` receives
one absolute deadline: the earliest deadline among all visible playing
registrations. The host may implement it with a waitable timer, a framework
animation pulse, a game-loop wakeup or another mechanism.

A scheduled wakeup does not draw by itself. At or after the deadline the host
calls `tick(now)`, requests/enters its normal paint path, evaluates the returned
instances and renders them.

Replacing callbacks preserves an already coalesced repaint request, including
a request caused by removal where no live entry remains. This lets a new host
binding clear retired presentation bounds correctly. If an absolute wakeup is
already armed, the old host receives `cancelWakeup` before the same deadline is
published to the new host. Destroying or move-replacing a player also cancels
its last published wakeup.

## Cadence and deadlines

For the current discrete Lottie evaluator, effective cadence is:

```text
effectiveRate = min(assetFrameRate * playbackRate,
                    maximumPresentationRate)
```

The player stores one deadline per visible playing entry and publishes the
minimum. When the control thread is late, it advances the deadline directly by
the number of elapsed periods:

```text
late by N periods
-> return at most one current frame per registration
-> count N-1 skipped deadlines
-> schedule the first future deadline
```

It never queues all missed frames. This is the application-level equivalent of
Telegram's bounded presentation principle without carrying over its bitmap
ring.

## Frame reasons

A returned `ScheduledFrame` records why the host should process it:

- `FirstFrame`;
- `TimelineAdvanced`;
- `PlaybackChanged`;
- `VisibilityChanged`;
- `ExplicitInvalidation`;
- `Completion`;
- `HostRepaint`.

`FrameSelection::DueOnly` returns changed/due entries. `AllVisible` additionally
returns every visible registration and is intended for expose/full-repaint
paths. A repaint does not mutate playback or force a geometry rebuild.

## Visibility

Two policies are supported:

### `KeepUp`

The hidden entry is removed from scheduled work, but its logical time continues.
When shown, the latest state is sampled immediately.

### `Freeze`

A playing entry is automatically paused when hidden and automatically resumed
from the same position when shown. An entry paused explicitly by the user is
not resumed implicitly.

Hiding posts one frame request so a host that remains visible can clear the old
presentation bounds. The presentation binding and dirty rectangle remain host
responsibilities.

## Mutations

The player provides wrappers for:

- play/pause/resume/stop;
- normalized seek;
- controlled progress;
- direction;
- playback rate;
- loop mode;
- visibility;
- maximum presentation rate;
- explicit invalidation.

Controls should normally go through `Player`, because it can publish a new
frame request and deadline immediately. If a host mutates a registered
`Instance` directly, `tick()` detects the changed playback revision, but no
host wakeup can be generated until the host calls the player again.

## Retained storage

The due-frame vector is reserved to registry capacity when registrations are
added. A steady `tick()` clears and reuses it. `storageGeneration` changes only
when retained registry/output capacity changes and is covered by regression
tests.

## Complexity

Part 21 uses a deterministic linear scan over registered instances per tick and
when recomputing the nearest deadline:

```text
O(number of registered instances)
```

This is simpler and cache-friendly for the expected UI scale (dozens rather
than millions of animations). A heap is intentionally deferred until profiling
shows the scan is material on weak hardware.

## Deliberate boundaries

Part 21 does not add:

- a worker pool;
- a frame/snapshot ring;
- per-instance timers;
- Qt or Win32 types in the public API;
- marker crossing;
- clip graphs or blending;
- audio parameters;
- TGS decompression;
- `.avm` serialization.
