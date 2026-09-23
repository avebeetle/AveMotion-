# Part25B — reference lifecycle foundations

Date: 2026-09-23. Status: bounded implementation, task reviews, measurements and
full Windows gates and independent whole-change review complete.
Continuation base: `273f2b3`. Design/plan: `51bee54`.
General persistent scene/model reuse remains unimplemented; Samsung is not all-green.

## Scope and contract

The user resumed autonomous work for two hours after Part25A, with a new deadline
07:11:24UTC/10:11Moscow. This stage targets an avoidable per-Instance mapping tree,
not the much broader persistent renderTree redesign. Direct2D ownership, Player
threading, visual feature coverage, public fallback policy, vendor sources and
goldens are outside the change.

The existing retained sceneAnimation is used only for normalized frame mapping.
Actual exact scenes already use a separate fresh animation for every sample;
CPU has a separate lazy animation; model preparation samples fresh animations.
The source audit found three mapping-tree uses: declaration, frameAtPosition and
createInstance. It found no additional ordinary immutable-asset validation in the
duplicate load after successful metadata loading.

The chosen ordinary mapping formula uses validated metadata count N and the
pinned variant's rule: Telegram truncation versus Samsung std::round of
normalizedPosition*(N-1). Instance already maps any nonfinite input to zero and
clamps finite input to[0,1]. Root start offsets cancel. Telegram truncates JSON
root endpoints to long; Samsung rounds them; metadata N retains that distinction.

For counts above LONG_MAX the old load/mapping path is retained. Telegram can
accept reversed root ranges and produce a huge unsigned frame count. This stage
neither silently rejects these inputs nor claims portable behavior for the old
out-of-range floating-to-integer conversion at positive positions. The testable
contract for that case is acceptance, old session selection, and position0.

Ordinary setup Scene-session count intentionally changes1→0. Omitting the second
load also removes its construction-failure opportunity; this is not a claim that
all failure timing is identical. The public error category and legacy outlier
branch remain. The six diagnostics remain cumulative successful constructions and
samples, not live-object gauges or allocation counts.

Source audit:out/part25b-frame-mapping/audit.md. Binding design:
superpowers/specs/2026-09-23-reference-lifecycle-foundations-design.md.

## New active-state evidence

Both pinned variants were tested with ten small self-authored128x128 JSON inputs,
using immediate owned scene copies and a fresh animation oracle. Each was sampled
0→1→1→0→1. Each variant produced50 comparisons and21 intentional mismatches:
seven reproductions fail the three frame1 visits, while three negative controls
pass all visits. This is MSVC Debug scene evidence, not CPU-pixel, Release, or
corpus-frequency evidence.

| Case | Fresh result versus retained result at frame1 |
|---|---|
|Translation1→1.0000005|Path points2/3 x:1.000000477 versus1.|
|Same translation plus independently changing opacity|Same stale geometry despite content update.|
|Translation0.0000012→0.0000005|Path points2/3 x:0 versus1.200000042e-6; Telegram matrix dx also differs.|
|Opacity50.19605→50.19610 percent|Telegram paint alpha128 versus127; Samsung active layer opacity128/255 versus127/255.|
|Stroke width2→2.0000005|Width1.999995351 versus1.999994874 in these builds.|
|Dash2→2.0000005 plus independently changing geometry|Dashed path coordinate differs by the same small width-scale boundary.|
|Simultaneous trim37→37.00005 percent plus geometry change|Interior endpoint x20.55992126 versus20.55999756.|

Passing controls are translation above tolerance, constant dash with animated
geometry, and trim50→50.00005 at a corner. The last remains a negative control;
the experiment does not establish which downstream cancellation hides it.

The common cause is comparison against retained previous values using absolute
tolerance1e-6. Fresh objects compare against constructor defaults. Setting dirty
flags does not itself bypass assignment early returns. An actual linked
VDrawable setter experiment on each variant retains width/dash2 after
DirtyState::All, while resetting the typed comparison payload to its constructor
baseline permits2.000000477. This is a scratch boundary experiment, not a general
renderer reset API or a proof that all other state has been identified.

The near-default translation case also shows why unconditional assignment is not
equivalent to fresh evaluation: fresh keeps identity (zero), not the newly
authored0.0000005. A future persistent design must reproduce fresh default-based
decisions, not just force every setter or flag dirty.

Selected permanent regression inputs:translation-near-default.json,width.json,
opacity.json under tests/fixtures/reference_sessions. The controller independently
reran just these three scratch cases:15 comparisons,9 mismatches on each variant,
expected exit1. Evidence:out/part25b-active-state/controller-selected-telegram.log
and controller-selected-samsung.log. Permanent-test status is recorded below.

Raw full evidence and exact reproduction commands:
out/part25b-active-state/findings.md,telegram.log,samsung.log,probe.cpp,
ActiveSceneComparison.hpp and build.cmd. The full comparator retains its approved
identity/history exclusions; this is not an epsilon or fingerprint-only check.

## Implementation, tests and reviews

Task1 implementation:d4217a46487600c1b4a933fa497eb297b32c4bc5.
The new frame-mapping target ran RED on both original variants after completing
the independent parity/edge cases: ordinary creation still made one Scene-role
animation instead of zero. It ran GREEN after the minimal fast-path change.
Coverage includes16 JSON inputs, dense positions and nextafter frame/tie
boundaries, fractional/negative/nonzero root endpoints, one-frame inputs,
nonfinite/out-of-range positions, moves, separate instances, playback snapshots,
quiescent diagnostic resets, null/foreign assets and the retained outlier path.

Worker full CTest:Telegram61/61; Samsung39/41 with the same two baseline hashes.
The controller independently rebuilt and ran frame_mapping,seams,
reference_sessions,asset_model:4/4 on each variant (22.57s and13.10s), exit0.
Historical Part25A reports validate with explicit setup count1; current smoke
validates with default0; wrong count and negative option both fail as intended.
Independent Task1 code review approved spec/quality. It requested complete Samsung
rows instead of abbreviated report values; an evidence-only correction with a
fresh two-golden rerun passed separate scoped review. The controller and reviewer
each compared all four complete rows equal to the saved baseline. Original worker
RED/full-suite output was observed in its terminal but not saved as standalone
logs; the report discloses that and distinguishes the fresh focused evidence.
Raw controller logs:
out/part25b-final/task1-controller-*-d4217a4.txt; full worker RED/GREEN record:
.superpowers/sdd/2026-09-23-reference-lifecycle-foundations/task-1-report.md.

Task2 permanent regressions committed at58236a9fec8e55de136ed319841c47ad690231c9;
independent task review approved spec/quality with no Critical/Important findings.
The three nested fixture bytes match the scratch
sources and indexed Git blobs. The retained-session probe against the copied
files still produces15/9 on each variant; permanent fresh-path tests pass.
Controller rebuilt/reran reference_sessions under the official Telegram and Samsung
presets:1/1 each (16.96s and12.26s). Controller's strict corpus check verifies16
assets and SHA256 checks confirm source/copy identity. The stale CPU-isolation
PASS text now correctly reports2Scene sessions. Full comparator/assertions remain.
Task2 raw logs:out/part25b-active-state/task2-{red,green,direct}-*.log;
controller logs:out/part25b-final/task2-controller-*-58236a9.txt.
### Full Windows verification

The controller freshly configured, built and ran complete CTest suites at
`58236a9` using the installed VS2022 BuildTools x64 environment:

| Preset | Configure/build | CTest | Duration |
|---|---|---|---:|
|windows-msvc-telegram-debug|PASS|61/61|79.13s|
|windows-msvc-samsung-debug|PASS|39/41, two baseline golden failures|67.40s|
|windows-msvc-win32-preview|PASS|55/55|48.20s|
|windows-msvc-direct2d|PASS|29/29|22.68s|

The explicit preview preset passes `avemotion.direct2d.capture`,
`avemotion.direct2d.capture_preflight` and `avemotion.win32.preview.selftest`
(hidden WARP/device-recreation path), not merely the general debug tests.
The no-reference install to a fresh local output prefix and external
`tests/consumer` find_package configure/build/run passed, exit 0. An additional
optimized Release mapping test passed 1/1 in 0.09s.

Each full gate used the following commands after
`VsDevCmd.bat -arch=x64 -host_arch=x64`, with the preset from the table:

```text
cmake --preset <preset>
cmake --build --preset <preset> --parallel 4
ctest --preset <preset> --parallel 4 --no-tests=error --output-on-failure
```

Install/consumer commands (prefix and build directories are new local paths):

```text
cmake --install out/build/windows-msvc-direct2d --prefix out/part25b-final/install-58236a9
cmake -S tests/consumer -B out/part25b-final/consumer-58236a9 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part25b-final/install-58236a9
cmake --build out/part25b-final/consumer-58236a9 --parallel 4
out/part25b-final/consumer-58236a9/avemotion_installed_consumer.exe
```

Complete outputs are retained at `out/part25b-final/*-58236a9.txt`, including
installed-consumer and release-frame-mapping logs. All full-gate test failures
remain visible; no test was excluded. The final whole-change reviewer independently
checked these logs and all four Samsung baseline rows, without rerunning suites.

### Final independent review

The whole-change review over `273f2b3..d7133ac` found no Critical or Important
defects and approved the bounded result. It also independently reaggregated all
four timing reports and 24 memory reports and checked both executable hashes.
Its only Minor finding was stale future-tense wording in STATE; docs-only cleanup
`59592fa` passed a separate scoped review. No product fix was requested. The existing vendor
C4251 warning remains a disclosed nonblocking limitation.

Delivery commits: `d4217a4` (mapping optimization), `58236a9` (regressions),
`d7133ac` (measurement/full-gate report), `59592fa` (reviewed state cleanup).
The subsequent closeout commits only record verified status; production/tests
remain exactly the state exercised by the complete `58236a9` gates. The remaining
vendor warning and Samsung baseline failures are disclosed limitations, not open
new-code review defects. The bounded scope finished before the two-hour deadline;
the scheduler is being paused at handoff instead of repeating completed work.

Known pre-existing limitation: Samsung scene.golden and plan.golden disagree with
stored Polystar endpoint hashes on this MSVC runtime. Part25A reproduced the exact
same failures at clean baseline cda415c. No goldens or allowances are changed.
The current full Samsung result must still be reported explicitly, never called
all-green based on focused tests.
The controller compared all four complete expected/actual Samsung rows from this
new full run against the saved pre-task baseline: exact match. Changed fields in
the failing Polystar p100/frame150 case remain:

| Field | Stored expected | MSVC actual |
|---|---|---|
|Scene fingerprint|0cca4a4d4395c248|d16b938d33790784|
|Geometry fingerprint|c52135375f4b4fd9|4c0fb2275b3068ab|
|Plan fingerprint|4e7835d97cd1de98|43cc77e5cb56f0e1|
|Plan geometry identity|f392889bad747440|4992a725d8c4bd2c|
|Plan presentation|83bafa6dc131d21b|38cbdf9a188f397b|
The existing pinned Telegram rlottie.h C4251 DLL-interface warning remains in
MSVC builds; it is not hidden, promoted to a new failure, or fixed by vendor edits.

## Measurement protocol and results

Baseline executable preserved before candidate rebuild:
out/benchmarks/part25b/baseline/avemotion_corpus_lab.exe.
SHA256:614714992fbacd80c1714683080811b66f5192aa4fd8ed8cd70ab42cea514f3b.
It was built at corrected instrument commit c484f4b. The controller verified no
src/include/apps/corpus_lab change from that commit to continuation base273f2b3.

Planned fresh-process order:A1,B1,B2,A2, identical Release corpus instrument,
all16 deterministic assets,1000 measured samples,20 warm-up samples,load-repeats1,
CPU-repeats0,128square viewport, strict validation. Baseline setup count1 and
candidate0 are validated explicitly. Fresh-process working-set observations use
1/16/64 instances for StickAndBall and firework in the same A-B-B-A order.
No agent builds/tests/probes may overlap the timings.
There is also one separate first frame before warm-up. The unchanged instrument's
percentiles contain that first sample plus the1000 steady samples; warm-up samples
are excluded. Per-phase diagnostics keep the first and steady counts separate.

Measurements completed at05:50UTC, using source state58236a9 with docs-only working
changes. Candidate binary was built atd4217a4; its SHA256 is
be9b98b42fd0f610665163da3c676316bc14725a6cdf862c87ee2208f418da69.
All four runs validated16 assets and their manifests were byte-identical. Setup
Scene sessions were exactly1 for baseline and0 for candidate; first sample1,
steady1000, and model constructions equal model samples in every run. All observed
evaluator/projector retained bytes and storage generations stayed unchanged from
prepare through warm-up and measured phases. Planner allocations are not measured.

| Run | Median of16 exact medians(ns) | Median of16 exact p95s(ns) | Median of16 pipeline medians(ns) | Median of16 pipeline p95s(ns) |
|---|---:|---:|---:|---:|
|A1|21550|32350|34300|53550|
|B1|20600|32250|32900|50700|
|B2|20700|33000|33050|51150|
|A2|20650|36100|33800|58100|

Mean of the two run-level summaries:A exact21100ns versus B20650ns(-2.13%);
pipeline34050ns versus32975ns(-3.16%). These are descriptive observations of one
A-B-B-A sequence, not pooled distributions or a statistically established
improvement. The code deliberately leaves fresh per-frame construction unchanged;
do not market the small timing differences as a steady-state speedup.

Working-set observations (bytes; order within each cell is A1/B1/B2/A2):

| Asset | Instances | Current bytes | Peak bytes | Mean B-A current bytes |
|---|---:|---|---|---:|
|StickAndBall|1|4542464/4542464/4550656/4538368|4546560/4546560/4554752/4542464|+6144|
|StickAndBall|16|4599808/4542464/4538368/4612096|4603904/4546560/4542464/4616192|-65536|
|StickAndBall|64|4759552/4550656/4542464/4755456|4763648/4554752/4546560/4759552|-210944|
|1667-firework|1|24961024/23969792/24829952/24424448|26357760/24027136/24834048/25468928|-292864|
|1667-firework|16|26648576/24649728/23855104/27430912|26652672/25276416/23859200/27435008|-2787328|
|1667-firework|64|34598912/21856256/24829952/35790848|34603008/23789568/24834048/35794944|-11851776|

At64 instances, mean observed reduction is206KiB for StickAndBall and about11.30MiB
for firework. The one-instance StickAndBall comparison instead rises6KiB, showing
the fixed/noisy process background is not a per-object allocation measurement.
Raw per-asset median/p95 data, all memory observations, input hashes, binary/compiler
context, orchestration and analysis scripts are retained under
out/benchmarks/part25b. summary.md lists every per-asset percentile in run order.

The four timing processes use the same CLI arguments, changing only executable
and a new output directory:

```text
<baseline-or-candidate.exe> --input tests/compatibility/tgs --output <new-run-dir> --samples 1000 --warmup-samples 20 --load-repeats 1 --cpu-repeats 0 --render-size 128 --strict
python scripts/test_corpus_lab_output.py --output <new-run-dir> --expected-assets 16 --expected-samples 1000 --expected-warmup-samples 20 --expected-render-size 128 --expected-setup-scene-sessions <1-for-A-or-0-for-B>
<baseline-or-candidate.exe> --input tests/compatibility/tgs/<asset>.tgs --output <new-memory-dir> --memory-instances <1-or-16-or-64> --render-size 128 --strict
```

The controller confirmed no active build/test/probe process before starting and
ran no builds/tests/probes until all timing and memory processes finished. Four
timing validations and all24 memory-report validations passed. These are synthetic
public corpus observations, not measurements of a representative private corpus.

The proven structural reduction is one setup mapping tree per ordinary instance,
not fewer exact-scene sessions. No2x frame speedup, representative
private-corpus performance, zero-allocation guarantee, or TSan result is claimed.
The instrument's load_model_median_us measures asset load/model preparation and
excludes createInstance; it must not be presented as instance-creation timing.
Working set is measured after model preparation and one frame per live Instance,
not a precise sum of C++ object allocations. Synthetic per_instance_bytes estimates
exclude upstream retained trees and need not change when this optimization lands.

## Decisions and limitations

1. Renewed delegation approves this reversible bounded stage on main. Wrong
   prioritization costs reviewable corrective commits and time.
2. Normal creation drops the redundant load/failure opportunity and setup counter;
   large counts preserve the legacy path. Wrong mapping assumptions require a
   corrective commit; differential boundary and outlier tests are required.
3. Persistent scene/model reuse stays disabled; three distinct active-state
   regressions replace a speculative general reset fix. The cost is deferred
   per-frame speedup.
4. Raw SDD evidence is retained and known Samsung failures are not relaxed. Costs
   are scratch disk usage and an explicitly incomplete Samsung validation result.
5. Accept the final review's remaining unclaimed evidence areas instead of
   expanding closeout. Costs are the unchanged vendor warning, limited historical
   auditability and no assurance beyond the tested scope: original worker terminal
   RED/full-suite output has no standalone raw log; current complete controller
   gates do. No pixel-equivalence, cross-platform/Release probe characterization,
   race-detector, allocation, representative-corpus or statistical-speedup claim
   is added.

Reviewer behaviors set aside, all explicitly accepted by the controller:
persistent reuse/reset (decision3); preservation of the removed ordinary load and
counter (decision2); portable positive reversed-timeline/invalid-endpoint behavior
(decision2); Samsung repair (decision4); vendor-warning repair and new graphics,
fallback, threading or feature changes (outside this bounded stage); unproved
pixel/platform/race/allocation/performance claims and unavailable standalone old
logs (decision5). Nothing here silently converts those areas into verified work.

Next substantial stage remains a separately designed fresh-equivalent recording
lifecycle, covering topology, active/inactive subtrees, modifiers, comparison
defaults, publication, resources and per-instance ownership together. This report
does not authorize or declare that broader change complete.
