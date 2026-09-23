# Playback and centralized presentation scheduling

## Instance playback — Part 6 onward

`runtime::Instance` owns deterministic logical playback state. The host supplies
absolute monotonic `MotionTime` values; the instance maps them to normalized
asset position and source frame.

```cpp
struct MotionTime {
    std::int64_t nanoseconds;
};
```

This is a portable SDK value, not a platform clock. A host can convert from
`std::chrono::steady_clock`, QPC, media time, a game clock or a deterministic
test clock.

Supported controls:

- play;
- pause;
- resume;
- stop;
- normalized seek;
- forward/reverse direction;
- positive playback rate;
- loop or once;
- directly controlled normalized progress.

The instance stores an anchor position and anchor host time. While playing:

```text
elapsedSeconds = (hostTime - anchorTime) / 1e9
travel = elapsedSeconds * playbackRate / assetDuration
signedTravel = direction == Forward ? travel : -travel
rawPosition = anchorPosition + signedTravel
```

Changing direction, speed, pause or seek re-anchors at the current logical
position, so the visual state does not jump. No frame-by-frame `deltaTime`
accumulation is used.

## Central player — Part 21

`AveMotion::Player` coordinates presentation opportunities for many instances:

```text
Instance playback state
+ visibility
+ presentation-rate cap
-> nearest absolute deadline
-> host wakeup callback
-> bounded due-frame view
```

The player does not evaluate or render and creates no platform timer. Host code
calls `tick(now)` at a scheduled wakeup or paint opportunity and then evaluates
only the returned instances.

The current discrete source-frame cadence is:

```text
min(assetFrameRate * playbackRate, maximumPresentationRate)
```

Late frames are skipped, not queued. `FrameSelection::AllVisible` supports
expose/full repaint without changing logical playback.

Visibility supports:

- `KeepUp`: hidden logical time continues;
- `Freeze`: hidden playing instances pause and resume at the same position.

See `docs/PLAYER_SCHEDULER.md` for the full host contract.

## Current limitations

- clips are currently one synthesized full-asset clip;
- no ping-pong mode;
- no marker/event crossing;
- no parameter block or state graph;
- normalized position and playback rate are currently `double`;
- the evaluator still maps to discrete Telegram frame indices;
- player and instance control remain synchronous/control-thread oriented.
