# Part25B — reference lifecycle foundations

Status: implementation and verification in progress,2026-09-23.
Continuation base:273f2b3. Design/plan:51bee54. This report does not claim a
completed optimization until the verification and measurement sections are filled.

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

Pending Task2 permanent regressions and independent review.
Pending final whole-change review and fresh complete gates.

Known pre-existing limitation: Samsung scene.golden and plan.golden disagree with
stored Polystar endpoint hashes on this MSVC runtime. Part25A reproduced the exact
same failures at clean baseline cda415c. No goldens or allowances are changed.
The current full Samsung result must still be reported explicitly, never called
all-green based on focused tests.
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

Results pending. The expected structural reduction is one setup mapping tree per
ordinary instance, not fewer exact-scene sessions. No2x frame speedup, representative
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

Next substantial stage remains a separately designed fresh-equivalent recording
lifecycle, covering topology, active/inactive subtrees, modifiers, comparison
defaults, publication, resources and per-instance ownership together. This report
does not authorize or declare that broader change complete.
