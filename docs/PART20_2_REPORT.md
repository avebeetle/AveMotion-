# AveMotion Part 20.2 report — MSVC duplicate-manifest hotfix

## Trigger

The first real Part 20.1 MSVC build reached the preview link step but failed
with:

```text
CVTRES : fatal error CVT1100: duplicate resource. type:MANIFEST, name:1, language:0x0409
LINK   : fatal error LNK1123: failure during conversion to COFF
```

The preview target compiled `AveMotionPreview.rc`, which embedded an
`RT_MANIFEST` resource with id 1. CMake/MSVC also generated the normal
executable manifest resource during linking. `cvtres` therefore received two
resources with the same type, id and language.

## Fix

- Removed the legacy `AveMotionPreview.rc` indirection.
- Kept `AveMotionPreview.manifest`, but list it directly as a source of the
  `avemotion_win32_preview` target.
- CMake now passes that file through `vs_link_exe --manifests`, where it is
  merged with the linker-generated manifest and embedded exactly once.
- Preserved the manifest's Per-Monitor-V2 DPI, long-path and UTF-8 active-code-
  page declarations.
- Preserved the explicit `SetProcessDpiAwarenessContext(...)` call before HWND
  creation as a defensive fallback for development hosts that replace
  manifests.
- Added `avemotion.cmake.win32_manifest_wiring`, which verifies that the target
  lists the `.manifest` exactly once, that no RC file embeds `RT_MANIFEST`, and
  that the required XML declarations remain present.

## Scope

No parser, evaluator, geometry, render-plan, Direct2D, playback, pixel, asset,
or rlottie behavior changed. The warnings emitted while compiling the pinned
Telegram source are legacy third-party warnings and were not the cause of the
link failure.
