# Reference Lifecycle Foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox syntax for tracking.

**Goal:** Remove ordinary per-Instance mapping trees with exact frame parity and
make active-state hazards reproducible, while retaining fresh scene sampling.
**Architecture:** Immutable metadata maps ordinary positions with pinned variant
rounding; outlier counts retain legacy mapping sessions. CPU/model/scene ownership
otherwise stays unchanged. Existing corpus instrument measures before/after.
**Tech Stack:** C++20, CMake/Ninja, MSVC19.44, Python3.9, both pinned rlottie variants.
**Spec:** docs/superpowers/specs/2026-09-23-reference-lifecycle-foundations-design.md

## Global Constraints

- Work directly on main as explicitly authorized; ordinary push only, no history rewrite.
- Deadline07:11:24UTC; no large new block after06:51UTC.
- One product implementer at a time, actual RED before production edits, independent task review.
- Preserve visual scenes, goldens, masks/mattes, geometry, Direct2D ownership, Player threading and public fallback policy.
- No vendor/dependency/license/Windows changes and no persistent renderTree reuse.
- Ordinary creation intentionally changes setup scene sessions1→0 and removes the redundant load's failure opportunity; other errors remain.
- Counts > LONG_MAX retain the old mapping load/path; do not silently reject or reinterpret malformed timelines.
- Six counters count successful constructions/samples; reset is quiescent and does not recreate sessions.
- Keep old Part25A documents as historical evidence, adding a successor note rather than rewriting measurements.
- Samsung scene/plan golden failures are known baseline limitations; report unchanged exact hashes without exclusions.

## Review Focus

- Wrong variant rounding/half-frame behavior: literal ties plus dense differential oracle tests in Task1.
- Fractional/nonzero/negative endpoints and nonfinite inputs: explicit accepted fixtures and wrapper normalization tests in Task1.
- Wrapped malformed Telegram count: legacy-session selection and safe position0 test in Task1.
- Hidden construction from mapping/playback/reset: zero-session diagnostic checks and integration tests in Task1.
- Small active state discarded by approximate comparisons: fresh-scene critical-field checks and independent probe RED in Task2.

## Task 0: Source audit and bounded active-state probe

**Files:** ignored out/part25b-frame-mapping/audit.md and out/part25b-active-state/.
**Interfaces:** inspect upstream frameAtPos and source-inspected epsilon risk;
do not mutate product or old evidence.

- [x] Verify all sceneAnimation uses, variant formulas and parser limits.
- [x] Record that the duplicate load supplies no new ordinary immutable-asset validation; explicitly accept its removal, not a false identical-error claim.
- [x] Finish both-variant scratch active-state probe with exact fields and passing controls; cap30minutes. Both variants:50 comparisons,21 mismatches; three controls pass.

## Task 1: Replace ordinary retained mapping trees

Status:complete at d4217a4; task review approved, evidence-only fix separately
reviewed, controller fresh4/4 gates on each variant. All items below completed.

**Files:** modify src/runtime/Runtime.cpp, CMakeLists.txt,
tests/runtime_seams_tests.cpp, tests/reference_session_tests.cpp,
tests/asset_model_tests.cpp, scripts/test_corpus_lab_output.py, docs/CORPUS_LAB.md.
May create tests/frame_mapping_tests.cpp as a focused independent target (preferred
over growing the seams file); define AVEMOTION_CORPUS_DIR and AVEMOTION_FIXTURE_DIR,
link AveMotion::Runtime and AveMotion::Reference, ref-enabled only.
**Interfaces:** public Runtime/Instance/ReferenceAnimation and existing diagnostics;
private AVEMOTION_REFERENCE_ROUND_FRAME_POSITION=1 for Samsung,0 otherwise.

### RED and differential gate

- [x] Before product edits add a real zero-construction check:

```cpp
runtime.resetDiagnostics(); // loaded valid asset already exists
auto instance = runtime.createInstance(asset);
require(bool(instance), "ordinary instance creation failed");
require(runtime.diagnostics().referenceSceneSessionsCreated == 0U,
        "ordinary instance construction must not create a mapping tree");
```

- [x] Add frame parity against independently loaded ReferenceAnimation for all16
  existing top-level corpus/fixture JSON files; no new corpus data. Normalize
  nonfinite oracle input to0 and clamp its return to totalFrames-1, as the old
  Instance wrapper did. Sample0..1 at i/1000, repeated order, exact integer and
  half-frame thresholds plus nextafter toward0/1. N is small for this enumeration.
- [x] Add in-memory minimal JSON (empty layers is sufficient): w/h64,fr60,
  endpoints(0,10),(100,110),(-10,10),(0.25,10.75), and one-frame fixture with
  op=1 on Telegram/op=0 on Samsung. Compare metadata and positions independently.
  Literal example at ip0/op10 and p=.05: Telegram frame0, Samsung frame1.
  Test -1,2,NaN,+inf,-inf all with old clamping behavior (infinities map0).
- [x] After many mapping/playbackSnapshot calls and a quiescent reset, require no
  metadata/scene/model/CPU session constructions or scene/model samples. Test a
  moved Instance and two independent Instances to catch reliance on moved state.
- [x] Telegram reversed fixture ip2/op1 remains accepted with N>LONG_MAX,
  createInstance creates one legacy Scene session and frameAtPosition(0)==0.
  Samsung still rejects that source. Never exercise undefined negative-result
  floating-to-size_t conversion at positive positions or huge frame loops.
- [x] Null/foreign createInstance still returns InvalidArgument; existing no-ref
  suite keeps ReferenceUnavailable. No synthetic allocator mocks.
- [x] Build/run new target and record expected RED (ordinary construction count1
  instead of0) for both variants; parity assertions should run before the count
  failure or as separate cases so their baseline validity is known.

### Minimal implementation

- [x] Add the private rounding compile definition on avemotion_runtime, choosing
  from AVEMOTION_RLOTTIE_VARIANT, not AVEMOTION_TELEGRAM_PARSED_MODEL.
- [x] Rename sceneAnimation to legacyFrameMappingAnimation. Load it in
  createInstance only if metadata.totalFrames exceeds numeric_limits<long>::max().
  Preserve the existing null-load failure branch and message inside that branch.
- [x] In frameAtPosition use legacy pointer when present, otherwise:

```cpp
const auto& metadata = data_->asset->metadata();
if (metadata.totalFrames <= 1U) return 0U;
const double scaled = clampNormalized(normalizedPosition)
    * static_cast<double>(metadata.totalFrames - 1U);
#if AVEMOTION_REFERENCE_ROUND_FRAME_POSITION
const auto frame = static_cast<std::size_t>(std::round(scaled));
#else
const auto frame = static_cast<std::size_t>(scaled);
#endif
return clampFrame(frame, metadata);
```

- [x] Leave exact extraction, model scan and CPU animation code untouched. Keep
  no-reference mapping0 branch. No new timer/thread/global mutable state.
- [x] Update only intentional setup-count expectations: seams creation0/after
  sample1; reference_session_tests counts==samples (instancesCreated unchanged);
  asset_model_tests afterScene1 instead of2. Preserve all visual/counter isolation
  assertions and add clear comments about outlier versus ordinary paths.
- [x] Validator adds integer --expected-setup-scene-sessions (default0), asserts
  exact supplied count and rejects negative option values; old baseline can use1.
  Run against saved Part25A reports with1 and against newly produced smoke with0.
  Confirm wrong explicit expectation fails. Do not alter corpus instrument/schema.
- [x] Document actual lifecycle/diagnostic change in docs/CORPUS_LAB.md; do not
  rewrite old Part25A report measurements (controller adds successor notes).

### Verify and commit

- [x] Run focused mapping/seams/reference_sessions/asset_model/playback/Player/TGS
  on Telegram and Samsung under VsDevCmd. Then complete Telegram/Samsung suites;
  record known Samsung scene/plan hashes explicitly. Source/corpus/goldens stay.
- [x] Review own diff and git diff --check. Commit only task files with message
  `perf: avoid retained mapping trees for ordinary instances`.
- [x] Report real commit SHA from git rev-parse HEAD, RED/GREEN exact commands,
  raw outputs, variant edge cases and intentional error/counter contract change.
  Controller performs independent task review and fresh focused gate before push.

## Task 2: Preserve minimal active-state regression cases

Status:complete at58236a9, independent task review approved; controller's official
variant tests, fixture hashes and strict16-asset corpus check passed.

**Files:** create selected small JSON under tests/fixtures/reference_sessions/;
modify tests/reference_session_tests.cpp. Controller owns the durable findings
in docs/PART25B_REFERENCE_LIFECYCLE_REPORT.md; implementer writes its task report.
**Interfaces:** existing complete comparator/fresh-oracle harness; no vendor APIs.

- [x] Read completed out/part25b-active-state/findings.md. Promote exactly
  out/part25b-active-state/translation-near-default.json, width.json and opacity.json
  into tests/fixtures/reference_sessions/ with those same filenames/bytes.
  These cover transform constructor-default semantics, style comparison cache,
  and exported alpha respectively; no additional trim fixture is needed here.
- [x] Add them to the existing ascending/reverse/repeated/viewport comparisons;
  do not change comparator exclusions, corpus generator or16 measured inputs.
- [x] Correct the existing CPU-isolation PASS text to sceneSessions=2 (two exact
  samples, no ordinary setup session). This is output text only; keep assertions.
- [x] At128square assert translation frame0 point2/3 x is nonzero and frame1 x
  is exactly0 (authored x changes0.0000012→0.0000005). Telegram local matrix dx
  is0 atframe1. Opacity changes50.19605→50.19610: Telegram paint alpha127→128;
  Samsung active layer opacity127/255→128/255. Locate the active layer reliably.
  Width authored2→2.0000005: assert frame widths differ and match independent
  fresh oracle exactly. Do not hardcode platform-specific getScale decimals.
- [x] Preserve actual direct persistent-session RED evidence for each selected
  fixture on both variants, then run production fresh-path comparisons GREEN.
  This is characterization/test coverage, not a new rendering implementation.
- [x] Run new reference-session test on both variants plus strict corpus
  integrity check. Commit tests/fixtures with no production edits; independent
  task review before push. Controller writes durable concise findings.

## Task 3: Measurements, complete gates and handoff

Status:complete, with explicit pre-existing Samsung/vendor limitations. Whole-change
review approved; minor status fix59592fa scoped-review approved. Technical report
and code pushed through8b9739f; avemotion-6 PAUSED and verified at06:03UTC.

**Files:** create docs/PART25B_REFERENCE_LIFECYCLE_REPORT.md; update STATE/README
and append historical successor notes to Part25A report/findings as needed.
Raw artifacts out/benchmarks/part25b/ and out/part25b-final/ are ignored.

- [x] Before candidate Release rebuild preserve the existing c484f4b binary
  (SHA256614714992fbacd80c1714683080811b66f5192aa4fd8ed8cd70ab42cea514f3b)
  to a unique baseline output path; verify hash. It is valid for273f2b3 because
  src/include/apps instrument are unchanged since c484f4b. Record the evidence.
- [x] Configure/build candidate windows-msvc-corpus-lab with same installed
  toolchain. After workers stop builds/tests run A1,B1,B2,A2, each:

```text
--input tests/compatibility/tgs --output <unique-run-dir> --samples 1000
--warmup-samples 20 --load-repeats 1 --cpu-repeats 0 --render-size 128 --strict
```

- [x] Validate16 assets, phase counts and hashes; validator expected setup1
  forA and0 forB. Fresh1/16/64 memory processes in the same order on StickAndBall;
  include firework if available budget. Report current/peak bytes, no thresholds.
- [x] Report real setup-session reduction and runtime/memory deltas, no2x claim
  or zero-hot-path-allocation promise. Retain A/B raw per-asset median/p95 and
  evaluator/projector observations; control background builds during timing.
- [x] Configure/build/full CTest all four presets: windows-msvc-telegram-debug,
  windows-msvc-samsung-debug, windows-msvc-win32-preview, windows-msvc-direct2d.
  Run local-prefix install/external consumer. Never exclude Samsung failures.
- [x] Independent whole-change review from273f2b3. One fix wave/scoped review
  if needed, fresh final gates after fixes; record all declined claims/rulings.
- [x] Ordinary push main, verify remote equality, final partial/complete scoped
  report and pause avemotion-6 by07:11:24UTC or safe-scope completion.
