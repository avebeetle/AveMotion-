# Persistent Reference Sessions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove per-frame construction of reference animation sessions while preserving every observable Part 24 scene, playback, model, CPU-oracle, and graphics behavior, and produce reproducible Windows evidence for the result.

**Architecture:** Keep the existing host-owned rendering architecture. Each `runtime::Instance` owns one eager mutable reference session for scene sampling and one separate lazy session for CPU rendering; stable model preparation owns one temporary session for its complete scan. A small internal role enum records successful session creation and sampling without changing the public error model. The existing corpus lab becomes the common behavior-neutral measurement instrument for the pre-change baseline and candidate.

**Tech Stack:** C++20, CMake/Ninja, MSVC 19.44, rlottie reference runtime, Python report checks, CTest, Windows PSAPI for observational working-set measurements.

**Spec:** `docs/superpowers/specs/2026-09-23-persistent-reference-sessions-design.md`

## Overnight Time Budget and Correctness Gate

The existing automation deadline remains **2026-09-23 08:32 UTC / 11:32 Moscow**, approximately 5.5 hours after the renewed planning request. User approval includes autonomous routine decisions and ordinary commits/pushes to main.

| Budget | Work | Required evidence |
| --- | --- | --- |
| 0–40 min | Historical tree-state reproduction and cause isolation | Exact failing fixture/order; separate scene and model-scan decisions |
| 40–100 min | Diagnostics and common measurement instrument | Tests green; pre-optimization baseline retained |
| 100–200 min | Bounded lifecycle fix and conditional scene/model reuse | Full access-order/content parity, viewport/CPU/concurrency gates |
| 200–270 min | Windows variants, WARP/preview, timing and memory | Fresh logs and before/after evidence |
| 270 min–deadline | Independent review, corrections, final tests and handoff | Verified main pushed; remaining risks recorded |

These are budgets, not unconditional promises. Start no large block after **08:12 UTC / 11:12 Moscow**. Protect the final review/verification time. If a safe lifecycle fix cannot be proved in budget, retain fresh sampling, deliver independently verified diagnostics/regression tests/baseline evidence, and mark reuse incomplete.

## Global Constraints

- Work directly on `main` because the user explicitly authorized autonomous main-only commits and ordinary pushes. Never force-push.
- Use strict TDD for every behavior change: add one focused failing assertion, run it and record the expected failure, implement the minimum production change, then rerun focused and regression gates.
- Preserve public error categories, handles, playback behavior, canonical tables, geometry, render plans, goldens, Direct2D ownership, and CPU-oracle isolation.
- Do not add ANGLE, D3D9, D3D11on12, OpenGL, a second graphics device/swap chain, a timer, or a worker thread.
- Treat one `Instance` as single-execution-stream state. Only separate instances may be evaluated concurrently.
- Count successful session construction only. Increment sample counters at the actual `renderTree()` boundary. All diagnostic atomics use `memory_order_relaxed`.
- Keep the measurement patch neutral to runtime and visual semantics and use that exact patch on both the pre-optimization baseline and candidate. Schema-2 timing/workload columns may change as documented; asset classifications and semantic goldens must not. Timing thresholds are evidence, not CI assertions.
- Do not alter golden files unless a separately proved semantic defect requires it; this stage expects no golden changes.
- Keep generated builds, raw benchmark output, and the detached baseline worktree out of Git. Commit only code, tests, documentation, and the concise evidence report.

## Review Focus

- Verify that no evaluation hot path can silently allocate a replacement scene session.
- Verify session ownership and role counters across success, failure, retry, reset, and variant-specific model preparation.
- Compare complete scene content, not only the four `RecordingBackend` fingerprints. Exclude only handles/instance identity, evaluation sequence, history-derived change flags, and upstream change bits from fresh-versus-reused comparisons.
- Exercise mutable upstream history: ascending, descending, repeated non-monotonic frames, viewport changes, CPU rendering between scene samples, and two independent instances on two host threads.
- Verify benchmark phase boundaries: setup, first sample, warm-up, measured steady loop, then CPU oracle. Snapshot diagnostics outside the measured wall-clock interval.
- Report the baseline honestly as `cda415c` plus the identical measurement-only commit; retain both raw A-B-B-A runs and do not overstate the 16-fixture smoke corpus.

---

## Task 0: Resolve the known render-tree state hazard

**Files:** read `docs/known-issues/RLOTTIE_RENDER_TREE_STATE.md`, both upstream drawable/update implementations, `src/runtime/RlottieSceneBridge.cpp`, `src/runtime/Runtime.cpp`, and `tests/runtime_seams_tests.cpp`. Scratch probe/report: `out/part25a-probe/`.

**Produces:** findings with separate go/no-go decisions for arbitrary scene seeks and ascending model scans. No product changes in this task.

- [ ] Reproduce `0 -> middle -> same middle -> last -> 0` with one upstream session and fresh-per-sample oracles. Deep-copy immediately; fingerprints locate discrepancies, full values explain them.
- [ ] Scan every source frame in ascending order with one session versus a fresh session per frame. Model preparation needs its own parity gate.
- [ ] Exercise corpus assets, Trim/Repeater and a genuine dashed stroke. Record exact differing path/paint/layer fields and mutation sites.
- [ ] Decide from evidence. A small passing probe does not replace the full Task 3/4 parity suites.
- [ ] If a bug reproduces, investigate one bounded non-consuming-state fix. Before vendor changes write a design addendum naming lifetime/ownership, affected variants, patch/provenance changes, tests and licensing impact. Routine decisions are delegated; preserve notices and dependencies.
- [ ] If no small semantics-preserving fix can be proved in the budget, retain fresh sampling for every affected path and continue diagnostics, stronger tests, baseline evidence and handoff. Never hide a per-frame reload/reset behind a persistent-session claim.

## Task 1: Define reference session diagnostics

**Files:**

- Modify: `include/avemotion/runtime/Diagnostics.hpp`
- Modify: `src/runtime/Runtime.cpp`
- Modify: `tests/runtime_seams_tests.cpp`
**Interface:** six public `std::uint64_t` snapshot fields and private session/sample roles. Existing allocation lifetime is unchanged.

### 1.1 Diagnostics RED

- [ ] Add compile-time/runtime assertions in `tests/runtime_seams_tests.cpp` for these exact `std::uint64_t` snapshot members:
  `referenceMetadataSessionsCreated`, `referenceSceneSessionsCreated`,
  `referenceModelSessionsCreated`, `referenceCpuSessionsCreated`,
  `referenceSceneSamples`, and `referenceModelSamples`.
- [ ] On `StickAndBall.json`, reset diagnostics while quiescent, then assert metadata creation after load, scene creation after instance creation, lazy CPU creation/reuse, and zeroed epoch counters after `resetDiagnostics()` without recreating live sessions.
- [ ] For the pre-optimization characterization only, assert that one exact scene evaluation creates an additional scene-role session. Mark this assertion for replacement by Task 3's permanent zero-hot-path contract. Use `evaluateFrame()` to keep implicit model preparation out of this assertion.
- [ ] Run the focused build and capture the expected missing-member compilation failure:

```powershell
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-telegram-debug --target avemotion_runtime_seams_tests --parallel 4'
```

### 1.2 Minimal diagnostic implementation

- [ ] Add the six public snapshot fields in the exact order above.
- [ ] Add six matching relaxed atomics to `RuntimeState`, then include every field in `snapshot()` and `reset()`.
- [ ] Add private enums such as `ReferenceSessionRole { Metadata, Scene, ModelPreparation, Cpu }` and `ReferenceSampleRole { Scene, ModelPreparation }`.
- [ ] Make the internal load helper accept a session role and increment the matching creation counter only after `rlottie::Animation::loadFromData()` succeeds.
- [ ] Tag all current creation sites accurately: asset validation/metadata, eager instance scene session, model-preparation per-frame sessions, and lazy CPU session.
- [ ] Pass a sample role into the exact-scene helper and increment the matching sample counter immediately at the `renderTree()` boundary. Do not add model samples to the existing instance-only `sceneEvaluations` metric.
- [ ] Rerun the focused seams target/test and verify the Task 1 characterization is green.

- [ ] Commit message: `feat: add reference session diagnostics`. Review this task independently before changing lifetime or the benchmark.

## Task 2: Establish the measurement contract and baseline

**Files:** modify `apps/corpus_lab/main.cpp`, `scripts/test_corpus_lab_output.py`, `scripts/run_part24_corpus_lab.py`, `CMakeLists.txt`, and `docs/CORPUS_LAB.md`. Extend existing Player/planner tests only where a necessary storage invariant lacks coverage.

**Interface:** consumes Task 1 counters. Produces summary `schema=2`, exact/pipeline median and p95 in ns, phase-specific session counts, workspace growth observations and a Windows memory report. No runtime lifetime changes.

### 2.1 Corpus-lab schema RED

- [ ] Extend `scripts/test_corpus_lab_output.py` first to require schema version 2 and exact timing/session columns; verify benchmark aliases match manifest aliases, numeric values are non-negative, p95 is at least median, and summary sample/warm-up metadata matches the CLI.
- [ ] Require phase counters for setup, first sample, and measured steady loop. At minimum record all four session-creation roles per phase plus setup model samples, first scene samples, and steady scene samples.
- [ ] Run the existing output test against the last smoke output and capture the expected missing-column/schema failure:

```powershell
python scripts/test_corpus_lab_output.py --output out/build/windows-msvc-telegram-debug/corpus-lab-smoke --expected-assets 16
```

### 2.2 Benchmark implementation

- [ ] Add `--warmup-samples` in `[0, 10000]` with a documented default of 20. Preserve `--samples` as the measured count.
- [ ] Add a nanosecond timing helper, conventional median, and nearest-rank p95 (`sorted[ceil(0.95 * N) - 1]`, clamped to the valid range).
- [ ] Make `runPipeline(std::size_t frame, bool collect)` consume the exact frame index. First sample is frame 0; warm-up and measured loops each use `sample % totalFrames`. Document that schema-2 sampled workload totals include first+measured frames and exclude warm-up; asset classification remains unchanged.
- [ ] For each measured full pipeline call, snapshot diagnostics immediately before and after the call but outside its wall-clock interval. Derive exact-scene duration from the delta of `sceneEvaluationNanoseconds`; record the wall-clock duration of evaluator → instance exact/model evaluation → projector → planner.
- [ ] Add `exact_scene_median_ns`, `exact_scene_p95_ns`, `pipeline_median_ns`, and `pipeline_p95_ns`. Preserve existing report columns for compatibility.
- [ ] Snapshot setup after load/model preparation/instance construction, then first-sample and steady-loop deltas. CPU oracle measurements must occur only after these snapshots.
- [ ] Add `--memory-instances` accepting only `1`, `16`, or `64`, requiring a single input asset. In this observational mode, prepare once, create N live instances, evaluate frame 0 at 128×128 on all of them, keep them alive, and on Windows write current `WorkingSetSize` and process-wide `PeakWorkingSetSize` from `GetProcessMemoryInfo`. Label both as process observations and never make them CI pass/fail thresholds. Add the narrow `Psapi` link only where required.
- [ ] Write `schema=2`, measured/warm-up counts, frame order, viewport, and timing units into the summary. Forward `--warmup-samples` through the Python runner. Verify the actual percentile helper on deterministic sample values.
- [ ] Record evaluator/projector `retainedBytes()` and `storageGeneration()` after prepare, warm-up and the measured loop. Check Player storage generation over 1,000 stable ticks with its existing test. Inspect planner cache diagnostics using existing tests; report missing observability explicitly. Stable plan-vector size does not prove zero allocations.
- [ ] Update `docs/CORPUS_LAB.md` with precise timing, percentile, diagnostic-phase, privacy, and memory-mode semantics.
- [ ] Build the corpus lab and run the smoke/output checks until green:

```powershell
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-telegram-debug --target avemotion_corpus_lab --parallel 4'
ctest --preset windows-msvc-telegram-debug -R '^avemotion\.corpus\.lab_(smoke|output)$' --no-tests=error --output-on-failure
```

### 2.3 Establish the comparable baseline

- [ ] Run focused diagnostics/corpus tests plus all existing runtime, model, and corpus-lab tests.
- [ ] Commit the behavior-neutral diagnostics and measurement contract. Record this commit as `MEASUREMENT_BASELINE`; its parent lineage is `cda415c` and it deliberately retains per-frame session construction.
- [ ] Configure/build `windows-msvc-corpus-lab` and run one baseline sanity timing process using the committed 16-asset corpus with:

```text
--samples 1000 --warmup-samples 20 --load-repeats 1
--cpu-repeats 0 --render-size 128 --strict
```

- [ ] Run one baseline memory sanity process for one instance using `tests/compatibility/tgs/StickAndBall.tgs`. Save raw output beneath `out/benchmarks/part25a/` with compiler, preset, commit, arguments, corpus hashes and machine context. Final A-B-B-A supplies repeated timing/memory observations; avoid duplicating them here.
- [ ] Commit message: `test: define reference session measurement contract`

## Task 3: Reuse the eager scene session and prove state isolation

**Gate:** Task 0 must permit scene reuse, including any separately proved lifecycle fix. Otherwise land only stronger regression coverage against the safe runtime and report reuse deferred.

**Files:**

- Modify: `src/runtime/Runtime.cpp`
- Modify: `tests/runtime_seams_tests.cpp`
- Create: `tests/reference_session_tests.cpp`
- Create: `tests/fixtures/dashed_stroke_session.json`
- Modify: `CMakeLists.txt`

### 3.1 Persistent-session RED

- [ ] Replace Task 1's characterization assertion with the permanent contract: loading one asset and creating N instances creates one metadata and exactly N scene sessions; any number of subsequent exact evaluations creates zero more sessions while `referenceSceneSamples` advances once per call.
- [ ] Add a dedicated `avemotion_reference_session_tests` executable and `avemotion.runtime.reference_sessions` CTest entry gated by reference-runtime availability.
- [ ] Run the test against the still-unoptimized runtime and capture the expected count failure caused by per-frame `loadFromData()`.

### 3.2 Full observable scene comparator

- [ ] Build a test-only content comparator that checks `RecordingBackend::record()` plus statistics, bounds, model application/counts, layer fields, child indices, draw-item source/model/local/projected geometry and paint fields, transforms, repeater/projection metadata, masks, and canonical values/pointer relationships where observable.
- [ ] Exclude only asset/instance handles and ID, evaluation sequence, history-derived `changes`, and upstream change bits when comparing a new instance oracle with a reused instance. Assert sequence and change behavior separately on the reused instance.
- [ ] Use representative fixtures for animated paths, Trim, Repeater, masks, mattes, and nested compositions. For each target `(frame, viewport)`, build a fresh-instance oracle, then compare one persistent instance in ascending, descending, and deterministic repeated non-monotonic orders. Include `128x128 → 96x160 → 128x128` viewport changes.

### 3.3 CPU and concurrency coverage

- [ ] Add the smallest valid dashed-stroke fixture because the committed corpus has no real dash array exercising rlottie's mutable dash path.
- [ ] Evaluate a scene, render two CPU frames on the same instance, and evaluate the same scene again. Require identical complete scene content, one CPU session, two CPU frames, one scene session, and two scene samples.
- [ ] Pre-prepare one asset, create exactly two instances, and evaluate each on its own `std::thread` in different repeated frame orders. Join and compare all results to sequential fresh-instance oracles. Never access one instance from both threads.

### 3.4 Minimal production change

- [ ] Change `extractExactScene()` to accept `rlottie::Animation&` and remove session construction from the helper.
- [ ] Keep this intermediate commit buildable: adapt the model caller by creating its current per-frame model-role session before calling the helper. Task 4 alone moves that construction outside the loop.
- [ ] Make `evaluateExactFrame()` reject a missing `InstanceData::sceneAnimation` as `EvaluationFailed`; never allocate a hot-path fallback.
- [ ] Pass `*instance.sceneAnimation` into the helper and keep all deep-copy/error/handle behavior unchanged.
- [ ] Keep `cpuAnimation` separate and lazy.
- [ ] Run the new test, runtime seams, scene golden, playback, and player tests. Then run the complete Telegram debug suite.
- [ ] Commit message: `perf: reuse instance reference scene sessions`

## Task 4: Reuse one temporary session for stable model preparation

**Gate:** Task 0 must permit ascending scan reuse. Arbitrary-seek success does not replace complete model parity. Otherwise retain the safe scan and report this optimization deferred.

**Files:**

- Modify: `src/runtime/Runtime.cpp`
- Modify: `tests/asset_model_tests.cpp`
- Modify: `tests/reference_session_tests.cpp` if shared helpers are required

### 4.1 Model-session RED

- [ ] In the Telegram variant, load `StickAndBall.json`, reset at a quiescent boundary where exact counts remain meaningful, and assert that the first successful `prepareModel()` creates exactly one model session and samples exactly `totalFrames` frames.
- [ ] Assert a second `prepareModel()` returns the same published model without changing model session/sample or model-build-success counters.
- [ ] Assert subsequent instance scene evaluation changes neither model counter.
- [ ] Preserve Samsung/no-extension behavior: zero model sessions and samples.
- [ ] Add a valid-load fixture with a timeline above the 10,000-frame preparation limit; two calls must both fail with the established message, record two attempts/failures, and create zero model sessions/samples because eligibility fails before construction.
- [ ] Run the Telegram model test and capture the expected `sessions == totalFrames` failure.
- [ ] Compare complete canonical model tables/values against a fresh-per-frame baseline for affected fixtures, in addition to existing goldens. Ignore only construction-local identity.

### 4.2 Minimal model implementation

- [ ] In `prepareStableAssetModel()`, keep parsed-model extraction and all eligibility checks first.
- [ ] Construct one local model session before the frame loop and use RAII on every exit. `Asset::prepareModel()` returns a failed `model::AssetModelResult` with a precise string; `applyPreparedAssetModel()` maps it to `AssetModelPreparationFailed`. Preserve both interfaces.
- [ ] Pass the same session into `extractExactScene()` for every observed frame. Do not borrow any instance session and do not store mutable session state on `AssetData`.
- [ ] Preserve `modelMutex`, idempotent publication, and retry-after-failure semantics.
- [ ] Run Telegram and Samsung focused tests, runtime/reference-session tests, all golden suites, and the complete Telegram debug suite.
- [ ] Commit message: `perf: reuse model preparation reference session`

## Task 5: Produce candidate evidence, document the contract, and finish

**Files:**

- Create: `docs/PART25A_PERSISTENT_SESSIONS_REPORT.md`
- Modify: `docs/superpowers/specs/2026-09-23-persistent-reference-sessions-design.md`
- Modify: `docs/superpowers/STATE.md`
- Modify: `README.md` only if the runtime contract index needs a link

### 5.1 Complete functional verification

- [ ] Configure and build both `windows-msvc-telegram-debug` and `windows-msvc-samsung-debug`.
- [ ] Run focused reference-session/model tests under both variants, then their complete suites. Explicitly configure/build/test `windows-msvc-win32-preview` for Direct2D capture, WARP and preview/device-recreation gates (this preset includes the capture options). Use `windows-msvc-direct2d` for no-reference/install gates where applicable.
- [ ] Run vendor-integrity, TGS, validation, playback/player, source-geometry, render-plan, scene/model/property/golden, corpus-lab, and packaging-related gates without weakening or exclusions.
- [ ] Run the two-instance test under an already available ThreadSanitizer toolchain, if present. Otherwise report Windows concurrency results and absence of TSan evidence. ASan is not race detection; install no new toolchain.
- [ ] Capture fresh `git diff --check`, `git status`, and test logs before any completion claim.

### 5.2 A-B-B-A timing and memory evidence

- [ ] Add a detached temporary worktree at `MEASUREMENT_BASELINE` outside the repository workspace and build its `windows-msvc-corpus-lab` preset. Use no branch and make no baseline edits.
- [ ] Build the candidate using the same compiler, preset, flags, corpus, viewport, warm-up, measured count, and power conditions.
- [ ] Run four fresh timing processes in this order: baseline A1, candidate B1, candidate B2, baseline A2. Retain all raw reports beneath ignored `out/benchmarks/part25a/`.
- [ ] Run the same A-B-B-A order for fresh 1-, 16-, and 64-instance memory processes on the same committed TGS.
- [ ] Summarize each asset using the median of two run medians and median of two run p95s, retaining raw values. Define aggregate median as median across the 16 per-asset medians. Require zero steady-loop session creations only for paths where safe reuse shipped. Check workspace snapshots and separate Player/planner evidence; do not infer zero allocation from missing observability.
- [ ] Compare the committed smoke corpus with the design target: candidate exact-scene median at least 2x faster overall and no asset p95 more than 10% worse. If the target is missed, retain a correct deterministic improvement but mark Part 25A performance status incomplete and make no Telegram-class claim.
- [ ] Report current/peak process working set for 1/16/64 and deltas from N=1 as observations only.
- [ ] Remove the detached worktree with `git worktree remove` after verifying the resolved path is the intended temporary directory; prune only its stale worktree registration if required.

### 5.3 Documentation and final review

- [ ] Document session lifetime, single-instance confinement, separate-instance concurrency, CPU isolation, reset-epoch semantics, benchmark method/results, limitations, and next-stage recommendation in `docs/PART25A_PERSISTENT_SESSIONS_REPORT.md`.
- [ ] Mark the design implemented or performance-incomplete based on evidence, and update `docs/superpowers/STATE.md` with exact commits, tests, measurements, and any unresolved risks.
- [ ] Request an independent whole-branch code review against the spec. Resolve every Critical/Important finding with a focused test and re-review; record Minor findings explicitly if deferred.
- [ ] Rerun fresh final verification after review fixes. Commit documentation/review changes with `docs: report persistent session evidence`.
- [ ] Push ordinary `main` commits to `origin/main`, verify local HEAD equals `origin/main`, and pause the six-hour heartbeat once all planned work is complete or the deadline is reached.

## Final Verification Commands

```powershell
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-telegram-debug && cmake --build --preset windows-msvc-telegram-debug --parallel 4 && ctest --preset windows-msvc-telegram-debug --output-on-failure'
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-samsung-debug && cmake --build --preset windows-msvc-samsung-debug --parallel 4 && ctest --preset windows-msvc-samsung-debug --output-on-failure'
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-win32-preview && cmake --build --preset windows-msvc-win32-preview --parallel 4 && ctest --preset windows-msvc-win32-preview --output-on-failure'
git diff --check
git status --short --branch
git rev-parse HEAD
git rev-parse origin/main
```
