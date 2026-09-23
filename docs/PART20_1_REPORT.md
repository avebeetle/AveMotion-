# AveMotion Part 20.1 report — CMake 4.4 compatibility hotfix

## Trigger

A real Windows configure using CMake 4.4.0 failed before compilation because
both pinned rlottie variants declare `cmake_minimum_required(VERSION 3.3)`.
CMake 4.0 removed policy compatibility below 3.5.  The same run also emitted
CMP0194 because the old dependency enables ASM under MSVC even though its
Windows build does not compile assembler sources.

## Fix

- AveMotion no longer enables ASM at the top level; only the dependency may do
  so where it needs it.
- Immediately around `add_subdirectory(rlottie)`, AveMotion sets
  `CMAKE_POLICY_VERSION_MINIMUM=3.5` on CMake 4.x, as documented for embedding
  an old third-party project without changing its source.
- `CMAKE_POLICY_DEFAULT_CMP0194=OLD` is scoped to that dependency so its legacy
  MSVC configuration is accepted without the policy warning.
- Both variables are restored after the dependency is added.
- Added `avemotion.cmake.legacy_subproject` regression coverage.

## Scope

No runtime, evaluator, geometry, Direct2D, Win32, pixel, asset, or rlottie
source behavior changed.  Vendored fingerprints and existing goldens remain
unchanged.
