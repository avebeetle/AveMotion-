# Part 25G — preview exit and native-scene readiness

Date: 2026-09-23. Base: `e0ca0165f7e636a70f709a8221c6b25e6644fad6`.
Scoped correction: `36a17e466142d25a7ee157eebcadefb043d8c1e4`.
Development resumed interactively. The user-canceled 15-minute automation remains
deleted; this continuation does not recreate it.

## Actual post-checkout-fix CI

[Run 35858877464](https://github.com/avebeetle/AveMotion-/actions/runs/35858877464)
completed with an overall failure. Windows Telegram Debug/Release, Direct2D
WARP/capture, all four lean module/package jobs and offline-package succeeded.
Three Linux reference/sanitizer jobs and Win32 preview failed. This stage does
not declare Linux exact numerical comparisons fixed or the whole workflow green.

The [preview job](https://github.com/avebeetle/AveMotion-/actions/runs/35858877464/job/107173889257)
passed its focused 18 tests and remaining 43 tests, printed `[AveMotion Part 23]
PASS`, then ended with process exit 1. Selected rendered log excerpts are in
`out/part25g-preview-exit/cloud-evidence.md`; they are not a full downloaded log.

The final native validator invocation deliberately rejects a forbidden sticker
with exit 2. The script checks both that exit and `Result: REJECT`, but previously
left the native exit status set at successful script completion. GitHub's
[PowerShell shell wrapper](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)
propagates `LASTEXITCODE`. The bounded correction adds `exit 0` only after the
existing `try/finally`, preserving all failure throws and negative checks.

## Verification and boundaries

- Static regression: the new missing-success-exit assertion failed before the
  correction and passed afterward. This is source wiring coverage, not execution
  of the full runner.
- An independently authored small PowerShell model reproduced exit leakage,
  successful terminal exit, and a nonzero throwing path. It tests the language
  mechanism, not the production runner's integration.
- Fresh direct MSVC-environment CTest: `windows-msvc-telegram-debug` **68/68**,
  80.47 seconds; `windows-msvc-win32-preview` **62/62**, 87.01 seconds.
  Saved logs: `out/part25g-preview-exit/telegram-debug-ctest.log` and
  `win32-preview-ctest.log`.
- The first Debug run lacked the Visual Studio compiler environment and failed
  the legacy-subproject configure test. The full suite was rerun with the
  installed VS environment; no test was weakened or suppressed.
- The controller separately reran the wiring test and successful isolated shell
  model, both exit 0, then seven focused preview/validation tests including WARP
  smoke, capture and preview selftest: **7/7**, 4.69 seconds. Raw controller logs
  are beside the suite logs.
- Direct execution of the downloaded production `.ps1` was blocked by Windows
  signature policy. That is a security-policy block, not functional RED. No
  execution policy, file security stream, system dependency or Windows setting
  was changed to bypass it. Actual runner integration requires the ordinary-push
  GitHub clean-checkout result.

Independent whole-diff review approved `e0ca016..36a17e4`, with no actionable
P0-P3 findings. It additionally verified the isolated wrapper under dot-sourced
invocation: success 0, throwing failure 1. Review:
`out/part25g-preview-exit/review.md`. The reviewer explicitly left the fresh
production CI result pending normal push; local tests do not establish it.
Historical static RED and the first no-VS run were tool-output evidence, not
separately preserved raw files. The full successful suite logs are preserved.

## Native-scene readiness: observations, not implementation

The standalone mapping probe completed 12 cases, each native exit 0. Its fresh
ordinary-reference and retained-recording comparisons were equal. This validates
the probe; it is not an independent native-emitter parity result. Raw commands,
exits and complete field dumps remain in `out/part25g-native-probe/`.

The one-slot fixture binds authored ellipse/fill nodes 3/4 to render draw 0.
Seven primitive groups render in reverse authored group order. Inactive layers
remain enumerated while their draws disappear; zero layer opacity similarly
omits the draw. At 50% opacity, observed byte alpha is 127, not 128. Viewport
changes preserve local geometry but change final geometry and placement.

Injected unknown and merge operators disappear from the parsed graph, leaving
the same surviving nodes/properties. A future native compiler therefore needs
an exhaustive raw-input admission check; a parsed-node allowlist or
`nativeDirect2DReady` alone cannot establish completeness. Frozen render tables
also lack authored path/paint pairing, so compatibility identities must be
proved separately rather than guessed from those tables.

The recommended eventual boundary is a private immutable program in Runtime,
with per-instance evaluation workspace. The existing primitive implementation can
have one compilation owner in Runtime, reached by Rendering through its existing
dependency. No new exported library, Runtime-to-Rendering cycle, public raw-JSON
API, per-frame cache, or reference-free loading claim is justified here.

Before architectural adoption, the next bounded experiment attempts full-field
single-ellipse emission using existing AveMotion evaluation/geometry, with one
reference-assisted preparation and no reference access inside emission. The
fixed matrix covers all frames, forward/reverse/repeated access, square and
nonsquare viewports, and a short active range. Its brief and evidence are in
`out/part25h-native-proof/`. A negative result must preserve exact differences;
it must not weaken the oracle. No product native emitter, speedup, zero-allocation
guarantee, broader feature support, or new fallback policy is delivered by G.
