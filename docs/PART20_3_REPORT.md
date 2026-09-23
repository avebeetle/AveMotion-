# AveMotion Part 20.3 report — cross-platform rlottie golden policy

## Trigger

The first complete Part 20.2 Windows run proved the native product path:

```text
Win32 manifest wiring                 PASS
Direct2D header / contract / WARP     PASS
Win32 preview hidden lifecycle        PASS
75-case Direct2D capture corpus       PASS
Capture preflight                     PASS
Vendor verification                   PASS
```

The remaining suite reported three characterization-golden failures. All three
pointed to the same pinned Telegram sample:

```text
polystar_line_clockwise_trim.json / p100 / frame 149 / 128x128
```

Scene topology, paint, path counts, plan topology, paint identity and update
statistics were unchanged. Only fingerprints containing exact floating-point
bits changed between the persisted Linux result and the MSVC result.

## Diagnosis

This terminal Polystar + Trim sample passes through the pinned Telegram
implementation's single-precision trigonometry and recursive path
measurement/splitting. The observed differences are confined to low-order
floating-point bits and are consistent with compiler/CRT math variance between
Linux and MSVC. They are not treated as a portable semantic contract.

The diagnosis is supported by independent Windows evidence from the same run:

- exact property/evaluator tests passed;
- Shape, spatial, world-transform, primitive, Trim and Repeater tests passed;
- the production Direct2D contract and D3D11 WARP smoke passed;
- the hidden Win32 preview lifecycle test passed;
- all 75 Direct2D-vs-Telegram capture cases passed;
- topology, paint and all structural counts in the failed rows were identical.

## Fix

Part 20.3 adds a narrow, explicit characterization policy:

- enabled only by an MSVC build;
- enabled only for the Telegram variant;
- enabled only for the exact asset hash, sample, frame and viewport above;
- enabled only for explicitly listed float-derived fingerprint columns;
- all structural, paint, count, revision and unsupported-feature fields remain
  exact.

The affected harnesses are:

```text
scene_golden_tests.cpp
plan_golden_tests.cpp
compare_source_geometry_golden.py
```

The persisted golden files are not rewritten for Windows. Linux remains strict
and byte-exact. The Windows harness acknowledges the one documented legacy
oracle variance while preserving every meaningful regression gate.

## Regression protection

Two new tests prevent the exception from expanding silently:

```text
avemotion.golden.cross_platform_policy
avemotion.golden.source_geometry_policy
```

They verify:

- exact rows pass normally;
- the captured MSVC scene/plan hashes fail when the policy is disabled;
- only approved fingerprint fields can differ;
- a changed topology/count field still fails;
- a different asset or sample still fails;
- the source-geometry TSV header and row count remain exact.

## Runtime invariants left unchanged

Part 20.3 does not change:

- evaluated floating-point values;
- runtime hashing or cache keys;
- geometry or paint revisions;
- dirty tracking;
- Telegram/Samsung sources;
- evaluator or geometry algorithms;
- MotionRenderPlan construction;
- Direct2D output;
- Win32 preview behavior;
- capture thresholds or evidence images.

The exception exists only at the persisted characterization-oracle boundary and
should be removed when this final legacy Telegram Polystar/Trim path is fully
replaced by AveMotion-owned deterministic geometry.

## Validation before delivery

```text
Linux Clang Telegram Debug:     38/38 PASS
Linux GCC Telegram Release:     38/38 PASS
Linux Clang Telegram ASan:      38/38 PASS (23 + 15 grouped run)
Linux Clang standalone/offline: 17/17 PASS
CMake install/export:           PASS
find_package(AveMotion 0.20.3): PASS
External consumer run:          PASS
```

The MSVC branch is covered by synthetic rows containing the exact hashes from
the Part 20.2 Windows run. A real Windows rerun is the final confirmation that
the three former failures are accepted only through this policy.
