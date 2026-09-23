# Part 25A: reference-session evidence and handoff

Date: 2026-09-23. **Status: safe partial delivery; full Windows verification
and independent whole-change review complete. Persistent scene/model reuse is
not implemented. Samsung's full suite is not green.**

## Outcome and boundary

The current fresh-per-sample boundary is retained. Direct reuse fails both seek
and ascending-scan parity on both pinned variants. See
[correctness findings](PART25A_REFERENCE_SESSION_FINDINGS.md).
No zero-session, 2x speedup, general zero-allocation or Telegram-class performance
claim is made. There is no optimized candidate in this run.

Diagnostics and the common schema-2 instrument are implemented. Access-order,
viewport, CPU isolation and separate-instance regression tests passed independent
review at `7b33c03`; model-retry tests passed review at `4ab196d`. The fixture
integration correction at `b3bb269` also passed scoped review.
No vendor/golden/dependency, Direct2D ownership, Player threading or public
fallback-policy change was made.

## Actual lifetime and threading contract

- Successful asset validation uses a short-lived metadata session. Creating an
  Instance retains one eager scene session for normalized-position frame mapping;
  it is not reused by exact sampling in this delivery.
- Each exact scene sample constructs its own scene-role reference session,
  copies the complete tree into owned scene values and releases that temporary
  session. The underlying parsed-model cache remains as before.
- Successful eligible Telegram model preparation samples each source frame with
  a fresh model-role session, under the existing asset model mutex, then publishes
  an immutable model. Later successful preparation calls return that model.
- An Instance creates its separate CPU session lazily and reuses it for later CPU
  renders. Never share this mutable CPU session with exact tree sampling.
- Access one Instance through one serial execution stream, including CPU renders.
  The functional concurrency test gives each of two threads its own Instance and
  prepares the shared asset before starting them. It does not establish race
  freedom or concurrent preparation/evaluation guarantees beyond existing APIs.
- Diagnostic counters are relaxed-atomic cumulative observations. Reset starts a
  new epoch, not new sessions; exact-count/reset tests use quiescent boundaries.
- Host graphics ownership is unchanged: the host owns D3D11/DXGI/Direct2D context,
  drawing/presentation and scheduling. No new timer, render thread, device or
  swap chain is introduced.

## Scene regression evidence

`7b33c03` adds an explicit complete-field comparator and tests nine fixture
families: dash, animated path, multi-trim, two repeater fixtures, mask, matte,
LoudMute and firework. Both variants compare every valid frame ascending and
reverse against new-Instance oracles, plus repeated non-monotonic seeks and
128x128 / 96x160 / 128x128 viewport changes. Fifteen field/alias mutations verify
that the comparator rejects differences invisible to recorder fingerprints.

A temporary naive-reuse mutation failed at dash frame 1 on both variants with
`scene.drawItems[0].path.verbs.size differs`. The patch and RED logs are saved in
ignored `out/part25a-regression/`; the mutation is absent from committed runtime
code. Restored fresh sampling passes. The controller independently rebuilt and
ran `avemotion.runtime.reference_sessions`: Telegram 1/1 in 16.86 s, Samsung 1/1
in 15.69 s. Independent spec and task-quality review passed.

The initial concurrency test exposed a test-precondition mismatch: the measured
asset was prepared but its oracle was not. Telegram preparation publishes source
IDs to the parsed model. Preparing the oracle identically fixed the mismatch;
source IDs remain compared, with no comparator relaxation.

### Existing Samsung golden failure: diagnosed, comparison unchanged

The focused Telegram gate passed 5/5. Samsung passed the new reference-session,
runtime seams, playback and Player checks, but its existing scene golden rejects
Polystar p100 / frame150 / 128x128. A separately built clean `cda415c` checkout
with the same MSVC toolchain fails the identical row, so this predates Part 25A.

| Field | Golden | Current MSVC and clean baseline |
| --- | --- | --- |
| Scene fingerprint | `0cca4a4d4395c248` | `d16b938d33790784` |
| Geometry fingerprint | `c52135375f4b4fd9` | `4c0fb2275b3068ab` |

Topology, paint, counts and other row fields match. The existing narrow variance
policy allows these hash pairs only for Telegram frame149, not Samsung frame150.
No golden or acceptance policy was changed; full Samsung success is not claimed.
The isolated numerical probe reproduces a sine rounding boundary: MSVC `sinf`
returns float `3f2b4c23`, while the diagnostic reconstruction of glibc's sine
polynomial returns `3f2b4c24` for the same angle bits `3f3ba864`. The existing
transform turns that one-ULP sine difference into a two-ULP displacement of one
rendered x coordinate: 20.3137626648 versus 20.3137588501, approximately
0.000003814697 pixels. Recomputing its path/scene left bounds recovers both
exact golden hashes. Full Trim forwards this endpoint unchanged; removing it
from diagnostic in-memory JSON preserves the failing hashes.

The controller ran the probe independently: all 32 directly constructed points
matched the clean runtime, and the polynomial-derived point plus bounds matched
both goldens. No Linux process or original golden-authoring glibc binary was run;
this is source-polynomial reconstruction, not a new Linux baseline. It does not
establish universal numerical or rendered-image equivalence.

Raw evidence: `out/part25a-samsung-golden/findings.md`,
`clean-baseline-build-test.txt`, `probe.cpp`, `probe-final.txt` and
`reproduce-probe.ps1`. Deterministic cross-platform geometry would be a separate
product change requiring its own parity proof; this run neither expands the
captured-hash allowance nor changes frame selection, tolerances or goldens.

The full Samsung suite also fails `avemotion.plan.golden` on the same endpoint.
The clean `cda415c` scratch build reproduces every expected/actual pair below;
the controller independently reran its executable and confirmed exit 1. All
other row fields match. Log: `clean-baseline-plan-build-test.txt` in the same
evidence directory. This establishes baseline provenance, not a separate
coordinate-by-coordinate proof of each plan hash.

| Plan field | Golden | Current MSVC and clean baseline |
| --- | --- | --- |
| Plan | `4e7835d97cd1de98` | `43cc77e5cb56f0e1` |
| Geometry identity | `f392889bad747440` | `4992a725d8c4bd2c` |
| Presentation | `83bafa6dc131d21b` | `38cbdf9a188f397b` |

The clean scratch checkout remains at
`C:/Users/USER/AppData/Local/Temp/avemotion-part25a-samsung-cda415c` for
reproduction. No full baseline Samsung-suite pass is claimed.

## Model preparation regression evidence

`4ab196d` adds real Asset/Instance checks for one fresh model session and sample
per source frame, idempotent publication of the same immutable model, and
separate scene-role counts. A valid long-timeline fixture exercises two failed
preparation attempts with two failure counts and zero sampling/construction.
Samsung retains its existing unsupported-capability result before that limit.

A temporary deletion of the limit-branch failed-counter increment produced RED
at `failed preparation was not retried and counted`. The production source was
restored and is unchanged in the commit. Controller fresh asset-model tests
passed: Telegram 1/1 in 7.49 s, Samsung 1/1 in 0.03 s. Worker focused tests passed
Telegram 8/8; Samsung's four applicable runtime/reference tests passed, while
the separately diagnosed scene-golden failure remained. Independent spec and
quality review passed with no findings.

## Full-gate integration correction

The first full Telegram run at `bbfad2d` passed 59/60, and Win32 preview passed
53/54. Both failed only the strict TGS corpus integrity check: placing the new
dash JSON among top-level fixtures unintentionally made it an input to the
compatibility-corpus generator. The correction at `b3bb269` keeps this
session-only fixture under `tests/fixtures/reference_sessions/` and update its
two explicit test consumers. The generator, 16-asset benchmark corpus, manifests
and goldens remain unchanged. This is a Part 25A integration regression, distinct
from the pre-existing Samsung numerical issue. The fixture bytes are unchanged;
both consumers pass on both variants, scoped review approved the fix, and fresh
full Telegram/preview gates pass. Initial logs are retained in
`out/part25a-final/windows-msvc-telegram-debug.txt` and
`out/part25a-final/windows-msvc-win32-preview.txt`.

## Final functional verification

Controller-run configure, build and complete CTest at product/test commit
`b3bb26957aa98f4a2486efbc0fade6cfb8b76ba4`, using MSVC 19.44.35229.0, then freshly
repeated after the final comment correction at
`7b5221dc9f9ce0793c71f56851fe55701bdf5902`:

| Preset | Configure/build | Complete CTest | Seconds |
| --- | --- | --- | ---: |
| `windows-msvc-telegram-debug` | PASS | 60/60 PASS | 52.43 |
| `windows-msvc-samsung-debug` | PASS | 38/40 PASS; scene and plan golden FAIL | 14.05 |
| `windows-msvc-win32-preview` | PASS | 54/54 PASS | 46.59 |
| `windows-msvc-direct2d` (reference=none) | PASS | 29/29 PASS | 2.47 |

The post-review full rerun produced the **same results without exclusions**:
Telegram60/60 in73.60s, Samsung38/40 in66.08s (the same scene/plan hashes),
preview54/54 in62.28s and no-referenceDirect2D29/29 in22.77s. All configure/build
steps passed. Its raw logs are `out/part25a-final/P-7b5221d.txt`; the four
functional presets ran concurrently, explaining why their elapsed times are not
comparable with isolated runs. No benchmark was collected during these gates.

No failed tests were excluded. The preview preset explicitly enabled Direct2D
capture and Win32 preview: `avemotion.direct2d.capture`, capture preflight,
contract and hidden WARP/device-recreation selftest all passed. The general
Telegram-debug result alone is not the graphics proof. Functional suites include
vendor integrity, unchanged TGS corpus, model/scene/plan/validation goldens,
playback/Player, reference isolation and output-directory safety where applicable.
Concurrent build/test runs were used only for functional gates, never for timing
observations; test elapsed times are not performance measurements.

For each preset P, from an x64/host-x64 VsDevCmd environment:

```text
cmake --preset P
cmake --build --preset P --parallel 4
ctest --preset P --parallel 4 --no-tests=error --output-on-failure
```

Complete logs: `out/part25a-final/P-b3bb269.txt`. The local helper
`out/part25a-final/run-gate.ps1` records the SHA and exact environment command.
`git diff --check` also passed. Separately, no-reference installation to a new
workspace-local prefix and the existing external `find_package(AveMotion 0.24.0)`
consumer configured, compiled, linked and ran with exit 0:

```text
cmake --install out/build/windows-msvc-direct2d --prefix out/part25a-final/install-b3bb269
cmake -S tests/consumer -B out/part25a-final/consumer-b3bb269 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Users/USER/Desktop/AveMotion-CorpusLab-Part24/out/part25a-final/install-b3bb269
cmake --build out/part25a-final/consumer-b3bb269 --parallel 4
out\part25a-final\consumer-b3bb269\avemotion_installed_consumer.exe
```

Log: `out/part25a-final/installed-consumer-b3bb269.txt`. This is a local package
smoke check, not installation of system dependencies or proof of a general
Lottie-capable standalone package; reference-linked installation remains disabled.

## Reviewed measurement baseline

- Instrument: `c484f4bec5a8a5699bfad194093da7280bd5c614`, following
  `94b08bf`; original source baseline `cda415c`.
- Working checkout at observation: `12aaa14bbad69839c7b32222de083ba28094b561`
  (documentation-only after instrument); prebuilt Release executable from
  `c484f4b`. No concurrent AveMotion build/test or mutation during observations.
- Binary SHA-256:
  `614714992fbacd80c1714683080811b66f5192aa4fd8ed8cd70ab42cea514f3b`.
- UTC observations: approximately 03:50–03:51.
- Windows 10 Pro 10.0.19045; AMD Ryzen 5 5500; 12 logical processors;
  34,223,529,984 physical-memory bytes.
- MSVC 19.44.35229.0, x64 toolset 14.44.35207, CMake preset
  `windows-msvc-corpus-lab`, Release, Telegram pinned reference
  `67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d`.
- The 16 committed TGS inputs use `tests/compatibility/tgs/SHA256SUMS.txt`.
  Aliases below are the report aliases, not private source names.
- Original observations at `94b08bf` are superseded: they measured the first
  sample after warm-up. Only corrected-instrument observations are used below.

Two **repeat-baseline** processes were run, not A/B optimization comparisons.
Each asset prepares its model and instance, measures first frame 0, warms 20
cyclic frames, then measures 1,000 cyclic frames. Median and nearest-rank p95 use
first + measured samples (1,001), excluding warm-up. Exact time comes from the
scene-evaluation diagnostic duration delta; full pipeline includes evaluator,
exact/model scene evaluation, projector and planner. Diagnostics snapshots are
outside that wall-clock interval. These are sequential per-asset CPU measurements,
not Direct2D drawing or simultaneous 64-animation frame times.

Commands, from repository root:

```powershell
# Repeat for A1 and A2, using separate output directories.
out/build/windows-msvc-corpus-lab/avemotion_corpus_lab.exe --input tests/compatibility/tgs --output out/benchmarks/part25a/repeat_baseline_A1 --samples 1000 --warmup-samples 20 --load-repeats 1 --cpu-repeats 0 --render-size 128 --strict
python scripts/test_corpus_lab_output.py --output out/benchmarks/part25a/repeat_baseline_A1 --expected-assets 16 --expected-samples 1000 --expected-warmup-samples 20 --expected-render-size 128
# Fresh process for each N=1,16,64 and each run A1,A2.
out/build/windows-msvc-corpus-lab/avemotion_corpus_lab.exe --input tests/compatibility/tgs/StickAndBall.tgs --output out/benchmarks/part25a/memory_A1_1 --memory-instances 1 --strict
```

Both timing processes returned 16/16 assets `ok`; both schema validators passed.
All six memory processes exited successfully. Raw reports remain ignored under
`out/benchmarks/part25a/repeat_baseline_A{1,2}/` and
`out/benchmarks/part25a/memory_A{1,2}_{1,16,64}/`.
Compiler/machine/hash context also exists in
`out/benchmarks/part25a/MEASUREMENT_BASELINE_REVIEWED.txt`.

## Timing observations

All table values are **nanoseconds**. The last two columns take the conventional
median of the two corresponding per-run statistics, not a pooled percentile.
Across the 16 per-asset two-run medians, the aggregate exact-scene median is
**22,125 ns (22.125 µs)**; aggregate full-pipeline median is
**34,625 ns (34.625 µs)**. Variation between these identical-runtime runs is not
a speedup or regression verdict.

### Exact scene

| Alias | Median A1 / A2 | p95 A1 / A2 | Two-run median | Two-run p95 summary |
| --- | ---: | ---: | ---: | ---: |
| `asset-003fa54a88cc1bdb` | 9700 / 10500 | 16200 / 21800 | 10100 | 19000 |
| `asset-16a00aef96349f58` | 116700 / 112600 | 220300 / 175800 | 114650 | 198050 |
| `asset-17e7a613fa7d22e0` | 28000 / 28000 | 42900 / 49200 | 28000 | 46050 |
| `asset-3d155172fe5ea8d3` | 611900 / 636800 | 1167100 / 1321000 | 624350 | 1244050 |
| `asset-4cada92226eb8bf8` | 36300 / 35400 | 60800 / 56700 | 35850 | 58750 |
| `asset-4dbd87473e13c546` | 5000 / 5100 | 6200 / 8300 | 5050 | 7250 |
| `asset-609a0437d8c89244` | 12600 / 13500 | 34200 / 37400 | 13050 | 35800 |
| `asset-65b1bbb40eb61ba0` | 4700 / 5000 | 10600 / 9000 | 4850 | 9800 |
| `asset-75d25e0d377cea8b` | 19600 / 20000 | 23000 / 32900 | 19800 | 27950 |
| `asset-7b555fd71676acee` | 25100 / 23800 | 55200 / 39200 | 24450 | 47200 |
| `asset-7e275942f7ac350d` | 105600 / 93600 | 214900 / 146100 | 99600 | 180500 |
| `asset-913c68450cabf3e2` | 16100 / 16600 | 18400 / 25600 | 16350 | 22000 |
| `asset-a3d03d5e3fb32d02` | 58300 / 58200 | 86500 / 96100 | 58250 | 91300 |
| `asset-acc55653000a6876` | 9200 / 10100 | 19800 / 21100 | 9650 | 20450 |
| `asset-c8d0c2800f0967bd` | 32300 / 30700 | 80800 / 51000 | 31500 | 65900 |
| `asset-efff3b729013051c` | 6000 / 6100 | 10700 / 8000 | 6050 | 9350 |

### Complete CPU pipeline

| Alias | Median A1 / A2 | p95 A1 / A2 | Two-run median | Two-run p95 summary |
| --- | ---: | ---: | ---: | ---: |
| `asset-003fa54a88cc1bdb` | 15400 / 16400 | 25700 / 32300 | 15900 | 29000 |
| `asset-16a00aef96349f58` | 196600 / 190600 | 345600 / 283200 | 193600 | 314400 |
| `asset-17e7a613fa7d22e0` | 40000 / 40000 | 60800 / 65100 | 40000 | 62950 |
| `asset-3d155172fe5ea8d3` | 630800 / 663200 | 1238800 / 1404300 | 647000 | 1321550 |
| `asset-4cada92226eb8bf8` | 67400 / 66300 | 112900 / 97300 | 66850 | 105100 |
| `asset-4dbd87473e13c546` | 6600 / 6700 | 8100 / 11100 | 6650 | 9600 |
| `asset-609a0437d8c89244` | 19700 / 20700 | 51200 / 57600 | 20200 | 54400 |
| `asset-65b1bbb40eb61ba0` | 8000 / 8300 | 16800 / 14900 | 8150 | 15850 |
| `asset-75d25e0d377cea8b` | 29100 / 29400 | 33500 / 48300 | 29250 | 40900 |
| `asset-7b555fd71676acee` | 42700 / 40700 | 88200 / 58100 | 41700 | 73150 |
| `asset-7e275942f7ac350d` | 168700 / 147500 | 304400 / 221100 | 158100 | 262750 |
| `asset-913c68450cabf3e2` | 22200 / 22700 | 24800 / 33400 | 22450 | 29100 |
| `asset-a3d03d5e3fb32d02` | 124300 / 125100 | 177500 / 204300 | 124700 | 190900 |
| `asset-acc55653000a6876` | 13600 / 14700 | 33700 / 37200 | 14150 | 35450 |
| `asset-c8d0c2800f0967bd` | 66200 / 62800 | 145600 / 102700 | 64500 | 124150 |
| `asset-efff3b729013051c` | 8200 / 8300 | 13900 / 10300 | 8250 | 12100 |

## Session and storage observations

In both runs, every asset reports setup metadata=1, eager scene=1, CPU=0.
Setup model-session count equals model-sample count (one fresh session per source
frame). First scene sample adds one scene session; all 1,000 steady samples add
**1,000 scene sessions**, not zero. First and steady metadata/model/CPU creation
deltas are zero. This directly records the retained safety boundary.

Evaluator/projector retained bytes and storage generation are unchanged between
after-prepare, after-warm-up and after-measured boundaries for all 16 assets in
both runs. These observed counters do not measure every heap allocation.
Player's stable-tick storage test was extended to 1,000 ticks. Planner exposes no
allocation/capacity metric, so its allocation stability remains **unverified**;
existing cache repeat/forget tests cannot substitute for allocation telemetry.

## Process memory observations

The asset is `asset-003fa54a88cc1bdb` (committed StickAndBall.tgs). Each process
prepares the asset once, creates N live instances, evaluates frame 0 at 128×128
on each, retains the instances, then queries Windows process working set.

| Live instances | Current bytes A1 / A2 | Peak bytes A1 / A2 |
| ---: | ---: | ---: |
| 1 | 4,521,984 / 4,526,080 | 4,526,080 / 4,530,176 |
| 16 | 4,599,808 / 4,599,808 | 4,603,904 / 4,603,904 |
| 64 | 4,763,648 / 4,759,552 | 4,767,744 / 4,763,648 |

Current-working-set deltas from N=1 are 77,824 / 73,728 bytes at N=16 and
241,664 / 233,472 bytes at N=64. These are fresh-process observations including
the executable, allocator and shared asset/cache state. Peak includes earlier
load/preparation. They are neither precise per-instance allocation totals nor
resident scene/plan-cache capacity measurements, and are not CI thresholds.

## Limitations and next work

- Persistent scene/model sessions, zero hot-path constructions and the specified
  2x speedup remain unimplemented. A future lifecycle experiment needs its own
  reviewed ownership/reset/publication contract and both-variant parity proof.
- The committed smoke corpus is not a representative private AveVoice workload;
  gather the intended private 10–30 assets before product performance claims.
- No suitable existing TSan toolchain was established on this machine. Windows
  separate-instance concurrency tests are required, but are not a race-detector
  proof; ASan would not establish that proof either.
- Power/system background conditions were not tightly controlled or changed.
  Repeated timings are descriptive evidence, not pass/fail timing thresholds.
- Samsung scene/plan golden failures remain visible. Do not reinterpret the
  successful new regressions or Telegram graphics checks as an all-variant pass.
- The review approves only this safe partial delivery, not completed reuse,
  general race freedom, all-green Samsung or representative product performance.

## Independent final review

One broad read-only review covered `cda415c..ee25b71`, including runtime counters,
benchmark phases/output safety, the complete-field comparator, regression tests,
saved full gates and independently recomputed timing/storage observations.
Verdict: ready for the safe partial delivery; no Critical or Important findings.
The sole Minor finding was a stale comment promising that completed Task3 would
replace fresh sampling. `7b5221d` corrects only that comment; both focused seams
tests passed, and a separate scoped reviewer marked it addressed with no new
breakage. The controller then ran the four full gates again as recorded above.

The reviewer explicitly declined to certify unimplemented persistent reuse or
speedup, a Samsung geometry/policy fix, race freedom without TSan, planner
zero-allocation, or representative AveVoice/Direct2D performance. These limits
are accepted because the required implementations/evidence are absent, and no
such claim is made here. No review finding remains open in the delivered diff.
The diagnosed pre-existing Samsung failures remain unresolved product limitations.

## Commit map

| Commit | Delivered change |
| --- | --- |
| `cda415c` | Original Part 24 import; byte-matched ZIP baseline |
| `b914e19`, `4584537` | Design and corrected correctness-gated plan |
| `bcde48b`, `f9b1e4e` | Session diagnostics and Samsung-aware assertions |
| `77bccc9` | Durable correctness investigation |
| `94b08bf`, `c484f4b` | Common measurement instrument, safe output handling and reviewed phase/runner fixes |
| `12aaa14` | Explicit deferred-reuse decision |
| `7b33c03`, `f94ae94` | Full scene isolation regressions and baseline-failure record |
| `4ab196d`, `bbfad2d` | Model lifetime/retry regressions and reviewed state |
| `b3bb269` | Nested fixture correction preserving the 16-asset corpus |
| `ee25b71` | Full Windows gates, measured baseline and limitations report |
| `7b5221d` | Reviewed comment correction; no behavior change |

## Rulings made during autonomous execution

These are the decisions from the execution ledger, in order, including their
tradeoffs. No routine approval was left waiting while the user was unavailable.

1. Work on main because the user explicitly chose it; mistakes require normal
   correction/revert commits, never rewritten history.
2. Execute the revised plan without another approval checkpoint because planning
   was delegated; a mistaken routine decision costs reviewable rework.
3. Gate both scene and model reuse on fresh-oracle parity because both reproduce
   corruption; excessive caution would delay optimization, not weaken visuals.
4. Permit documented cyclic workload totals in schema 2 because comparisons need
   identical instrumentation; downstream readers must recognize the new schema.
5. Retain the original 57/57 baseline proof for unchanged runtime, then rerun
   after changes; unrelated nondeterminism remains possible.
6. Remove unconditional recursive output deletion because it can erase unrelated
   user files; stale non-report files intentionally remain on repeated runs.
7. Report planner allocation stability as unverified because no appropriate
   counter exists; this postpones that claim rather than expanding the public API.
8. Investigate one bounded recording-lifecycle design while retaining fresh
   sampling; the cost is delaying speedup until a defensible contract exists.
9. Defer reuse after broader state dependencies were found; the cost is no
   session-reduction or speedup result in this delivery.
10. Use repeated baseline observations instead of A-B-B-A because no optimized B
    exists; comparative acceptance is deferred, not inferred from noise.
11. Continue independently passing work while preserving the clean-baseline
    Samsung golden failure; the full Samsung gate remains explicitly unresolved.
12. Nest the session-only fixture outside auto-enumerated corpus inputs; the cost
    is a documented directory convention, with no removed coverage or new TGS.

## Recommended next bounded stages

1. Design and prove fresh-equivalent recording lifecycle on both pinned variants:
   retained topology, explicit scratch invalidation, inactive/repeater state and
   non-consuming publication. Vendor changes need their own provenance/patch
   addendum. Only then enable reuse and run identical-instrument A-B-B-A.
2. Resolve cross-platform exact geometry as a separate acceptance decision with
   real Linux/MSVC coordinate evidence; do not widen golden exceptions to hide it.
3. Measure the private AveVoice corpus and Direct2D draw costs, then design host
   adapter scheduling/device generations and explicit completeness/fallback.
   Preserve host-owned graphics and avoid freezing a DLL/ABI prematurely.
