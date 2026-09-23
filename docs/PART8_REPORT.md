# AveMotion Part 8 report — independent property evaluator

## Executive result

Part 8 completes the first real semantic replacement inside AveMotion.

```text
Before Part 8
Canonical authored tables
    -> Telegram evaluator
    -> evaluated visual scene

After Part 8
Canonical authored tables
    -> AveMotion PropertyEvaluator
       static / hold / linear / cubic temporal properties
       scalar / Vec2 / color / matrix
       local 2D transform and opacity
    -> retained canonical property buffers

Telegram evaluator
    -> exact parity oracle and unsupported-feature path
```

The new evaluator is a standalone installable C++ component. It does not include
or call `rlottie`, and its public headers contain no Telegram, Qt, Win32, COM or
Direct2D types.

## Delivered architecture

New exported CMake target:

```text
AveMotion::Evaluation
```

New public API:

```cpp
PropertyEvaluator
PropertyEvaluationWorkspace
PropertyEvaluationView
EvaluatedProperty
EvaluatedNodeTransform
MotionPropertyValue
```

The immutable evaluator can be shared. Mutable cursor/value history belongs to a
workspace owned by one instance or evaluation stream.

## Supported semantics

- static scalar, vector, color and matrix values;
- hold, linear and temporal cubic-Bezier segments;
- exact before-first and after-last endpoint sampling;
- combined and separated position;
- scale, authored numeric rotation, anchor and opacity;
- static transform matrices;
- direct random seek;
- forward and reverse segment traversal;
- value-level dirty revisions;
- retained output with no vector resizing after `prepare()`.

## Explicitly unsupported

Part 8 never substitutes an approximation for:

- spatial position paths;
- auto-orient;
- animated shapes;
- animated gradients;
- path morphing;
- world transform hierarchy;
- masks, mattes and compositing.

These values are marked `Unsupported`; Telegram remains authoritative for the
complete visual path.

## Segment and easing implementation

Segment selection uses cursor reuse, adjacent traversal and binary-search
fallback. A cursor cannot alter the result and may be reset at any point.

Temporal cubic easing uses the same mature hybrid design as Telegram:

```text
11-entry X sample table
+ Newton iteration
+ subdivision fallback
```

The solver has fixed iteration limits and finite-data validation. Synthetic tests
cover exact endpoints, hold boundaries, midpoint linear/cubic values and corrupt
segments.

## Telegram parity

The parity test derives 196 unique integer source frames from all eight corpus
assets. Frames include authored boundaries, segment midpoints and deterministic
random seeks. Each is tested in forward, reverse and fresh-direct order.

Final result:

```text
assets=8
samples=196
supported property comparisons=24,969
supported transform comparisons=4,752
skipped spatial values=389
skipped animated shape/gradient values=195
skipped unsupported transforms=915
direct binary searches=332
forward cursor fast paths=332
reverse cursor fast paths=332
```

Every supported comparison matched Telegram. Repeated same-time evaluation
reported no changes.

## Persisted property golden

The five-sample-per-asset characterizer records 40 rows and the following totals:

```text
properties visited: 1,545
supported compared: 1,450
unsupported: 95
local transforms: 270
cubic samples: 37
transform hash parity: 40/40
```

File:

```text
tests/golden/property-telegram.tsv
SHA-256 66259ac79ec437fe8d81b2eaf1c9432d3c398351cedab38ecf575170d21def52
```

Clang and GCC generate byte-identical property manifests.

## Dirty/revision hardening

Materialized numeric values deliberately discard source `MotionValueRef`
identity. This prevents a boundary between equal-valued keyframes from creating a
false revision. A dedicated two-segment constant-value test proves this behavior.

Revisions advance only when the canonical published value changes. Cursor
movement, segment identity and source-storage references do not count as visual
changes.

## Validation and safety

Evaluator construction validates all referenced arrays, types, ranges, finite
values and contiguous segment ordering. Invalid or moved-from evaluator objects
remain safe to inspect and prepare; they return an explicit invalid-model view.

The following malformed cases are tested:

- invalid model;
- corrupt segment;
- non-finite requested frame;
- unprepared workspace;
- unsupported spatial value;
- constant-value segment-boundary crossing.

## Regression gates

All pre-Part8 persisted goldens remain byte-identical to Part 7:

- CPU pixel oracles;
- evaluated-scene oracles;
- render-plan oracles;
- canonical model summary;
- parsed-model structural oracle;
- corpus and vendor fingerprints.

Part 8 adds only the property-evaluation oracle and evaluator-specific tests.

## Build and test matrix

Validated locally on Linux x86-64:

| Configuration | Result |
|---|---:|
| Telegram + Clang Debug | 24/24 |
| Telegram + GCC Release | 24/24 |
| Telegram + Clang AddressSanitizer | 24/24 |
| Samsung + Clang Debug | 16/16 |
| Samsung + GCC Release | 16/16 |
| Samsung + Clang AddressSanitizer | 16/16 |
| Offline/installable SDK | 7/7 |
| Offline AveMotion-owned code + ASan/UBSan | 7/7 |
| External `find_package(AveMotion 0.8)` consumer | passed |

The Samsung matrix verifies that the new generic evaluator library and unchanged
comparison paths still build cleanly; Telegram-only semantic parity tests are not
pretended to exist for Samsung.

## Installation

The offline package exports:

```text
AveMotion::Core
AveMotion::Model
AveMotion::Evaluation
AveMotion::Runtime
AveMotion::Rendering
AveMotion::Reference
```

A clean external consumer configured with `find_package(AveMotion 0.8)` compiled,
linked and executed successfully.

## Honest limitations

- exact evaluator input is still `double` source frame, not final integer time;
- broad Telegram parity uses integer source frames; synthetic tests cover
  subframes;
- the new property output is not yet the sole source for `EvaluatedScene`;
- spatial tracks, world hierarchy and animated complex values remain Telegram;
- Direct2D implementation is still not locally built because this environment
  has no Windows SDK;
- public API/ABI remain experimental;
- no scheduler/player publication protocol exists yet.

## Next stage

Part 9 will add spatial cubic position and parent/world-transform propagation,
then compare those results against Telegram before any shape/path evaluator is
replaced.

The project continues to follow:

```text
characterize -> isolate -> replace -> compare -> remove old path
```
