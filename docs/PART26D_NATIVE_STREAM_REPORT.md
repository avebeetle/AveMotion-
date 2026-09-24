# Part26D — private native ellipse scene stream

2026-09-24. Bounded private stage accepted after task reviews, one Task2 fix round,
fresh final gates and independent whole-stage review. No blocking finding remains.
Stage base6d154ca; stream product1dd5932; lifecycle tests e534a1e/fixea9facf.
Reviewed task handoff5964c23 ordinarily pushed with exact remote equality verified.
This final report seal changes documentation only, not the verified product tree.

## Delivered boundary

The private Rendering stream generates model-applied scenes for the certified
ellipse subset with our PropertyEvaluator and primitive generator. It retains a
const certificate/model, its own evaluation workspace and successful history;
it does not retain or call a reference Runtime, Instance, Animation, tree or
per-frame oracle. Returned vectors own their bytes. Canonical resources alias the
actual frozen model's records/control block. Distinct streams keep distinct
animated geometry identities and history while static asset resources can share.

Invalid viewports consume an attempt ordinal but publish no scene/history; later
successes compare only with previous successful output. Requested frames clamp
to the final frame. Generated transforms/path values are checked for finiteness.
The API remains private and Telegram-only: no ordinary production route selects it.
It is not a registered Runtime instance and honestly exposes an invalid instance
handle plus an explicit caller-supplied private identity.

Cold preparation still depends on Telegram. The C certificate factory parses,
builds a frozen model and scans the complete source timeline. D removes reference
work from its own per-frame scene emission, not from loading or complete playback.
No own raster, full Lottie support, production UI activation or speedup is claimed.

## Evidence and comparison contract

All15 named inputs in the plan passed certification. The scene matrix performs
7,920 exact scene/RecordingBackend comparisons: for each input, four viewports
(512x512,256x256,384x256,256x384),61 forward frames,61 reverse frames,8 repeated/
boundary requests, and8 additional alternating-viewport requests. Inputs include
activity boundaries, static offscreen placement, edge crossing, fractional values,
nonlinear easing, equivalent decimal spellings, and tiny collapsing/surviving paths.

The expected source is a cache-disabled parse with its own stamped source lease
and independently frozen model. Each expected request creates a fresh ordinary
Animation. Only expected scene asset/instance identity and attempt ordinal are
normalized; geometry/model/history are not replaced by candidate output. The
oracle reports12,495 direct samples and150 parses for this matrix, separately
from native-emission work. These tests establish the named requests, not arbitrary
numeric inputs, viewports or every admitted file.

Lifecycle tests compare60 ordered scenes and420 full presentation plans across
four assets, plus four failed-viewport/clamp follow-ups. The explicit comparator
checks every declared RenderPlan field, not just hashes: stamps, source scenes,
draws/cache keys, updates, bounds, statistics, fingerprints and history. Fourteen
real-plan mutation categories prove detection. History covers repeat/reverse,
disappearance/reappearance, viewport changes, presentation translation/opacity/
visibility/layout revision, stale sequence rejection and failure recovery.

Model control blocks are deliberately independent in expected versus candidate
results; each canonical alias is separately checked against its own actual record
and owner. Scene identity/sequence/change flags are checked explicitly. The single
documented upstream-history exclusion is upstreamChangeBits: native output is
SceneChangeNone rather than pretending to reproduce vendor-internal history bits.

Two threads own separate streams/planners,128 requests each, and their256 results
match sequential expectations. Lifetime checks retain scenes/plans/canonical
aliases after factory Runtime, external owners and stream destruction, verifying
weak-owner expiry only after the final intentional lease is released. This is
bounded functional concurrency coverage, not TSan or proof of absence of all races.

Actual diagnostics on the retained factory Runtime show cold preparation of61
model samples plus61 scene samples and no CPU sessions. In the native-only
create/61-emission window, all36 DiagnosticsSnapshot fields have zero delta.
This is measured absence of reference work in that window, not zero allocations,
zero CPU cost or a speed comparison. Ordinary CPU pixel comparisons check
interference at frames0,30,60; they do not test a native pixel renderer.

## Reviews and retained evidence

Task1 independent spec/quality review accepted1dd5932. Task2's first review found
that pre-advance retained copies shared model/canonical pointees, so simultaneous
mutation could evade the immutability assertion. The accepted fix uses independent
expected values and demonstrates corruption detection. The controller also
strengthened the CPU oracle requirement to a cache-disabled distinct source;
two Runtime wrappers alone can share the identical-byte source cache.

Raw TDD/evidence is retained under out/part26d/task1 and task2; reports/reviews
under .superpowers/sdd/2026-09-24-native-ellipse-stream. Task1 functional RED:
native stream creation was absent after a successful build. Task2 functional RED:
the initial comparator failed to detect a changed stamp. Initial activity-test
setup incorrectly assumed frame0 had a populated draw; the setup was corrected
without weakening production behavior or the comparator. Those logs remain.

Pre-fix root configure/build/full CTest on e534a1e passed Telegram74/74 (98.71s),
Win32 preview68/68 (103.70s), none/Direct2D31/31 (3.90s), all-vendor and TGS16.
Twenty logs were copied and hash-verified into out/part26d/pre-fix-e534a1e before
reruns. Passing those suites did not substitute for the uncovered immutability
requirement. Final-code results on ea9facf are below.

Fresh root commands, under VsDevCmd -arch=x64: cmake --preset PRESET;
cmake --build --preset PRESET --parallel 4;
ctest --preset PRESET --output-on-failure. Preserved runner scripts are
out/part26d/run-preset-gate.cmd and provenance-gate.cmd.

| Final preset | Full CTest result |
| --- | --- |
| windows-msvc-telegram-debug |74/74,109.09s|
| windows-msvc-win32-preview |68/68,109.83s|
| windows-msvc-direct2d (none) |31/31,4.29s|

Preview includes capture/WARP/device recreation and the Win32 preview selftest;
those are existing regression gates, not newly proven native pixels. All configure/
build exits are0. Vendor/TGS commands also pass: python scripts/verify_vendor.py
--variant all; python scripts/generate_tgs_compatibility_corpus.py --check (16assets).
Final fresh configure graphs/hashes are in final-private-boundaries.json: none40
and Samsung92 translation units with zero D sources/tests; Telegram116 units with
one stream in Rendering, two tests, one primitive generator and no Runtime cycle.
None has no vendor reference dependency. No private header enters installed include.
The Samsung check is configure/graph evidence only, not an all-green Samsung suite;
historical Polystar failures remain separate. Protected vendor/fixture/golden/public
Runtime/Player/backend diff is empty.

Fixea9facf contains tests only. Functional snapshot-corruption and shared-CPU-source
REDs each compiled before failing; focused GREEN is2/2. Independent scoped review
accepts both with no new findings. Independent value-owned snapshots detect model,
canonical paint and canonical geometry corruption; four retained plans remain
stable. Cache-disabled ordinary CPU expected buffers are all captured before
native work, with actual source pointer separation proved. Six oracle pixel and
three retained-primary comparisons include dimensions/stride. Full fix1 evidence
and reviews remain in the task workspace/raw directory.

Whole-stage /root/native_stream_whole_stage_review accepted6d154ca..ea9facf with
zero Critical/Important findings and no final fix wave. Existing test-only C4251
from rlottie.h is explicitly accepted: no observed link/ownership/runtime defect,
no D vendor class export change, and warning suppression is unnecessary for this
private proof. The raw diagnostic remains; no warning-free build is claimed.
No vendor edit or global warning-policy change was made.

The graph probe's first overly broad `rlottie` text match counted disabled macros
and none-provenance strings as dependencies. Inspection identified all five false
positives; corrected path/enabled-macro checks find zero vendor dependencies in
none, with Samsung/Telegram positive controls. Initial JSON and the explanation
remain in out/part26d/private-graph-instrument-note.md. No product change was needed.

## Decisions made under delegated authority

1. Keep D private/reference-assisted preparation plus own emission: separates
   geometry/history proof from ingress/timeline/raster. If wrong, the temporary
   private API costs later integration rework.
2. Derive the oracle descriptor/handle from a cache-disabled seed rather than an
   extra Runtime Asset: avoids shared candidate source. If wrong, its metadata
   recipe could diverge; full model comparison checks that risk.
3. Work directly in main and retain raw/SDD evidence per user choice: scoped
   append-only commits and remote guards replace branch isolation. Costs are
   scratch retention and less isolation, not permission to rewrite history.
4. Require a cache-disabled distinct CPU expected source, not only wrappers:
   pre-native expected pixels already prove sampled frame stability, but shared
   source could hide between-frame interference. Cost is a test-only parse and
   identity check, with no production API change.
5. Accept the retained test-only vendor C4251 warning after independent triage:
   no associated defect was observed and suppressing it adds no proof. If wrong,
   a later ABI-boundary investigation is needed; no warning-free build is promised.

The final review's eight declined-scope items are resolved explicitly: own ingress/
model, generic/exhaustive feature coverage, native pixels and production/UI route
are future stages; Samsung all-green, TSan/single-stream concurrency/performance
and upstream-history/ID-origin reproduction remain unclaimed limitations. Final
documentation/remote integration is controller-owned closure, not omitted product
work. No declined item silently widens D acceptance.

## Continuation and limits

Next: separately designed first-party bounded input reader, own model/IDs/static
resources, reference-neutral timeline, native Direct2D/WARP pixel validation and
isolated static Avelabs Motion Lab integration. Parser characterization exposes
legacy numeric and Unicode quirks; a replacement must explicitly resolve them
instead of silently claiming complete compatibility from a finite test matrix.

Avelabs remains the final host at D:/rvc/c++/DragonianVoice/Avelabs-UI. This stage
does not edit/build/run it or recreate UI/out. Its existing Motion Lab is still
reference CPU, and accepted build/Release is not replaced. No separate AveMotion
DLL, automation, dependency installation or Windows change is introduced.
