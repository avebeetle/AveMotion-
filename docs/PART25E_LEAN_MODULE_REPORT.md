# Part25E — lean module and optional comparison boundary

Part25E adds two opt-in Release build surfaces. The Windows Telegram module is
an internal build-tree integration: nine AveMotion product static libraries plus
the selected Telegram `rlottie` archive, with Direct2D enabled. The offline
package installs the same nine AveMotion product libraries and public headers,
with no reference engine. Its Runtime returns `ReferenceUnavailable` for valid
Lottie JSON; this package cannot load and evaluate animations. Neither surface
is a single DLL, a stable ABI, a performance result, or redistribution approval.

The package exports `AveMotion::Formats`, `Core`, `Model`, `Validation`,
`Evaluation`, `Runtime`, `Player`, `Rendering`, and Windows `Direct2D`. The
laboratory `AveMotion::Reference` target and headers remain in source/build-tree
workflows but are excluded from default lean output and installation. Existing
experimental installed Reference consumers must use that source/build-tree lab
target. Telegram-backed installation retains its fatal guard. Existing developer
presets remain available; four new module/package presets cover Windows and
Linux. The source geometry characterizer defaults on for developer builds and
can be disabled without losing its independent unit test.

The normal CI workflow retains Telegram correctness, no-reference, graphics,
capture and preview gates and adds lean output/package jobs. Samsung source,
presets, patches, notices and full checks remain available in a separate manual
comparison workflow; no comparison was dispatched for this stage and the known
Samsung Polystar golden failures remain visible if it runs.

## Evidence at handoff

StageBASE is `de4d18f49af8952a5b0250edf96d3508cb704aea`; frozen product code
is `d567d75c903481ffc18db341b8a56f0121578f91`; fresh Windows boundary gates
ran at `cb84046ab8f9877bda93d1aae91a5722e1f38abe`. Unique raw build,
install, prefix, consumer, negative-install, registration, vendor and 661-file
protected-byte evidence is under `out/part25e/task3-final-20260923T113732Z/`;
the exact commands and native exits are recorded in the ignored
`.superpowers/sdd/2026-09-23-lean-module-boundaries/task-3-report.md`.

Fresh Windows module and offline builds and artifact verifiers passed. The
offline prefix has exactly nine archives, 18 headers and four CMake package
files; original and relocated prefix verifiers and Release external consumers
passed. The consumer asserts `ReferenceUnavailable`. The Telegram install
negative configure failed for the intended guard. With tests on and the source
characterizer off, its unit test passed 1/1 and the two characterizer CTests
were absent; the default developer configure registered them. Both vendor
fingerprints and the committed corpus verified, and protected vendor, patch,
notice, corpus and golden bytes matched stageBASE. The frozen product full
Windows suites passed Telegram Debug 68/68, explicit Win32 preview 62/62
(capture 75/75, WARP self-test), and no-reference Direct2D 31/31. Existing
upstream compiler warnings remain.

Actual [CI run 35856219338](https://github.com/avebeetle/AveMotion-/actions/runs/35856219338)
at `cb84046` passed Linux module, Linux offline package and Windows module
jobs. Its Windows offline consumer job failed with a Debug/Release MSVC link
mismatch because the workflow's consumer configure omitted
`CMAKE_BUILD_TYPE=Release`. Scoped correction `514f7ab` makes all four lean CI
consumer commands and both documented commands explicit Release. Its local
RED run reproduced the MSVC mismatch; fresh original and relocated Release
consumers linked and ran. Independent scoped review accepted the correction.
The subsequent ordinary-push [CI run 35857070290](https://github.com/avebeetle/AveMotion-/actions/runs/35857070290)
at `514f7ab` passed all four new lean module/package jobs, including both
original and relocated Windows consumers. Earlier cloud golden failures are documented in
[Part25D's report](PART25D_PERSISTENT_SESSIONS_REPORT.md) and were not changed.
No Linux/cloud all-green conclusion follows from the local Windows gates.

Independent whole-stage review of `de4d18f..30bb67c` approved spec compliance
and code quality with no Critical or Important findings. It corroborated the
four successful lean CI jobs and zero raw-byte changes in 661 protected files.
One nonblocking test-maintenance minor remains: an inert legacy `MANIFEST` mock
in the vendor-selection test. Part25E is accepted; this is not a claim that the
whole cloud workflow is green. Review and task evidence are retained in
`.superpowers/sdd/2026-09-23-lean-module-boundaries/`.

Native topology, full fallback policy, host MotionService and
a redistributable runtime require separate design and evidence; this stage
makes no licensing decision.
