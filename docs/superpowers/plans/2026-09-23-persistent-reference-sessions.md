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

- [x] Reproduce `0 -> middle -> same middle -> last -> 0` with one upstream session and fresh-per-sample oracles. Deep-copy immediately; fingerprints locate discrepancies, full values explain them.
- [x] Scan every source frame in ascending order with one session versus a fresh session per frame. Model preparation needs its own parity gate.
- [x] Exercise corpus assets, Trim/Repeater and a genuine dashed stroke. Record exact differing path/paint/layer fields and mutation sites.
- [x] Decide from evidence. A small passing probe does not replace the full Task 3/4 parity suites.
- [x] If a bug reproduces, investigate one bounded non-consuming-state fix. Before vendor changes write a design addendum naming lifetime/ownership, affected variants, patch/provenance changes, tests and licensing impact. Routine decisions are delegated; preserve notices and dependencies.
- [x] If no small semantics-preserving fix can be proved in the budget, retain fresh sampling for every affected path and continue diagnostics, stronger tests, baseline evidence and handoff. Never hide a per-frame reload/reset behind a persistent-session claim.

## Task 1: Define reference session diagnostics

**Files:**

- Modify: `include/avemotion/runtime/Diagnostics.hpp`
- Modify: `src/runtime/Runtime.cpp`
- Modify: `tests/runtime_seams_tests.cpp`
**Interface:** six public `std::uint64_t` snapshot fields and private session/sample roles. Existing allocation lifetime is unchanged.

### 1.1 Diagnostics RED

- [x] Add compile-time/runtime assertions in `tests/runtime_seams_tests.cpp` for these exact `std::uint64_t` snapshot members:
  `referenceMetadataSessionsCreated`, `referenceSceneSessionsCreated`,
  `referenceModelSessionsCreated`, `referenceCpuSessionsCreated`,
  `referenceSceneSamples`, and `referenceModelSamples`.
- [x] On `StickAndBall.json`, reset diagnostics while quiescent, then assert metadata creation after load, scene creation after instance creation, lazy CPU creation/reuse, and zeroed epoch counters after `resetDiagnostics()` without recreating live sessions.
- [x] For the pre-optimization characterization only, assert that one exact scene evaluation creates an additional scene-role session. Mark this assertion for replacement by Task 3's permanent zero-hot-path contract. Use `evaluateFrame()` to keep implicit model preparation out of this assertion.
- [x] Run the focused build and capture the expected missing-member compilation failure:

```powershell
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-telegram-debug --target avemotion_runtime_seams_tests --parallel 4'
```

### 1.2 Minimal diagnostic implementation

- [x] Add the six public snapshot fields in the exact order above.
- [x] Add six matching relaxed atomics to `RuntimeState`, then include every field in `snapshot()` and `reset()`.
- [x] Add private enums such as `ReferenceSessionRole { Metadata, Scene, ModelPreparation, Cpu }` and `ReferenceSampleRole { Scene, ModelPreparation }`.
- [x] Make the internal load helper accept a session role and increment the matching creation counter only after `rlottie::Animation::loadFromData()` succeeds.
- [x] Tag all current creation sites accurately: asset validation/metadata, eager instance scene session, model-preparation per-frame sessions, and lazy CPU session.
- [x] Pass a sample role into the exact-scene helper and increment the matching sample counter immediately at the `renderTree()` boundary. Do not add model samples to the existing instance-only `sceneEvaluations` metric.
- [x] Rerun the focused seams target/test and verify the Task 1 characterization is green.

- [x] Commit message: `feat: add reference session diagnostics`. Review this task independently before changing lifetime or the benchmark.

## Task 2: Establish the measurement contract and baseline

**Files:** modify `apps/corpus_lab/main.cpp`, `scripts/test_corpus_lab_output.py`, `scripts/run_part24_corpus_lab.py`, `CMakeLists.txt`, and `docs/CORPUS_LAB.md`. Create `apps/corpus_lab/BenchmarkMetrics.hpp` and `tests/corpus_benchmark_metrics_tests.cpp` for reusable/tested percentile and phase-delta helpers. Create `scripts/test_corpus_lab_output_safety.py` for the discovered output-directory deletion risk. Extend existing Player/planner tests only where a necessary storage invariant lacks coverage.

**Interface:** consumes Task 1 counters. Produces summary `schema=2`, exact/pipeline median and p95 in ns, phase-specific session counts, workspace growth observations and a Windows memory report. No runtime lifetime changes.

Concrete report contract: preserve every existing TSV column and append
`exact_scene_median_ns`, `exact_scene_p95_ns`, `pipeline_median_ns`,
`pipeline_p95_ns`, and `{setup,first,steady}_{metadata,scene,model,cpu}_sessions`.
Also append `setup_model_samples`, `first_scene_samples`, `steady_scene_samples`,
and evaluator/projector retained-byte and storage-generation values for the
after-prepare, after-warm-up and after-measured boundaries. Setup excludes the
separate load-timing runtimes. First/steady fields are deltas; warm-up is excluded.
Report column units and phase meanings in the documentation.

Memory mode writes `memory_observation.tsv` with header
`asset\tinstances\tworking_set_bytes\tpeak_working_set_bytes\n` and one row.
Its single asset uses the existing privacy alias. Hold all instances live until
after `GetProcessMemoryInfo`; reject this mode explicitly on non-Windows rather
than emitting invented zero measurements. Forward `--memory-instances` through
the runner too. Its normal corpus mode retains existing report filenames.

The planner currently exposes revision/update counts but no allocation/capacity
metric. Use its existing repeat/forget regression tests and label allocation
stability unverified; adding a new public planner observability API is outside
this measurement task.

### 2.1 Corpus-lab schema RED

- [x] Extend `scripts/test_corpus_lab_output.py` first to require schema version 2 and exact timing/session columns; verify benchmark aliases match manifest aliases, numeric values are non-negative, p95 is at least median, and summary sample/warm-up metadata matches the CLI.
- [x] Require phase counters for setup, first sample, and measured steady loop. At minimum record all four session-creation roles per phase plus setup model samples, first scene samples, and steady scene samples.
- [x] Run the existing output test against the last smoke output and capture the expected missing-column/schema failure:

```powershell
python scripts/test_corpus_lab_output.py --output out/build/windows-msvc-telegram-debug/corpus-lab-smoke --expected-assets 16
```

- [x] Add a temporary-directory regression that invokes the actual corpus CLI with an existing output directory containing a nested sentinel and requires the sentinel to survive; cover the Python runner with `--skip-build` too. Observe RED against the current unconditional `std::filesystem::remove_all(options.output)` / `shutil.rmtree(args.output)`. Replace both with non-destructive directory creation and overwrite only the tool's named report files. Input equal to output must preserve the original asset. Do not introduce an arbitrary recursive-delete guard when no deletion is needed.

### 2.2 Benchmark implementation

- [x] Add `--warmup-samples` in `[0, 10000]` with a documented default of 20. Preserve `--samples` as the measured count.
- [x] Add a nanosecond timing helper, conventional median, and nearest-rank p95 (`sorted[ceil(0.95 * N) - 1]`, clamped to the valid range).
- [x] Make `runPipeline(std::size_t frame, bool collect)` consume the exact frame index. First sample is frame 0; warm-up and measured loops each use `sample % totalFrames`. Document that schema-2 sampled workload totals include first+measured frames and exclude warm-up; asset classification remains unchanged.
- [x] For each measured full pipeline call, snapshot diagnostics immediately before and after the call but outside its wall-clock interval. Derive exact-scene duration from the delta of `sceneEvaluationNanoseconds`; record the wall-clock duration of evaluator → instance exact/model evaluation → projector → planner.
- [x] Add `exact_scene_median_ns`, `exact_scene_p95_ns`, `pipeline_median_ns`, and `pipeline_p95_ns`. Preserve existing report columns for compatibility.
- [x] Snapshot setup after load/model preparation/instance construction, then first-sample and steady-loop deltas. CPU oracle measurements must occur only after these snapshots.
- [x] Add `--memory-instances` accepting only `1`, `16`, or `64`, requiring a single input asset. In this observational mode, prepare once, create N live instances, evaluate frame 0 at 128×128 on all of them, keep them alive, and on Windows write current `WorkingSetSize` and process-wide `PeakWorkingSetSize` from `GetProcessMemoryInfo`. Label both as process observations and never make them CI pass/fail thresholds. Add the narrow `Psapi` link only where required.
- [x] Write `schema=2`, measured/warm-up counts, frame order, viewport, and timing units into the summary. Forward `--warmup-samples` through the Python runner. Verify the actual percentile helper on deterministic sample values.
- [x] Record evaluator/projector `retainedBytes()` and `storageGeneration()` after prepare, warm-up and the measured loop. Check Player storage generation over 1,000 stable ticks with its existing test. Inspect planner cache diagnostics using existing tests; report missing observability explicitly. Stable plan-vector size does not prove zero allocations.
- [x] Update `docs/CORPUS_LAB.md` with precise timing, percentile, diagnostic-phase, privacy, and memory-mode semantics.
- [x] Build the corpus lab and run the smoke/output checks until green:

```powershell
& cmd.exe /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-telegram-debug --target avemotion_corpus_lab --parallel 4'
ctest --preset windows-msvc-telegram-debug -R '^avemotion\.corpus\.lab_(smoke|output)$' --no-tests=error --output-on-failure
```

### 2.3 Establish the comparable baseline

- [x] Run focused diagnostics/corpus tests plus all existing runtime, model, and corpus-lab tests.
- [x] Commit the behavior-neutral diagnostics and measurement contract. Record this commit as `MEASUREMENT_BASELINE`; its parent lineage is `cda415c` and it deliberately retains per-frame session construction.
- [x] Configure/build `windows-msvc-corpus-lab` and run one baseline sanity timing process using the committed 16-asset corpus with:

```text
--samples 1000 --warmup-samples 20 --load-repeats 1
--cpu-repeats 0 --render-size 128 --strict
```

- [x] Run one baseline memory sanity process for one instance using `tests/compatibility/tgs/StickAndBall.tgs`. Save raw output beneath `out/benchmarks/part25a/` with compiler, preset, commit, arguments, corpus hashes and machine context. Final A-B-B-A supplies repeated timing/memory observations; avoid duplicating them here.
- [x] Commit message: `test: define reference session measurement contract`

## Task 3: Lock fresh-sampling scene isolation with regression tests

**Execution decision:** Task 0 rejected direct reuse and the bounded architecture
assessment found additional active-state dependencies. This is the approved safe
branch: ship tests, not persistent reuse. Keep Runtime.cpp and Task 1's
fresh-per-sample counter characterization unchanged. The original zero-hot-path
session target remains deferred, not satisfied.

**Files:** create `tests/reference_session_tests.cpp`,
`tests/fixtures/dashed_stroke_session.json`; modify `CMakeLists.txt`.
A focused test-only comparison header under `tests/` is allowed if it makes the
complete comparator easier to audit. No production/vendor/golden edits.

**Interfaces:** use `Runtime`, `Asset`, `Instance::evaluateFrame()`,
`renderCpuFrame()`, `DiagnosticsSnapshot`, and `RecordingBackend::record()`.
Use the six existing reference counters without redefining lifetime.

### 3.1 Full observable comparator and RED evidence

- [ ] Register `avemotion_reference_session_tests` and CTest
  `avemotion.runtime.reference_sessions` only when reference runtime is enabled.
- [ ] Build a test-only exact comparator covering recorder output plus scene
  statistics/bounds/model counts, complete layers/child indices/masks, every draw
  item's source/model/local/projected geometry and paint, transforms, projection
  and repeater metadata, and canonical object values/alias relationships.
  Enumerate fields explicitly against `EvaluatedScene.hpp`; no bytewise struct
  comparison, epsilon relaxation or fingerprint-only shortcut.
- [ ] Exclude only asset/instance handles and identity, evaluation sequence,
  history-derived `changes`, and upstream change bits. Assert reused-instance
  sequence and repeated-frame change behavior separately.
- [ ] Test the comparator itself by copying one scene and independently changing
  representative fields omitted by RecordingBackend fingerprints; require a
  descriptive mismatch. Include geometry, paint, layer/mask and source-local
  metadata mutations.
- [ ] Demonstrate meaningful regression RED with a temporary narrowly scoped
  naive scene-session reuse mutation on the new dashed/access-order test. Save
  the exact patch and failing output under ignored `out/part25a-regression/`,
  restore that mutation with apply_patch, and verify the committed runtime diff
  is empty. Never commit or retain the unsafe mutation.

### 3.2 Access-order and viewport coverage

- [ ] Add the static dashed rounded rectangle from
  `out/part25a-probe/dash.json` as the small committed fixture, with no external
  asset or licensing dependency.
- [ ] Use animated paths, multi-trim, Repeater, mask/matte and nested-composition
  fixtures, including LoudMute and firework from the known failures. Read
  `out/part25a-probe/findings.md` for exact existing paths.
- [ ] For each fixture, compare every valid source frame in ascending and reverse
  order on a reused Instance against new-Instance oracles. Add deterministic
  repeated non-monotonic seeks and `128x128 -> 96x160 -> 128x128` viewport changes.
  Preserve variant-reported totalFrames; precompute immutable oracle scenes once
  per frame/viewport where useful. Current fresh sampling must pass both variants.
- [ ] Assert actual safe-path counts: every successful exact sample creates one
  additional scene-role session and advances scene samples once. Eager instance
  creation adds one scene session. Keep oracle diagnostics isolated from the
  measured Runtime; resets happen only while quiescent.

### 3.3 CPU isolation and separate-instance concurrency

- [ ] On the dashed fixture, evaluate a scene, render two CPU frames, and evaluate
  the same scene again. Require identical complete content, one lazy CPU session,
  two CPU renders, three scene sessions including eager construction, and two
  scene samples. Repeated CPU calls must not recreate their session.
- [ ] Prepare one asset before threading when supported, create exactly two
  instances, and evaluate each on its own std::thread using different repeated
  frame orders. Join before reading results; compare with sequential fresh
  oracles. Never access an Instance from both workers and capture failures safely.
- [ ] Run the new target under Telegram and Samsung, then focused runtime seams,
  scene golden, playback and Player tests. Use MSVC VsDevCmd; full suites follow
  in Task 5. Record TSan unavailable unless an existing suitable toolchain is
  positively identified; MSVC/ASan success is not race detection.
- [ ] Commit `test: lock reference scene access-order isolation`; report exact
  RED/GREEN commands and comparator exclusions. Independent task review required.

## Task 4: Lock current model preparation lifetime and retry semantics

**Execution decision:** Full ascending scans fail parity, so the single-session
model optimization is deferred. This task exercises the unchanged fresh scan,
idempotent publication, failure/retry, and variant isolation. Do not change
Runtime.cpp or canonical/golden data.

**Files:** modify `tests/asset_model_tests.cpp`; add a small test-only JSON fixture
only if constructing a valid long-timeline asset inline is inconsistent with the
existing tests. No public API change.

### 4.1 Characterization and meaningful RED

- [ ] On Telegram, load `StickAndBall.json`, reset while quiescent, prepare once,
  and assert model sessions == model samples == reported totalFrames, exactly one
  successful build, and zero scene-role sessions/samples for the scan.
- [ ] A second prepareModel returns the same immutable published model and
  changes no model sessions/samples/build-success counter.
- [ ] Subsequent exact Instance evaluation changes neither model counter; its
  separate scene-role counters behave as Task 1 documented.
- [ ] Samsung unsupported preparation creates zero model sessions and samples,
  preserving its existing failure/result behavior.
- [ ] Load a valid animation with timeline above the 10,000-frame preparation
  limit. Two prepareModel calls must fail with the established limit message,
  record two attempts/failures, and create zero model sessions/samples because
  eligibility rejects before construction. Keep this assertion variant-aware.
- [ ] Prove the new assertions detect a relevant regression via one temporary
  narrow mutation (such as suppressing the model-build-failure increment).
  Save mutation and RED output in ignored out/part25a-regression, restore with
  apply_patch, then show GREEN. Do not commit the mutation.
- [ ] Existing complete model/canonical goldens remain authoritative. No new
  persistent-model parity claim is possible because that path was not enabled.
- [ ] Run focused model/runtime/reference-session/golden tests on both variants.
  Commit `test: cover reference model preparation lifetime and retries`; record
  all commands and route the independent review before Task 5.

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

**Safe-branch override (2026-09-23):** No runtime optimization shipped in Tasks
3/4. Therefore there is no optimized B candidate to compare. Retain the reviewed
measurement baseline and run two fresh Release timing processes plus two fresh
memory processes at each of 1/16/64 live instances with the exact arguments
below. Record raw medians/p95s, counters, workspace observations, compiler,
commit and hashes. Label these repeat baseline observations, not before/after
speedup. Do not construct a duplicate worktree or interpret timing noise between
identical runtime code as an optimization. The original A-B-B-A checklist below
is deferred together with reuse; it remains the future acceptance method.

- [ ] Execute and validate two repeated baseline timing reports for all 16
  committed TGS assets: samples 1000, warmup 20, load repeats 1, CPU repeats 0,
  viewport 128, strict mode, windows-msvc-corpus-lab Release.
- [ ] Record two independent fresh-process working-set observations at each of
  1/16/64 live instances on StickAndBall.tgs; retain current and peak bytes and
  describe process-wide limitations.
- [ ] Summarize per-asset median of two medians and median of two p95 values,
  actual steady session creations and evaluator/projector storage changes.
  Explicitly state that zero-creation and 2x speedup targets were not achieved.

Deferred optimization acceptance procedure:

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
- [ ] Mark the design partial, reuse-deferred (not implemented), and update `docs/superpowers/STATE.md` with exact commits, tests, measurements, and unresolved risks.
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
